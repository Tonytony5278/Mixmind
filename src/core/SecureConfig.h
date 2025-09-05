#pragma once

#include "result.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

namespace mixmind::core {

// ============================================================================
// CRITICAL SECURITY: Secure Configuration Manager
// - Never stores API keys in source code or config files
// - Uses OS credential stores (Windows Credential Manager, macOS Keychain)
// - Implements rate limiting to prevent API abuse
// - Provides encrypted local cache for performance
// ============================================================================

class SecureConfig {
public:
    SecureConfig();
    ~SecureConfig();
    
    // Non-copyable due to security concerns
    SecureConfig(const SecureConfig&) = delete;
    SecureConfig& operator=(const SecureConfig&) = delete;
    
    // Initialize secure configuration system
    bool initialize();
    void shutdown();
    
    // API Key Management (SECURE)
    Result<std::string> getAPIKey(const std::string& service);  // "openai", "anthropic"
    bool setAPIKey(const std::string& service, const std::string& key);
    bool hasAPIKey(const std::string& service) const;
    void clearAPIKey(const std::string& service);
    
    // Configuration Values (safe to store)
    Result<std::string> getString(const std::string& key, const std::string& defaultValue = "");
    Result<int> getInt(const std::string& key, int defaultValue = 0);
    Result<double> getDouble(const std::string& key, double defaultValue = 0.0);
    Result<bool> getBool(const std::string& key, bool defaultValue = false);
    
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setDouble(const std::string& key, double value);
    void setBool(const std::string& key, bool value);
    
    // Load configuration from safe sources
    bool loadFromEnvironment();
    bool loadFromConfigFile(const std::string& filePath);
    bool saveToConfigFile(const std::string& filePath) const; // Never saves API keys
    
    // Security features
    bool isSecure() const { return initialized_.load(); }
    void enableEncryption(bool enable) { encryptionEnabled_.store(enable); }
    
    // Get configuration summary (NO SENSITIVE DATA)
    struct ConfigSummary {
        bool hasOpenAIKey = false;
        bool hasAnthropicKey = false;
        std::unordered_map<std::string, std::string> safeSettings;
    };
    
    ConfigSummary getSummary() const;
    
private:
    // Platform-specific secure storage
    bool storeSecurely(const std::string& service, const std::string& key);
    Result<std::string> retrieveSecurely(const std::string& service);
    void deleteSecurely(const std::string& service);
    
    // Encryption for local cache
    std::string encrypt(const std::string& data) const;
    std::string decrypt(const std::string& encrypted) const;
    
    // Generate encryption key from machine characteristics
    std::string generateMachineKey() const;
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> encryptionEnabled_{true};
    
    mutable std::mutex configMutex_;
    std::unordered_map<std::string, std::string> safeConfig_; // Never contains API keys
    
    // Cache for API keys (encrypted in memory)
    mutable std::mutex keysCacheMutex_;
    std::unordered_map<std::string, std::string> encryptedKeysCache_;
    
    std::string machineKey_;
};

// ============================================================================
// API Rate Limiter - Prevents Bankruptcy from API Abuse
// ============================================================================

class APIRateLimiter {
public:
    APIRateLimiter(int maxRequestsPerMinute = 20, int maxRequestsPerHour = 300);
    ~APIRateLimiter();
    
    // Check if request is allowed
    bool canMakeRequest(const std::string& service);
    
    // Record a successful request
    void recordRequest(const std::string& service, double costUSD = 0.0);
    
    // Get current usage
    struct UsageStats {
        int requestsThisMinute = 0;
        int requestsThisHour = 0;
        int requestsToday = 0;
        double costThisHour = 0.0;
        double costToday = 0.0;
        double totalCost = 0.0;
    };
    
    UsageStats getUsage(const std::string& service) const;
    
    // Emergency controls
    void setEmergencyStop(bool enable) { emergencyStop_.store(enable); }
    void setMaxHourlyCost(double maxCostUSD) { maxHourlyCost_.store(maxCostUSD); }
    
    // Alert system
    using CostAlertCallback = std::function<void(const std::string& service, double cost, const std::string& alert)>;
    void setCostAlertCallback(CostAlertCallback callback) { costAlertCallback_ = std::move(callback); }
    
private:
    void cleanupOldRecords();
    void checkCostAlerts(const std::string& service, double newCost);
    
    struct RequestRecord {
        std::chrono::system_clock::time_point timestamp;
        double costUSD;
    };
    
    int maxRequestsPerMinute_;
    int maxRequestsPerHour_;
    std::atomic<bool> emergencyStop_{false};
    std::atomic<double> maxHourlyCost_{50.0}; // $50/hour emergency limit
    
    mutable std::mutex recordsMutex_;
    std::unordered_map<std::string, std::vector<RequestRecord>> serviceRecords_;
    
    CostAlertCallback costAlertCallback_;
};

// ============================================================================
// Secure API Client - Combines Security and Rate Limiting
// ============================================================================

class SecureAPIClient {
public:
    SecureAPIClient();
    ~SecureAPIClient();
    
    // Initialize with secure config
    bool initialize(std::shared_ptr<SecureConfig> config, 
                   std::shared_ptr<APIRateLimiter> rateLimiter);
    
    // Make secure API requests
    struct APIRequest {
        std::string service;        // "openai", "anthropic"
        std::string endpoint;       // "/v1/chat/completions"
        std::string method = "POST"; // "GET", "POST", etc.
        std::string headers;        // Additional headers
        std::string body;           // Request body
        double estimatedCostUSD = 0.0; // For rate limiting
    };
    
    struct APIResponse {
        int httpStatus = 0;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        double actualCostUSD = 0.0;
        std::chrono::milliseconds latency{0};
    };
    
    // Async API request (never blocks audio thread)
    core::AsyncResult<APIResponse> makeRequest(const APIRequest& request);
    
    // Synchronous API request (for non-audio thread use only)
    Result<APIResponse> makeRequestSync(const APIRequest& request);
    
    // Health check
    bool isHealthy() const;
    
    // Get metrics
    struct ClientMetrics {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        double totalCostUSD = 0.0;
        std::chrono::milliseconds avgLatency{0};
        bool rateLimitHealthy = true;
        bool securityHealthy = true;
    };
    
    ClientMetrics getMetrics() const;
    
private:
    Result<std::string> buildAuthHeader(const std::string& service);
    void updateMetrics(const APIRequest& request, const APIResponse& response);
    void handleAPIError(const APIRequest& request, int httpStatus, const std::string& error);
    
    std::shared_ptr<SecureConfig> config_;
    std::shared_ptr<APIRateLimiter> rateLimiter_;
    
    mutable std::mutex metricsMutex_;
    ClientMetrics metrics_;
    
    std::atomic<bool> initialized_{false};
};

// ============================================================================
// Global Secure Configuration
// ============================================================================

// Get global secure config (thread-safe singleton)
SecureConfig& getGlobalSecureConfig();
APIRateLimiter& getGlobalRateLimiter();
SecureAPIClient& getGlobalSecureAPIClient();

// Initialize security system
bool initializeSecuritySystem();
void shutdownSecuritySystem();

} // namespace mixmind::core