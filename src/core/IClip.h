#pragma once

#include "types.h"
#include "result.h"
#include <memory>
#include <vector>
#include <optional>

namespace mixmind::core {

// ============================================================================
// Clip Interface - Individual clip management (audio and MIDI)
// ============================================================================

class IClip {
public:
    virtual ~IClip() = default;
    
    // ========================================================================
    // Clip Identity and Type
    // ========================================================================
    
    /// Get clip ID
    virtual ClipID getId() const = 0;
    
    /// Get clip name
    virtual std::string getName() const = 0;
    
    /// Set clip name
    virtual VoidResult setName(const std::string& name) = 0;
    
    /// Check if this is an audio clip
    virtual bool isAudioClip() const = 0;
    
    /// Check if this is a MIDI clip
    virtual bool isMIDIClip() const = 0;
    
    /// Get parent track ID
    virtual TrackID getTrackId() const = 0;
    
    // ========================================================================
    // Clip Position and Timing
    // ========================================================================
    
    /// Get clip start position on timeline
    virtual TimestampSamples getStartPosition() const = 0;
    
    /// Set clip start position
    virtual AsyncResult<VoidResult> setStartPosition(TimestampSamples position) = 0;
    
    /// Get clip length
    virtual TimestampSamples getLength() const = 0;
    
    /// Set clip length (may truncate or extend content)
    virtual AsyncResult<VoidResult> setLength(TimestampSamples length) = 0;
    
    /// Get clip end position (start + length)
    virtual TimestampSamples getEndPosition() const = 0;
    
    /// Move clip to new position
    virtual AsyncResult<VoidResult> moveTo(TimestampSamples newPosition) = 0;
    
    /// Resize clip from start (changes both position and length)
    virtual AsyncResult<VoidResult> resizeFromStart(TimestampSamples newStart) = 0;
    
    /// Resize clip from end (changes length only)
    virtual AsyncResult<VoidResult> resizeFromEnd(TimestampSamples newEnd) = 0;
    
    // ========================================================================
    // Clip Content Timing (Source Material)
    // ========================================================================
    
    /// Get source start offset (how much of source material to skip)
    virtual TimestampSamples getSourceOffset() const = 0;
    
    /// Set source start offset
    virtual AsyncResult<VoidResult> setSourceOffset(TimestampSamples offset) = 0;
    
    /// Get source length (total length of source material)
    virtual TimestampSamples getSourceLength() const = 0;
    
    /// Get playback speed/rate (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
    virtual float getPlaybackRate() const = 0;
    
    /// Set playback speed/rate
    virtual AsyncResult<VoidResult> setPlaybackRate(float rate) = 0;
    
    /// Check if pitch correction is enabled during rate changes
    virtual bool isPitchCorrectionEnabled() const = 0;
    
    /// Enable/disable pitch correction
    virtual AsyncResult<VoidResult> setPitchCorrectionEnabled(bool enabled) = 0;
    
    // ========================================================================
    // Audio Clip Specific Features
    // ========================================================================
    
    /// Get source audio file path (for audio clips)
    virtual std::string getSourceFilePath() const = 0;
    
    /// Set source audio file
    virtual AsyncResult<VoidResult> setSourceFile(const std::string& filePath) = 0;
    
    /// Get audio file format information
    virtual std::optional<MediaFileInfo> getSourceFileInfo() const = 0;
    
    /// Get number of audio channels
    virtual int32_t getChannelCount() const = 0;
    
    /// Get sample rate of source material
    virtual SampleRate getSourceSampleRate() const = 0;
    
    /// Get bit depth of source material
    virtual int32_t getSourceBitDepth() const = 0;
    
    /// Reverse audio clip playback
    virtual AsyncResult<VoidResult> setReversed(bool reversed) = 0;
    
    /// Check if clip is reversed
    virtual bool isReversed() const = 0;
    
    /// Get current peak levels for visualization
    virtual std::vector<float> getPeakLevels(int32_t numSamples = 1000) const = 0;
    
    /// Get waveform data for visualization
    struct WaveformData {
        std::vector<float> minValues;
        std::vector<float> maxValues; 
        int32_t samplesPerPixel;
        TimestampSamples startSample;
        TimestampSamples endSample;
    };
    
    virtual AsyncResult<Result<WaveformData>> getWaveformData(int32_t pixelWidth, 
                                                              TimestampSamples startSample = 0,
                                                              TimestampSamples endSample = -1) = 0;
    
