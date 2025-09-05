#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include "../core/SecureConfig.h"
#include <string>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

namespace mixmind::licensing {

// ============================================================================
// PRODUCTION: Commercial License Management System
// Handles license validation, feature gating, and subscription management
// ============================================================================

class LicenseManager {
public:
    enum class LicenseType {
        INVALID,
        TRIAL,      // 14 days, full features
        BASIC,      // $49/month - No AI features
        PRO,        // $149/month - All AI features  
        STUDIO      // $499/month - Multi-seat, priority support
    };
    
    enum class ValidationResult {
        VALID,
        EXPIRED,
        INVALID_KEY,
        MACHINE_MISMATCH,
        SERVER_ERROR,
        OFFLINE_FALLBACK,
        RATE_LIMITED
    };
    
    struct LicenseInfo {
        LicenseType type = LicenseType::INVALID;
        std::string userEmail;
        std::string organizationName;
        std::chrono::system_clock::time_point expirationDate;
        std::chrono::system_clock::time_point lastValidated;
        bool isSubscription = false;
        bool autoRenewal = false;
        int maxSessions = 1;
        int currentSessions = 0;
        
        // Feature flags
        bool aiAssistantEnabled = false;
        bool voiceControlEnabled = false;
        bool styleMappingEnabled = false;
        bool cloudSyncEnabled = false;
        bool prioritySupportEnabled = false;
        bool multiSeatEnabled = false;
        
        bool isValid() const {
            return type != LicenseType::INVALID && 
                   std::chrono::system_clock::now() < expirationDate;
        }
        
        bool hasFeature(const std::string& feature) const {
            if (feature == "ai_assistant") return aiAssistantEnabled;
            if (feature == "voice_control") return voiceControlEnabled;
            if (feature == "style_mapping") return styleMappingEnabled;
            if (feature == "cloud_sync") return cloudSyncEnabled;
            if (feature == "priority_support") return prioritySupportEnabled;
            if (feature == "multi_seat") return multiSeatEnabled;
            return false;
        }
    };
    
    // License validation callback
    using ValidationCallback = std::function<void(ValidationResult, const LicenseInfo&)>;
    
    LicenseManager();
    ~LicenseManager();
    
    // License management
    core::AsyncResult<ValidationResult> validateLicense(const std::string& licenseKey = "");
    core::Result<void> setLicenseKey(const std::string& licenseKey);
    std::string getLicenseKey() const;
    
    // License information
    LicenseInfo getCurrentLicense() const { return currentLicense_; }
    LicenseType getLicenseType() const { return currentLicense_.type; }
    bool isLicenseValid() const { return currentLicense_.isValid(); }
    
    // Feature gating
    bool hasFeature(const std::string& feature) const;
    core::Result<void> requireFeature(const std::string& feature) const;
    
    // Trial management
    bool isTrialActive() const;
    std::chrono::hours getTrialTimeRemaining() const;
    core::Result<void> startTrial();
    
    // Subscription management
    bool isSubscriptionActive() const;
    std::chrono::system_clock::time_point getSubscriptionRenewalDate() const;
    core::AsyncResult<std::string> getSubscriptionManagementUrl() const;
    
    // Session management (for multi-seat licenses)
    core::Result<void> startSession();
    void endSession();
    int getActiveSessions() const { return currentLicense_.currentSessions; }
    int getMaxSessions() const { return currentLicense_.maxSessions; }
    
    // License server communication
    void setLicenseServerUrl(const std::string& url) { licenseServerUrl_ = url; }
    void setOfflineMode(bool offline) { offlineMode_.store(offline); }
    bool isOfflineMode() const { return offlineMode_.load(); }
    
    // Periodic validation
    void startPeriodicValidation(std::chrono::minutes interval = std::chrono::minutes(60));
    void stopPeriodicValidation();
    void setValidationCallback(ValidationCallback callback) { validationCallback_ = std::move(callback); }
    
    // Hardware fingerprinting
    std::string getMachineId() const;
    
    // License purchase/upgrade
    core::AsyncResult<std::string> getPurchaseUrl(LicenseType targetType) const;
    core::AsyncResult<std::string> getUpgradeUrl(LicenseType targetType) const;
    
    // License statistics
    struct LicenseStats {
        int validationAttempts = 0;
        int successfulValidations = 0;
        int offlineValidations = 0;
        int serverErrors = 0;
        std::chrono::system_clock::time_point lastValidation;
        std::chrono::system_clock::time_point lastServerContact;
    };
    
