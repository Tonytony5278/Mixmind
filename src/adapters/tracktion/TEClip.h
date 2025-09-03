#pragma once

#include "TEAdapter.h"
#include "../../core/IClip.h"
#include "../../core/types.h"
#include "../../core/result.h"
#include <tracktion_engine/tracktion_engine.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TE Clip Adapter - Tracktion Engine implementation of IClip
// ============================================================================

class TEClip : public TEAdapter, public core::IClip {
public:
    explicit TEClip(te::Engine& engine);
    ~TEClip() override = default;
    
    // Non-copyable, movable
    TEClip(const TEClip&) = delete;
    TEClip& operator=(const TEClip&) = delete;
    TEClip(TEClip&&) = default;
    TEClip& operator=(TEClip&&) = default;
    
    // ========================================================================
    // IClip Implementation
    // ========================================================================
    
    // Clip Creation and Management
    core::AsyncResult<core::Result<core::ClipID>> createAudioClip(
        core::TrackID trackId,
        const std::string& filePath,
        core::TimePosition startPosition,
        std::optional<core::TimeDuration> length = std::nullopt
    ) override;
    
    core::AsyncResult<core::Result<core::ClipID>> createMIDIClip(
        core::TrackID trackId,
        core::TimePosition startPosition,
        core::TimeDuration length
    ) override;
    
    core::AsyncResult<core::VoidResult> deleteClip(core::ClipID clipId) override;
    
    core::AsyncResult<core::Result<core::ClipID>> duplicateClip(
        core::ClipID clipId,
        std::optional<core::TrackID> targetTrackId = std::nullopt,
        std::optional<core::TimePosition> targetPosition = std::nullopt
    ) override;
    
    // Clip Information
    core::AsyncResult<core::Result<ClipInfo>> getClip(core::ClipID clipId) const override;
    core::AsyncResult<core::Result<std::vector<ClipInfo>>> getAllClips() const override;
    core::AsyncResult<core::Result<std::vector<ClipInfo>>> getClipsOnTrack(core::TrackID trackId) const override;
    core::AsyncResult<core::Result<std::vector<ClipInfo>>> getClipsInTimeRange(
        core::TimePosition startTime,
        core::TimePosition endTime
    ) const override;
    
    // Clip Properties
    core::AsyncResult<core::VoidResult> setClipName(core::ClipID clipId, const std::string& name) override;
    core::AsyncResult<core::VoidResult> setClipColor(core::ClipID clipId, core::ColorRGBA color) override;
    core::AsyncResult<core::VoidResult> setClipGain(core::ClipID clipId, float gainDB) override;
    core::AsyncResult<core::VoidResult> setClipPan(core::ClipID clipId, float pan) override;
    core::AsyncResult<core::VoidResult> setClipMuted(core::ClipID clipId, bool muted) override;
    
    // Clip Timing
    core::AsyncResult<core::VoidResult> moveClip(
        core::ClipID clipId,
        core::TimePosition newStartPosition,
        std::optional<core::TrackID> newTrackId = std::nullopt
    ) override;
    
    core::AsyncResult<core::VoidResult> resizeClip(
        core::ClipID clipId,
        core::TimeDuration newLength,
        ResizeMode mode = ResizeMode::End
    ) override;
    
    core::AsyncResult<core::VoidResult> trimClip(
        core::ClipID clipId,
        core::TimePosition trimStart,
        core::TimePosition trimEnd
    ) override;
    
    core::AsyncResult<core::Result<std::vector<core::ClipID>>> splitClip(
        core::ClipID clipId,
        const std::vector<core::TimePosition>& splitPositions
    ) override;
    
    // Audio Clip Specific Operations
    core::AsyncResult<core::VoidResult> setAudioClipStartOffset(
        core::ClipID clipId,
        core::TimeDuration offset
    ) override;
    
    core::AsyncResult<core::VoidResult> setAudioClipTimeStretch(
        core::ClipID clipId,
        float ratio,
        bool preservePitch = true
    ) override;
    
    core::AsyncResult<core::VoidResult> setAudioClipPitchShift(
        core::ClipID clipId,
        float semitones
    ) override;
    
    core::AsyncResult<core::VoidResult> setAudioClipReverse(
        core::ClipID clipId,
        bool reversed
    ) override;
    
    core::AsyncResult<core::VoidResult> setAudioClipFadeIn(
        core::ClipID clipId,
        core::TimeDuration fadeTime,
        FadeType fadeType = FadeType::Linear
    ) override;
    
    core::AsyncResult<core::VoidResult> setAudioClipFadeOut(
        core::ClipID clipId,
        core::TimeDuration fadeTime,
        FadeType fadeType = FadeType::Linear
    ) override;
    
    // MIDI Clip Specific Operations
    core::AsyncResult<core::VoidResult> addMIDINote(
        core::ClipID clipId,
        const MIDINote& note
    ) override;
    
    core::AsyncResult<core::VoidResult> removeMIDINote(
        core::ClipID clipId,
        core::MIDINoteID noteId
    ) override;
    
    core::AsyncResult<core::VoidResult> updateMIDINote(
        core::ClipID clipId,
        core::MIDINoteID noteId,
        const MIDINote& updatedNote
    ) override;
    
    core::AsyncResult<core::Result<std::vector<MIDINote>>> getMIDINotes(
        core::ClipID clipId,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) const override;
    
    core::AsyncResult<core::VoidResult> clearMIDINotes(
        core::ClipID clipId,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) override;
    
    // MIDI Controllers and Automation
    core::AsyncResult<core::VoidResult> addMIDIController(
        core::ClipID clipId,
        const MIDIController& controller
    ) override;
    
