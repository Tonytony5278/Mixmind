#pragma once

#include "../../core/ITrack.h"
#include "TEAdapter.h"
#include <tracktion_engine/tracktion_engine.h>
#include <memory>
#include <atomic>
#include <unordered_map>

namespace mixmind::adapters::tracktion {

// Forward declarations
class TEClip;
class TEPluginInstance;
class TEAutomation;

// ============================================================================
// TE Track Adapter - Implements ITrack using Tracktion Engine Track
// ============================================================================

class TETrack : public core::ITrack, public TEAdapter {
public:
    explicit TETrack(te::Engine& engine, te::Track::Ptr teTrack);
    ~TETrack() override;
    
    // ========================================================================
    // ITrack Implementation - Track Identity and Properties
    // ========================================================================
    
    core::TrackID getId() const override;
    std::string getName() const override;
    core::VoidResult setName(const std::string& name) override;
    bool isAudioTrack() const override;
    bool isMIDITrack() const override;
    int32_t getChannelCount() const override;
    core::AsyncResult<core::VoidResult> setChannelCount(int32_t channels) override;
    
    // ========================================================================
    // ITrack Implementation - Track Color and Appearance
    // ========================================================================
    
    core::VoidResult setColor(const std::string& color) override;
    std::string getColor() const override;
    core::VoidResult setHeight(int32_t height) override;
    int32_t getHeight() const override;
    
    // ========================================================================
    // ITrack Implementation - Track State
    // ========================================================================
    
    float getVolume() const override;
    core::AsyncResult<core::VoidResult> setVolume(float volume) override;
    float getVolumeDB() const override;
    core::AsyncResult<core::VoidResult> setVolumeDB(float volumeDB) override;
    float getPan() const override;
    core::AsyncResult<core::VoidResult> setPan(float pan) override;
    bool isMuted() const override;
    core::AsyncResult<core::VoidResult> setMuted(bool muted) override;
    bool isSoloed() const override;
    core::AsyncResult<core::VoidResult> setSoloed(bool soloed) override;
    bool isRecordArmed() const override;
    core::AsyncResult<core::VoidResult> setRecordArmed(bool armed) override;
    bool isInputMonitored() const override;
    core::AsyncResult<core::VoidResult> setInputMonitored(bool monitored) override;
    
    // ========================================================================
    // ITrack Implementation - Track I/O
    // ========================================================================
    
    std::string getInputSource() const override;
    core::AsyncResult<core::VoidResult> setInputSource(const std::string& source) override;
    std::vector<std::string> getAvailableInputSources() const override;
    std::string getOutputDestination() const override;
    core::AsyncResult<core::VoidResult> setOutputDestination(const std::string& destination) override;
    std::vector<std::string> getAvailableOutputDestinations() const override;
    
    // ========================================================================
    // ITrack Implementation - Clip Management
    // ========================================================================
    
    core::AsyncResult<core::Result<core::ClipID>> createClip(const core::ClipConfig& config) override;
    core::AsyncResult<core::VoidResult> deleteClip(core::ClipID clipId) override;
    std::shared_ptr<core::IClip> getClip(core::ClipID clipId) override;
    std::vector<std::shared_ptr<core::IClip>> getAllClips() override;
    std::vector<std::shared_ptr<core::IClip>> getClipsInRange(core::TimestampSamples start, core::TimestampSamples end) override;
    int32_t getClipCount() const override;
    core::AsyncResult<core::VoidResult> moveClip(core::ClipID clipId, core::TimestampSamples newPosition) override;
    core::AsyncResult<core::Result<core::ClipID>> duplicateClip(core::ClipID clipId, core::TimestampSamples position) override;
    core::AsyncResult<core::Result<std::vector<core::ClipID>>> splitClip(core::ClipID clipId, core::TimestampSamples position) override;
    core::AsyncResult<core::Result<core::ClipID>> joinClips(const std::vector<core::ClipID>& clipIds) override;
    
    // ========================================================================
    // ITrack Implementation - Plugin Chain Management
    // ========================================================================
    
    int32_t getPluginSlotCount() const override;
    core::AsyncResult<core::Result<core::PluginInstanceID>> insertPlugin(const core::PluginID& pluginId, int32_t slotIndex) override;
    core::AsyncResult<core::Result<core::PluginInstanceID>> addPlugin(const core::PluginID& pluginId) override;
    core::AsyncResult<core::VoidResult> removePlugin(int32_t slotIndex) override;
    std::shared_ptr<core::IPluginInstance> getPlugin(int32_t slotIndex) override;
    std::vector<std::shared_ptr<core::IPluginInstance>> getAllPlugins() override;
    core::AsyncResult<core::VoidResult> movePlugin(int32_t fromSlot, int32_t toSlot) override;
    core::AsyncResult<core::VoidResult> bypassPlugin(int32_t slotIndex, bool bypassed) override;
    bool isPluginBypassed(int32_t slotIndex) const override;
    core::AsyncResult<core::VoidResult> bypassAllPlugins(bool bypassed) override;
    bool areAllPluginsBypassed() const override;
    