    LicenseStats getLicenseStats() const { return stats_; }
    void resetLicenseStats() { stats_ = LicenseStats{}; }
    
private:
    // Machine fingerprinting
    std::string generateMachineId() const;
    std::string getCpuId() const;
    std::string getMacAddress() const;
    std::string getWindowsProductId() const;
    std::string getSystemUUID() const;
    std::string sha256(const std::string& input) const;
    
    // License validation
    core::AsyncResult<ValidationResult> validateOnline(const std::string& licenseKey);
    ValidationResult validateOffline(const std::string& licenseKey);
    bool verifyOfflineLicense(const std::string& licenseData, const std::string& signature);
    
    // License storage
    bool saveLicenseToRegistry(const LicenseInfo& license);
    core::Result<LicenseInfo> loadLicenseFromRegistry();
    void clearStoredLicense();
    
    // Feature configuration
    void applyLicenseRestrictions(LicenseType type);
    void configureFeaturesForType(LicenseType type);
    
    // Network communication
    struct HttpResponse {
        int statusCode;
        std::string body;
        std::map<std::string, std::string> headers;
    };
    
    core::AsyncResult<HttpResponse> makeHttpRequest(
        const std::string& method,
        const std::string& endpoint,
        const std::map<std::string, std::string>& data = {},
        const std::map<std::string, std::string>& headers = {}
    );
    
    // Periodic validation thread
    void periodicValidationLoop();
    void scheduleNextValidation();
    
    // License server endpoints
    std::string licenseServerUrl_ = "https://api.mixmindai.com";
    
    // Current license state
    mutable std::mutex licenseMutex_;
    LicenseInfo currentLicense_;
    std::string cachedMachineId_;
    
    // Validation state
    std::atomic<bool> offlineMode_{false};
    std::atomic<bool> periodicValidationEnabled_{false};
    std::unique_ptr<std::thread> validationThread_;
    std::chrono::minutes validationInterval_{60};
    ValidationCallback validationCallback_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    LicenseStats stats_;
    
    // Secure configuration for license storage
    std::unique_ptr<core::SecureConfig> secureConfig_;
    
    // Trial state
    std::chrono::system_clock::time_point trialStartTime_;
    static constexpr std::chrono::hours TRIAL_DURATION{14 * 24}; // 14 days
};

// ============================================================================
// License Feature Guard - RAII Feature Access Control
// ============================================================================

class LicenseFeatureGuard {
public:
    explicit LicenseFeatureGuard(const std::string& feature);
    ~LicenseFeatureGuard() = default;
    
    LicenseFeatureGuard(const LicenseFeatureGuard&) = delete;
    LicenseFeatureGuard& operator=(const LicenseFeatureGuard&) = delete;
    
    bool isAuthorized() const { return authorized_; }
    explicit operator bool() const { return authorized_; }
    
    // Get user-friendly error message for display
    std::string getUpgradeMessage() const;
    std::string getRequiredLicenseType() const;
    
private:
    bool authorized_;
    std::string feature_;
    LicenseManager::LicenseType requiredType_;
};

// ============================================================================
// License Exception Handling
// ============================================================================

class LicenseException : public std::exception {
public:
    explicit LicenseException(const std::string& message, const std::string& feature = "")
        : message_(message), feature_(feature) {}
    
    const char* what() const noexcept override { return message_.c_str(); }
    const std::string& getFeature() const { return feature_; }
    
private:
    std::string message_;
    std::string feature_;
};

// ============================================================================
// Global License Manager
// ============================================================================

LicenseManager& getGlobalLicenseManager();
void initializeLicenseSystem();
void shutdownLicenseSystem();

// Convenience macros for feature gating
#define REQUIRE_LICENSE_FEATURE(feature) \
    do { \
        auto result = getGlobalLicenseManager().requireFeature(feature); \
        if (!result) { \
            throw LicenseException(result.error(), feature); \
        } \
    } while(0)

#define CHECK_LICENSE_FEATURE(feature) \
    getGlobalLicenseManager().hasFeature(feature)

#define LICENSE_GUARD(feature) \
    LicenseFeatureGuard guard(feature); \
    if (!guard) { \
        throw LicenseException(guard.getUpgradeMessage(), feature); \
    }

} // namespace mixmind::licensing