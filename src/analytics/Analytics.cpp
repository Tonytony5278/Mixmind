#include "Analytics.h"
#include "../core/logging.h"
#include <json/json.h>
#include <curl/curl.h>
#include <sstream>
#include <fstream>
#include <random>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#endif

namespace mixmind::analytics {

static std::unique_ptr<Analytics> g_analytics;

// ============================================================================
// Analytics Implementation
// ============================================================================

Analytics::Analytics() {
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Generate initial session ID
    currentSessionId_ = generateSessionId();
    
    LOG_INFO("Analytics system initialized");
}

Analytics::~Analytics() {
    stopPeriodicUpload();
    
    // Upload any remaining events
    if (!optedOut_.load() && analyticsEnabled_.load()) {
        uploadNow();
    }
    
    curl_global_cleanup();
}

void Analytics::track(const std::string& eventName, 
                     const std::map<std::string, std::any>& properties,
                     EventPriority priority) {
    
    if (optedOut_.load() || !analyticsEnabled_.load()) {
        return;
    }
    
    Event event(eventName);
    event.properties = properties;
    event.sessionId = currentSessionId_;
    event.userId = currentUser_.userId;
    
    // Enrich event with context
    enrichEvent(event);
    
    // Apply event filters
    if (!shouldFilterEvent(event)) {
        if (debugMode_.load()) {
            LOG_INFO("Filtered event: " + eventName);
        }
        return;
    }
    
    // Queue event based on priority
    {
        std::lock_guard<std::mutex> lock(eventQueueMutex_);
        
        if (priority == EventPriority::CRITICAL) {
            criticalEventQueue_.push(event);
        } else {
            eventQueue_.push(event);
        }
        
        stats_.totalEvents++;
    }
    
    // Save locally for offline resilience
    saveEventToLocal(event);
    
    // Handle critical events immediately
    if (priority == EventPriority::CRITICAL) {
        uploadCriticalEvents();
    }
    
    if (debugMode_.load()) {
        LOG_INFO("Tracked event: " + eventName);
    }
}

void Analytics::trackAppStart() {
    startSession();
    
    std::map<std::string, std::any> properties = {
        {"os", getOSInfo()},
        {"hardware", getHardwareInfo()},
        {"memory_gb", getMemoryUsage() / 1024.0},
        {"cpu_cores", std::thread::hardware_concurrency()}
    };
    
    track("app_started", properties, EventPriority::HIGH);
}

void Analytics::trackAppExit() {
    auto sessionDuration = getCurrentSessionDuration();
    
    std::map<std::string, std::any> properties = {
        {"session_duration_seconds", static_cast<int>(sessionDuration.count())},
        {"clean_exit", true}
    };
    
    track("app_exited", properties, EventPriority::CRITICAL);
    endSession();
}

void Analytics::trackFeatureUsage(const std::string& feature, 
                                 std::chrono::milliseconds duration,
                                 bool success) {
    std::map<std::string, std::any> properties = {
        {"feature", feature},
        {"duration_ms", static_cast<int>(duration.count())},
        {"success", success}
    };
    
    if (!success) {
        properties["error"] = true;
    }
    
    track("feature_used", properties);
    
    // Update user feature usage count
    {
        std::lock_guard<std::mutex> lock(userMutex_);
        currentUser_.featureUsageCounts[feature]++;
    }
}

void Analytics::trackError(const std::string& error, const std::string& context) {
    std::map<std::string, std::any> properties = {
        {"error", error},
        {"context", context},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    
    track("error_occurred", properties, EventPriority::HIGH);
}

void Analytics::trackPerformanceMetrics() {
    std::map<std::string, std::any> properties = {
        {"cpu_usage", getCPUUsage()},
        {"memory_mb", getMemoryUsage()},
        {"audio_latency_ms", getAudioLatency()},
        {"plugin_count", getPluginCount()},
        {"track_count", getTrackCount()}
    };
    
    track("performance_snapshot", properties);
}

void Analytics::trackUserAction(const std::string& action, const std::string& context) {
    std::map<std::string, std::any> properties = {
        {"action", action},
        {"context", context}
    };
    
    track("user_action", properties);
}

void Analytics::trackAIUsage(const std::string& aiFeature, int tokensUsed, 
                            std::chrono::milliseconds responseTime) {
    std::map<std::string, std::any> properties = {
        {"ai_feature", aiFeature},
        {"tokens_used", tokensUsed},
        {"response_time_ms", static_cast<int>(responseTime.count())},
        {"cost_estimate", tokensUsed * 0.00002} // Rough OpenAI GPT-4 pricing
    };
    
    track("ai_feature_used", properties);
}

void Analytics::trackProjectActivity(const std::string& action, const std::string& projectType) {
    std::map<std::string, std::any> properties = {
        {"action", action},
        {"project_type", projectType}
    };
    
    track("project_activity", properties);
}

void Analytics::trackPluginUsage(const std::string& pluginName, const std::string& action) {
    std::map<std::string, std::any> properties = {
        {"plugin_name", pluginName},
        {"action", action}
    };
    
    track("plugin_used", properties);
}

void Analytics::startSession() {
    currentSessionId_ = generateSessionId();
    sessionStartTime_ = std::chrono::system_clock::now();
    sessionActive_.store(true);
    
    {
        std::lock_guard<std::mutex> lock(userMutex_);
        currentUser_.totalSessions++;
        currentUser_.lastSeen = sessionStartTime_;
        
        if (currentUser_.firstSeen.time_since_epoch().count() == 0) {
            currentUser_.firstSeen = sessionStartTime_;
        }
    }
    
    LOG_INFO("Analytics session started: " + currentSessionId_);
}

void Analytics::endSession() {
    if (!sessionActive_.load()) return;
    
    auto sessionDuration = getCurrentSessionDuration();
    
    {
        std::lock_guard<std::mutex> lock(userMutex_);
        currentUser_.totalUsageTime += sessionDuration;
    }
    
    sessionActive_.store(false);
    
    LOG_INFO("Analytics session ended, duration: " + std::to_string(sessionDuration.count()) + "s");
}

std::chrono::seconds Analytics::getCurrentSessionDuration() const {
    if (!sessionActive_.load()) return std::chrono::seconds(0);
    
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - sessionStartTime_);
}

void Analytics::identifyUser(const std::string& userId, const UserProfile& profile) {
    std::lock_guard<std::mutex> lock(userMutex_);
    
    currentUser_.userId = userId;
    if (!profile.email.empty()) currentUser_.email = profile.email;
    if (!profile.licenseType.empty()) currentUser_.licenseType = profile.licenseType;
    if (!profile.version.empty()) currentUser_.version = profile.version;
    if (!profile.os.empty()) currentUser_.os = profile.os;
    if (!profile.country.empty()) currentUser_.country = profile.country;
    
    LOG_INFO("User identified: " + userId);
}

void Analytics::setUserProperty(const std::string& key, const std::any& value) {
    std::lock_guard<std::mutex> lock(userMutex_);
    userProperties_[key] = value;
}

Analytics::UserProfile Analytics::getCurrentUser() const {
    std::lock_guard<std::mutex> lock(userMutex_);
    return currentUser_;
}

void Analytics::startPeriodicUpload() {
    if (uploadThreadRunning_.load()) return;
    
    uploadThreadRunning_.store(true);
    uploadThread_ = std::make_unique<std::thread>(&Analytics::processEventQueue, this);
    
    LOG_INFO("Periodic analytics upload started");
}

void Analytics::stopPeriodicUpload() {
    if (!uploadThreadRunning_.load()) return;
    
    uploadThreadRunning_.store(false);
    
    if (uploadThread_ && uploadThread_->joinable()) {
        uploadThread_->join();
    }
    
    uploadThread_.reset();
    
    LOG_INFO("Periodic analytics upload stopped");
}

void Analytics::uploadNow() {
    if (optedOut_.load() || !analyticsEnabled_.load()) return;
    
    std::vector<Event> events;
    
    {
        std::lock_guard<std::mutex> lock(eventQueueMutex_);
        
        // Collect all events
        while (!eventQueue_.empty()) {
            events.push_back(eventQueue_.front());
            eventQueue_.pop();
        }
        
        while (!criticalEventQueue_.empty()) {
            events.push_back(criticalEventQueue_.front());
            criticalEventQueue_.pop();
        }
    }
    
    if (!events.empty()) {
        uploadEventBatch(events);
    }
}

void Analytics::uploadCriticalEvents() {
    if (optedOut_.load() || !analyticsEnabled_.load()) return;
    
    std::vector<Event> events;
    
    {
        std::lock_guard<std::mutex> lock(eventQueueMutex_);
        
        while (!criticalEventQueue_.empty()) {
            events.push_back(criticalEventQueue_.front());
            criticalEventQueue_.pop();
        }
    }
    
    if (!events.empty()) {
        uploadEventBatch(events);
    }
}

void Analytics::processEventQueue() {
    while (uploadThreadRunning_.load()) {
        try {
            std::vector<Event> batch;
            
            {
                std::lock_guard<std::mutex> lock(eventQueueMutex_);
                
                // Collect batch
                int batchCount = 0;
                while (!eventQueue_.empty() && batchCount < batchSize_) {
                    batch.push_back(eventQueue_.front());
                    eventQueue_.pop();
                    batchCount++;
                }
            }
            
            if (!batch.empty() && !optedOut_.load() && analyticsEnabled_.load()) {
                uploadEventBatch(batch);
            }
            
            // Wait for next upload interval
            std::this_thread::sleep_for(uploadInterval_);
            
        } catch (const std::exception& e) {
            LOG_ERROR("Analytics upload error: " + std::string(e.what()));
            
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.lastError = e.what();
                stats_.lastFailure = std::chrono::system_clock::now();
            }
        }
    }
}