    // ========================================================================
    // MIDI Clip Specific Features  
    // ========================================================================
    
    /// Get MIDI data (for MIDI clips)
    virtual MidiBuffer getMIDIData() const = 0;
    
    /// Set MIDI data
    virtual AsyncResult<VoidResult> setMIDIData(const MidiBuffer& midiData) = 0;
    
    /// Add MIDI event
    virtual AsyncResult<VoidResult> addMIDIEvent(const MidiMessage& message) = 0;
    
    /// Remove MIDI event
    virtual AsyncResult<VoidResult> removeMIDIEvent(int32_t eventIndex) = 0;
    
    /// Get MIDI events in time range
    virtual MidiBuffer getMIDIEventsInRange(TimestampSamples start, TimestampSamples end) const = 0;
    
    /// Clear all MIDI events
    virtual AsyncResult<VoidResult> clearMIDIEvents() = 0;
    
    /// Quantize MIDI events
    enum class QuantizeGrid {
        Quarter = 1,
        Eighth = 2,
        Sixteenth = 4,
        ThirtySecond = 8,
        Triplet = 3
    };
    
    virtual AsyncResult<VoidResult> quantizeMIDI(QuantizeGrid grid, float strength = 1.0f) = 0;
    
    /// Transpose MIDI events
    virtual AsyncResult<VoidResult> transposeMIDI(int32_t semitones) = 0;
    
    /// Get MIDI note range (lowest and highest notes)
    virtual std::pair<int32_t, int32_t> getMIDINoteRange() const = 0;
    
    // ========================================================================
    // Clip Volume and Gain
    // ========================================================================
    
    /// Get clip gain/volume (multiplier, 1.0 = unity)
    virtual float getGain() const = 0;
    
    /// Set clip gain/volume
    virtual AsyncResult<VoidResult> setGain(float gain) = 0;
    
    /// Get clip gain in dB
    virtual float getGainDB() const = 0;
    
    /// Set clip gain in dB
    virtual AsyncResult<VoidResult> setGainDB(float gainDB) = 0;
    
    /// Get clip pan (for stereo clips)
    virtual float getPan() const = 0;
    
    /// Set clip pan
    virtual AsyncResult<VoidResult> setPan(float pan) = 0;
    
    /// Check if clip is muted
    virtual bool isMuted() const = 0;
    
    /// Set clip mute state
    virtual AsyncResult<VoidResult> setMuted(bool muted) = 0;
    
    // ========================================================================
    // Clip Fades and Crossfades
    // ========================================================================
    
    enum class FadeType {
        Linear,
        Exponential,
        Logarithmic,
        SCurve,
        Equal_Power  // For crossfades
    };
    
    /// Set fade-in
    virtual AsyncResult<VoidResult> setFadeIn(TimestampSamples length, FadeType type = FadeType::Equal_Power) = 0;
    
    /// Get fade-in length
    virtual TimestampSamples getFadeInLength() const = 0;
    
    /// Get fade-in type
    virtual FadeType getFadeInType() const = 0;
    
    /// Set fade-out
    virtual AsyncResult<VoidResult> setFadeOut(TimestampSamples length, FadeType type = FadeType::Equal_Power) = 0;
    
    /// Get fade-out length
    virtual TimestampSamples getFadeOutLength() const = 0;
    
    /// Get fade-out type
    virtual FadeType getFadeOutType() const = 0;
    
    /// Remove fades
    virtual AsyncResult<VoidResult> clearFades() = 0;
    
    // ========================================================================
    // Clip Looping
    // ========================================================================
    
    /// Enable/disable clip looping
    virtual AsyncResult<VoidResult> setLooped(bool looped) = 0;
    
    /// Check if clip is looped
    virtual bool isLooped() const = 0;
    
    /// Set loop length (if different from source length)
    virtual AsyncResult<VoidResult> setLoopLength(TimestampSamples length) = 0;
    
    /// Get loop length
    virtual TimestampSamples getLoopLength() const = 0;
    
    /// Set loop start point within source material
    virtual AsyncResult<VoidResult> setLoopStart(TimestampSamples start) = 0;
    
    /// Get loop start point
    virtual TimestampSamples getLoopStart() const = 0;
    
    // ========================================================================
    // Clip Processing and Effects
    // ========================================================================
    
