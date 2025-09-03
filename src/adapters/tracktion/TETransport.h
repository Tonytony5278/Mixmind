#pragma once

#include "../../core/ITransport.h"
#include "TEAdapter.h"
#include <tracktion_engine/tracktion_engine.h>
#include <memory>
#include <atomic>
#include <mutex>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TE Transport Adapter - Implements ITransport using Tracktion Engine
// ============================================================================

class TETransport : public core::ITransport, public TEAdapter {
public:
    explicit TETransport(te::Engine& engine, te::Edit& edit);
    ~TETransport() override;
    
    // ========================================================================
    // ITransport Implementation - Basic Transport Control
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> play() override;
    core::AsyncResult<core::VoidResult> stop() override;
    core::AsyncResult<core::VoidResult> pause() override;
    core::AsyncResult<core::VoidResult> record() override;
    core::AsyncResult<core::VoidResult> togglePlayPause() override;
    core::AsyncResult<core::VoidResult> toggleRecord() override;
    
    // ========================================================================
    // ITransport Implementation - Position Control
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> locate(core::TimestampSamples position) override;
    core::AsyncResult<core::VoidResult> locateSeconds(core::TimestampSeconds seconds) override;
    core::AsyncResult<core::VoidResult> locateMusical(const std::string& musicalTime) override;
    core::AsyncResult<core::VoidResult> gotoStart() override;
    core::AsyncResult<core::VoidResult> gotoEnd() override;
    core::AsyncResult<core::VoidResult> rewind(core::TimestampSamples samples) override;
    core::AsyncResult<core::VoidResult> fastForward(core::TimestampSamples samples) override;
    
    // ========================================================================
    // ITransport Implementation - Current State
    // ========================================================================
    
    core::TransportState getState() const override;
    core::TimestampSamples getPosition() const override;
    core::TimestampSeconds getPositionSeconds() const override;
    std::string getPositionMusical() const override;
    bool isPlaying() const override;
    bool isRecording() const override;
    bool isPaused() const override;
    bool isStopped() const override;
    
    // ========================================================================
    // ITransport Implementation - Loop Control
    // ========================================================================
    
    core::VoidResult setLoopEnabled(bool enabled) override;
    bool isLoopEnabled() const override;
    core::VoidResult setLoopRegion(core::TimestampSamples start, core::TimestampSamples end) override;
    core::TimestampSamples getLoopStart() const override;
    core::TimestampSamples getLoopEnd() const override;
    core::VoidResult setLoopMode(core::LoopMode mode) override;
    core::LoopMode getLoopMode() const override;
    
    // ========================================================================
    // ITransport Implementation - Punch Recording
    // ========================================================================
    
    core::VoidResult setPunchEnabled(bool enabled) override;
    bool isPunchEnabled() const override;
    core::VoidResult setPunchRegion(core::TimestampSamples punchIn, core::TimestampSamples punchOut) override;
    core::TimestampSamples getPunchIn() const override;
    core::TimestampSamples getPunchOut() const override;
    core::VoidResult setAutoPunchEnabled(bool enabled) override;
    bool isAutoPunchEnabled() const override;
    
    // ========================================================================
    // ITransport Implementation - Metronome Control
    // ========================================================================
    
    core::VoidResult setMetronomeEnabled(bool enabled) override;
    bool isMetronomeEnabled() const override;
    core::VoidResult setMetronomeRecordOnly(bool recordOnly) override;
    bool isMetronomeRecordOnly() const override;
    core::VoidResult setMetronomeVolume(float volume) override;
    float getMetronomeVolume() const override;
    core::VoidResult setMetronomeSound(MetronomeSound sound, const std::string& customSoundPath = "") override;
    MetronomeSound getMetronomeSound() const override;
    
    // ========================================================================
    // ITransport Implementation - Pre-roll and Post-roll
    // ========================================================================
    
    core::VoidResult setPreRollEnabled(bool enabled) override;
    bool isPreRollEnabled() const override;
    core::VoidResult setPreRollLength(core::TimestampSamples samples) override;
    core::TimestampSamples getPreRollLength() const override;
    core::VoidResult setPreRollBars(int32_t bars) override;
    int32_t getPreRollBars() const override;
    
    core::VoidResult setPostRollEnabled(bool enabled) override;
    bool isPostRollEnabled() const override;
    core::VoidResult setPostRollLength(core::TimestampSamples samples) override;
    core::TimestampSamples getPostRollLength() const override;
    
    // ========================================================================
    // ITransport Implementation - Follow Modes
    // ========================================================================
    
    core::VoidResult setFollowMode(FollowMode mode) override;
    FollowMode getFollowMode() const override;
    
    // ========================================================================
    // ITransport Implementation - Scrubbing
    // ========================================================================
    
    core::VoidResult setScrubEnabled(bool enabled) override;
    bool isScrubEnabled() const override;
    core::AsyncResult<core::VoidResult> scrubToPosition(core::TimestampSamples position) override;
    
    // ========================================================================
    // ITransport Implementation - Playback Speed and Pitch
    // ========================================================================
    
    core::VoidResult setPlaybackSpeed(float speed) override;
    float getPlaybackSpeed() const override;
    core::VoidResult resetPlaybackSpeed() override;
    core::VoidResult setPitchCorrectionEnabled(bool enabled) override;
    bool isPitchCorrectionEnabled() const override;
    
    // ========================================================================
    // ITransport Implementation - Transport Synchronization
    // ========================================================================
    
    core::VoidResult setSyncSource(SyncSource source) override;
    SyncSource getSyncSource() const override;
    bool isExternallysynced() const override;
    std::string getSyncStatus() const override;
    
