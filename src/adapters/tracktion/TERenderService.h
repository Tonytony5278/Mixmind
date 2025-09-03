#pragma once

#include "TEAdapter.h"
#include "../../core/IRenderService.h"
#include "../../core/types.h"
#include "../../core/result.h"
#include <tracktion_engine/tracktion_engine.h>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <future>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TE Render Service Adapter - Tracktion Engine implementation of IRenderService
// ============================================================================

class TERenderService : public TEAdapter, public core::IRenderService {
public:
    explicit TERenderService(te::Engine& engine);
    ~TERenderService() override = default;
    
    // Non-copyable, movable
    TERenderService(const TERenderService&) = delete;
    TERenderService& operator=(const TERenderService&) = delete;
    TERenderService(TERenderService&&) = default;
    TERenderService& operator=(TERenderService&&) = default;
    
    // ========================================================================
    // IRenderService Implementation
    // ========================================================================
    
    // Session Rendering
    core::AsyncResult<core::VoidResult> renderSession(
        const std::string& outputPath,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::Result<core::FloatAudioBuffer>> renderSessionToBuffer(
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    // Track Rendering
    core::AsyncResult<core::VoidResult> renderTrack(
        core::TrackID trackId,
        const std::string& outputPath,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::Result<core::FloatAudioBuffer>> renderTrackToBuffer(
        core::TrackID trackId,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::VoidResult> renderMultipleTracks(
        const std::vector<core::TrackID>& trackIds,
        const std::string& outputDirectory,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    // Clip Rendering
    core::AsyncResult<core::VoidResult> renderClip(
        core::ClipID clipId,
        const std::string& outputPath,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::Result<core::FloatAudioBuffer>> renderClipToBuffer(
        core::ClipID clipId,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    // Time Range Rendering
    core::AsyncResult<core::VoidResult> renderTimeRange(
        core::TimePosition startTime,
        core::TimePosition endTime,
        const std::string& outputPath,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::Result<core::FloatAudioBuffer>> renderTimeRangeToBuffer(
        core::TimePosition startTime,
        core::TimePosition endTime,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    // Real-time Rendering
    core::AsyncResult<core::VoidResult> startRealtimeRender(
        const std::string& outputPath,
        const RenderSettings& settings
    ) override;
    
    core::AsyncResult<core::VoidResult> stopRealtimeRender() override;
    core::AsyncResult<core::VoidResult> pauseRealtimeRender() override;
    core::AsyncResult<core::VoidResult> resumeRealtimeRender() override;
    bool isRealtimeRenderActive() const override;
    
    // Stems and Multi-channel Rendering
    core::AsyncResult<core::VoidResult> renderStems(
        const std::vector<core::TrackID>& trackIds,
        const std::string& outputDirectory,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::VoidResult> renderMultiChannelMix(
        const std::string& outputPath,
        const std::vector<ChannelRouting>& channelRoutings,
        const RenderSettings& settings,
        core::ProgressCallback progress = nullptr
    ) override;
    
    // Render Queue Management
    core::AsyncResult<core::Result<core::RenderJobID>> queueRenderJob(
        const RenderJob& job,
        int32_t priority = 0
    ) override;
    
    core::AsyncResult<core::VoidResult> cancelRenderJob(core::RenderJobID jobId) override;
    core::AsyncResult<core::VoidResult> pauseRenderJob(core::RenderJobID jobId) override;
    core::AsyncResult<core::VoidResult> resumeRenderJob(core::RenderJobID jobId) override;
    
    core::AsyncResult<core::Result<std::vector<RenderJobInfo>>> getRenderQueue() const override;
    core::AsyncResult<core::Result<RenderJobInfo>> getRenderJobStatus(core::RenderJobID jobId) const override;
    core::AsyncResult<core::VoidResult> clearRenderQueue() override;
    
    // Render Monitoring
    core::AsyncResult<core::Result<RenderProgress>> getCurrentRenderProgress() const override;
    core::AsyncResult<core::Result<std::vector<RenderStatistics>>> getRenderHistory() const override;
    core::AsyncResult<core::VoidResult> clearRenderHistory() override;
    
    // Render Presets and Templates
    core::AsyncResult<core::VoidResult> saveRenderPreset(
        const std::string& presetName,
        const RenderSettings& settings
    ) override;
    
    core::AsyncResult<core::Result<RenderSettings>> loadRenderPreset(
        const std::string& presetName
    ) const override;
    
    core::AsyncResult<core::Result<std::vector<std::string>>> getRenderPresets() const override;
    core::AsyncResult<core::VoidResult> deleteRenderPreset(const std::string& presetName) override;
    
    // Format Support and Validation
    std::vector<AudioFormat> getSupportedFormats() const override;
    bool isFormatSupported(const AudioFormat& format) const override;
    core::Result<core::VoidResult> validateRenderSettings(const RenderSettings& settings) const override;
    
    // Event Callbacks
    void setRenderProgressCallback(RenderProgressCallback callback) override;
    void setRenderCompleteCallback(RenderCompleteCallback callback) override;
    void clearRenderCallbacks() override;

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Get current edit for rendering
    te::Edit* getCurrentEdit() const;
    
    /// Convert RenderSettings to TE render parameters
    te::Renderer::Parameters convertRenderSettingsToTE(const RenderSettings& settings) const;
    
    /// Convert AudioFormat to TE audio format
    std::unique_ptr<te::AudioFormat> convertAudioFormatToTE(const AudioFormat& format) const;
    
    /// Convert ChannelRouting to TE bus arrangement
    te::EditPlaybackContext::BusArrangement convertChannelRoutingToTE(const ChannelRouting& routing) const;
    
    /// Create TE render context
    std::unique_ptr<te::RenderContext> createRenderContext(
        const RenderSettings& settings,
        te::Edit& edit
    ) const;
    
    /// Setup render progress tracking
    void setupRenderProgressTracking(
        te::Renderer& renderer,
        core::ProgressCallback progress,
        RenderProgressCallback renderProgress
    );
    
    /// Process render job from queue
    void processRenderJob(const RenderJobInfo& jobInfo);
    
    /// Start render queue processing
    void startRenderQueueProcessing();
    
    /// Stop render queue processing
    void stopRenderQueueProcessing();
    
    /// Generate unique render job ID
    core::RenderJobID generateRenderJobID();
    
    /// Update render statistics
    void updateRenderStatistics(const RenderJobInfo& jobInfo, bool success, double renderTime);
    
    /// Emit render progress event
    void emitRenderProgressEvent(const RenderProgress& progress);
    
    /// Emit render complete event
    void emitRenderCompleteEvent(core::RenderJobID jobId, bool success, const std::string& outputPath);

private:
    // Render queue management
    std::vector<RenderJobInfo> renderQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::thread queueProcessingThread_;
    std::atomic<bool> shouldStopQueueProcessing_{false};
    
    // Active render tracking
    std::atomic<bool> isRenderingActive_{false};
    std::atomic<bool> isRealtimeRenderActive_{false};
    core::RenderJobID currentRenderJobId_{0};
    mutable RenderProgress currentRenderProgress_;
    mutable std::mutex renderProgressMutex_;
    
    // Render job ID generation
    std::atomic<uint32_t> nextRenderJobId_{1};
    
    // Render history and statistics
    std::vector<RenderStatistics> renderHistory_;
    mutable std::shared_mutex renderHistoryMutex_;
    
    // Render presets
    std::unordered_map<std::string, RenderSettings> renderPresets_;
    mutable std::shared_mutex renderPresetsMutex_;
    
    // Event callbacks
    RenderProgressCallback renderProgressCallback_;
    RenderCompleteCallback renderCompleteCallback_;
    std::mutex callbackMutex_;
    
    // Current edit reference
    mutable te::Edit* currentEdit_ = nullptr;
    mutable std::mutex editMutex_;
    
    // Real-time render state
    std::unique_ptr<te::Renderer> realtimeRenderer_;
    std::string realtimeOutputPath_;
    RenderSettings realtimeSettings_;
    mutable std::mutex realtimeRenderMutex_;
    
    // Supported formats cache
    mutable std::vector<AudioFormat> supportedFormats_;
    mutable bool supportedFormatsInitialized_ = false;
    mutable std::mutex supportedFormatsMutex_;
    
    // Constants
    static constexpr int32_t MAX_RENDER_HISTORY = 100;
    static constexpr int32_t DEFAULT_RENDER_TIMEOUT_MS = 300000; // 5 minutes
};

} // namespace mixmind::adapters::tracktion