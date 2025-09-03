#pragma once

#include "types.h"
#include "result.h"
#include <memory>
#include <vector>

namespace mixmind::core {

// Forward declarations
class IClip;
class IPluginInstance;
class IAutomation;

// ============================================================================
// Track Interface - Individual track management
// ============================================================================

class ITrack {
public:
    virtual ~ITrack() = default;
    
    // ========================================================================
    // Track Identity and Properties
    // ========================================================================
    
    /// Get track ID
    virtual TrackID getId() const = 0;
    
    /// Get track name
    virtual std::string getName() const = 0;
    
    /// Set track name
    virtual VoidResult setName(const std::string& name) = 0;
    
    /// Check if this is an audio track (vs MIDI track)
    virtual bool isAudioTrack() const = 0;
    
    /// Check if this is a MIDI track
    virtual bool isMIDITrack() const = 0;
    
    /// Get number of channels (for audio tracks)
    virtual int32_t getChannelCount() const = 0;
    
    /// Set number of channels
    virtual AsyncResult<VoidResult> setChannelCount(int32_t channels) = 0;
    
    // ========================================================================
    // Track Color and Appearance
    // ========================================================================
    
    /// Set track color (hex color string, e.g., "#FF0000")
    virtual VoidResult setColor(const std::string& color) = 0;
    
    /// Get track color
    virtual std::string getColor() const = 0;
    
    /// Set track height in pixels (for UI)
    virtual VoidResult setHeight(int32_t height) = 0;
    
    /// Get track height
    virtual int32_t getHeight() const = 0;
    
    // ========================================================================
    // Track State
    // ========================================================================
    
    /// Get track volume (0.0 to 1.0, but can exceed 1.0 for gain)
    virtual float getVolume() const = 0;
    
    /// Set track volume
    virtual AsyncResult<VoidResult> setVolume(float volume) = 0;
    
    /// Get track volume in dB
    virtual float getVolumeDB() const = 0;
    
    /// Set track volume in dB
    virtual AsyncResult<VoidResult> setVolumeDB(float volumeDB) = 0;
    
    /// Get track pan (-1.0 = hard left, 0.0 = center, 1.0 = hard right)
    virtual float getPan() const = 0;
    
    /// Set track pan
    virtual AsyncResult<VoidResult> setPan(float pan) = 0;
    
    /// Check if track is muted
    virtual bool isMuted() const = 0;
    
    /// Set track mute state
    virtual AsyncResult<VoidResult> setMuted(bool muted) = 0;
    
    /// Check if track is soloed
    virtual bool isSoloed() const = 0;
    
    /// Set track solo state
    virtual AsyncResult<VoidResult> setSoloed(bool soloed) = 0;
    
    /// Check if track is record armed
    virtual bool isRecordArmed() const = 0;
    
    /// Set track record arm state
    virtual AsyncResult<VoidResult> setRecordArmed(bool armed) = 0;
    
    /// Check if track input is being monitored
    virtual bool isInputMonitored() const = 0;
    
    /// Set input monitoring state
    virtual AsyncResult<VoidResult> setInputMonitored(bool monitored) = 0;
    
    // ========================================================================
    // Track I/O
    // ========================================================================
    
    /// Get input source name
    virtual std::string getInputSource() const = 0;
    
    /// Set input source
    virtual AsyncResult<VoidResult> setInputSource(const std::string& source) = 0;
    
    /// Get available input sources
    virtual std::vector<std::string> getAvailableInputSources() const = 0;
    
    /// Get output destination
    virtual std::string getOutputDestination() const = 0;
    
    /// Set output destination  
    virtual AsyncResult<VoidResult> setOutputDestination(const std::string& destination) = 0;
    
    /// Get available output destinations
    virtual std::vector<std::string> getAvailableOutputDestinations() const = 0;
    
    // ========================================================================
    // Clip Management
    // ========================================================================
    
    /// Create new clip on this track
    virtual AsyncResult<Result<ClipID>> createClip(const ClipConfig& config) = 0;
    
    /// Delete clip
    virtual AsyncResult<VoidResult> deleteClip(ClipID clipId) = 0;
    
    /// Get clip by ID
    virtual std::shared_ptr<IClip> getClip(ClipID clipId) = 0;
    
