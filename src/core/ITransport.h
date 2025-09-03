#pragma once

#include "types.h"
#include "result.h"
#include <functional>

namespace mixmind::core {

// ============================================================================
// Transport Interface - Playback and recording control
// ============================================================================

class ITransport {
public:
    virtual ~ITransport() = default;
    
    // ========================================================================
    // Basic Transport Control
    // ========================================================================
    
    /// Start playback
    virtual AsyncResult<VoidResult> play() = 0;
    
    /// Stop playback/recording
    virtual AsyncResult<VoidResult> stop() = 0;
    
    /// Pause playback (can resume from same position)
    virtual AsyncResult<VoidResult> pause() = 0;
    
    /// Start recording
    virtual AsyncResult<VoidResult> record() = 0;
    
    /// Toggle play/pause
    virtual AsyncResult<VoidResult> togglePlayPause() = 0;
    
    /// Toggle recording
    virtual AsyncResult<VoidResult> toggleRecord() = 0;
    
    // ========================================================================
    // Position Control
    // ========================================================================
    
    /// Jump to specific position
    virtual AsyncResult<VoidResult> locate(TimestampSamples position) = 0;
    
    /// Jump to specific time in seconds
    virtual AsyncResult<VoidResult> locateSeconds(TimestampSeconds seconds) = 0;
    
    /// Jump to musical position (bars:beats:ticks)
    virtual AsyncResult<VoidResult> locateMusical(const std::string& musicalTime) = 0;
    
    /// Jump to beginning
    virtual AsyncResult<VoidResult> gotoStart() = 0;
    
    /// Jump to end
    virtual AsyncResult<VoidResult> gotoEnd() = 0;
    
    /// Rewind (jump back by specified amount)
    virtual AsyncResult<VoidResult> rewind(TimestampSamples samples) = 0;
    
    /// Fast forward (jump ahead by specified amount)
    virtual AsyncResult<VoidResult> fastForward(TimestampSamples samples) = 0;
    
    // ========================================================================
    // Current State
    // ========================================================================
    
    /// Get current transport state
    virtual TransportState getState() const = 0;
    
    /// Get current playback position
    virtual TimestampSamples getPosition() const = 0;
    
    /// Get current position in seconds
    virtual TimestampSeconds getPositionSeconds() const = 0;
    
    /// Get current position in musical time
    virtual std::string getPositionMusical() const = 0;
    
    /// Check if currently playing
    virtual bool isPlaying() const = 0;
    
    /// Check if currently recording
    virtual bool isRecording() const = 0;
    
    /// Check if currently paused
    virtual bool isPaused() const = 0;
    
    /// Check if currently stopped
    virtual bool isStopped() const = 0;
    
    // ========================================================================
    // Loop Control
    // ========================================================================
    
    /// Enable/disable looping
    virtual VoidResult setLoopEnabled(bool enabled) = 0;
    
    /// Check if looping is enabled
    virtual bool isLoopEnabled() const = 0;
    
    /// Set loop region
    virtual VoidResult setLoopRegion(TimestampSamples start, TimestampSamples end) = 0;
    
    /// Get loop start position
    virtual TimestampSamples getLoopStart() const = 0;
    
    /// Get loop end position  
    virtual TimestampSamples getLoopEnd() const = 0;
    
    /// Set loop mode (normal, ping-pong, etc.)
    virtual VoidResult setLoopMode(LoopMode mode) = 0;
    
    /// Get current loop mode
    virtual LoopMode getLoopMode() const = 0;
    
    // ========================================================================
    // Punch Recording
    // ========================================================================
    
    /// Enable/disable punch recording
    virtual VoidResult setPunchEnabled(bool enabled) = 0;
    
    /// Check if punch recording is enabled
    virtual bool isPunchEnabled() const = 0;
    
    /// Set punch region
    virtual VoidResult setPunchRegion(TimestampSamples punchIn, TimestampSamples punchOut) = 0;
    
    /// Get punch in position
    virtual TimestampSamples getPunchIn() const = 0;
    
    /// Get punch out position
    virtual TimestampSamples getPunchOut() const = 0;
    
    /// Auto punch (automatically start/stop recording in punch region)
    virtual VoidResult setAutoPunchEnabled(bool enabled) = 0;
    