    // ========================================================================
    // ITrack Implementation - Built-in Processing
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> setEQEnabled(bool enabled) override;
    bool isEQEnabled() const override;
    int32_t getEQBandCount() const override;
    core::AsyncResult<core::VoidResult> setEQBand(int32_t bandIndex, const EQBand& band) override;
    EQBand getEQBand(int32_t bandIndex) const override;
    
    core::AsyncResult<core::VoidResult> setCompressorEnabled(bool enabled) override;
    bool isCompressorEnabled() const override;
    core::AsyncResult<core::VoidResult> setCompressorSettings(const CompressorSettings& settings) override;
    CompressorSettings getCompressorSettings() const override;
    
    // ========================================================================
    // ITrack Implementation - Send Effects
    // ========================================================================
    
    int32_t getSendSlotCount() const override;
    core::AsyncResult<core::VoidResult> setSend(int32_t sendIndex, core::TrackID destinationTrack, float level) override;
    core::AsyncResult<core::VoidResult> setSendEnabled(int32_t sendIndex, bool enabled) override;
    float getSendLevel(int32_t sendIndex) const override;
    core::TrackID getSendDestination(int32_t sendIndex) const override;
    bool isSendEnabled(int32_t sendIndex) const override;
    core::AsyncResult<core::VoidResult> setSendPreFader(int32_t sendIndex, bool preFader) override;
    bool isSendPreFader(int32_t sendIndex) const override;
    
    // ========================================================================
    // ITrack Implementation - Automation
    // ========================================================================
    
    std::shared_ptr<core::IAutomation> getAutomation() override;
    std::shared_ptr<core::IAutomation> getParameterAutomation(const core::ParamID& paramId) override;
    core::AsyncResult<core::VoidResult> setAutomationRead(bool enabled) override;
    bool isAutomationRead() const override;
    core::AsyncResult<core::VoidResult> setAutomationWrite(bool enabled) override;
    bool isAutomationWrite() const override;
    core::AsyncResult<core::VoidResult> setAutomationMode(AutomationMode mode) override;
    AutomationMode getAutomationMode() const override;
    
    // ========================================================================
    // ITrack Implementation - Track Freezing
    // ========================================================================
    
    bool canFreeze() const override;
    core::AsyncResult<core::VoidResult> freeze() override;
    core::AsyncResult<core::VoidResult> unfreeze() override;
    bool isFrozen() const override;
    std::string getFrozenFilePath() const override;
    
    // ========================================================================
    // ITrack Implementation - Track Templates and Presets
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> saveAsTemplate(const std::string& templateName, const std::string& description = "") override;
    core::AsyncResult<core::VoidResult> loadTemplate(const std::string& templateName) override;
    std::vector<std::string> getAvailableTemplates() const override;
    
    // ========================================================================
    // ITrack Implementation - Recording
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> startRecording() override;
    core::AsyncResult<core::VoidResult> stopRecording() override;
    bool isCurrentlyRecording() const override;
    core::AsyncResult<core::VoidResult> setRecordingMode(TrackRecordingMode mode) override;
    TrackRecordingMode getRecordingMode() const override;
    
    // ========================================================================
    // ITrack Implementation - MIDI-Specific Features
    // ========================================================================
    
    int32_t getMIDIChannel() const override;
    core::AsyncResult<core::VoidResult> setMIDIChannel(int32_t channel) override;
    int32_t getMIDIProgram() const override;
    core::AsyncResult<core::VoidResult> setMIDIProgram(int32_t program) override;
    int32_t getMIDIBank() const override;
    core::AsyncResult<core::VoidResult> setMIDIBank(int32_t bank) override;
    core::AsyncResult<core::VoidResult> setMIDIThru(bool enabled) override;
    bool isMIDIThru() const override;
    
    // ========================================================================
    // ITrack Implementation - Performance and Metering
    // ========================================================================
    
    std::vector<float> getCurrentOutputLevel() const override;
    std::vector<float> getCurrentInputLevel() const override;
    core::VoidResult setMeteringEnabled(bool enabled) override;
    bool isMeteringEnabled() const override;
    float getCPUUsage() const override;
    size_t getMemoryUsage() const override;
    
    // ========================================================================
    // ITrack Implementation - Event Notifications
    // ========================================================================
    
    void addEventListener(TrackEventCallback callback) override;
    void removeEventListener(TrackEventCallback callback) override;
    
    // ========================================================================
    // ITrack Implementation - Track Statistics and Info
    // ========================================================================
    