    /// Get all clips on this track
    virtual std::vector<std::shared_ptr<IClip>> getAllClips() = 0;
    
    /// Get clips in time range
    virtual std::vector<std::shared_ptr<IClip>> getClipsInRange(TimestampSamples start, TimestampSamples end) = 0;
    
    /// Get number of clips
    virtual int32_t getClipCount() const = 0;
    
    /// Move clip to new position
    virtual AsyncResult<VoidResult> moveClip(ClipID clipId, TimestampSamples newPosition) = 0;
    
    /// Duplicate clip
    virtual AsyncResult<Result<ClipID>> duplicateClip(ClipID clipId, TimestampSamples position) = 0;
    
    /// Split clip at position
    virtual AsyncResult<Result<std::vector<ClipID>>> splitClip(ClipID clipId, TimestampSamples position) = 0;
    
    /// Join adjacent clips
    virtual AsyncResult<Result<ClipID>> joinClips(const std::vector<ClipID>& clipIds) = 0;
    
    // ========================================================================
    // Plugin Chain Management
    // ========================================================================
    
    /// Get number of plugin slots
    virtual int32_t getPluginSlotCount() const = 0;
    
    /// Insert plugin at specific slot
    virtual AsyncResult<Result<PluginInstanceID>> insertPlugin(const PluginID& pluginId, int32_t slotIndex) = 0;
    
    /// Add plugin at end of chain
    virtual AsyncResult<Result<PluginInstanceID>> addPlugin(const PluginID& pluginId) = 0;
    
    /// Remove plugin from slot
    virtual AsyncResult<VoidResult> removePlugin(int32_t slotIndex) = 0;
    
    /// Get plugin at slot
    virtual std::shared_ptr<IPluginInstance> getPlugin(int32_t slotIndex) = 0;
    
    /// Get all plugins
    virtual std::vector<std::shared_ptr<IPluginInstance>> getAllPlugins() = 0;
    
    /// Move plugin to different slot
    virtual AsyncResult<VoidResult> movePlugin(int32_t fromSlot, int32_t toSlot) = 0;
    
    /// Bypass plugin
    virtual AsyncResult<VoidResult> bypassPlugin(int32_t slotIndex, bool bypassed) = 0;
    
    /// Check if plugin is bypassed
    virtual bool isPluginBypassed(int32_t slotIndex) const = 0;
    
    /// Bypass entire plugin chain
    virtual AsyncResult<VoidResult> bypassAllPlugins(bool bypassed) = 0;
    
    /// Check if plugin chain is bypassed
    virtual bool areAllPluginsBypassed() const = 0;
    
    // ========================================================================
    // Built-in Processing
    // ========================================================================
    
    /// Enable/disable built-in EQ
    virtual AsyncResult<VoidResult> setEQEnabled(bool enabled) = 0;
    
    /// Check if built-in EQ is enabled
    virtual bool isEQEnabled() const = 0;
    
    /// EQ band configuration
    struct EQBand {
        float frequency = 1000.0f;  // Hz
        float gain = 0.0f;          // dB
        float q = 1.0f;             // Q factor
        enum class Type { HighPass, LowShelf, Bell, HighShelf, LowPass } type = Type::Bell;
        bool enabled = true;
    };
    
    /// Get EQ band count
    virtual int32_t getEQBandCount() const = 0;
    
    /// Configure EQ band
    virtual AsyncResult<VoidResult> setEQBand(int32_t bandIndex, const EQBand& band) = 0;
    
    /// Get EQ band configuration
    virtual EQBand getEQBand(int32_t bandIndex) const = 0;
    
    /// Enable/disable built-in compressor
    virtual AsyncResult<VoidResult> setCompressorEnabled(bool enabled) = 0;
    
    /// Check if built-in compressor is enabled
    virtual bool isCompressorEnabled() const = 0;
    
    /// Compressor settings
    struct CompressorSettings {
        float threshold = -20.0f;   // dB
        float ratio = 4.0f;         // compression ratio
        float attack = 10.0f;       // ms
        float release = 100.0f;     // ms
        float knee = 2.0f;          // dB
        float makeupGain = 0.0f;    // dB
        bool autoMakeupGain = true;
    };
    
