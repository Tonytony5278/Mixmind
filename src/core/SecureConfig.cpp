#include "SecureConfig.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "advapi32.lib")
#endif

namespace mixmind::core {

// ============================================================================
// SecureConfig Implementation
// ============================================================================

SecureConfig::SecureConfig() = default;
SecureConfig::~SecureConfig() = default;

bool SecureConfig::initialize() {
    if (initialized_.load()) {
        return true;
    }
    
    try {
        // Generate machine-specific encryption key
        machineKey_ = generateMachineKey();
        
        // Load safe configuration from environment
        loadFromEnvironment();
        
        initialized_.store(true);
        std::cout << "🔒 Secure configuration initialized" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to initialize secure config: " << e.what() << std::endl;
        return false;
    }
}

void SecureConfig::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    // Clear sensitive data from memory
    {
        std::lock_guard<std::mutex> lock(keysCacheMutex_);
        encryptedKeysCache_.clear();
    }
    
    machineKey_.clear();
    initialized_.store(false);
}

Result<std::string> SecureConfig::getAPIKey(const std::string& service) {
    if (!initialized_.load()) {
        return Result<std::string>::failure("Secure config not initialized");
    }
    
    // Check encrypted cache first
    {
        std::lock_guard<std::mutex> lock(keysCacheMutex_);
        auto it = encryptedKeysCache_.find(service);
        if (it != encryptedKeysCache_.end()) {
            try {
                std::string decrypted = decrypt(it->second);
                if (!decrypted.empty()) {
                    return Result<std::string>::success(decrypted);
                }
            } catch (...) {
                // Cache corrupted, try fresh retrieval
            }
        }
    }
    
    // Retrieve from secure storage
    auto result = retrieveSecurely(service);
    if (result.isSuccess()) {
        // Cache encrypted version
        try {
            std::string encrypted = encrypt(result.getValue());
            std::lock_guard<std::mutex> lock(keysCacheMutex_);
            encryptedKeysCache_[service] = encrypted;
        } catch (...) {
            // Encryption failed but we still have the key
        }
    }
    
    return result;
}

bool SecureConfig::setAPIKey(const std::string& service, const std::string& key) {
    if (!initialized_.load()) {
        return false;
    }
    
    if (key.empty() || key == "USE_SECURE_STORAGE") {
        return false; // Invalid key
    }
    
    // Validate key format
    if (service == "openai" && !key.starts_with("sk-")) {
        std::cerr << "⚠️ Invalid OpenAI API key format" << std::endl;
        return false;
    }
    
    if (service == "anthropic" && !key.starts_with("sk-ant-")) {
        std::cerr << "⚠️ Invalid Anthropic API key format" << std::endl;
        return false;
    }
    
    // Store securely
    if (storeSecurely(service, key)) {
        // Update cache
        try {
            std::string encrypted = encrypt(key);
            std::lock_guard<std::mutex> lock(keysCacheMutex_);
            encryptedKeysCache_[service] = encrypted;
        } catch (...) {
            // Cache update failed but key was stored
        }
        
        std::cout << "✅ API key for " << service << " stored securely" << std::endl;
        return true;
    }
    
    return false;
}

bool SecureConfig::hasAPIKey(const std::string& service) const {
    if (!initialized_.load()) {
        return false;
    }
    
    // Quick check in cache first
    {
        std::lock_guard<std::mutex> lock(keysCacheMutex_);
        if (encryptedKeysCache_.count(service) > 0) {
            return true;
        }
    }
    
    // Check secure storage
    auto result = const_cast<SecureConfig*>(this)->retrieveSecurely(service);
    return result.isSuccess() && !result.getValue().empty();
}

void SecureConfig::clearAPIKey(const std::string& service) {
    if (!initialized_.load()) {
        return;
    }
    
    // Remove from cache
    {
        std::lock_guard<std::mutex> lock(keysCacheMutex_);
        encryptedKeysCache_.erase(service);
    }
    
    // Remove from secure storage
    deleteSecurely(service);
    
    std::cout << "🗑️ API key for " << service << " cleared" << std::endl;
}

