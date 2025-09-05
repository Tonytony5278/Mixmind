#pragma once

#include "PluginHost.h"
#include "../core/result.h"
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

// Forward declarations for JUCE types
namespace juce {
    class AudioProcessor;
    class AudioProcessorEditor;
    class AudioBuffer;
    class MidiBuffer;
    class PluginDescription;
}

namespace mixmind::plugins {

// ============================================================================
// VST3 Plugin Wrapper Implementation
// ============================================================================

class VST3Plugin : public PluginInstance {
public:
    explicit VST3Plugin(const std::string& pluginPath);
    ~VST3Plugin() override;
    
    // Non-copyable
    VST3Plugin(const VST3Plugin&) = delete;
    VST3Plugin& operator=(const VST3Plugin&) = delete;
    
    // PluginInstance interface implementation
    bool initialize(double sampleRate, int maxBufferSize) override;
    bool activate() override;
    void deactivate() override;
    void cleanup() override;
    
    void processAudio(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output) override;
    
    std::vector<PluginParameter> getParameters() const override;
    bool setParameter(const std::string& id, float value) override;
    float getParameter(const std::string& id) const override;
    void automateParameter(const std::string& id, 
                          const std::vector<std::pair<int, float>>& automation) override;
    
    std::vector<std::string> getPresets() const override;
    bool loadPreset(const std::string& presetName) override;
    bool savePreset(const std::string& presetName) override;
    std::string getCurrentPreset() const override;
    
    PluginInfo getInfo() const override;
    std::string getStateData() const override;
    bool setStateData(const std::string& data) override;
    
    double getCurrentCpuUsage() const override;
    int getCurrentLatency() const override;
    bool isProcessing() const override;
    
    bool hasCustomUI() const override;
    void showUI() override;
    void hideUI() override;
    bool isUIVisible() const override;
    
    // VST3-specific methods
    bool loadFromFile(const std::string& pluginPath);
    bool scanPlugin(PluginInfo& info);
    
    // Advanced VST3 features
    struct VST3ExtendedInfo {
        std::string vendorName;
        std::string vendorVersion;
        std::string vendorEmail;
        std::string vendorUrl;
        std::vector<std::string> supportedFeatures;
        std::vector<std::string> categories;
        bool supportsDoubleReplacing = false;
        bool canReceiveSysexEvents = false;
        bool canReceiveTimeInfo = false;
        bool supportsBypass = false;
        bool supportsOfflineProcessing = false;
    };
    
    VST3ExtendedInfo getExtendedInfo() const;
    
    // Parameter automation enhancements
    struct ParameterAutomation {
        std::string parameterId;
        std::vector<std::pair<double, float>> points; // time, value pairs
        bool isRecording = false;
        bool isPlaying = false;
    };
    
    void startParameterRecording(const std::string& parameterId);
    void stopParameterRecording(const std::string& parameterId);
    std::vector<ParameterAutomation> getParameterAutomations() const;
    
    // MIDI support
    void processMidi(const std::vector<uint8_t>& midiData, int sampleOffset = 0);
    bool acceptsMidi() const;
    bool producesMidi() const;
    
    // Performance optimization
    void setProcessingPrecision(bool useDoublePrecision);
    void setThreadingMode(bool enableMultiThreading);
    void optimizeForLatency(bool lowLatencyMode);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// VST3 Plugin Scanner - Advanced Discovery System
// ============================================================================

class VST3Scanner {
public:
    VST3Scanner();
    ~VST3Scanner();
    
    // Plugin discovery
    struct ScanResult {
        std::vector<PluginInfo> foundPlugins;
        std::vector<std::string> failedPaths;
        std::vector<std::string> errors;
        double scanTimeSeconds = 0.0;
        int totalFilesScanned = 0;
    };
    
    core::AsyncResult<ScanResult> scanDirectory(const std::string& directory, bool recursive = true);
    core::AsyncResult<ScanResult> scanSystemDirectories();
    
    // Advanced scanning options
    void setDeepScan(bool enabled); // Analyze plugin capabilities deeply
    void setAIAnalysis(bool enabled); // Use AI to analyze plugin quality
    void setPerformanceTest(bool enabled); // Test plugin performance
    void setTimeout(int timeoutSeconds); // Timeout for problematic plugins
    
    // Plugin verification
    bool verifyPlugin(const std::string& pluginPath);
    PluginInfo getPluginInfo(const std::string& pluginPath);
    
    // Cache management
    void loadCache(const std::string& cacheFile);
    void saveCache(const std::string& cacheFile);
    void clearCache();
    
    // Filtering and sorting
    enum class SortCriteria {
        NAME, MANUFACTURER, CATEGORY, QUALITY, CPU_USAGE, POPULARITY
    };
    
    std::vector<PluginInfo> filterPlugins(const std::vector<PluginInfo>& plugins,
                                         PluginCategory category,
                                         PluginQuality minQuality = PluginQuality::POOR);
    
    std::vector<PluginInfo> sortPlugins(const std::vector<PluginInfo>& plugins,
                                       SortCriteria criteria,
                                       bool ascending = true);
    
