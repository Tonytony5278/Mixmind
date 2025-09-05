#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <string>
#include <map>
#include <queue>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <functional>
#include <any>

namespace mixmind::analytics {

// ============================================================================
// PRODUCTION: Analytics & Telemetry System
// Tracks usage patterns, performance metrics, and user behavior
// ============================================================================

class Analytics {
public:
    struct Event {
        std::string name;
        std::map<std::string, std::any> properties;
        std::chrono::system_clock::time_point timestamp;
        std::string sessionId;
        std::string userId;
        
        Event(const std::string& eventName) 
            : name(eventName)
            , timestamp(std::chrono::system_clock::now()) {}
    };
    
    struct UserProfile {
        std::string userId;
        std::string email;
        std::string licenseType;
        std::string version;
        std::string os;
        std::string country;
        std::chrono::system_clock::time_point firstSeen;
        std::chrono::system_clock::time_point lastSeen;
        int totalSessions = 0;
        std::chrono::seconds totalUsageTime{0};
        std::map<std::string, int> featureUsageCounts;
    };
    
    enum class EventPriority {
        LOW,        // Batch upload
        NORMAL,     // Regular batch upload  
        HIGH,       // Upload within 30 seconds
        CRITICAL    // Upload immediately
    };
    
    Analytics();
    ~Analytics();
    
    // Event tracking
    void track(const std::string& eventName, 
               const std::map<std::string, std::any>& properties = {},
               EventPriority priority = EventPriority::NORMAL);
    
    // Convenience methods for common events
    void trackAppStart();
    void trackAppExit();
    void trackFeatureUsage(const std::string& feature, 
                          std::chrono::milliseconds duration = std::chrono::milliseconds(0),
                          bool success = true);
    void trackError(const std::string& error, const std::string& context = "");
    void trackPerformanceMetrics();
    void trackUserAction(const std::string& action, const std::string& context = "");
    void trackAIUsage(const std::string& aiFeature, int tokensUsed = 0, 
                     std::chrono::milliseconds responseTime = std::chrono::milliseconds(0));
    void trackProjectActivity(const std::string& action, const std::string& projectType = "");
    void trackPluginUsage(const std::string& pluginName, const std::string& action = "load");
    
    // Session management
    void startSession();
    void endSession();
    std::string getCurrentSessionId() const { return currentSessionId_; }
    std::chrono::seconds getCurrentSessionDuration() const;
    
    // User identification
    void identifyUser(const std::string& userId, const UserProfile& profile = {});
    void setUserProperty(const std::string& key, const std::any& value);
    UserProfile getCurrentUser() const;
    
    // Configuration
    void setAnalyticsEndpoint(const std::string& endpoint) { analyticsEndpoint_ = endpoint; }
    void setUploadBatchSize(int batchSize) { batchSize_ = batchSize; }
    void setUploadInterval(std::chrono::seconds interval) { uploadInterval_ = interval; }
    void enableAnalytics(bool enabled) { analyticsEnabled_.store(enabled); }
    void enableDebugMode(bool enabled) { debugMode_.store(enabled); }
    
    // Privacy controls
    void optOut() { optedOut_.store(true); }
    void optIn() { optedOut_.store(false); }
    bool isOptedOut() const { return optedOut_.load(); }
    void clearUserData();
    
    // Upload control
    void startPeriodicUpload();
    void stopPeriodicUpload();
    void uploadNow();
    void uploadCriticalEvents();
    
    // Statistics
    struct AnalyticsStats {
        int totalEvents = 0;
        int eventsUploaded = 0;
        int uploadAttempts = 0;
        int failedUploads = 0;
        int pendingEvents = 0;
        std::chrono::system_clock::time_point lastUpload;
        std::chrono::system_clock::time_point lastFailure;
        std::string lastError;
    };
    
    AnalyticsStats getStats() const;
    void resetStats();
    