    /// Check if auto punch is enabled
    virtual bool isAutoPunchEnabled() const = 0;
    
    // ========================================================================
    // Metronome Control
    // ========================================================================
    
    /// Enable/disable metronome
    virtual VoidResult setMetronomeEnabled(bool enabled) = 0;
    
    /// Check if metronome is enabled
    virtual bool isMetronomeEnabled() const = 0;
    
    /// Enable metronome only during recording
    virtual VoidResult setMetronomeRecordOnly(bool recordOnly) = 0;
    
    /// Check if metronome is record-only
    virtual bool isMetronomeRecordOnly() const = 0;
    
    /// Set metronome volume
    virtual VoidResult setMetronomeVolume(float volume) = 0;  // 0.0 to 1.0
    
    /// Get metronome volume
    virtual float getMetronomeVolume() const = 0;
    
    /// Set metronome sound (built-in click types)
    enum class MetronomeSound {
        Click,
        Beep,
        Woodblock,
        Cowbell,
        Custom
    };
    
    virtual VoidResult setMetronomeSound(MetronomeSound sound, const std::string& customSoundPath = "") = 0;
    
    /// Get current metronome sound
    virtual MetronomeSound getMetronomeSound() const = 0;
    
    // ========================================================================
    // Pre-roll and Post-roll
    // ========================================================================
    
    /// Enable/disable pre-roll
    virtual VoidResult setPreRollEnabled(bool enabled) = 0;
    
    /// Check if pre-roll is enabled
    virtual bool isPreRollEnabled() const = 0;
    
    /// Set pre-roll length
    virtual VoidResult setPreRollLength(TimestampSamples samples) = 0;
    
    /// Get pre-roll length
    virtual TimestampSamples getPreRollLength() const = 0;
    
    /// Set pre-roll length in bars
    virtual VoidResult setPreRollBars(int32_t bars) = 0;
    
    /// Get pre-roll length in bars
    virtual int32_t getPreRollBars() const = 0;
    
    /// Enable/disable post-roll
    virtual VoidResult setPostRollEnabled(bool enabled) = 0;
    
    /// Check if post-roll is enabled
    virtual bool isPostRollEnabled() const = 0;
    
    /// Set post-roll length
    virtual VoidResult setPostRollLength(TimestampSamples samples) = 0;
    
    /// Get post-roll length
    virtual TimestampSamples getPostRollLength() const = 0;
    
    // ========================================================================
    // Follow Modes
    // ========================================================================
    
    enum class FollowMode {
        None,           // No automatic following
        Page,           // Follow by page
        Continuous,     // Smooth continuous following
        Centre          // Keep playhead centered
    };
    
    /// Set follow mode for UI
    virtual VoidResult setFollowMode(FollowMode mode) = 0;
    
    /// Get current follow mode
    virtual FollowMode getFollowMode() const = 0;
    
    // ========================================================================
    // Scrubbing
    // ========================================================================
    
    /// Enable/disable scrubbing (audio feedback while dragging position)
    virtual VoidResult setScrubEnabled(bool enabled) = 0;
    
    /// Check if scrubbing is enabled
    virtual bool isScrubEnabled() const = 0;
    
    /// Scrub to position (with audio feedback)
    virtual AsyncResult<VoidResult> scrubToPosition(TimestampSamples position) = 0;
    
    // ========================================================================
    // Playback Speed and Pitch
    // ========================================================================
    
    /// Set playback speed (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
    virtual VoidResult setPlaybackSpeed(float speed) = 0;
    
    /// Get current playback speed
    virtual float getPlaybackSpeed() const = 0;
    
    /// Reset playback speed to normal
    virtual VoidResult resetPlaybackSpeed() = 0;
    
    /// Enable/disable pitch correction during speed changes
    virtual VoidResult setPitchCorrectionEnabled(bool enabled) = 0;
    
    /// Check if pitch correction is enabled
    virtual bool isPitchCorrectionEnabled() const = 0;
    
    // ========================================================================
    // Transport Synchronization
    // ========================================================================
    
    enum class SyncSource {
        Internal,       // Internal clock
        MTC,           // MIDI Time Code
        MMC,           // MIDI Machine Control  
        LTC,           // Linear Time Code
        WordClock,     // Word clock
        AES3,          // AES3/AES-EBU
        ADAT           // ADAT optical
    };
    
