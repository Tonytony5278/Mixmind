#pragma once

#include "types.h"
#include "result.h"
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace mixmind::core {

// Forward declarations
class ISession;
class ITrack;

// ============================================================================
// Render Service Interface - Audio rendering and export
// ============================================================================

class IRenderService {
public:
    virtual ~IRenderService() = default;
    
    // ========================================================================
    // Render Formats and Quality
    // ========================================================================
    
    enum class RenderFormat {
        WAV,
        FLAC,
        MP3,
        AAC,
        OGG,
        AIFF,
        M4A,
        WMA
    };
    
    enum class BitDepth {
        Int16 = 16,
        Int24 = 24,
        Int32 = 32,
        Float32 = 32,
        Float64 = 64
    };
    
    enum class MP3Quality {
        VBR_High = 0,    // ~245 kbps
        VBR_Standard = 4, // ~165 kbps
        VBR_Medium = 6,   // ~130 kbps
        CBR_320 = 320,
        CBR_256 = 256,
        CBR_192 = 192,
        CBR_128 = 128
    };
    
    struct RenderConfig {
        RenderFormat format = RenderFormat::WAV;
        SampleRate sampleRate = 44100;
        BitDepth bitDepth = BitDepth::Int16;
        int32_t channels = 2;
        
        // Format-specific settings
        MP3Quality mp3Quality = MP3Quality::VBR_Standard;
        int32_t flacCompressionLevel = 5;  // 0-8
        float oggQuality = 0.5f;           // 0.0-1.0
        
        // Dithering
        bool enableDithering = true;
        enum class DitherType { None, Triangular, Shaped } ditherType = DitherType::Triangular;
        
        // Normalization
        bool enableNormalization = false;
        float normalizationLevel = -0.1f;  // dB
        
        // Fade in/out
        bool enableFadeIn = false;
        bool enableFadeOut = false;
        TimestampSamples fadeInLength = 0;
        TimestampSamples fadeOutLength = 0;
        
        // Metadata
        std::string title;
        std::string artist;
        std::string album;
        std::string genre;
        std::string comment;
        int32_t year = 0;
        int32_t trackNumber = 0;
    };
    
    // ========================================================================
    // Render Operations
    // ========================================================================
    
    /// Render entire session to file
    virtual AsyncResult<VoidResult> renderSession(
        std::shared_ptr<ISession> session,
        const std::string& outputPath,
        const RenderConfig& config,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Render specific time range
    virtual AsyncResult<VoidResult> renderRange(
        std::shared_ptr<ISession> session,
        TimestampSamples startTime,
        TimestampSamples endTime,
        const std::string& outputPath,
        const RenderConfig& config,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Render specific tracks only
    virtual AsyncResult<VoidResult> renderTracks(
        std::shared_ptr<ISession> session,
        const std::vector<TrackID>& trackIds,
        const std::string& outputPath,
        const RenderConfig& config,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Render tracks as separate files (stem export)
    virtual AsyncResult<VoidResult> renderStems(
        std::shared_ptr<ISession> session,
        const std::vector<TrackID>& trackIds,
        const std::string& outputDirectory,
        const RenderConfig& config,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Render selected regions/clips
    virtual AsyncResult<VoidResult> renderSelection(
        std::shared_ptr<ISession> session,
        const std::vector<std::pair<TimestampSamples, TimestampSamples>>& regions,
        const std::string& outputPath,
        const RenderConfig& config,
        ProgressCallback progress = nullptr
    ) = 0;
    
    // ========================================================================
    // Real-time Rendering
    // ========================================================================
    
    /// Start real-time rendering to buffer
    virtual AsyncResult<VoidResult> startRealtimeRender(
        std::shared_ptr<ISession> session,
        const RenderConfig& config
    ) = 0;
    
    /// Stop real-time rendering
    virtual AsyncResult<VoidResult> stopRealtimeRender() = 0;
    
    /// Get real-time render buffer
    virtual std::vector<float> getRealtimeBuffer() = 0;
    
    /// Check if real-time rendering is active
    virtual bool isRealtimeRenderActive() const = 0;
    
    // ========================================================================
    // Bounce Operations
    // ========================================================================
    
    /// Bounce track to audio (freeze with replacement)
    virtual AsyncResult<Result<ClipID>> bounceTrack(
        std::shared_ptr<ITrack> track,
        TimestampSamples startTime,
        TimestampSamples endTime,
        const RenderConfig& config
    ) = 0;
    
    /// Bounce track range to new track
    virtual AsyncResult<Result<TrackID>> bounceToNewTrack(
        std::shared_ptr<ISession> session,
        const std::vector<TrackID>& sourceTracks,
        TimestampSamples startTime,
        TimestampSamples endTime,
        const RenderConfig& config
    ) = 0;
    
    /// Bounce selection in place
    virtual AsyncResult<VoidResult> bounceSelectionInPlace(
        std::shared_ptr<ISession> session,
        const std::vector<std::pair<TrackID, std::pair<TimestampSamples, TimestampSamples>>>& selections,
        const RenderConfig& config
    ) = 0;
    
    // ========================================================================
    // Render Queue Management
    // ========================================================================
    
    struct RenderJob {
        std::string jobId;
        std::string description;
        std::string outputPath;
        RenderConfig config;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point started;
        std::chrono::system_clock::time_point completed;
        
        enum class Status {
            Queued,
            Running,
            Completed,
            Failed,
            Cancelled
        } status = Status::Queued;
        
        float progress = 0.0f;
        std::string statusMessage;
        size_t estimatedSize = 0;    // bytes
        double estimatedDuration = 0.0; // seconds
    };
    
    /// Add render job to queue
    virtual AsyncResult<Result<std::string>> queueRenderJob(
        std::shared_ptr<ISession> session,
        const std::string& description,
        const std::string& outputPath,
        const RenderConfig& config
    ) = 0;
    
    /// Cancel render job
    virtual AsyncResult<VoidResult> cancelRenderJob(const std::string& jobId) = 0;
    
    /// Get render job status
    virtual RenderJob getRenderJobStatus(const std::string& jobId) const = 0;
    
    /// Get all render jobs
    virtual std::vector<RenderJob> getAllRenderJobs() const = 0;
    
    /// Clear completed render jobs
    virtual VoidResult clearCompletedJobs() = 0;
    
    /// Set maximum concurrent render jobs
    virtual VoidResult setMaxConcurrentJobs(int32_t maxJobs) = 0;
    
    /// Get maximum concurrent render jobs
    virtual int32_t getMaxConcurrentJobs() const = 0;
    
    // ========================================================================
    // Render Statistics and Analysis
    // ========================================================================
    
    struct RenderStats {
        double renderTime = 0.0;        // seconds
        double audioLength = 0.0;       // seconds
        double realtimeRatio = 0.0;     // render_time / audio_length
        size_t outputFileSize = 0;      // bytes
        float peakLevel = 0.0f;         // dB
        float rmsLevel = 0.0f;          // dB
        float lufsLevel = 0.0f;         // LUFS
        bool clippingDetected = false;
        int32_t totalSamples = 0;
        int32_t clippedSamples = 0;
    };
    
    /// Get statistics for last completed render
    virtual RenderStats getLastRenderStats() const = 0;
    
    /// Get statistics for specific render job
    virtual RenderStats getRenderJobStats(const std::string& jobId) const = 0;
    
    /// Enable/disable render statistics collection
    virtual VoidResult setStatsCollectionEnabled(bool enabled) = 0;
    
    /// Check if stats collection is enabled
    virtual bool isStatsCollectionEnabled() const = 0;
    
    // ========================================================================
    // Quality Analysis
    // ========================================================================
    
    struct QualityAnalysis {
        float dynamicRange = 0.0f;      // dB
        float stereoWidth = 0.0f;       // 0.0-1.0
        float bassEnergy = 0.0f;        // 0.0-1.0
        float midEnergy = 0.0f;         // 0.0-1.0
        float trebleEnergy = 0.0f;      // 0.0-1.0
        float spectralCentroid = 0.0f;  // Hz
        float spectralRolloff = 0.0f;   // Hz
        bool phaseIssues = false;
        std::vector<float> frequencySpectrum;
    };
    
    /// Analyze rendered audio quality
    virtual AsyncResult<Result<QualityAnalysis>> analyzeQuality(const std::string& filePath) = 0;
    
    /// Compare quality between formats
    virtual AsyncResult<Result<std::vector<QualityAnalysis>>> compareFormats(
        std::shared_ptr<ISession> session,
        const std::vector<RenderConfig>& configs,
        TimestampSamples startTime = 0,
        TimestampSamples endTime = 0
    ) = 0;
    
    // ========================================================================
    // Render Templates and Presets
    // ========================================================================
    
    /// Save render configuration as preset
    virtual VoidResult saveRenderPreset(const std::string& presetName, const RenderConfig& config) = 0;
    
    /// Load render configuration preset
    virtual std::optional<RenderConfig> loadRenderPreset(const std::string& presetName) const = 0;
    
    /// Get available render presets
    virtual std::vector<std::string> getAvailablePresets() const = 0;
    
    /// Delete render preset
    virtual VoidResult deleteRenderPreset(const std::string& presetName) = 0;
    
    /// Get default render configuration
    virtual RenderConfig getDefaultConfig() const = 0;
    
    /// Set default render configuration
    virtual VoidResult setDefaultConfig(const RenderConfig& config) = 0;
    
    // ========================================================================
    // Format Support and Capabilities
    // ========================================================================
    
    /// Check if render format is supported
    virtual bool isFormatSupported(RenderFormat format) const = 0;
    
    /// Get supported formats
    virtual std::vector<RenderFormat> getSupportedFormats() const = 0;
    
    /// Get supported sample rates for format
    virtual std::vector<SampleRate> getSupportedSampleRates(RenderFormat format) const = 0;
    
    /// Get supported bit depths for format
    virtual std::vector<BitDepth> getSupportedBitDepths(RenderFormat format) const = 0;
    
    /// Get maximum channels for format
    virtual int32_t getMaxChannels(RenderFormat format) const = 0;
    
    /// Get file extension for format
    virtual std::string getFileExtension(RenderFormat format) const = 0;
    
    // ========================================================================
    // Render Engine Configuration
    // ========================================================================
    
    /// Set render thread count
    virtual VoidResult setRenderThreadCount(int32_t threadCount) = 0;
    
    /// Get render thread count
    virtual int32_t getRenderThreadCount() const = 0;
    
    /// Set render buffer size
    virtual VoidResult setRenderBufferSize(BufferSize bufferSize) = 0;
    
    /// Get render buffer size
    virtual BufferSize getRenderBufferSize() const = 0;
    
    /// Enable/disable render optimization
    virtual VoidResult setOptimizationEnabled(bool enabled) = 0;
    
    /// Check if optimization is enabled
    virtual bool isOptimizationEnabled() const = 0;
    
    /// Set render priority
    enum class RenderPriority {
        Low,
        Normal,
        High,
        Realtime
    };
    
    virtual VoidResult setRenderPriority(RenderPriority priority) = 0;
    
    /// Get render priority
    virtual RenderPriority getRenderPriority() const = 0;
    
    // ========================================================================
    // Disk I/O and Caching
    // ========================================================================
    
    /// Set render cache directory
    virtual VoidResult setRenderCacheDirectory(const std::string& directory) = 0;
    
    /// Get render cache directory
    virtual std::string getRenderCacheDirectory() const = 0;
    
    /// Clear render cache
    virtual VoidResult clearRenderCache() = 0;
    
    /// Get render cache size
    virtual size_t getRenderCacheSize() const = 0;
    
    /// Set maximum cache size
    virtual VoidResult setMaxCacheSize(size_t sizeBytes) = 0;
    
    /// Enable/disable disk streaming during render
    virtual VoidResult setDiskStreamingEnabled(bool enabled) = 0;
    
    /// Check if disk streaming is enabled
    virtual bool isDiskStreamingEnabled() const = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class RenderEvent {
        JobQueued,
        JobStarted,
        JobProgress,
        JobCompleted,
        JobFailed,
        JobCancelled,
        QueueEmpty,
        ConfigChanged
    };
    
    using RenderEventCallback = std::function<void(RenderEvent event, const std::string& details, const std::optional<std::string>& jobId)>;
    
    /// Subscribe to render events
    virtual void addEventListener(RenderEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(RenderEventCallback callback) = 0;
    
    // ========================================================================
    // Advanced Rendering Features
    // ========================================================================
    
    /// Render with tail (capture plugin reverb tails)
    virtual AsyncResult<VoidResult> renderWithTail(
        std::shared_ptr<ISession> session,
        const std::string& outputPath,
        const RenderConfig& config,
        TimestampSamples tailLength,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Render with freeze mode (temporarily disable effects)
    virtual AsyncResult<VoidResult> renderFrozen(
        std::shared_ptr<ISession> session,
        const std::vector<TrackID>& tracksToFreeze,
        const std::string& outputPath,
        const RenderConfig& config,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Render with latency compensation
    virtual VoidResult setLatencyCompensationEnabled(bool enabled) = 0;
    
    /// Check if latency compensation is enabled
    virtual bool isLatencyCompensationEnabled() const = 0;
    
    /// Set render look-ahead samples
    virtual VoidResult setRenderLookahead(int32_t samples) = 0;
    
    /// Get render look-ahead samples
    virtual int32_t getRenderLookahead() const = 0;
};

} // namespace mixmind::core