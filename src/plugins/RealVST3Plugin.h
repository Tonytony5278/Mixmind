#pragma once

#include "PluginHost.h"
#include "../core/result.h"

// Real JUCE includes (when available)
#ifdef MIXMIND_JUCE_ENABLED
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#else
// Fallback when JUCE is not available
namespace juce {
    class AudioProcessor;
    class AudioProcessorEditor;
    class AudioBuffer;
    class MidiBuffer;
    class PluginDescription;
}
#endif

#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>

namespace mixmind::plugins {

// ============================================================================
// Real VST3 Plugin Implementation with JUCE
// ============================================================================

class RealVST3Plugin : public PluginInstance {
public:
    explicit RealVST3Plugin(const std::string& pluginPath);
    ~RealVST3Plugin() override;
    
    // Non-copyable
    RealVST3Plugin(const RealVST3Plugin&) = delete;
    RealVST3Plugin& operator=(const RealVST3Plugin&) = delete;
    
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
    
    // Advanced VST3 features
    bool loadFromFile(const std::string& pluginPath);
    bool scanPlugin(PluginInfo& info);
    
    // MIDI support
    void processMidi(const std::vector<uint8_t>& midiData, int sampleOffset = 0);
    bool acceptsMidi() const;
    bool producesMidi() const;
    
    // Real-time safe parameter changes
    void queueParameterChange(const std::string& parameterId, float value, int sampleOffset);
    void processParameterChanges();
    
    // Performance monitoring
    struct RealTimeStats {
        std::atomic<double> averageCpuMs{0.0};
        std::atomic<double> peakCpuMs{0.0};
        std::atomic<int> processedBuffers{0};
        std::atomic<int> droppedBuffers{0};
        std::atomic<bool> hasXruns{false};
    };
    
    const RealTimeStats& getRealtimeStats() const { return stats_; }
    void resetStats();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    
    // Real-time safe data
    mutable RealTimeStats stats_;
    
    // Performance timing
    mutable std::chrono::high_resolution_clock::time_point lastProcessStart_;
    mutable std::atomic<bool> isProcessing_{false};
    
    void updatePerformanceStats(std::chrono::microseconds processingTime);
};

// ============================================================================
// Real VST3 Plugin Scanner with JUCE Integration
// ============================================================================

class RealVST3Scanner {
public:
    RealVST3Scanner();
    ~RealVST3Scanner();
    
    struct ScanResult {
        std::vector<PluginInfo> foundPlugins;
        std::vector<std::string> failedPaths;
        std::vector<std::string> errors;
        double scanTimeSeconds = 0.0;
        int totalFilesScanned = 0;
    };
    
    // Real plugin scanning with JUCE
    mixmind::core::AsyncResult<ScanResult> scanDirectory(const std::string& directory, bool recursive = true);
    mixmind::core::AsyncResult<ScanResult> scanSystemDirectories();
    
    // Advanced scanning with AI analysis
    void enableAIAnalysis(bool enable) { aiAnalysisEnabled_ = enable; }
    void enablePerformanceTest(bool enable) { performanceTestEnabled_ = enable; }
    void setTimeout(int timeoutSeconds) { timeoutSeconds_ = timeoutSeconds; }
    
    // Plugin verification
    bool verifyPlugin(const std::string& pluginPath);
    PluginInfo getPluginInfo(const std::string& pluginPath);
    
    // Cache management for faster subsequent scans
    void loadCache(const std::string& cacheFile);
    void saveCache(const std::string& cacheFile);
    void clearCache();
    
    // Quality analysis
    PluginQuality analyzePluginQuality(const PluginInfo& plugin);
    std::string generateQualityReport(const PluginInfo& plugin);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    
    bool aiAnalysisEnabled_ = true;
    bool performanceTestEnabled_ = false;
    int timeoutSeconds_ = 10;
    
    std::unordered_map<std::string, PluginInfo> pluginCache_;
    
    void performAIAnalysis(PluginInfo& info);
    void performPerformanceTest(PluginInfo& info);
};

// ============================================================================
// Real Plugin Factory with Advanced Features
// ============================================================================

class RealPluginFactory {
public:
    // Plugin creation with full JUCE integration
    static std::unique_ptr<RealVST3Plugin> createPlugin(const PluginInfo& info);
    static std::unique_ptr<RealVST3Plugin> createPluginFromPath(const std::string& pluginPath);
    
    // Plugin format support
    static std::vector<std::string> getSupportedFormats();
    static bool isFormatSupported(PluginFormat format);
    
    // Advanced plugin validation
    static bool validatePlugin(const std::string& pluginPath);
    static std::string getValidationReport(const std::string& pluginPath);
    
    struct LoadingOptions {
        bool enableSandboxing = true;
        bool enableCrashRecovery = true;  
        int timeoutMs = 10000;
        bool useBackgroundThread = false;  // For real-time loading
        bool validateSignature = true;
        bool analyzeCapabilities = true;
        double sampleRate = 44100.0;
        int bufferSize = 512;
    };
    
    static std::unique_ptr<RealVST3Plugin> createPluginWithOptions(
        const PluginInfo& info, 
        const LoadingOptions& options);
    