void Analytics::uploadEventBatch(const std::vector<Event>& events) {
    if (events.empty()) return;
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.uploadAttempts++;
    }
    
    // Convert events to JSON
    Json::Value batch(Json::arrayValue);
    
    for (const auto& event : events) {
        Json::Value eventJson;
        eventJson["name"] = event.name;
        eventJson["session_id"] = event.sessionId;
        eventJson["user_id"] = event.userId;
        eventJson["timestamp"] = static_cast<int64_t>(
            std::chrono::system_clock::to_time_t(event.timestamp));
        
        // Convert properties
        Json::Value properties;
        for (const auto& [key, value] : event.properties) {
            try {
                if (value.type() == typeid(std::string)) {
                    properties[key] = std::any_cast<std::string>(value);
                } else if (value.type() == typeid(int)) {
                    properties[key] = std::any_cast<int>(value);
                } else if (value.type() == typeid(double)) {
                    properties[key] = std::any_cast<double>(value);
                } else if (value.type() == typeid(bool)) {
                    properties[key] = std::any_cast<bool>(value);
                }
            } catch (const std::bad_any_cast&) {
                // Skip properties we can't serialize
            }
        }
        eventJson["properties"] = properties;
        
        batch.append(eventJson);
    }
    
    // Upload batch
    std::string requestBody = batch.toStyledString();
    
    auto response = makeHttpRequest("POST", "/events", requestBody);
    
    if (response) {
        auto httpResponse = response.get();
        
        if (httpResponse.statusCode == 200) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.eventsUploaded += static_cast<int>(events.size());
            stats_.lastUpload = std::chrono::system_clock::now();
            
            if (debugMode_.load()) {
                LOG_INFO("Uploaded " + std::to_string(events.size()) + " events");
            }
        } else {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.failedUploads++;
            stats_.lastError = "HTTP " + std::to_string(httpResponse.statusCode);
            
            LOG_WARNING("Analytics upload failed: " + std::to_string(httpResponse.statusCode));
        }
    } else {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.failedUploads++;
        stats_.lastError = response.error();
        
        LOG_ERROR("Analytics upload error: " + response.error());
    }
}