    /// Check if clip has built-in processing enabled
    virtual bool hasProcessingEnabled() const = 0;
    
    /// Enable/disable built-in processing
    virtual AsyncResult<VoidResult> setProcessingEnabled(bool enabled) = 0;
    
    /// Normalize clip audio
    virtual AsyncResult<VoidResult> normalize(float targetLevel = -3.0f) = 0;  // dBFS
    
    /// Reverse clip content
    virtual AsyncResult<VoidResult> reverse() = 0;
    
    /// Apply time stretch to clip
    virtual AsyncResult<VoidResult> timeStretch(float ratio, bool preservePitch = true) = 0;
    
    /// Apply pitch shift to clip
    virtual AsyncResult<VoidResult> pitchShift(float semitones, bool preserveTiming = true) = 0;
    
    // ========================================================================
    // Clip Comping and Takes
    // ========================================================================
    
    /// Check if this clip is part of a comp
    virtual bool isPartOfComp() const = 0;
    
    /// Get comp group ID
    virtual std::optional<std::string> getCompGroupId() const = 0;
    
    /// Get take number within comp
    virtual int32_t getTakeNumber() const = 0;
    
    /// Check if this is the active take
    virtual bool isActiveTake() const = 0;
    
    /// Set as active take
    virtual AsyncResult<VoidResult> setAsActiveTake() = 0;
    
    /// Get all takes in comp group
    virtual std::vector<ClipID> getAllTakes() const = 0;
    
    /// Create new take from this clip
    virtual AsyncResult<Result<ClipID>> createTake() = 0;
    
    /// Delete this take
    virtual AsyncResult<VoidResult> deleteTake() = 0;
    
    // ========================================================================
    // Clip Color and Appearance
    // ========================================================================
    
    /// Set clip color
    virtual VoidResult setColor(const std::string& color) = 0;
    
    /// Get clip color
    virtual std::string getColor() const = 0;
    
    /// Set clip comment/description
    virtual VoidResult setComment(const std::string& comment) = 0;
    
    /// Get clip comment
    virtual std::string getComment() const = 0;
    
    // ========================================================================
    // Clip Markers and Regions
    // ========================================================================
    
    struct ClipMarker {
        std::string name;
        TimestampSamples position;  // Relative to clip start
        std::string color;
    };
    
    /// Add marker within clip
    virtual VoidResult addMarker(const ClipMarker& marker) = 0;
    
    /// Remove marker
    virtual VoidResult removeMarker(const std::string& name) = 0;
    
    /// Get all clip markers
    virtual std::vector<ClipMarker> getMarkers() const = 0;
    
    /// Find marker at position
    virtual std::optional<ClipMarker> getMarkerAtPosition(TimestampSamples position) const = 0;
    
    // ========================================================================
    // Clip Analysis and Detection
    // ========================================================================
    
    /// Detect tempo of audio clip
    virtual AsyncResult<Result<double>> detectTempo() = 0;
    
    /// Detect beats in audio clip
    virtual AsyncResult<Result<std::vector<TimestampSamples>>> detectBeats() = 0;
    
    /// Detect key/pitch of audio clip
    virtual AsyncResult<Result<std::string>> detectKey() = 0;
    
    /// Get audio analysis data
    struct AudioAnalysis {
        double detectedTempo = 0.0;
        std::string detectedKey;
        std::vector<TimestampSamples> beatPositions;
        std::vector<TimestampSamples> onsetPositions;
        float averageLevel = 0.0f;  // RMS
        float peakLevel = 0.0f;     // Peak
        double duration = 0.0;      // Seconds
        bool hasClipping = false;
    };
    
    /// Get comprehensive audio analysis
    virtual AsyncResult<Result<AudioAnalysis>> analyzeAudio() = 0;
    
    // ========================================================================
    // Clip Export and Rendering
    // ========================================================================
    
    /// Export clip to audio file
    virtual AsyncResult<VoidResult> exportToFile(const std::string& filePath, 
                                                 const RenderSettings& settings) = 0;
    
    /// Render clip with current processing to new audio file
    virtual AsyncResult<Result<std::string>> renderToFile(const RenderSettings& settings) = 0;
    
    /// Get rendered audio data
    virtual AsyncResult<Result<FloatAudioBuffer>> renderToBuffer() = 0;
    