    /// Configure compressor
    virtual AsyncResult<VoidResult> setCompressorSettings(const CompressorSettings& settings) = 0;
    
    /// Get compressor settings
    virtual CompressorSettings getCompressorSettings() const = 0;
    
    // ========================================================================
    // Send Effects
    // ========================================================================
    
    /// Get number of send slots
    virtual int32_t getSendSlotCount() const = 0;
    
    /// Configure send
    virtual AsyncResult<VoidResult> setSend(int32_t sendIndex, TrackID destinationTrack, float level) = 0;
    
    /// Enable/disable send
    virtual AsyncResult<VoidResult> setSendEnabled(int32_t sendIndex, bool enabled) = 0;
    
    /// Get send level
    virtual float getSendLevel(int32_t sendIndex) const = 0;
    
    /// Get send destination
    virtual TrackID getSendDestination(int32_t sendIndex) const = 0;
    
    /// Check if send is enabled
    virtual bool isSendEnabled(int32_t sendIndex) const = 0;
    
    /// Set send pre/post fader
    virtual AsyncResult<VoidResult> setSendPreFader(int32_t sendIndex, bool preFader) = 0;
    
    /// Check if send is pre-fader
    virtual bool isSendPreFader(int32_t sendIndex) const = 0;
    
    // ========================================================================
    // Automation
    // ========================================================================
    
    /// Get automation interface for this track
    virtual std::shared_ptr<IAutomation> getAutomation() = 0;
    
    /// Get automation for specific parameter
    virtual std::shared_ptr<IAutomation> getParameterAutomation(const ParamID& paramId) = 0;
    
    /// Enable/disable read automation
    virtual AsyncResult<VoidResult> setAutomationRead(bool enabled) = 0;
    
    /// Check if automation read is enabled
    virtual bool isAutomationRead() const = 0;
    
    /// Enable/disable write automation
    virtual AsyncResult<VoidResult> setAutomationWrite(bool enabled) = 0;
    
    /// Check if automation write is enabled
    virtual bool isAutomationWrite() const = 0;
    
    /// Set automation mode
    enum class AutomationMode {
        Off,        // No automation
        Read,       // Read only
        Write,      // Write/record automation
        Touch,      // Touch mode (write when touching control)
        Latch       // Latch mode (write after touching until stop)
    };
    
    /// Set automation mode
    virtual AsyncResult<VoidResult> setAutomationMode(AutomationMode mode) = 0;
    
    /// Get automation mode
    virtual AutomationMode getAutomationMode() const = 0;
    
    // ========================================================================
    // Track Freezing
    // ========================================================================
    
    /// Check if track can be frozen
    virtual bool canFreeze() const = 0;
    
    /// Freeze track (render all processing to audio)
    virtual AsyncResult<VoidResult> freeze() = 0;
    
    /// Unfreeze track
    virtual AsyncResult<VoidResult> unfreeze() = 0;
    
    /// Check if track is frozen
    virtual bool isFrozen() const = 0;
    
    /// Get frozen audio file path
    virtual std::string getFrozenFilePath() const = 0;
    
    // ========================================================================
    // Track Templates and Presets
    // ========================================================================
    
    /// Save track as template
    virtual AsyncResult<VoidResult> saveAsTemplate(const std::string& templateName, 
                                                   const std::string& description = "") = 0;
    
    /// Load track template
    virtual AsyncResult<VoidResult> loadTemplate(const std::string& templateName) = 0;
    
    /// Get available track templates
    virtual std::vector<std::string> getAvailableTemplates() const = 0;
    
    // ========================================================================
    // Recording
    // ========================================================================
    
    /// Start recording on this track
    virtual AsyncResult<VoidResult> startRecording() = 0;
    
    /// Stop recording on this track
    virtual AsyncResult<VoidResult> stopRecording() = 0;
    
    /// Check if currently recording
    virtual bool isCurrentlyRecording() const = 0;
    
    /// Set recording mode for this track
    enum class TrackRecordingMode {
        Normal,         // Standard recording
        Overdub,        // Layer on existing audio
        Replace,        // Replace existing audio
        TouchReplace    // Replace only while recording
    };
    
