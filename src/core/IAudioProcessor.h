#pragma once

#include "types.h"
#include "result.h"
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

namespace mixmind::core {

// Forward declarations
class IPluginInstance;

// ============================================================================
// Audio Processor Interface - Real-time audio processing pipeline
// ============================================================================

class IAudioProcessor {
public:
    virtual ~IAudioProcessor() = default;
    
    // ========================================================================
    // Processor Configuration and Setup
    // ========================================================================
    
    /// Initialize audio processor
    virtual AsyncResult<VoidResult> initialize(
        SampleRate sampleRate,
        BufferSize maxBufferSize,
        int32_t inputChannels,
        int32_t outputChannels
    ) = 0;
    
    /// Release audio processor resources
    virtual AsyncResult<VoidResult> release() = 0;
    
    /// Check if processor is initialized
    virtual bool isInitialized() const = 0;
    
    /// Get current configuration
    struct ProcessorConfig {
        SampleRate sampleRate = 0;
        BufferSize maxBufferSize = 0;
        int32_t inputChannels = 0;
        int32_t outputChannels = 0;
        int32_t latencySamples = 0;
        bool isRealtime = true;
    };
    
    virtual ProcessorConfig getConfig() const = 0;
    
    /// Reconfigure processor (may cause audio glitches)
    virtual AsyncResult<VoidResult> reconfigure(const ProcessorConfig& config) = 0;
    
    // ========================================================================
    // Audio Processing Chain
    // ========================================================================
    
    /// Process audio buffer
    virtual VoidResult processAudio(FloatAudioBuffer& audioBuffer, MidiBuffer& midiBuffer) = 0;
    
    /// Process audio in place
    virtual VoidResult processAudioInPlace(float* const* channels, int32_t numChannels, int32_t numSamples) = 0;
    
    /// Process with separate input/output buffers
    virtual VoidResult processAudio(
        const float* const* inputChannels,
        float* const* outputChannels,
        int32_t numChannels,
        int32_t numSamples,
        const MidiBuffer& midiInput,
        MidiBuffer& midiOutput
    ) = 0;
    
    /// Prepare for processing (called before processing starts)
    virtual AsyncResult<VoidResult> prepareToProcess() = 0;
    
    /// Stop processing (cleanup)
    virtual AsyncResult<VoidResult> stopProcessing() = 0;
    
    /// Reset processor state
    virtual AsyncResult<VoidResult> reset() = 0;
    
    // ========================================================================
    // Plugin Chain Management
    // ========================================================================
    
    /// Add plugin to processing chain
    virtual AsyncResult<Result<int32_t>> addPlugin(std::shared_ptr<IPluginInstance> plugin) = 0;
    
    /// Insert plugin at specific position
    virtual AsyncResult<VoidResult> insertPlugin(std::shared_ptr<IPluginInstance> plugin, int32_t position) = 0;
    
    /// Remove plugin from chain
    virtual AsyncResult<VoidResult> removePlugin(int32_t position) = 0;
    
    /// Move plugin to new position
    virtual AsyncResult<VoidResult> movePlugin(int32_t fromPosition, int32_t toPosition) = 0;
    
    /// Get plugin at position
    virtual std::shared_ptr<IPluginInstance> getPlugin(int32_t position) = 0;
    
    /// Get all plugins in chain
    virtual std::vector<std::shared_ptr<IPluginInstance>> getAllPlugins() = 0;
    
    /// Get plugin count
    virtual int32_t getPluginCount() const = 0;
    
    /// Bypass plugin
    virtual AsyncResult<VoidResult> bypassPlugin(int32_t position, bool bypassed) = 0;
    
    /// Check if plugin is bypassed
    virtual bool isPluginBypassed(int32_t position) const = 0;
    
    /// Bypass entire chain
    virtual AsyncResult<VoidResult> bypassChain(bool bypassed) = 0;
    
    /// Check if chain is bypassed
    virtual bool isChainBypassed() const = 0;
    
    // ========================================================================
    // Built-in Processing Modules
    // ========================================================================
    
    /// Enable/disable built-in gain module
    virtual AsyncResult<VoidResult> setGainEnabled(bool enabled) = 0;
    