    // Real-time metrics (for monitoring dashboard)
    struct LiveMetrics {
        double cpuUsage = 0.0;
        double memoryUsageMB = 0.0;
        double audioLatencyMs = 0.0;
        int activePlugins = 0;
        int activeTracks = 0;
        int activeProjects = 1;
        bool aiServiceConnected = false;
        std::string currentFeature = "idle";
        std::chrono::system_clock::time_point lastUpdate;
    };
    
    void updateLiveMetrics(const LiveMetrics& metrics);
    LiveMetrics getCurrentMetrics() const;
    
    // Event filtering and privacy
    using EventFilter = std::function<bool(const Event&)>;
    void addEventFilter(const std::string& name, EventFilter filter);
    void removeEventFilter(const std::string& name);
    
    // Cohort analysis
    void trackCohort(const std::string& cohortName, const std::map<std::string, std::any>& properties = {});
    
    // A/B Testing support
    void trackExperiment(const std::string& experimentName, const std::string& variant);
    std::string getExperimentVariant(const std::string& experimentName) const;
    
private:
    // Event processing
    void processEventQueue();
    void uploadEventBatch(const std::vector<Event>& events);
    bool shouldFilterEvent(const Event& event) const;
    void enrichEvent(Event& event) const;
    
    // Network communication
    struct HttpResponse {
        int statusCode;
        std::string body;
        std::map<std::string, std::string> headers;
    };
    
    core::AsyncResult<HttpResponse> makeHttpRequest(
        const std::string& method,
        const std::string& endpoint,
        const std::string& body,
        const std::map<std::string, std::string>& headers = {}
    );
    
    // Session management
    std::string generateSessionId();
    void updateSessionMetrics();
    
    // System information
    std::string getOSInfo() const;
    std::string getHardwareInfo() const;
    double getCPUUsage() const;
    double getMemoryUsage() const;
    double getAudioLatency() const;
    int getPluginCount() const;
    int getTrackCount() const;
    
    // Data persistence
    void saveEventToLocal(const Event& event);
    std::vector<Event> loadLocalEvents();
    void clearLocalEvents();
    
    // Configuration
    std::string analyticsEndpoint_ = "https://analytics.mixmindai.com";
    int batchSize_ = 50;
    std::chrono::seconds uploadInterval_{300}; // 5 minutes
    std::atomic<bool> analyticsEnabled_{true};
    std::atomic<bool> optedOut_{false};
    std::atomic<bool> debugMode_{false};
    
    // Event queue
    mutable std::mutex eventQueueMutex_;
    std::queue<Event> eventQueue_;
    std::queue<Event> criticalEventQueue_;
    
    // Upload thread
    std::atomic<bool> uploadThreadRunning_{false};
    std::unique_ptr<std::thread> uploadThread_;
    
    // Session state
    std::string currentSessionId_;
    std::chrono::system_clock::time_point sessionStartTime_;
    std::atomic<bool> sessionActive_{false};
    
    // User information
    mutable std::mutex userMutex_;
    UserProfile currentUser_;
    std::map<std::string, std::any> userProperties_;
    
    // Live metrics
    mutable std::mutex metricsMutex_;
    LiveMetrics currentMetrics_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    AnalyticsStats stats_;
    
    // Event filters
    mutable std::mutex filtersMutex_;
    std::map<std::string, EventFilter> eventFilters_;
    
    // A/B Testing
    std::map<std::string, std::string> experimentVariants_;
};

// ============================================================================
// Performance Monitor - Real-time System Performance Tracking
// ============================================================================

class PerformanceMonitor {
public:
    struct PerformanceSnapshot {
        std::chrono::system_clock::time_point timestamp;
        double cpuUsage;
        double memoryUsageMB;
        double audioLatencyMs;
        int audioDropouts;
        int pluginCount;
        int trackCount;
        double diskUsageGB;
        int networkLatencyMs;
        
        // Audio-specific metrics
        double audioThreadCPU;
        int bufferUnderruns;
        int bufferOverruns;
        double sampleRate;
        int bufferSize;
        
