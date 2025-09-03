#pragma once

#include "IOSSService.h"
#include "../core/types.h"
#include "../core/result.h"
#include <SoundTouch.h>
#include <RubberBandStretcher.h>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <queue>

namespace mixmind::services {

// ============================================================================
// Time Stretch Service - Audio time stretching using SoundTouch and RubberBand
// ============================================================================

class TimeStretchService : public IAudioProcessingService {
public:
    TimeStretchService();
    ~TimeStretchService() override;
    
    // Non-copyable, movable
    TimeStretchService(const TimeStretchService&) = delete;
    TimeStretchService& operator=(const TimeStretchService&) = delete;
    TimeStretchService(TimeStretchService&&) = default;
    TimeStretchService& operator=(TimeStretchService&&) = default;
    
    // ========================================================================
    // IOSSService Implementation
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> initialize() override;
    core::AsyncResult<core::VoidResult> shutdown() override;
    bool isInitialized() const override;
    std::string getServiceName() const override;
    std::string getServiceVersion() const override;
    ServiceInfo getServiceInfo() const override;
    core::VoidResult configure(const std::unordered_map<std::string, std::string>& config) override;
    std::optional<std::string> getConfigValue(const std::string& key) const override;
    core::VoidResult resetConfiguration() override;
    bool isHealthy() const override;
    std::string getLastError() const override;
    core::AsyncResult<core::VoidResult> runSelfTest() override;
    PerformanceMetrics getPerformanceMetrics() const override;
    void resetPerformanceMetrics() override;
    
    // ========================================================================
    // IAudioProcessingService Implementation
    // ========================================================================
    
    core::VoidResult processBuffer(
        core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate
    ) override;
    
    core::VoidResult processBuffer(
        const core::FloatAudioBuffer& inputBuffer,
        core::FloatAudioBuffer& outputBuffer,
        core::SampleRate sampleRate
    ) override;
    
    core::VoidResult setParameters(const std::unordered_map<std::string, double>& parameters) override;
    std::unordered_map<std::string, double> getParameters() const override;
    core::VoidResult resetState() override;
    int32_t getLatencySamples() const override;
    
    // ========================================================================
    // Time Stretching Engines
    // ========================================================================
    
    /// Available time stretching engines
    enum class StretchEngine {
        SoundTouch,     // Good for speech, moderate quality, fast
        RubberBand,     // High quality, slower, good for music
        Automatic       // Choose best engine based on content
    };
    
    /// Set time stretching engine
    core::VoidResult setStretchEngine(StretchEngine engine);
    
    /// Get current time stretching engine
    StretchEngine getStretchEngine() const;
    
    /// Get available engines
    std::vector<StretchEngine> getAvailableEngines() const;
    
    // ========================================================================
    // Time and Pitch Control
    // ========================================================================
    
    /// Set time stretch ratio (1.0 = original speed, 0.5 = half speed, 2.0 = double speed)
    core::VoidResult setTimeRatio(double ratio);
    
    /// Get current time stretch ratio
    double getTimeRatio() const;
    
    /// Set pitch shift in semitones
    core::VoidResult setPitchShift(double semitones);
    
    /// Get current pitch shift
    double getPitchShift() const;
    
    /// Set pitch scale ratio (1.0 = original pitch, 2.0 = octave up, 0.5 = octave down)
    core::VoidResult setPitchRatio(double ratio);
    
    /// Get current pitch scale ratio
    double getPitchRatio() const;
    
    /// Set both time and pitch ratios simultaneously
    core::VoidResult setTimeAndPitchRatios(double timeRatio, double pitchRatio);
    
    /// Reset to original time and pitch
    core::VoidResult resetTimeAndPitch();
    
    // ========================================================================
    // Quality Settings
    // ========================================================================
    
    /// Quality presets
    enum class QualityPreset {
        Draft,          // Fastest, lowest quality
        Low,            // Fast, acceptable quality
        Standard,       // Balanced speed/quality
        High,           // Slower, high quality
        Premium,        // Slowest, highest quality
        Custom          // User-defined settings
    };
    
    /// Set quality preset
    core::VoidResult setQualityPreset(QualityPreset preset);
    
    /// Get current quality preset
    QualityPreset getQualityPreset() const;
    
    // SoundTouch specific settings
    struct SoundTouchSettings {
        bool useAntiAliasing = true;
        bool useQuickSeek = false;
        int sequenceMs = 0;         // 0 = auto
        int seekWindowMs = 0;       // 0 = auto  
        int overlapMs = 0;          // 0 = auto
    };
    
    /// Configure SoundTouch-specific settings
    core::VoidResult configureSoundTouch(const SoundTouchSettings& settings);
    
    /// Get SoundTouch settings
    SoundTouchSettings getSoundTouchSettings() const;
    
    // RubberBand specific settings
    struct RubberBandSettings {
        RubberBand::RubberBandStretcher::Options options = 
            RubberBand::RubberBandStretcher::OptionProcessRealTime |
            RubberBand::RubberBandStretcher::OptionStretchElastic;
        size_t debugLevel = 0;
    };
    