bool Analytics::shouldFilterEvent(const Event& event) const {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    
    for (const auto& [name, filter] : eventFilters_) {
        if (!filter(event)) {
            return false;
        }
    }
    
    return true;
}

void Analytics::enrichEvent(Event& event) const {
    // Add system context
    event.properties["os"] = getOSInfo();
    event.properties["session_duration"] = static_cast<int>(getCurrentSessionDuration().count());
    
    // Add user context
    {
        std::lock_guard<std::mutex> lock(userMutex_);
        if (!currentUser_.licenseType.empty()) {
            event.properties["license_type"] = currentUser_.licenseType;
        }
        if (!currentUser_.version.empty()) {
            event.properties["app_version"] = currentUser_.version;
        }
    }
    
    // Add performance context
    {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        if (currentMetrics_.lastUpdate.time_since_epoch().count() > 0) {
            event.properties["cpu_usage"] = currentMetrics_.cpuUsage;
            event.properties["memory_mb"] = currentMetrics_.memoryUsageMB;
        }
    }
}

std::string Analytics::generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::string sessionId;
    for (int i = 0; i < 32; i++) {
        sessionId += "0123456789abcdef"[dis(gen)];
    }
    
    return sessionId;
}

std::string Analytics::getOSInfo() const {
#ifdef _WIN32
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    GetVersionEx((OSVERSIONINFO*)&osvi);
    
    return "Windows " + std::to_string(osvi.dwMajorVersion) + "." + 
           std::to_string(osvi.dwMinorVersion);
#else
    return "Unknown";
#endif
}