    /// Set gain value
    virtual AsyncResult<VoidResult> setGain(float gainDB) = 0;
    
    /// Get current gain
    virtual float getGain() const = 0;
    
    /// Enable/disable built-in limiter
    virtual AsyncResult<VoidResult> setLimiterEnabled(bool enabled) = 0;
    
    /// Configure limiter
    struct LimiterSettings {
        float threshold = -0.1f;    // dB
        float release = 50.0f;      // ms
        bool enableISP = true;      // Inter-sample peaks
    };
    
    virtual AsyncResult<VoidResult> setLimiterSettings(const LimiterSettings& settings) = 0;
    
    /// Get limiter settings
    virtual LimiterSettings getLimiterSettings() const = 0;
    
    /// Enable/disable DC filter
    virtual AsyncResult<VoidResult> setDCFilterEnabled(bool enabled) = 0;
    
    /// Check if DC filter is enabled
    virtual bool isDCFilterEnabled() const = 0;
    
    // ========================================================================
    // Audio Routing and Bus Management
    // ========================================================================
    
    /// Set input routing
    virtual AsyncResult<VoidResult> setInputRouting(const std::vector<int32_t>& channelMapping) = 0;
    
    /// Set output routing
    virtual AsyncResult<VoidResult> setOutputRouting(const std::vector<int32_t>& channelMapping) = 0;
    
    /// Get input routing
    virtual std::vector<int32_t> getInputRouting() const = 0;
    
    /// Get output routing
    virtual std::vector<int32_t> getOutputRouting() const = 0;
    
    /// Add auxiliary send
    virtual AsyncResult<Result<int32_t>> addAuxSend() = 0;
    
    /// Remove auxiliary send
    virtual AsyncResult<VoidResult> removeAuxSend(int32_t sendIndex) = 0;
    
    /// Set aux send level
    virtual AsyncResult<VoidResult> setAuxSendLevel(int32_t sendIndex, float level) = 0;
    
    /// Get aux send level
    virtual float getAuxSendLevel(int32_t sendIndex) const = 0;
    
    /// Set aux send pre/post
    virtual AsyncResult<VoidResult> setAuxSendPrePost(int32_t sendIndex, bool preFader) = 0;
    
    /// Check if aux send is pre-fader
    virtual bool isAuxSendPreFader(int32_t sendIndex) const = 0;
    
    // ========================================================================
    // Latency Management
    // ========================================================================
    
    /// Get total processor latency
    virtual int32_t getLatencySamples() const = 0;
    
    /// Get plugin latency at position
    virtual int32_t getPluginLatency(int32_t position) const = 0;
    
    /// Enable/disable automatic delay compensation
    virtual VoidResult setLatencyCompensationEnabled(bool enabled) = 0;
    
    /// Check if latency compensation is enabled
    virtual bool isLatencyCompensationEnabled() const = 0;
    
    /// Force latency recalculation
    virtual AsyncResult<VoidResult> recalculateLatency() = 0;
    
    /// Set manual latency offset
    virtual VoidResult setLatencyOffset(int32_t samples) = 0;
    
    /// Get manual latency offset
    virtual int32_t getLatencyOffset() const = 0;
    
    // ========================================================================
    // Performance Monitoring
    // ========================================================================
    
    /// Get CPU usage percentage
    virtual float getCPUUsage() const = 0;
    
    /// Get CPU usage per plugin
    virtual std::vector<float> getPluginCPUUsage() const = 0;
    
    /// Get memory usage in bytes
    virtual size_t getMemoryUsage() const = 0;
    
    /// Get processing time statistics
    struct ProcessingStats {
        double averageProcessingTime = 0.0;  // milliseconds
        double maxProcessingTime = 0.0;      // milliseconds
        double minProcessingTime = 0.0;      // milliseconds
        int64_t totalProcessedSamples = 0;
        int64_t totalDroppedSamples = 0;
        int32_t xrunCount = 0;               // Buffer underruns/overruns
        double cpuLoadPercent = 0.0;
    };
    
    virtual ProcessingStats getProcessingStats() const = 0;
    
    /// Reset performance statistics
    virtual VoidResult resetPerformanceStats() = 0;
    
