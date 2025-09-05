#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>
#include <memory>

namespace mixmind::core {

// ============================================================================
// Basic Types
// ============================================================================

using SampleType = float;
using SampleRate = int32_t;
using BufferSize = int32_t;
using TimestampSamples = int64_t;
using TimestampSeconds = double;

// ============================================================================
// ID Types (Strong typing for different ID categories)
// ============================================================================

template<typename Tag>
struct StrongID {
    using ValueType = uint64_t;
    ValueType value;
    
    StrongID() : value(0) {}
    explicit StrongID(ValueType v) : value(v) {}
    
    bool operator==(const StrongID& other) const { return value == other.value; }
    bool operator!=(const StrongID& other) const { return value != other.value; }
    bool operator<(const StrongID& other) const { return value < other.value; }
    
    bool isValid() const { return value != 0; }
    explicit operator bool() const { return isValid(); }
};

// ID type tags
struct SessionTag {};
struct TrackTag {};
struct ClipTag {};
struct PluginInstanceTag {};
struct AutomationLaneTag {};
struct RenderJobTag {};

// Concrete ID types
using SessionID = StrongID<SessionTag>;
using TrackID = StrongID<TrackTag>;
using ClipID = StrongID<ClipTag>;
using PluginInstanceID = StrongID<PluginInstanceTag>;
using AutomationLaneID = StrongID<AutomationLaneTag>;
using RenderJobID = StrongID<RenderJobTag>;

// Plugin identification
struct PluginID {
    std::string manufacturer;
    std::string name;
    std::string version;
    std::string uniqueID;  // VST3 UID, AU type/subtype, etc.
    
    bool operator==(const PluginID& other) const {
        return uniqueID == other.uniqueID;
    }
    
    std::string toString() const {
        return manufacturer + "::" + name + "::" + version + "::" + uniqueID;
    }
};

// Parameter identification  
using ParamID = std::string;

// ============================================================================
// Audio Configuration Types
// ============================================================================

struct AudioConfig {
    SampleRate sampleRate = 44100;
    BufferSize bufferSize = 512;
    int32_t inputChannels = 2;
    int32_t outputChannels = 2;
    
    bool operator==(const AudioConfig& other) const {
        return sampleRate == other.sampleRate &&
               bufferSize == other.bufferSize &&
               inputChannels == other.inputChannels &&
               outputChannels == other.outputChannels;
    }
};

// ============================================================================
// Time and Tempo Types
// ============================================================================

struct TimeSignature {
    int32_t numerator = 4;
    int32_t denominator = 4;
    
    bool operator==(const TimeSignature& other) const {
        return numerator == other.numerator && denominator == other.denominator;
    }
};

struct TempoPoint {
    TimestampSamples position;
    double beatsPerMinute;
    
    bool operator<(const TempoPoint& other) const {
        return position < other.position;
    }
};

using TempoMap = std::vector<TempoPoint>;

// ============================================================================
// Audio Buffer Types
// ============================================================================

template<typename T>
class AudioBuffer {
public:
    AudioBuffer() = default;
    AudioBuffer(int32_t numChannels, int32_t numSamples)
        : channels_(numChannels), samples_(numSamples) {
        data_.resize(channels_ * samples_);
    }
    
    void resize(int32_t numChannels, int32_t numSamples) {
        channels_ = numChannels;
        samples_ = numSamples;
        data_.resize(channels_ * samples_);
    }
    
    T* getChannelData(int32_t channel) {
        return data_.data() + (channel * samples_);
    }
    
    const T* getChannelData(int32_t channel) const {
        return data_.data() + (channel * samples_);
    }
    
    int32_t getNumChannels() const { return channels_; }
    int32_t getNumSamples() const { return samples_; }
    
    void clear() {
        std::fill(data_.begin(), data_.end(), T{0});
    }
    
    // Sample access methods for audio generators
    void setSample(int32_t sampleIndex, int32_t channel, T value) {
        if (channel >= 0 && channel < channels_ && sampleIndex >= 0 && sampleIndex < samples_) {
            data_[sampleIndex * channels_ + channel] = value;
        }
    }
    
    T getSample(int32_t sampleIndex, int32_t channel) const {
        if (channel >= 0 && channel < channels_ && sampleIndex >= 0 && sampleIndex < samples_) {
            return data_[sampleIndex * channels_ + channel];
        }
        return T{0};
    }
    