    // AI-powered plugin analysis
    struct PluginAnalysisResult {
        PluginQuality estimatedQuality;
        std::vector<std::string> detectedFeatures;
        std::vector<std::string> usageRecommendations;
        float performanceScore = 0.5f;
        float stabilityScore = 0.5f;
        float soundQualityScore = 0.5f;
        std::string aiSummary;
    };
    
    core::AsyncResult<PluginAnalysisResult> analyzePlugin(const std::string& pluginPath);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// VST3 Plugin Factory - Advanced Plugin Creation
// ============================================================================

class VST3Factory {
public:
    static std::unique_ptr<VST3Plugin> createPlugin(const PluginInfo& info);
    static std::vector<std::string> getSupportedFormats();
    static bool isFormatSupported(PluginFormat format);
    
    // Plugin validation
    static bool validatePlugin(const std::string& pluginPath);
    static std::string getValidationReport(const std::string& pluginPath);
    
    // Advanced loading options
    struct LoadingOptions {
        bool enableSandboxing = true;        // Run plugin in sandbox for security
        bool enableCrashRecovery = true;     // Recover from plugin crashes
        int timeoutMs = 5000;               // Loading timeout
        bool useBackgroundThread = true;    // Load in background
        bool validateSignature = true;      // Verify plugin signature
        bool analyzeCapabilities = true;    // Deep capability analysis
    };
    
    static std::unique_ptr<VST3Plugin> createPluginWithOptions(
        const PluginInfo& info, 
        const LoadingOptions& options);
    
    // Plugin compatibility testing
    struct CompatibilityTest {
        bool isCompatible = false;
        std::vector<std::string> issues;
        std::vector<std::string> warnings;
        std::string recommendation;
    };
    
    static CompatibilityTest testCompatibility(const std::string& pluginPath);
    
private:
    VST3Factory() = delete; // Static class only
};

// ============================================================================
// Advanced Plugin Parameter Mapping
// ============================================================================

class PluginParameterMapper {
public:
    // Smart parameter mapping between plugins
    struct MappingRule {
        std::string sourceParam;
        std::string targetParam;
        std::function<float(float)> transformFunction;
        float confidence = 0.0f;
        std::string reasoning;
    };
    
    static std::vector<MappingRule> createMapping(
        const VST3Plugin& sourcePlugin,
        const VST3Plugin& targetPlugin);
    
    // Preset conversion
    static bool convertPreset(
        const VST3Plugin& sourcePlugin,
        const VST3Plugin& targetPlugin,
        const std::string& presetName);
    
    // Parameter learning system
    class ParameterLearner {
    public:
        void startLearning();
        void recordParameterChange(const std::string& param, float value, double timestamp);
        void stopLearning();
        
        std::vector<MappingRule> generateMappings() const;
        
    private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };
    
    // AI-powered mapping suggestions
    static std::vector<MappingRule> generateAIMappings(
        const PluginInfo& sourceInfo,
        const PluginInfo& targetInfo);
};

// ============================================================================
// Plugin Performance Monitor
// ============================================================================

class PluginPerformanceMonitor {
public:
    struct PerformanceMetrics {
        double averageCpuUsage = 0.0;
        double peakCpuUsage = 0.0;
        double memoryUsage = 0.0;           // MB
        int processedBuffers = 0;
        int droppedBuffers = 0;
        double averageLatency = 0.0;        // ms
        std::vector<double> cpuHistory;     // Last N measurements
        bool hasGlitches = false;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    
    static void startMonitoring(const std::string& pluginUid);
    static void stopMonitoring(const std::string& pluginUid);
    static PerformanceMetrics getMetrics(const std::string& pluginUid);
    
    // Performance analysis
    static std::string generatePerformanceReport(const std::string& pluginUid);
    static std::vector<std::string> identifyBottlenecks();
    static std::vector<std::string> getOptimizationSuggestions(const std::string& pluginUid);
    
    // Real-time monitoring callbacks
    using PerformanceCallback = std::function<void(const std::string&, const PerformanceMetrics&)>;
    static void setPerformanceCallback(PerformanceCallback callback);
    
private:
    PluginPerformanceMonitor() = delete; // Static class only
};

// ============================================================================
// Plugin Security and Sandboxing
// ============================================================================

class PluginSandbox {
public:
    struct SecurityPolicy {
        bool allowFileAccess = false;
        bool allowNetworkAccess = false;
        bool allowRegistryAccess = false;
        bool allowProcessCreation = false;
        std::vector<std::string> allowedDirectories;
        int maxMemoryMB = 512;
        int maxCpuPercent = 25;
    };
    
    static bool runInSandbox(VST3Plugin& plugin, const SecurityPolicy& policy);
    static SecurityPolicy getDefaultPolicy();
    static SecurityPolicy getStrictPolicy();
    static SecurityPolicy getTrustedPolicy();
    
    // Security analysis
    static std::vector<std::string> analyzePluginSecurity(const std::string& pluginPath);
    static bool isPluginTrusted(const std::string& pluginPath);
    
private:
    PluginSandbox() = delete; // Static class only
};

} // namespace mixmind::plugins