    /// Enable/disable performance monitoring
    virtual VoidResult setPerformanceMonitoringEnabled(bool enabled) = 0;
    
    /// Check if performance monitoring is enabled
    virtual bool isPerformanceMonitoringEnabled() const = 0;
    
    // ========================================================================
    // Audio Metering and Analysis
    // ========================================================================
    
    /// Get current input levels (peak)
    virtual std::vector<float> getInputLevels() const = 0;
    
    /// Get current output levels (peak)
    virtual std::vector<float> getOutputLevels() const = 0;
    
    /// Get RMS levels
    virtual std::vector<float> getRMSLevels() const = 0;
    
    /// Enable/disable level metering
    virtual VoidResult setMeteringEnabled(bool enabled) = 0;
    
    /// Check if metering is enabled
    virtual bool isMeteringEnabled() const = 0;
    
    /// Set metering decay rate
    virtual VoidResult setMeteringDecayRate(float decayRateDB_per_second) = 0;
    
    /// Get metering decay rate
    virtual float getMeteringDecayRate() const = 0;
    
    /// Enable/disable spectrum analysis
    virtual VoidResult setSpectrumAnalysisEnabled(bool enabled) = 0;
    
    /// Get spectrum data
    virtual std::vector<float> getSpectrumData(int32_t fftSize = 1024) const = 0;
    
    /// Get phase correlation
    virtual float getPhaseCorrelation() const = 0;
    
    // ========================================================================
    // Buffer Management
    // ========================================================================
    
    /// Set buffer size (may cause audio interruption)
    virtual AsyncResult<VoidResult> setBufferSize(BufferSize bufferSize) = 0;
    
    /// Get current buffer size
    virtual BufferSize getBufferSize() const = 0;
    
    /// Get maximum supported buffer size
    virtual BufferSize getMaxBufferSize() const = 0;
    
    /// Get minimum supported buffer size
    virtual BufferSize getMinBufferSize() const = 0;
    
    /// Force buffer flush
    virtual AsyncResult<VoidResult> flushBuffers() = 0;
    
    // ========================================================================
    // Thread Safety and Concurrency
    // ========================================================================
    
    /// Enable/disable multi-threaded processing
    virtual VoidResult setMultiThreadedProcessingEnabled(bool enabled) = 0;
    
    /// Check if multi-threaded processing is enabled
    virtual bool isMultiThreadedProcessingEnabled() const = 0;
    
    /// Set processing thread count
    virtual VoidResult setProcessingThreadCount(int32_t threadCount) = 0;
    
    /// Get processing thread count
    virtual int32_t getProcessingThreadCount() const = 0;
    
    /// Set thread priority
    enum class ThreadPriority {
        Low,
        Normal,
        High,
        Realtime
    };
    
    virtual VoidResult setThreadPriority(ThreadPriority priority) = 0;
    
    /// Get thread priority
    virtual ThreadPriority getThreadPriority() const = 0;
    
    /// Lock processor for configuration changes
    virtual VoidResult lockProcessor() = 0;
    
    /// Unlock processor
    virtual VoidResult unlockProcessor() = 0;
    
    /// Check if processor is locked
    virtual bool isProcessorLocked() const = 0;
    
    // ========================================================================
    // Freeze and Offline Processing
    // ========================================================================
    
    /// Freeze processor (render to audio)
    virtual AsyncResult<Result<std::string>> freeze(
        TimestampSamples startSample,
        TimestampSamples lengthSamples,
        const std::string& outputPath = ""
    ) = 0;
    
    /// Unfreeze processor (restore processing chain)
    virtual AsyncResult<VoidResult> unfreeze() = 0;
    
    /// Check if processor is frozen
    virtual bool isFrozen() const = 0;
    
    /// Get frozen audio file path
    virtual std::string getFrozenFilePath() const = 0;
    
    /// Process offline (non-realtime)
    virtual AsyncResult<VoidResult> processOffline(
        const float* const* inputChannels,
        float* const* outputChannels,
        int32_t numChannels,
        int32_t numSamples,
        ProgressCallback progress = nullptr
    ) = 0;
    
    // ========================================================================
    // Processor Templates and Presets
    // ========================================================================
    