bool SecureConfig::loadFromEnvironment() {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    // Load safe configuration values (NOT API keys)
    auto loadEnvVar = [this](const char* envVar, const std::string& key, const std::string& defaultVal = "") {
        if (const char* value = std::getenv(envVar)) {
            std::string strValue = value;
            // Skip API keys - they should be handled securely
            if (strValue != "USE_SECURE_STORAGE") {
                safeConfig_[key] = strValue;
            }
        } else if (!defaultVal.empty()) {
            safeConfig_[key] = defaultVal;
        }
    };
    
    // Audio settings
    loadEnvVar("AUDIO_SAMPLE_RATE", "audio.sample_rate", "48000");
    loadEnvVar("AUDIO_BUFFER_SIZE", "audio.buffer_size", "128");
    loadEnvVar("AUDIO_LATENCY_MS", "audio.latency_ms", "3");
    
    // AI settings (safe ones only)
    loadEnvVar("AI_MODEL", "ai.model", "gpt-4-turbo-preview");
    loadEnvVar("AI_VOICE_MODEL", "ai.voice_model", "whisper-1");
    loadEnvVar("AI_TEMPERATURE", "ai.temperature", "0.3");
    loadEnvVar("AI_MAX_TOKENS", "ai.max_tokens", "500");
    
    // Feature flags
    loadEnvVar("ENABLE_VOICE_CONTROL", "features.voice_control", "true");
    loadEnvVar("ENABLE_STYLE_TRANSFER", "features.style_transfer", "true");
    loadEnvVar("ENABLE_AI_MASTERING", "features.ai_mastering", "true");
    loadEnvVar("ENABLE_PROACTIVE_SUGGESTIONS", "features.proactive", "true");
    
    // Paths
    loadEnvVar("VST3_SCAN_PATH", "paths.vst3", "C:\\Program Files\\Common Files\\VST3");
    loadEnvVar("MODELS_PATH", "paths.models", "models");
    loadEnvVar("PRESETS_PATH", "paths.presets", "presets");
    loadEnvVar("PROJECTS_PATH", "paths.projects", "projects");
    
    return true;
}

Result<std::string> SecureConfig::getString(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(configMutex_);
    auto it = safeConfig_.find(key);
    return Result<std::string>::success(it != safeConfig_.end() ? it->second : defaultValue);
}

Result<int> SecureConfig::getInt(const std::string& key, int defaultValue) {
    auto strResult = getString(key);
    if (strResult.isSuccess()) {
        try {
            return Result<int>::success(std::stoi(strResult.getValue()));
        } catch (...) {
            // Invalid integer
        }
    }
    return Result<int>::success(defaultValue);
}

Result<bool> SecureConfig::getBool(const std::string& key, bool defaultValue) {
    auto strResult = getString(key);
    if (strResult.isSuccess()) {
        std::string value = strResult.getValue();
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            return Result<bool>::success(true);
        } else if (value == "false" || value == "0" || value == "no" || value == "off") {
            return Result<bool>::success(false);
        }
    }
    return Result<bool>::success(defaultValue);
}

SecureConfig::ConfigSummary SecureConfig::getSummary() const {
    ConfigSummary summary;
    summary.hasOpenAIKey = hasAPIKey("openai");
    summary.hasAnthropicKey = hasAPIKey("anthropic");
    
    std::lock_guard<std::mutex> lock(configMutex_);
    summary.safeSettings = safeConfig_;
    
    return summary;
}

// Platform-specific secure storage implementation
#ifdef _WIN32

bool SecureConfig::storeSecurely(const std::string& service, const std::string& key) {
    std::wstring target = L"MixMindAI:" + std::wstring(service.begin(), service.end());
    std::wstring keyW(key.begin(), key.end());
    
    CREDENTIAL cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(keyW.size() * sizeof(wchar_t));
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<wchar_t*>(keyW.c_str()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    
    return CredWriteW(&cred, 0) != 0;
}

Result<std::string> SecureConfig::retrieveSecurely(const std::string& service) {
    std::wstring target = L"MixMindAI:" + std::wstring(service.begin(), service.end());
    
    PCREDENTIALW pcred = nullptr;
    if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &pcred)) {
        std::wstring keyW(reinterpret_cast<wchar_t*>(pcred->CredentialBlob), 
                         pcred->CredentialBlobSize / sizeof(wchar_t));
        std::string key(keyW.begin(), keyW.end());
        
        CredFree(pcred);
        return Result<std::string>::success(key);
    }
    
    return Result<std::string>::failure("API key not found in secure storage");
}