    // ========================================================================
    // ITransport Implementation - MIDI Control
    // ========================================================================
    
    core::VoidResult setMIDIControlEnabled(bool enabled) override;
    bool isMIDIControlEnabled() const override;
    core::VoidResult mapMIDIControl(const core::MidiMessage& message, const std::string& function) override;
    core::VoidResult clearMIDIControlMappings() override;
    
    // ========================================================================
    // ITransport Implementation - Event Notifications
    // ========================================================================
    
    void addEventListener(core::TransportEventCallback callback) override;
    void removeEventListener(core::TransportEventCallback callback) override;
    
    // ========================================================================
    // ITransport Implementation - Advanced Control
    // ========================================================================
    
    core::TransportInfo getTransportInfo() const override;
    core::AsyncResult<core::VoidResult> applySettings(const TransportSettings& settings) override;
    TransportSettings getCurrentSettings() const override;
    
    // ========================================================================
    // ITransport Implementation - Quantization
    // ========================================================================
    
    core::VoidResult setQuantizationGrid(QuantizationGrid grid) override;
    QuantizationGrid getQuantizationGrid() const override;
    core::VoidResult setCustomQuantization(core::TimestampSamples samples) override;
    core::AsyncResult<core::VoidResult> quantizeCurrentPosition() override;
    
    // ========================================================================
    // ITransport Implementation - Recording Modes
    // ========================================================================
    
    core::VoidResult setRecordingMode(RecordingMode mode) override;
    RecordingMode getRecordingMode() const override;
    core::VoidResult setMaxRecordingDuration(core::TimestampSamples maxDuration) override;
    core::TimestampSamples getMaxRecordingDuration() const override;
    
    // ========================================================================
    // TE-Specific Methods
    // ========================================================================
    
    /// Get the underlying TE transport control
    te::TransportControl& getTransportControl() { return transportControl_; }
    const te::TransportControl& getTransportControl() const { return transportControl_; }
    
    /// Get the TE edit
    te::Edit& getEdit() { return edit_; }
    const te::Edit& getEdit() const { return edit_; }
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Convert musical time string to TE time position
    te::TimePosition parseMusicalTime(const std::string& musicalTime) const;
    
    /// Format TE time position as musical time string
    std::string formatMusicalTime(te::TimePosition position) const;
    
    /// Setup TE transport callbacks
    void setupTransportCallbacks();
    void cleanupTransportCallbacks();
    
    /// Notify transport event to listeners
    void notifyTransportEvent(core::TransportEvent event, const std::string& details = "");
    
    /// Update cached transport info
    void updateTransportInfo() const;
    
    /// Convert our sync source to TE sync source
    te::TimecodeDisplayFormat convertToTESyncSource(SyncSource source) const;
    
    /// Convert TE sync source to our sync source
    SyncSource convertFromTESyncSource(te::TimecodeDisplayFormat teSource) const;
    
    /// Convert our quantization grid to TE time position
    te::TimePosition convertQuantizationToTimePosition(QuantizationGrid grid) const;
    
private:
    te::Edit& edit_;
    te::TransportControl& transportControl_;
    
    // Transport state
    mutable std::recursive_mutex transportMutex_;
    std::atomic<bool> isPaused_{false};
    std::atomic<float> playbackSpeed_{1.0f};
    std::atomic<bool> pitchCorrectionEnabled_{true};
    
    // Loop and punch settings
    std::atomic<bool> loopEnabled_{false};
    std::atomic<bool> punchEnabled_{false};
    std::atomic<bool> autoPunchEnabled_{false};
    core::TimestampSamples loopStart_{0};
    core::TimestampSamples loopEnd_{0};
    core::TimestampSamples punchIn_{0};
    core::TimestampSamples punchOut_{0};
    
    // Metronome settings
    std::atomic<bool> metronomeEnabled_{false};
    std::atomic<bool> metronomeRecordOnly_{false};
    std::atomic<float> metronomeVolume_{0.8f};
    MetronomeSound metronomeSound_{MetronomeSound::Click};
    std::string customMetronomePath_;
    
    // Pre/post roll settings
    std::atomic<bool> preRollEnabled_{false};
    std::atomic<bool> postRollEnabled_{false};
    core::TimestampSamples preRollLength_{0};
    core::TimestampSamples postRollLength_{0};
    
    // Scrubbing and follow mode
    std::atomic<bool> scrubEnabled_{true};
    FollowMode followMode_{FollowMode::Page};
    
    // Sync and MIDI control
    SyncSource syncSource_{SyncSource::Internal};
    std::atomic<bool> midiControlEnabled_{false};
    std::unordered_map<std::string, std::string> midiControlMappings_;
    
    // Quantization
    QuantizationGrid quantizationGrid_{QuantizationGrid::Off};
    core::TimestampSamples customQuantization_{0};
    
    // Recording mode
    RecordingMode recordingMode_{RecordingMode::Overwrite};
    core::TimestampSamples maxRecordingDuration_{0}; // 0 = unlimited
    
    // Event callbacks
    std::vector<core::TransportEventCallback> eventCallbacks_;
    std::mutex callbackMutex_;
    
    // Cached info
    mutable core::TransportInfo cachedTransportInfo_;
    mutable std::chrono::steady_clock::time_point lastInfoUpdate_;
    static constexpr auto INFO_CACHE_DURATION = std::chrono::milliseconds(50);
    
    // TE callbacks
    std::unique_ptr<te::TransportControl::Listener> teTransportListener_;
    
    // Position tracking for pause state
    te::TimePosition pausedPosition_{};
};

} // namespace mixmind::adapters::tracktion