    TrackInfo getTrackInfo() const override;
    core::TimestampSamples getLength() const override;
    bool hasContent() const override;
    bool isEmpty() const override;
    
    // ========================================================================
    // ITrack Implementation - Advanced Features
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> setGrouped(bool grouped) override;
    bool isGrouped() const override;
    std::optional<std::string> getGroupID() const override;
    core::AsyncResult<core::VoidResult> setFolderTrack(bool isFolder) override;
    bool isFolderTrack() const override;
    std::optional<core::TrackID> getParentFolder() const override;
    std::vector<core::TrackID> getChildTracks() const override;
    
    // ========================================================================
    // TE-Specific Methods
    // ========================================================================
    
    /// Get the underlying TE Track
    te::Track::Ptr getTETrack() const { return teTrack_; }
    
    /// Set track ID (used by session)
    void setTrackID(core::TrackID trackId) { trackId_ = trackId; }
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize track mappings and state
    void initializeTrack();
    
    /// Update cached track info
    void updateTrackInfo() const;
    
    /// Notify track event to listeners
    void notifyTrackEvent(TrackEvent event, const std::string& details = "");
    
    /// Generate unique clip ID
    core::ClipID generateClipID() const;
    
    /// Find TE clip by our clip ID
    te::Clip::Ptr findTEClip(core::ClipID clipId) const;
    
    /// Wrap TE clip in our interface
    std::shared_ptr<core::IClip> wrapClip(te::Clip::Ptr teClip);
    
    /// Setup TE track callbacks
    void setupTrackCallbacks();
    void cleanupTrackCallbacks();
    
    /// Get volume plugin (create if needed)
    te::VolumeAndPanPlugin::Ptr getVolumePlugin() const;
    
    /// Get EQ plugin (create if needed)
    te::Plugin::Ptr getEQPlugin() const;
    
    /// Convert our EQ band to TE EQ parameters
    void applyEQBandToPlugin(te::Plugin::Ptr eqPlugin, int32_t bandIndex, const EQBand& band);
    
    /// Extract EQ band from TE EQ plugin
    EQBand extractEQBandFromPlugin(te::Plugin::Ptr eqPlugin, int32_t bandIndex) const;
    
private:
    te::Track::Ptr teTrack_;
    core::TrackID trackId_;
    
    // Clip management
    std::unordered_map<core::ClipID, te::Clip::Ptr> clipMap_;
    std::unordered_map<te::Clip*, core::ClipID> reverseClipMap_;
    mutable std::atomic<uint32_t> nextClipId_{1};
    
    // Plugin management
    std::vector<std::shared_ptr<core::IPluginInstance>> pluginInstances_;
    
    // Track state
    mutable std::recursive_mutex trackMutex_;
    std::atomic<bool> meteringEnabled_{true};
    
    // Built-in processing
    std::atomic<bool> eqEnabled_{false};
    std::atomic<bool> compressorEnabled_{false};
    std::array<EQBand, 4> eqBands_; // 4-band EQ
    CompressorSettings compressorSettings_;
    
    // Send effects
    static constexpr int32_t MAX_SENDS = 8;
    struct SendInfo {
        bool enabled = false;
        core::TrackID destination{0};
        float level = 0.0f;
        bool preFader = false;
    };
    std::array<SendInfo, MAX_SENDS> sends_;
    
    // Automation
    AutomationMode automationMode_{AutomationMode::Off};
    std::atomic<bool> automationRead_{true};
    std::atomic<bool> automationWrite_{false};
    std::shared_ptr<core::IAutomation> mainAutomation_;
    std::unordered_map<core::ParamID, std::shared_ptr<core::IAutomation>> parameterAutomation_;
    
    // Recording
    TrackRecordingMode recordingMode_{TrackRecordingMode::Normal};
    std::atomic<bool> currentlyRecording_{false};
    
    // MIDI settings (for MIDI tracks)
    std::atomic<int32_t> midiChannel_{1};
    std::atomic<int32_t> midiProgram_{0};
    std::atomic<int32_t> midiBank_{0};
    std::atomic<bool> midiThru_{true};
    
    // Track organization
    std::atomic<bool> isGrouped_{false};
    std::string groupId_;
    std::atomic<bool> isFolderTrack_{false};
    
    // Event callbacks
    std::vector<TrackEventCallback> eventCallbacks_;
    std::mutex callbackMutex_;
    
    // Cached info
    mutable TrackInfo cachedTrackInfo_;
    mutable std::chrono::steady_clock::time_point lastInfoUpdate_;
    static constexpr auto INFO_CACHE_DURATION = std::chrono::milliseconds(100);
    
    // TE callbacks and listeners
    std::unique_ptr<te::Track::Listener> teTrackListener_;
};

} // namespace mixmind::adapters::tracktion