    core::AsyncResult<core::VoidResult> removeMIDIController(
        core::ClipID clipId,
        core::MIDIControllerID controllerId
    ) override;
    
    core::AsyncResult<core::Result<std::vector<MIDIController>>> getMIDIControllers(
        core::ClipID clipId,
        std::optional<int32_t> controllerNumber = std::nullopt
    ) const override;
    
    // Clip Effects and Processing
    core::AsyncResult<core::VoidResult> addClipEffect(
        core::ClipID clipId,
        const std::string& effectName,
        const std::unordered_map<std::string, float>& parameters = {}
    ) override;
    
    core::AsyncResult<core::VoidResult> removeClipEffect(
        core::ClipID clipId,
        core::ClipEffectID effectId
    ) override;
    
    core::AsyncResult<core::Result<std::vector<ClipEffectInfo>>> getClipEffects(
        core::ClipID clipId
    ) const override;
    
    // Clip Rendering and Export
    core::AsyncResult<core::VoidResult> renderClipToFile(
        core::ClipID clipId,
        const std::string& outputPath,
        const AudioFormat& format,
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::Result<core::FloatAudioBuffer>> renderClipToBuffer(
        core::ClipID clipId,
        core::SampleRate sampleRate,
        core::ProgressCallback progress = nullptr
    ) override;
    
    // Bulk Operations
    core::AsyncResult<core::VoidResult> deleteMultipleClips(
        const std::vector<core::ClipID>& clipIds
    ) override;
    
    core::AsyncResult<core::VoidResult> moveMultipleClips(
        const std::vector<core::ClipID>& clipIds,
        core::TimeDuration timeOffset,
        std::optional<core::TrackID> targetTrackId = std::nullopt
    ) override;
    
    core::AsyncResult<core::VoidResult> setMultipleClipsProperty(
        const std::vector<core::ClipID>& clipIds,
        const std::string& propertyName,
        const std::any& value
    ) override;
    
    // Quantization and Timing
    core::AsyncResult<core::VoidResult> quantizeClip(
        core::ClipID clipId,
        QuantizeSettings settings
    ) override;
    
    core::AsyncResult<core::VoidResult> setClipGroove(
        core::ClipID clipId,
        const std::string& grooveTemplate,
        float strength = 1.0f
    ) override;
    
    // Clip Analysis
    core::AsyncResult<core::Result<AudioAnalysis>> analyzeAudioClip(
        core::ClipID clipId,
        core::ProgressCallback progress = nullptr
    ) const override;
    
    core::AsyncResult<core::Result<MIDIAnalysis>> analyzeMIDIClip(
        core::ClipID clipId
    ) const override;
    
    // Event Callbacks
    void setClipEventCallback(ClipEventCallback callback) override;
    void clearClipEventCallback() override;

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Get Tracktion Engine clip by ID
    te::Clip* getTEClip(core::ClipID clipId) const;
    
    /// Get Tracktion Engine audio clip by ID
    te::WaveAudioClip* getTEAudioClip(core::ClipID clipId) const;
    
    /// Get Tracktion Engine MIDI clip by ID
    te::MidiClip* getTEMIDIClip(core::ClipID clipId) const;
    
    /// Get track from clip ID
    te::AudioTrack* getTrackFromClip(core::ClipID clipId) const;
    
    /// Convert TE clip to our ClipInfo structure
    ClipInfo convertTEClipToInfo(te::Clip* clip) const;
    
    /// Convert our MIDINote to TE MIDI note
    te::MidiNote convertMIDINoteToTE(const MIDINote& note) const;
    
    /// Convert TE MIDI note to our MIDINote
    MIDINote convertTEMIDINoteToCore(const te::MidiNote& teNote) const;
    
    /// Convert fade type to TE fade type
    te::AudioFadeCurve::Type convertFadeTypeToTE(FadeType fadeType) const;
    
    /// Apply quantize settings to clip
    void applyQuantizeSettings(te::Clip* clip, const QuantizeSettings& settings);
    
    /// Get current edit for clip operations
    te::Edit* getCurrentEdit() const;
    
    /// Emit clip event
    void emitClipEvent(ClipEventType eventType, core::ClipID clipId, const std::string& details);
    
    /// Update clip mapping
    void updateClipMapping();
    
    /// Generate unique clip ID
    core::ClipID generateClipID();

private:
    // Clip ID mapping
    std::unordered_map<core::ClipID, te::Clip*> clipMap_;
    std::unordered_map<te::Clip*, core::ClipID> reverseClipMap_;
    mutable std::shared_mutex clipMapMutex_;
    
    // ID generation
    std::atomic<uint32_t> nextClipId_{1};
    
    // Event callback
    ClipEventCallback clipEventCallback_;
    std::mutex callbackMutex_;
    
    // Current edit reference
    mutable te::Edit* currentEdit_ = nullptr;
    mutable std::mutex editMutex_;
    
    // MIDI note ID tracking
    std::unordered_map<core::ClipID, std::atomic<uint32_t>> clipNoteIdCounters_;
    std::unordered_map<core::ClipID, std::unordered_map<core::MIDINoteID, te::MidiNote*>> noteIdMap_;
    mutable std::shared_mutex noteMapMutex_;
    
    // Clip effect ID tracking
    std::unordered_map<core::ClipID, std::atomic<uint32_t>> clipEffectIdCounters_;
    std::unordered_map<core::ClipID, std::unordered_map<core::ClipEffectID, te::Plugin*>> effectIdMap_;
    mutable std::shared_mutex effectMapMutex_;
};

} // namespace mixmind::adapters::tracktion