    /// Set recording mode
    virtual AsyncResult<VoidResult> setRecordingMode(TrackRecordingMode mode) = 0;
    
    /// Get recording mode
    virtual TrackRecordingMode getRecordingMode() const = 0;
    
    // ========================================================================
    // MIDI-Specific Features (for MIDI tracks)
    // ========================================================================
    
    /// Get MIDI channel (1-16, 0 = any)
    virtual int32_t getMIDIChannel() const = 0;
    
    /// Set MIDI channel
    virtual AsyncResult<VoidResult> setMIDIChannel(int32_t channel) = 0;
    
    /// Get MIDI program (0-127)
    virtual int32_t getMIDIProgram() const = 0;
    
    /// Set MIDI program
    virtual AsyncResult<VoidResult> setMIDIProgram(int32_t program) = 0;
    
    /// Get MIDI bank (0-127)
    virtual int32_t getMIDIBank() const = 0;
    
    /// Set MIDI bank
    virtual AsyncResult<VoidResult> setMIDIBank(int32_t bank) = 0;
    
    /// Enable/disable MIDI thru
    virtual AsyncResult<VoidResult> setMIDIThru(bool enabled) = 0;
    
    /// Check if MIDI thru is enabled
    virtual bool isMIDIThru() const = 0;
    
    // ========================================================================
    // Performance and Metering
    // ========================================================================
    
    /// Get current output level (peak)
    virtual std::vector<float> getCurrentOutputLevel() const = 0;  // Per channel
    
    /// Get current input level (when monitoring)
    virtual std::vector<float> getCurrentInputLevel() const = 0;   // Per channel
    
    /// Enable/disable level metering
    virtual VoidResult setMeteringEnabled(bool enabled) = 0;
    
    /// Check if metering is enabled
    virtual bool isMeteringEnabled() const = 0;
    
    /// Get CPU usage for this track (0.0 to 1.0)
    virtual float getCPUUsage() const = 0;
    
    /// Get memory usage for this track in bytes
    virtual size_t getMemoryUsage() const = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class TrackEvent {
        NameChanged,
        VolumeChanged,
        PanChanged,
        MuteChanged,
        SoloChanged,
        RecordArmChanged,
        MonitoringChanged,
        ClipAdded,
        ClipRemoved,
        ClipMoved,
        PluginAdded,
        PluginRemoved,
        PluginBypassed,
        AutomationChanged,
        RecordingStarted,
        RecordingStopped,
        FreezeChanged
    };
    
    using TrackEventCallback = std::function<void(TrackEvent event, const std::string& details)>;
    
    /// Subscribe to track events
    virtual void addEventListener(TrackEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(TrackEventCallback callback) = 0;
    
    // ========================================================================
    // Track Statistics and Info
    // ========================================================================
    
    struct TrackInfo {
        TrackID id;
        std::string name;
        bool isAudio;
        int32_t channelCount;
        int32_t clipCount;
        int32_t pluginCount;
        TimestampSamples totalLength;
        size_t memoryUsage;
        float cpuUsage;
        bool isFrozen;
        std::string color;
    };
    
    /// Get comprehensive track information
    virtual TrackInfo getTrackInfo() const = 0;
    
    /// Get track length (longest clip end time)
    virtual TimestampSamples getLength() const = 0;
    
    /// Check if track has any audio content
    virtual bool hasContent() const = 0;
    
    /// Check if track is empty
    virtual bool isEmpty() const = 0;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Enable/disable track grouping
    virtual AsyncResult<VoidResult> setGrouped(bool grouped) = 0;
    
    /// Check if track is part of a group
    virtual bool isGrouped() const = 0;
    
    /// Get group ID (if part of a group)
    virtual std::optional<std::string> getGroupID() const = 0;
    
    /// Set track as folder track (can contain other tracks)
    virtual AsyncResult<VoidResult> setFolderTrack(bool isFolder) = 0;
    
    /// Check if this is a folder track
    virtual bool isFolderTrack() const = 0;
    
    /// Get parent folder track (if nested)
    virtual std::optional<TrackID> getParentFolder() const = 0;
    
    /// Get child tracks (if folder track)
    virtual std::vector<TrackID> getChildTracks() const = 0;
};

} // namespace mixmind::core