    // ========================================================================
    // Clip Automation
    // ========================================================================
    
    /// Check if clip has automation
    virtual bool hasAutomation() const = 0;
    
    /// Get automation for clip parameter
    virtual std::optional<AutomationCurve> getAutomation(const ParamID& paramId) const = 0;
    
    /// Set automation for clip parameter
    virtual AsyncResult<VoidResult> setAutomation(const ParamID& paramId, const AutomationCurve& curve) = 0;
    
    /// Clear automation for parameter
    virtual AsyncResult<VoidResult> clearAutomation(const ParamID& paramId) = 0;
    
    /// Get all automated parameters
    virtual std::vector<ParamID> getAutomatedParameters() const = 0;
    
    // ========================================================================
    // Clip Validation
    // ========================================================================
    
    /// Check if clip is valid (source file exists, no corruption, etc.)
    virtual bool isValid() const = 0;
    
    /// Get validation issues
    virtual std::vector<std::string> getValidationIssues() const = 0;
    
    /// Attempt to repair clip issues
    virtual AsyncResult<VoidResult> repair() = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class ClipEvent {
        PositionChanged,
        LengthChanged,
        ContentChanged,
        GainChanged,
        MuteChanged,
        ColorChanged,
        FadeChanged,
        LoopChanged,
        ProcessingChanged,
        TakeChanged,
        MarkerAdded,
        MarkerRemoved,
        AutomationChanged
    };
    
    using ClipEventCallback = std::function<void(ClipEvent event, const std::string& details)>;
    
    /// Subscribe to clip events
    virtual void addEventListener(ClipEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(ClipEventCallback callback) = 0;
    
    // ========================================================================
    // Clip Statistics
    // ========================================================================
    
    struct ClipStats {
        ClipID id;
        std::string name;
        bool isAudio;
        TimestampSamples startPosition;
        TimestampSamples length;
        TimestampSamples sourceLength;
        float gain;
        bool muted;
        bool looped;
        bool reversed;
        int32_t takeNumber;
        std::string sourceFile;
        size_t memoryUsage;  // bytes
    };
    
    /// Get comprehensive clip statistics
    virtual ClipStats getClipStats() const = 0;
    
    // ========================================================================
    // Advanced Editing
    // ========================================================================
    
    /// Split clip at position (relative to clip start)
    virtual AsyncResult<Result<ClipID>> splitAt(TimestampSamples position) = 0;
    
    /// Trim silence from beginning and end
    virtual AsyncResult<VoidResult> trimSilence(float thresholdDB = -60.0f) = 0;
    
    /// Detect and split at silence gaps
    virtual AsyncResult<Result<std::vector<ClipID>>> splitAtSilence(float thresholdDB = -60.0f, 
                                                                    TimestampSamples minGapLength = 1000) = 0;
    
    /// Create copy of clip
    virtual AsyncResult<Result<ClipID>> duplicate() = 0;
    
    /// Create linked copy (shares source but has independent settings)
    virtual AsyncResult<Result<ClipID>> createLinkedCopy() = 0;
    
    /// Check if this is a linked copy
    virtual bool isLinkedCopy() const = 0;
    
    /// Get original clip if this is a linked copy
    virtual std::optional<ClipID> getOriginalClip() const = 0;
    
    /// Get all linked copies of this clip
    virtual std::vector<ClipID> getLinkedCopies() const = 0;
};

// ============================================================================
// Clip Factory Interface
// ============================================================================

class IClipFactory {
public:
    virtual ~IClipFactory() = default;
    
    /// Create audio clip from file
    virtual AsyncResult<Result<std::shared_ptr<IClip>>> createAudioClip(
        TrackID trackId, 
        const std::string& filePath,
        TimestampSamples position,
        TimestampSamples length = -1  // -1 = use full file
    ) = 0;
    
    /// Create MIDI clip
    virtual AsyncResult<Result<std::shared_ptr<IClip>>> createMIDIClip(
        TrackID trackId,
        TimestampSamples position,
        TimestampSamples length,
        const MidiBuffer& initialData = {}
    ) = 0;
    
    /// Create empty clip
    virtual AsyncResult<Result<std::shared_ptr<IClip>>> createEmptyClip(
        TrackID trackId,
        bool isAudio,
        TimestampSamples position,
        TimestampSamples length
    ) = 0;
};

} // namespace mixmind::core