    // Compatibility testing
    struct CompatibilityTest {
        bool isCompatible = false;
        std::vector<std::string> issues;
        std::vector<std::string> warnings;
        std::string recommendation;
        double qualityScore = 0.0;
    };
    
    static CompatibilityTest testCompatibility(const std::string& pluginPath);
    
private:
    RealPluginFactory() = delete; // Static class only
};

// ============================================================================
// Real Plugin Parameter Automation
// ============================================================================

class RealParameterAutomation {
public:
    struct AutomationPoint {
        double timestamp;     // Time in seconds
        float value;         // Parameter value
        int sampleOffset;    // Sample-accurate timing
    };
    
    struct AutomationCurve {
        std::string parameterId;
        std::vector<AutomationPoint> points;
        bool isActive = false;
        bool isRecording = false;
        double startTime = 0.0;
        double endTime = 0.0;
    };
    
    RealParameterAutomation(RealVST3Plugin* plugin);
    ~RealParameterAutomation();
    
    // Real-time automation playback
    void processAutomation(int bufferSize, double sampleRate);
    void setPlaybackPosition(double timeInSeconds);
    
    // Automation curve management
    bool addAutomationCurve(const AutomationCurve& curve);
    bool removeAutomationCurve(const std::string& parameterId);
    void clearAllAutomation();
    
    // Recording automation
    void startRecording(const std::string& parameterId);
    void stopRecording();
    bool isRecording() const { return isRecording_; }
    
    // Automation data
    std::vector<AutomationCurve> getAutomationCurves() const;
    AutomationCurve getAutomationCurve(const std::string& parameterId) const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    
    RealVST3Plugin* plugin_;
    std::atomic<bool> isRecording_{false};
    std::string recordingParameterId_;
    double currentPlaybackPosition_ = 0.0;
    
    void recordParameterChange(const std::string& parameterId, float value, double timestamp);
    float interpolateAutomationValue(const AutomationCurve& curve, double timestamp);
};

// ============================================================================
// Advanced Plugin Chain with Real Processing
// ============================================================================

class RealPluginChain {
public:
    RealPluginChain(const std::string& name = "");
    ~RealPluginChain();
    
    // Plugin management
    bool addPlugin(std::unique_ptr<RealVST3Plugin> plugin, int position = -1);
    bool removePlugin(int position);
    bool movePlugin(int fromPosition, int toPosition);
    void clearAllPlugins();
    
    // Real-time audio processing
    void processAudio(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output);
    
    // Chain control
    void setActive(bool active) { isActive_ = active; }
    bool isActive() const { return isActive_; }
    void setBypass(bool bypass) { isBypassed_ = bypass; }
    bool isBypassed() const { return isBypassed_; }
    
    // Master controls
    void setMasterInputGain(float gain) { masterInputGain_ = gain; }
    void setMasterOutputGain(float gain) { masterOutputGain_ = gain; }
    float getMasterInputGain() const { return masterInputGain_; }
    float getMasterOutputGain() const { return masterOutputGain_; }
    
    // Plugin access
    size_t getPluginCount() const;
    RealVST3Plugin* getPlugin(int position);
    const RealVST3Plugin* getPlugin(int position) const;
    
    // Chain analysis
    struct ChainStats {
        double totalCpuUsage = 0.0;
        int totalLatency = 0;
        int activePluginCount = 0;
        bool hasIssues = false;
        std::vector<std::string> issues;
    };
    
    ChainStats getChainStats() const;
    
    // Automation
    void setAutomationEnabled(bool enabled) { automationEnabled_ = enabled; }
    bool isAutomationEnabled() const { return automationEnabled_; }
    void processChainAutomation(int bufferSize, double sampleRate);
    
    // Preset management
    bool saveChainPreset(const std::string& presetName);
    bool loadChainPreset(const std::string& presetName);
    std::vector<std::string> getAvailablePresets() const;
    
private:
    struct PluginSlot {
        std::unique_ptr<RealVST3Plugin> plugin;
        std::unique_ptr<RealParameterAutomation> automation;
        bool isActive = true;
        bool isBypassed = false;
        float inputGain = 1.0f;
        float outputGain = 1.0f;
        float wetDryMix = 1.0f;
    };
    
    std::string name_;
    std::vector<PluginSlot> pluginSlots_;
    
    std::atomic<bool> isActive_{true};
    std::atomic<bool> isBypassed_{false};
    std::atomic<bool> automationEnabled_{true};
    
    std::atomic<float> masterInputGain_{1.0f};
    std::atomic<float> masterOutputGain_{1.0f};
    
    mutable std::mutex pluginMutex_;
    
    // Real-time processing optimization
    audio::AudioBufferPool::AudioBuffer tempBuffer_;
    bool needsTempBuffer_ = false;
    
    void initializeTempBuffer(int numChannels, int numSamples);
    void processPluginSlot(const PluginSlot& slot, 
                          const audio::AudioBufferPool::AudioBuffer& input,
                          audio::AudioBufferPool::AudioBuffer& output);
};

} // namespace mixmind::plugins