        // AI-specific metrics
        int aiRequestsPerMinute;
        double avgAIResponseTime;
        int aiServiceErrors;
        bool aiServiceHealthy;
    };
    
    PerformanceMonitor(Analytics* analytics);
    ~PerformanceMonitor();
    
    void startMonitoring(std::chrono::seconds interval = std::chrono::seconds(30));
    void stopMonitoring();
    
    PerformanceSnapshot takeSnapshot();
    std::vector<PerformanceSnapshot> getRecentSnapshots(int count = 10) const;
    
    // Performance alerts
    using PerformanceAlertCallback = std::function<void(const std::string&, const PerformanceSnapshot&)>;
    void setPerformanceAlert(const std::string& metric, double threshold, PerformanceAlertCallback callback);
    
    // Performance reporting
    void generatePerformanceReport();
    
private:
    void monitoringLoop();
    void checkPerformanceAlerts(const PerformanceSnapshot& snapshot);
    
    Analytics* analytics_;
    std::atomic<bool> monitoring_{false};
    std::unique_ptr<std::thread> monitoringThread_;
    std::chrono::seconds monitoringInterval_;
    
    mutable std::mutex snapshotsMutex_;
    std::deque<PerformanceSnapshot> recentSnapshots_;
    static constexpr int MAX_SNAPSHOTS = 1000;
    
    // Performance alerts
    struct AlertConfig {
        double threshold;
        PerformanceAlertCallback callback;
        std::chrono::system_clock::time_point lastTriggered;
        std::chrono::seconds cooldown{300}; // 5 minutes
    };
    
    std::map<std::string, AlertConfig> performanceAlerts_;
};

// ============================================================================
// Feature Usage Tracker - Track Feature Adoption and Usage Patterns
// ============================================================================

class FeatureTracker {
public:
    struct FeatureUsage {
        std::string featureName;
        int usageCount = 0;
        std::chrono::seconds totalTime{0};
        std::chrono::system_clock::time_point firstUsed;
        std::chrono::system_clock::time_point lastUsed;
        double averageSessionTime = 0.0;
        int errorCount = 0;
        double successRate = 1.0;
        
        void recordUsage(std::chrono::milliseconds duration, bool success = true) {
            usageCount++;
            totalTime += std::chrono::duration_cast<std::chrono::seconds>(duration);
            lastUsed = std::chrono::system_clock::now();
            
            if (!success) {
                errorCount++;
            }
            
            successRate = static_cast<double>(usageCount - errorCount) / usageCount;
            averageSessionTime = static_cast<double>(totalTime.count()) / usageCount;
        }
    };
    
    FeatureTracker(Analytics* analytics);
    
    void trackFeatureStart(const std::string& feature);
    void trackFeatureEnd(const std::string& feature, bool success = true);
    void trackFeatureEvent(const std::string& feature, const std::string& event);
    
    std::map<std::string, FeatureUsage> getFeatureUsageStats() const;
    FeatureUsage getFeatureUsage(const std::string& feature) const;
    
    void generateFeatureReport();
    
private:
    Analytics* analytics_;
    
    mutable std::mutex usageMutex_;
    std::map<std::string, FeatureUsage> featureUsage_;
    std::map<std::string, std::chrono::system_clock::time_point> featureStartTimes_;
};

// ============================================================================
// Global Analytics Instance
// ============================================================================

Analytics& getGlobalAnalytics();
void initializeAnalytics();
void shutdownAnalytics();

// Convenience macros for analytics
#define TRACK_EVENT(name, ...) getGlobalAnalytics().track(name, ##__VA_ARGS__)
#define TRACK_FEATURE(feature, duration, success) getGlobalAnalytics().trackFeatureUsage(feature, duration, success)
#define TRACK_ERROR(error, context) getGlobalAnalytics().trackError(error, context)
#define TRACK_USER_ACTION(action, context) getGlobalAnalytics().trackUserAction(action, context)
#define TRACK_AI_USAGE(feature, tokens, time) getGlobalAnalytics().trackAIUsage(feature, tokens, time)

} // namespace mixmind::analytics