    void addSample(int32_t sampleIndex, int32_t channel, T value) {
        if (channel >= 0 && channel < channels_ && sampleIndex >= 0 && sampleIndex < samples_) {
            data_[sampleIndex * channels_ + channel] += value;
        }
    }
    
    // Resize method that the generators expect
    void resize(size_t totalSamples) {
        if (channels_ > 0) {
            samples_ = static_cast<int32_t>(totalSamples / channels_);
            data_.resize(totalSamples);
        }
    }
    
    // Get raw data pointer
    T* getData() { return data_.data(); }
    const T* getData() const { return data_.data(); }
    
    // Get total size
    size_t size() const { return data_.size(); }
    
private:
    std::vector<T> data_;
    int32_t channels_ = 0;
    int32_t samples_ = 0;
};

using FloatAudioBuffer = AudioBuffer<float>;
using DoubleAudioBuffer = AudioBuffer<double>;

// ============================================================================
// MIDI Types
// ============================================================================

enum class MidiMessageType : uint8_t {
    NoteOff = 0x80,
    NoteOn = 0x90, 
    PolyAftertouch = 0xA0,
    ControlChange = 0xB0,
    ProgramChange = 0xC0,
    ChannelAftertouch = 0xD0,
    PitchBend = 0xE0,
    SystemExclusive = 0xF0
};

struct MidiMessage {
    TimestampSamples timestamp;
    uint8_t data[3];
    uint8_t size;
    
    MidiMessage(TimestampSamples ts, uint8_t byte1, uint8_t byte2 = 0, uint8_t byte3 = 0)
        : timestamp(ts) {
        data[0] = byte1;
        data[1] = byte2;
        data[2] = byte3;
        
        // Determine message size based on status byte
        if (byte1 >= 0xF0) {
            size = 1; // System messages vary, but we'll handle them specially
        } else if (byte1 >= 0xC0 && byte1 <= 0xDF) {
            size = 2; // Program change and channel aftertouch
        } else {
            size = 3; // Most other messages
        }
    }
    
    MidiMessageType getType() const {
        return static_cast<MidiMessageType>(data[0] & 0xF0);
    }
    
    uint8_t getChannel() const {
        return data[0] & 0x0F;
    }
};

using MidiBuffer = std::vector<MidiMessage>;

// ============================================================================
// File and Media Types
// ============================================================================

enum class AudioFileFormat {
    Unknown,
    WAV,
    AIFF,
    FLAC,
    MP3,
    AAC,
    OGG
};

enum class VideoFileFormat {
    Unknown,
    MOV,
    MP4,
    AVI,
    MXF
};

struct MediaFileInfo {
    std::string filePath;
    AudioFileFormat audioFormat = AudioFileFormat::Unknown;
    VideoFileFormat videoFormat = VideoFileFormat::Unknown;
    
    // Audio properties
    SampleRate sampleRate = 0;
    int32_t channels = 0;
    int32_t bitDepth = 0;
    TimestampSamples lengthSamples = 0;
    
    // Video properties (if applicable)
    int32_t frameRate = 0;
    int32_t width = 0;
    int32_t height = 0;
    TimestampSamples lengthFrames = 0;
    
    // Metadata
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    int32_t year = 0;
    
    bool isValid() const {
        return !filePath.empty() && (audioFormat != AudioFileFormat::Unknown || 
                                     videoFormat != VideoFileFormat::Unknown);
    }
};

// ============================================================================
// Plugin Types  
// ============================================================================

enum class PluginType {
    Unknown,
    VST2,
    VST3, 
    AudioUnit,
    AAX,
    LV2,
    Built_in
};

enum class PluginCategory {
    Unknown,
    Synthesizer,
    Drum,
    Sampler,
    Effect,
    Analyzer,
    Compressor,
    Reverb,
    Delay,
    Filter,
    Distortion,
    Modulation,
    Utility
};

struct PluginInfo {
    PluginID id;
    PluginType type = PluginType::Unknown;
    PluginCategory category = PluginCategory::Unknown;
    std::string displayName;
    std::string description;
    std::string filePath;
    bool hasEditor = false;
    bool isSynth = false;
    int32_t numInputs = 0;
    int32_t numOutputs = 0;
    