    /// Save processor chain as template
    virtual AsyncResult<VoidResult> saveAsTemplate(
        const std::string& templateName,
        const std::string& description = ""
    ) = 0;
    
    /// Load processor template
    virtual AsyncResult<VoidResult> loadTemplate(const std::string& templateName) = 0;
    
    /// Get available templates
    virtual std::vector<std::string> getAvailableTemplates() const = 0;
    
    /// Delete template
    virtual VoidResult deleteTemplate(const std::string& templateName) = 0;
    
    /// Save processor state
    virtual AsyncResult<VoidResult> saveState(std::vector<uint8_t>& data) const = 0;
    
    /// Load processor state
    virtual AsyncResult<VoidResult> loadState(const std::vector<uint8_t>& data) = 0;
    
    // ========================================================================
    // MIDI Processing
    // ========================================================================
    
    /// Enable/disable MIDI processing
    virtual VoidResult setMIDIProcessingEnabled(bool enabled) = 0;
    
    /// Check if MIDI processing is enabled
    virtual bool isMIDIProcessingEnabled() const = 0;
    
    /// Process MIDI buffer
    virtual VoidResult processMIDI(MidiBuffer& midiBuffer) = 0;
    
    /// Filter MIDI events
    struct MIDIFilter {
        bool allowNoteOn = true;
        bool allowNoteOff = true;
        bool allowControlChange = true;
        bool allowProgramChange = true;
        bool allowChannelPressure = true;
        bool allowPitchBend = true;
        bool allowSysEx = false;
        std::vector<int32_t> allowedChannels;  // empty = all channels
        std::vector<int32_t> blockedChannels;
    };
    
    virtual VoidResult setMIDIFilter(const MIDIFilter& filter) = 0;
    
    /// Get MIDI filter settings
    virtual MIDIFilter getMIDIFilter() const = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class ProcessorEvent {
        ConfigChanged,
        PluginAdded,
        PluginRemoved,
        PluginBypassed,
        ChainBypassed,
        LatencyChanged,
        PerformanceWarning,
        XRunDetected,
        FreezeStarted,
        FreezeCompleted,
        ProcessingStarted,
        ProcessingStopped
    };
    
    using ProcessorEventCallback = std::function<void(ProcessorEvent event, const std::string& details)>;
    
    /// Subscribe to processor events
    virtual void addEventListener(ProcessorEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(ProcessorEventCallback callback) = 0;
    
    // ========================================================================
    // Quality and Oversampling
    // ========================================================================
    
    enum class ProcessingQuality {
        Draft,      // Fastest, lowest quality
        Good,       // Balanced performance/quality
        Better,     // Higher quality, slower
        Best        // Highest quality, slowest
    };
    
    /// Set processing quality
    virtual VoidResult setProcessingQuality(ProcessingQuality quality) = 0;
    
    /// Get processing quality
    virtual ProcessingQuality getProcessingQuality() const = 0;
    
    /// Enable/disable oversampling
    virtual AsyncResult<VoidResult> setOversamplingEnabled(bool enabled, int32_t factor = 2) = 0;
    
    /// Check if oversampling is enabled
    virtual bool isOversamplingEnabled() const = 0;
    
    /// Get oversampling factor
    virtual int32_t getOversamplingFactor() const = 0;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Enable/disable denormal protection
    virtual VoidResult setDenormalProtectionEnabled(bool enabled) = 0;
    
    /// Check if denormal protection is enabled
    virtual bool isDenormalProtectionEnabled() const = 0;
    
    /// Set processing precision
    enum class ProcessingPrecision {
        Single,     // 32-bit float
        Double      // 64-bit double
    };
    
    virtual VoidResult setProcessingPrecision(ProcessingPrecision precision) = 0;
    
    /// Get processing precision
    virtual ProcessingPrecision getProcessingPrecision() const = 0;
    
    /// Enable/disable SIMD optimization
    virtual VoidResult setSIMDEnabled(bool enabled) = 0;
    
    /// Check if SIMD is enabled
    virtual bool isSIMDEnabled() const = 0;
    
    /// Get SIMD capabilities
    virtual std::vector<std::string> getSIMDCapabilities() const = 0;
};

} // namespace mixmind::core