    /// Set sync source
    virtual VoidResult setSyncSource(SyncSource source) = 0;
    
    /// Get current sync source
    virtual SyncSource getSyncSource() const = 0;
    
    /// Check if transport is synced to external source
    virtual bool isExternallysynced() const = 0;
    
    /// Get sync status/health
    virtual std::string getSyncStatus() const = 0;
    
    // ========================================================================
    // MIDI Control
    // ========================================================================
    
    /// Enable/disable MIDI control of transport
    virtual VoidResult setMIDIControlEnabled(bool enabled) = 0;
    
    /// Check if MIDI control is enabled
    virtual bool isMIDIControlEnabled() const = 0;
    
    /// Map MIDI message to transport function
    virtual VoidResult mapMIDIControl(const MidiMessage& message, const std::string& function) = 0;
    
    /// Clear MIDI control mappings
    virtual VoidResult clearMIDIControlMappings() = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class TransportEvent {
        StateChanged,       // Play/stop/record state changed
        PositionChanged,    // Playback position changed
        LoopChanged,        // Loop settings changed
        TempoChanged,       // Tempo changed
        TimeSignatureChanged,
        SyncStatusChanged   // Sync status changed
    };
    
    using TransportEventCallback = std::function<void(TransportEvent event, const TransportInfo& info)>;
    
    /// Subscribe to transport events
    virtual void addEventListener(TransportEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(TransportEventCallback callback) = 0;
    
    // ========================================================================
    // Advanced Control
    // ========================================================================
    
    /// Get comprehensive transport information
    virtual TransportInfo getTransportInfo() const = 0;
    
    /// Apply multiple transport changes atomically
    struct TransportSettings {
        std::optional<bool> loopEnabled;
        std::optional<TimestampSamples> loopStart;
        std::optional<TimestampSamples> loopEnd;
        std::optional<bool> metronomeEnabled;
        std::optional<float> metronomeVolume;
        std::optional<bool> preRollEnabled;
        std::optional<TimestampSamples> preRollLength;
        std::optional<float> playbackSpeed;
        std::optional<SyncSource> syncSource;
    };
    
    /// Apply multiple settings in one transaction
    virtual AsyncResult<VoidResult> applySettings(const TransportSettings& settings) = 0;
    
    /// Get current settings
    virtual TransportSettings getCurrentSettings() const = 0;
    
    // ========================================================================
    // Quantization
    // ========================================================================
    
    enum class QuantizationGrid {
        Off,            // No quantization
        Bar,            // Quantize to bars
        Beat,           // Quantize to beats  
        Half,           // Half notes
        Quarter,        // Quarter notes
        Eighth,         // Eighth notes
        Sixteenth,      // Sixteenth notes
        ThirtySecond,   // Thirty-second notes
        Triplet,        // Triplet quantization
        Custom          // Custom grid
    };
    
    /// Set position quantization for locate operations
    virtual VoidResult setQuantizationGrid(QuantizationGrid grid) = 0;
    
    /// Get current quantization grid
    virtual QuantizationGrid getQuantizationGrid() const = 0;
    
    /// Set custom quantization value (in samples)
    virtual VoidResult setCustomQuantization(TimestampSamples samples) = 0;
    
    /// Quantize current position to grid
    virtual AsyncResult<VoidResult> quantizeCurrentPosition() = 0;
    
    // ========================================================================
    // Recording Modes
    // ========================================================================
    
    enum class RecordingMode {
        Overwrite,      // Replace existing audio
        Overdub,        // Layer on top of existing audio
        AutoPunch,      // Automatically punch in/out
        Loop,           // Loop recording (create takes)
        Comping         // Create multiple takes for comping
    };
    
    /// Set recording mode
    virtual VoidResult setRecordingMode(RecordingMode mode) = 0;
    
    /// Get current recording mode
    virtual RecordingMode getRecordingMode() const = 0;
    
    /// Set maximum recording duration (0 = unlimited)
    virtual VoidResult setMaxRecordingDuration(TimestampSamples maxDuration) = 0;
    
    /// Get maximum recording duration
    virtual TimestampSamples getMaxRecordingDuration() const = 0;
};

} // namespace mixmind::core