    /// Configure RubberBand-specific settings
    core::VoidResult configureRubberBand(const RubberBandSettings& settings);
    
    /// Get RubberBand settings
    RubberBandSettings getRubberBandSettings() const;
    
    // ========================================================================
    // Content Analysis and Automatic Settings
    // ========================================================================
    
    /// Audio content types for automatic optimization
    enum class ContentType {
        Unknown,
        Speech,
        Music,
        Percussion,
        Harmonic,
        Transient,
        Mixed
    };
    
    /// Analyze audio content type
    core::AsyncResult<core::Result<ContentType>> analyzeContentType(
        const core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate
    );
    
    /// Set content type manually
    core::VoidResult setContentType(ContentType contentType);
    
    /// Get current content type
    ContentType getContentType() const;
    
    /// Enable automatic parameter optimization based on content
    core::VoidResult setAutoOptimizationEnabled(bool enabled);
    
    /// Check if auto optimization is enabled
    bool isAutoOptimizationEnabled() const;
    
    // ========================================================================
    // Advanced Processing Options
    // ========================================================================
    
    /// Processing modes
    enum class ProcessingMode {
        Realtime,       // Low latency, streaming
        Offline,        // High quality, batch processing
        Preview         // Fast preview, lower quality
    };
    
    /// Set processing mode
    core::VoidResult setProcessingMode(ProcessingMode mode);
    
    /// Get current processing mode
    ProcessingMode getProcessingMode() const;
    
    /// Enable/disable formant preservation (for vocal content)
    core::VoidResult setFormantPreservationEnabled(bool enabled);
    
    /// Check if formant preservation is enabled
    bool isFormantPreservationEnabled() const;
    
    /// Set transient preservation level (0.0-1.0)
    core::VoidResult setTransientPreservation(double level);
    
    /// Get transient preservation level
    double getTransientPreservation() const;
    
    /// Enable/disable phase coherence maintenance
    core::VoidResult setPhaseCoherenceEnabled(bool enabled);
    
    /// Check if phase coherence is enabled
    bool isPhaseCoherenceEnabled() const;
    
    // ========================================================================
    // Batch Processing
    // ========================================================================
    
    /// Process entire audio file with time stretching
    core::AsyncResult<core::VoidResult> processFile(
        const std::string& inputPath,
        const std::string& outputPath,
        double timeRatio,
        double pitchRatio = 1.0,
        core::ProgressCallback progress = nullptr
    );
    
    /// Process audio buffer with time stretching
    core::AsyncResult<core::Result<core::FloatAudioBuffer>> processAudioBuffer(
        const core::FloatAudioBuffer& inputBuffer,
        core::SampleRate sampleRate,
        double timeRatio,
        double pitchRatio = 1.0,
        core::ProgressCallback progress = nullptr
    );
    
    /// Batch process multiple files
    core::AsyncResult<core::VoidResult> batchProcessFiles(
        const std::vector<std::pair<std::string, std::string>>& filePairs, // input, output
        double timeRatio,
        double pitchRatio = 1.0,
        core::ProgressCallback progress = nullptr
    );
    
    // ========================================================================
    // Real-time Streaming
    // ========================================================================
    
    /// Initialize streaming processor
    core::AsyncResult<core::VoidResult> initializeStreaming(
        core::SampleRate sampleRate,
        int32_t channels,
        int32_t maxFrameSize = 1024
    );
    
    /// Process streaming audio frame
    core::VoidResult processStreamingFrame(
        const float* inputSamples,
        float* outputSamples,
        int32_t frameSize,
        int32_t& outputFrameSize
    );
    
    /// Flush streaming processor
    core::VoidResult flushStreaming(
        float* outputSamples,
        int32_t maxOutputFrames,
        int32_t& outputFrameSize
    );
    
    /// Stop streaming processor
    core::VoidResult stopStreaming();
    
    /// Check if streaming is active
    bool isStreamingActive() const;
    
    /// Get streaming latency in samples
    int32_t getStreamingLatency() const;
    
    // ========================================================================
    // Time Stretching Presets
    // ========================================================================
    
    struct TimeStretchPreset {
        std::string name;
        std::string description;
        StretchEngine engine;
        QualityPreset quality;
        ContentType contentType;
        ProcessingMode processingMode;
        bool formantPreservation = false;
        double transientPreservation = 0.5;
        bool phaseCoherence = true;
        SoundTouchSettings soundTouchSettings;
        RubberBandSettings rubberBandSettings;
    };
    
    /// Get built-in presets
    std::vector<TimeStretchPreset> getBuiltInPresets() const;
    
    /// Load preset by name
    core::VoidResult loadPreset(const std::string& presetName);
    
    /// Save current settings as preset
    core::VoidResult savePreset(const std::string& presetName, const std::string& description = "");
    
    /// Delete custom preset
    core::VoidResult deletePreset(const std::string& presetName);
    
