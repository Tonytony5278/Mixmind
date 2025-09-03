#pragma once

#include "types.h"
#include <string>
#include <functional>
#include <memory>

namespace mixmind::core {

// ============================================================================
// Crash Reporting System (Opt-in via MIXMIND_CRASHDUMPS=1)
// ============================================================================

struct CrashReport {
    std::string sessionId;
    std::string version;
    std::string platform;
    std::string buildConfig;
    
    // Crash details
    std::string crashType;
    std::string stackTrace;
    std::string exceptionMessage;
    
    // System context
    std::string osVersion;
    std::string cpuInfo;
    size_t memoryTotal;
    size_t memoryAvailable;
    
    // Audio context
    std::string audioDriver;
    int sampleRate;
    int bufferSize;
    
    // User context (anonymized)
    std::string projectType;
    int trackCount;
    int pluginCount;
    
    // Timestamp
    uint64_t timestamp;
};

using CrashReportCallback = std::function<void(const CrashReport&)>;

class CrashReporter {
public:
    CrashReporter();
    ~CrashReporter();
    
    // Non-copyable
    CrashReporter(const CrashReporter&) = delete;
    CrashReporter& operator=(const CrashReporter&) = delete;
    
    /// Initialize crash reporting (only if MIXMIND_CRASHDUMPS=1)
    VoidResult initialize();
    
    /// Shutdown crash reporting
    void shutdown();
    
    /// Check if crash reporting is enabled
    bool isEnabled() const { return enabled_; }
    
    /// Set callback for crash reports (for custom handling)
    void setCallback(CrashReportCallback callback);
    
    /// Manually trigger crash report (for testing)
    void triggerTestCrash() const;
    
    /// Generate crash report from current state
    CrashReport generateReport(const std::string& crashType, 
                              const std::string& message = "") const;

private:
    bool enabled_ = false;
    CrashReportCallback callback_;
    std::string sessionId_;
    
    // Platform-specific implementation
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    /// Check environment variable for opt-in
    bool checkOptIn() const;
    
    /// Initialize platform-specific crash handling
    VoidResult initializePlatform();
};

// ============================================================================
// Telemetry System (Opt-in via MIXMIND_TELEMETRY=1) 
// ============================================================================

struct TelemetryEvent {
    std::string category;
    std::string action;
    std::string label;
    int64_t value = 0;
    
    // Context
    uint64_t timestamp;
    std::string sessionId;
    
    // Custom properties (anonymized)
    std::unordered_map<std::string, std::string> properties;
};

using TelemetryCallback = std::function<void(const TelemetryEvent&)>;

class TelemetryReporter {
public:
    TelemetryReporter();
    ~TelemetryReporter();
    
    // Non-copyable
    TelemetryReporter(const TelemetryReporter&) = delete;
    TelemetryReporter& operator=(TelemetryReporter&) = delete;
    
    /// Initialize telemetry (only if MIXMIND_TELEMETRY=1)
    VoidResult initialize();
    
    /// Shutdown telemetry
    void shutdown();
    
    /// Check if telemetry is enabled
    bool isEnabled() const { return enabled_; }
    
    /// Set callback for telemetry events
    void setCallback(TelemetryCallback callback);
    
    /// Track application lifecycle events
    void trackAppStart();
    void trackAppShutdown();
    
    /// Track feature usage
    void trackFeatureUse(const std::string& feature, 
                        const std::string& context = "");
    
    /// Track performance metrics
    void trackPerformance(const std::string& operation, 
                         int64_t durationMs,
                         const std::unordered_map<std::string, std::string>& context = {});
    
    /// Track errors (anonymized)
    void trackError(const std::string& errorType,
                   const std::string& context = "");
    
    /// Track audio system metrics
    void trackAudioSetup(const std::string& driver,
                        int sampleRate, 
                        int bufferSize);
    
    /// Track plugin usage (anonymized)
    void trackPluginUsage(const std::string& pluginType,
                         const std::string& category = "");

private:
    bool enabled_ = false;
    TelemetryCallback callback_;
    std::string sessionId_;
    
    /// Check environment variable for opt-in
    bool checkOptIn() const;
    
    /// Send telemetry event
    void sendEvent(const TelemetryEvent& event);
    
    /// Generate anonymous session ID
    std::string generateSessionId() const;
};

// ============================================================================
// Privacy-Safe Helpers
// ============================================================================

/// Generate anonymous hash of sensitive data
std::string anonymizeString(const std::string& input);

/// Generate system fingerprint (for duplicate crash detection)
std::string generateSystemFingerprint();

/// Check if user has consented to crash reporting
bool hasCrashReportingConsent();

/// Check if user has consented to telemetry
bool hasTelemetryConsent();

/// Show privacy consent dialog (platform-specific)
VoidResult showPrivacyConsentDialog();

// ============================================================================
// Global Instances
// ============================================================================

/// Get global crash reporter instance
CrashReporter& getCrashReporter();

/// Get global telemetry reporter instance  
TelemetryReporter& getTelemetryReporter();

/// Initialize both systems with environment variable checks
VoidResult initializeReporting();

/// Shutdown both systems
void shutdownReporting();

} // namespace mixmind::core