std::string Analytics::getHardwareInfo() const {
    std::stringstream ss;
    ss << "CPU cores: " << std::thread::hardware_concurrency();
    
#ifdef _WIN32
    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);
    ss << ", RAM: " << (memInfo.ullTotalPhys / (1024 * 1024 * 1024)) << "GB";
#endif
    
    return ss.str();
}

double Analytics::getCPUUsage() const {
#ifdef _WIN32
    static PDH_HQUERY cpuQuery;
    static PDH_HCOUNTER cpuTotal;
    static bool initialized = false;
    
    if (!initialized) {
        PdhOpenQuery(NULL, NULL, &cpuQuery);
        PdhAddCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
        PdhCollectQueryData(cpuQuery);
        initialized = true;
    }
    
    PdhCollectQueryData(cpuQuery);
    
    PDH_FMT_COUNTERVALUE counterVal;
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    
    return counterVal.doubleValue;
#else
    return 0.0;
#endif
}

double Analytics::getMemoryUsage() const {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024 * 1024); // Convert to MB
    }
#endif
    return 0.0;
}

core::AsyncResult<Analytics::HttpResponse> Analytics::makeHttpRequest(
    const std::string& method,
    const std::string& endpoint,
    const std::string& body,
    const std::map<std::string, std::string>& headers) {
    
    return core::async([this, method, endpoint, body, headers]() -> HttpResponse {
        CURL* curl = curl_easy_init();
        HttpResponse response;
        
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        std::string url = analyticsEndpoint_ + endpoint;
        std::string responseBody;
        
        // Set CURL options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            [](void* contents, size_t size, size_t nmemb, std::string* body) -> size_t {
                size_t totalSize = size * nmemb;
                body->append(static_cast<char*>(contents), totalSize);
                return totalSize;
            });
        
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
        
        // Set headers
        struct curl_slist* headerList = nullptr;
        headerList = curl_slist_append(headerList, "Content-Type: application/json");
        
        for (const auto& [key, value] : headers) {
            std::string header = key + ": " + value;
            headerList = curl_slist_append(headerList, header.c_str());
        }
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        
        // Perform request
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            long statusCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
            response.statusCode = static_cast<int>(statusCode);
            response.body = responseBody;
        } else {
            response.statusCode = 0;
            response.body = curl_easy_strerror(res);
        }
        
        curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);
        
        return response;
    });
}

Analytics::AnalyticsStats Analytics::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    auto stats = stats_;
    
    // Add current queue size
    {
        std::lock_guard<std::mutex> queueLock(eventQueueMutex_);
        stats.pendingEvents = static_cast<int>(eventQueue_.size() + criticalEventQueue_.size());
    }
    
    return stats;
}

// ============================================================================
// Global Analytics Instance
// ============================================================================

Analytics& getGlobalAnalytics() {
    if (!g_analytics) {
        throw std::runtime_error("Analytics system not initialized");
    }
    return *g_analytics;
}

void initializeAnalytics() {
    if (!g_analytics) {
        g_analytics = std::make_unique<Analytics>();
        g_analytics->startPeriodicUpload();
        LOG_INFO("Global analytics initialized");
    }
}

void shutdownAnalytics() {
    if (g_analytics) {
        g_analytics.reset();
        LOG_INFO("Analytics system shutdown");
    }
}

} // namespace mixmind::analytics