    /// Get all available presets (built-in + custom)
    std::vector<std::string> getAvailablePresets() const;
    
    // ========================================================================
    // Analysis and Metrics
    // ========================================================================
    
    struct StretchingMetrics {
        double inputDuration = 0.0;         // seconds
        double outputDuration = 0.0;        // seconds
        double actualTimeRatio = 0.0;       // achieved ratio
        double actualPitchRatio = 0.0;      // achieved ratio
        double processingTime = 0.0;        // seconds
        double realtimeRatio = 0.0;         // processing_time / input_duration
        size_t inputSamples = 0;
        size_t outputSamples = 0;
        double qualityScore = 0.0;          // 0.0-1.0 estimated quality
        std::vector<std::string> warnings;
    };
    
    /// Get metrics from last processing operation
    StretchingMetrics getLastStretchingMetrics() const;
    
    /// Estimate processing time for given parameters
    double estimateProcessingTime(
        double audioDuration,
        double timeRatio,
        double pitchRatio,
        StretchEngine engine,
        QualityPreset quality
    ) const;
    
    /// Analyze stretched audio quality
    core::AsyncResult<core::Result<double>> analyzeStretchingQuality(
        const core::FloatAudioBuffer& originalBuffer,
        const core::FloatAudioBuffer& stretchedBuffer,
        core::SampleRate sampleRate
    );
    
    // ========================================================================
    // Utilities
    // ========================================================================
    
    /// Convert time ratio to tempo change percentage
    static double timeRatioToTempoChange(double timeRatio);
    
    /// Convert tempo change percentage to time ratio
    static double tempoChangeToTimeRatio(double tempoChangePercent);
    
    /// Convert semitones to pitch ratio
    static double semitonesToPitchRatio(double semitones);
    
    /// Convert pitch ratio to semitones
    static double pitchRatioToSemitones(double pitchRatio);
    
    /// Validate time stretch parameters
    static bool areParametersValid(double timeRatio, double pitchRatio);
    
    /// Get recommended engine for content type
    static StretchEngine getRecommendedEngine(ContentType contentType);
    
    /// Get recommended quality for real-time processing
    static QualityPreset getRecommendedRealtimeQuality();
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize SoundTouch processor
    core::VoidResult initializeSoundTouch();
    
    /// Initialize RubberBand processor
    core::VoidResult initializeRubberBand(core::SampleRate sampleRate, int32_t channels);
    
    /// Cleanup processors
    void cleanupProcessors();
    
    /// Process with SoundTouch
    core::VoidResult processSoundTouch(
        const core::FloatAudioBuffer& inputBuffer,
        core::FloatAudioBuffer& outputBuffer
    );
    
    /// Process with RubberBand
    core::VoidResult processRubberBand(
        const core::FloatAudioBuffer& inputBuffer,
        core::FloatAudioBuffer& outputBuffer
    );
    
    /// Apply automatic optimization based on content
    void applyAutoOptimization();
    
    /// Update performance metrics
    void updatePerformanceMetrics(double processingTime, bool success);
    
    /// Detect audio content characteristics
    ContentType detectContentType(
        const core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate
    );
    
    /// Validate processing parameters
    core::VoidResult validateParameters() const;
    
private:
    // Processing engines
    std::unique_ptr<soundtouch::SoundTouch> soundTouch_;
    std::unique_ptr<RubberBand::RubberBandStretcher> rubberBand_;
    std::mutex processorMutex_;
    
    // Current settings
    StretchEngine currentEngine_ = StretchEngine::SoundTouch;
    QualityPreset qualityPreset_ = QualityPreset::Standard;
    ProcessingMode processingMode_ = ProcessingMode::Realtime;
    ContentType contentType_ = ContentType::Unknown;
    
    // Time and pitch parameters
    std::atomic<double> timeRatio_{1.0};
    std::atomic<double> pitchRatio_{1.0};
    std::atomic<bool> formantPreservation_{false};
    std::atomic<double> transientPreservation_{0.5};
    std::atomic<bool> phaseCoherence_{true};
    std::atomic<bool> autoOptimization_{false};
    
    // Engine-specific settings
    SoundTouchSettings soundTouchSettings_;
    RubberBandSettings rubberBandSettings_;
    
    // Streaming state
    std::atomic<bool> isStreamingActive_{false};
    core::SampleRate streamingSampleRate_ = 0;
    int32_t streamingChannels_ = 0;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
    
    // Configuration
    std::unordered_map<std::string, std::string> config_;
    std::mutex configMutex_;
    
    // Presets
    std::vector<TimeStretchPreset> builtInPresets_;
    std::unordered_map<std::string, TimeStretchPreset> customPresets_;
    std::mutex presetsMutex_;
    
    // Metrics
    mutable StretchingMetrics lastMetrics_;
    mutable std::mutex metricsMutex_;
    
    // Performance tracking
    mutable PerformanceMetrics performanceMetrics_;
    mutable std::mutex performanceMetricsMutex_;
    
    // Error tracking
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;
};

} // namespace mixmind::services