    bool isValid() const {
        return !id.uniqueID.empty() && type != PluginType::Unknown;
    }
};

// ============================================================================
// Automation Types
// ============================================================================

enum class AutomationCurveType {
    Linear,
    Exponential,  
    Logarithmic,
    SCurve,
    Hold
};

struct AutomationPoint {
    TimestampSamples position;
    float value;
    AutomationCurveType curveType = AutomationCurveType::Linear;
    
    bool operator<(const AutomationPoint& other) const {
        return position < other.position;
    }
};

using AutomationCurve = std::vector<AutomationPoint>;

// ============================================================================
// Transport and Timing Types
// ============================================================================

enum class TransportState {
    Stopped,
    Playing,
    Recording,
    Paused
};

enum class LoopMode {
    Off,
    Loop,
    PingPong
};

struct TransportInfo {
    TransportState state = TransportState::Stopped;
    TimestampSamples position = 0;
    TimestampSamples loopStart = 0;
    TimestampSamples loopEnd = 0;
    LoopMode loopMode = LoopMode::Off;
    bool recording = false;
    bool metronomeEnabled = false;
    bool preRollEnabled = false;
    TimestampSamples preRollLength = 0;
};

// ============================================================================
// Session Types
// ============================================================================

struct TrackConfig {
    std::string name;
    bool isAudioTrack = true;  // false = MIDI track
    int32_t numChannels = 2;
    std::string inputSource;
    bool recordArmed = false;
    bool monitored = false;
};

struct ClipConfig {
    std::string name;
    TrackID trackId;
    TimestampSamples startPosition;
    TimestampSamples length;
    std::string sourceFile;  // For audio clips
    MidiBuffer midiData;     // For MIDI clips
};

struct ImportConfig {
    std::vector<std::string> filePaths;
    TrackID targetTrackId;  // If invalid, create new tracks
    TimestampSamples insertPosition = 0;
    bool createNewTracks = true;
};

struct PluginConfig {
    PluginID pluginId;
    TrackID trackId;
    int32_t slotIndex = -1;  // -1 = append to end
    std::string presetPath;
};

// ============================================================================
// Render Types
// ============================================================================

enum class RenderFormat {
    WAV,
    AIFF, 
    FLAC,
    MP3,
    AAC
};

enum class RenderQuality {
    Draft,      // Fast, lower quality
    Standard,   // Balanced
    High,       // High quality, slower
    Archival    // Highest quality, slowest
};

struct RenderSettings {
    RenderFormat format = RenderFormat::WAV;
    RenderQuality quality = RenderQuality::Standard;
    SampleRate sampleRate = 44100;
    int32_t bitDepth = 24;  // For PCM formats
    int32_t mp3Bitrate = 320;  // For MP3
    bool normalize = true;
    float normalizeLevel = -0.1f;  // dBFS
    bool dither = true;
    TimestampSamples startPosition = 0;
    TimestampSamples endPosition = -1;  // -1 = end of session
    std::string outputPath;
};

// ============================================================================
// Error and Diagnostic Types
// ============================================================================

enum class Severity {
    Info,
    Warning,
    Error,
    Critical
};

struct DiagnosticMessage {
    Severity severity;
    std::string code;
    std::string message;
    std::string context;
    std::chrono::system_clock::time_point timestamp;
    
    DiagnosticMessage(Severity sev, std::string code, std::string msg, std::string ctx = "")
        : severity(sev), code(std::move(code)), message(std::move(msg)), 
          context(std::move(ctx)), timestamp(std::chrono::system_clock::now()) {}
};

// ============================================================================
// Utility Functions
// ============================================================================

namespace utils {

inline TimestampSamples secondsToSamples(TimestampSeconds seconds, SampleRate sampleRate) {
    return static_cast<TimestampSamples>(seconds * sampleRate);
}

inline TimestampSeconds samplesToSeconds(TimestampSamples samples, SampleRate sampleRate) {
    return static_cast<TimestampSeconds>(samples) / sampleRate;
}

inline std::string formatTime(TimestampSamples samples, SampleRate sampleRate) {
    auto totalSeconds = samplesToSeconds(samples, sampleRate);
    auto minutes = static_cast<int>(totalSeconds / 60);
    auto seconds = static_cast<int>(totalSeconds) % 60;
    auto milliseconds = static_cast<int>((totalSeconds - static_cast<int>(totalSeconds)) * 1000);
    
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d.%03d", minutes, seconds, milliseconds);
    return std::string(buffer);
}

} // namespace utils

} // namespace mixmind::core