void SecureConfig::deleteSecurely(const std::string& service) {
    std::wstring target = L"MixMindAI:" + std::wstring(service.begin(), service.end());
    CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

#else
// macOS/Linux implementation would use Keychain/libsecret
bool SecureConfig::storeSecurely(const std::string& service, const std::string& key) {
    // TODO: Implement macOS Keychain/Linux libsecret
    std::cerr << "⚠️ Secure storage not implemented on this platform" << std::endl;
    return false;
}

Result<std::string> SecureConfig::retrieveSecurely(const std::string& service) {
    return Result<std::string>::failure("Secure storage not implemented on this platform");
}

void SecureConfig::deleteSecurely(const std::string& service) {
    // TODO: Implement
}
#endif

std::string SecureConfig::generateMachineKey() const {
    // Generate a key based on machine characteristics
    std::stringstream ss;
    
#ifdef _WIN32
    // Use Windows machine GUID
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char machineGuid[256];
        DWORD size = sizeof(machineGuid);
        if (RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr, reinterpret_cast<LPBYTE>(machineGuid), &size) == ERROR_SUCCESS) {
            ss << machineGuid;
        }
        RegCloseKey(hKey);
    }
#endif
    
    // Fallback to process-specific key
    ss << std::hash<std::string>{}("MixMindAI") << std::chrono::steady_clock::now().time_since_epoch().count();
    
    return ss.str();
}

std::string SecureConfig::encrypt(const std::string& data) const {
    if (!encryptionEnabled_.load()) {
        return data;
    }
    
    // Simple XOR encryption with machine key (for cache protection)
    std::string result = data;
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] ^= machineKey_[i % machineKey_.size()];
    }
    
    return result;
}

std::string SecureConfig::decrypt(const std::string& encrypted) const {
    if (!encryptionEnabled_.load()) {
        return encrypted;
    }
    
    // XOR decryption (same as encryption)
    return encrypt(encrypted);
}

// ============================================================================
// APIRateLimiter Implementation
// ============================================================================

APIRateLimiter::APIRateLimiter(int maxRequestsPerMinute, int maxRequestsPerHour) 
    : maxRequestsPerMinute_(maxRequestsPerMinute)
    , maxRequestsPerHour_(maxRequestsPerHour) {
}

APIRateLimiter::~APIRateLimiter() = default;

bool APIRateLimiter::canMakeRequest(const std::string& service) {
    if (emergencyStop_.load()) {
        return false;
    }
    
    cleanupOldRecords();
    
    std::lock_guard<std::mutex> lock(recordsMutex_);
    auto& records = serviceRecords_[service];
    
    auto now = std::chrono::system_clock::now();
    auto oneMinuteAgo = now - std::chrono::minutes(1);
    auto oneHourAgo = now - std::chrono::hours(1);
    
    // Count recent requests
    int requestsThisMinute = 0;
    int requestsThisHour = 0;
    double costThisHour = 0.0;
    
    for (const auto& record : records) {
        if (record.timestamp > oneHourAgo) {
            requestsThisHour++;
            costThisHour += record.costUSD;
            
            if (record.timestamp > oneMinuteAgo) {
                requestsThisMinute++;
            }
        }
    }
    
    // Check limits
    if (requestsThisMinute >= maxRequestsPerMinute_) {
        return false;
    }
    
    if (requestsThisHour >= maxRequestsPerHour_) {
        return false;
    }
    
    if (costThisHour >= maxHourlyCost_.load()) {
        return false;
    }
    
    return true;
}

void APIRateLimiter::recordRequest(const std::string& service, double costUSD) {
    std::lock_guard<std::mutex> lock(recordsMutex_);
    
    RequestRecord record;
    record.timestamp = std::chrono::system_clock::now();
    record.costUSD = costUSD;
    
    serviceRecords_[service].push_back(record);
    
    // Check cost alerts
    if (costAlertCallback_) {
        checkCostAlerts(service, costUSD);
    }
}

void APIRateLimiter::cleanupOldRecords() {
    std::lock_guard<std::mutex> lock(recordsMutex_);
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24);
    
    for (auto& [service, records] : serviceRecords_) {
        records.erase(
            std::remove_if(records.begin(), records.end(),
                [cutoff](const RequestRecord& record) {
                    return record.timestamp < cutoff;
                }),
            records.end());
    }
}

void APIRateLimiter::checkCostAlerts(const std::string& service, double newCost) {
    // Calculate recent costs
    auto usage = getUsage(service);
    
    if (usage.costThisHour > 10.0) {
        costAlertCallback_(service, usage.costThisHour, "High hourly cost: $" + std::to_string(usage.costThisHour));
    }
    
    if (usage.costToday > 100.0) {
        costAlertCallback_(service, usage.costToday, "High daily cost: $" + std::to_string(usage.costToday));
    }
}

APIRateLimiter::UsageStats APIRateLimiter::getUsage(const std::string& service) const {
    std::lock_guard<std::mutex> lock(recordsMutex_);
    
    UsageStats stats;
    auto it = serviceRecords_.find(service);
    if (it == serviceRecords_.end()) {
        return stats;
    }
    
    auto now = std::chrono::system_clock::now();
    auto oneMinuteAgo = now - std::chrono::minutes(1);
    auto oneHourAgo = now - std::chrono::hours(1);
    auto oneDayAgo = now - std::chrono::hours(24);
    
    for (const auto& record : it->second) {
        stats.totalCost += record.costUSD;
        
        if (record.timestamp > oneDayAgo) {
            stats.requestsToday++;
            stats.costToday += record.costUSD;
            
            if (record.timestamp > oneHourAgo) {
                stats.requestsThisHour++;
                stats.costThisHour += record.costUSD;
                
                if (record.timestamp > oneMinuteAgo) {
                    stats.requestsThisMinute++;
                }
            }
        }
    }
    
    return stats;
}

// ============================================================================
// Global instances
// ============================================================================

static std::unique_ptr<SecureConfig> g_secureConfig;
static std::unique_ptr<APIRateLimiter> g_rateLimiter;
static std::unique_ptr<SecureAPIClient> g_secureAPIClient;
static std::mutex g_securityMutex;

SecureConfig& getGlobalSecureConfig() {
    std::lock_guard<std::mutex> lock(g_securityMutex);
    if (!g_secureConfig) {
        g_secureConfig = std::make_unique<SecureConfig>();
    }
    return *g_secureConfig;
}

APIRateLimiter& getGlobalRateLimiter() {
    std::lock_guard<std::mutex> lock(g_securityMutex);
    if (!g_rateLimiter) {
        g_rateLimiter = std::make_unique<APIRateLimiter>();
    }
    return *g_rateLimiter;
}

bool initializeSecuritySystem() {
    try {
        auto& config = getGlobalSecureConfig();
        auto& rateLimiter = getGlobalRateLimiter();
        
        if (!config.initialize()) {
            return false;
        }
        
        // Set up cost alerts
        rateLimiter.setCostAlertCallback([](const std::string& service, double cost, const std::string& alert) {
            std::cout << "💰 COST ALERT [" << service << "]: " << alert << std::endl;
        });
        
        std::cout << "🔐 Security system initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to initialize security system: " << e.what() << std::endl;
        return false;
    }
}

void shutdownSecuritySystem() {
    std::lock_guard<std::mutex> lock(g_securityMutex);
    
    if (g_secureAPIClient) {
        g_secureAPIClient.reset();
    }
    
    if (g_rateLimiter) {
        g_rateLimiter.reset();
    }
    
    if (g_secureConfig) {
        g_secureConfig->shutdown();
        g_secureConfig.reset();
    }
}

} // namespace mixmind::core