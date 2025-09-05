#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>

// Forward declare JSON type to avoid including nlohmann/json in header
namespace nlohmann {
    class json;
}

namespace mixmind::session {

// Session data structures
struct TrackData {
    std::string id;
    std::string name;
    std::string type; // "audio", "midi", "instrument"
    bool muted = false;
    bool solo = false;
    double gain = 0.0; // dB
    double pan = 0.0;  // -1.0 to 1.0
    std::string color = "#808080";
    
    // Plugin chain
    std::vector<std::string> pluginIds;
    
    // Audio clips
    struct ClipData {
        std::string id;
        std::string name;
        double startTime;
        double duration;
        std::string audioFile; // Path to audio file
        double fadeInDuration = 0.0;
        double fadeOutDuration = 0.0;
    };
    std::vector<ClipData> clips;
    
    // MIDI data
    struct MidiData {
        std::string id;
        double startTime;
        double duration;
        std::vector<uint8_t> midiEvents; // Raw MIDI data
    };
    std::vector<MidiData> midiClips;
};

struct PluginData {
    std::string id;
    std::string name;
    std::string pluginType; // "vst3", "au", "builtin"
    std::string pluginPath; // Path to plugin file
    std::string uniqueId;   // Plugin unique identifier
    bool bypassed = false;
    std::unordered_map<std::string, double> parameters;
    std::vector<uint8_t> stateData; // Plugin state blob
};

struct SessionData {
    // Metadata
    std::string version = "1.0";
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;
    std::string author;
    
    // Audio settings
    int sampleRate = 44100;
    int bufferSize = 512;
    
    // Timeline settings
    double tempo = 120.0;
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;
    double length = 0.0; // Session length in seconds
    
    // Transport state
    double currentPosition = 0.0;
    bool isLooping = false;
    double loopStart = 0.0;
    double loopEnd = 8.0;
    
    // Tracks and content
    std::vector<TrackData> tracks;
    std::vector<PluginData> plugins;
    
    // Mixer settings
    double masterVolume = 0.0; // dB
    bool masterMuted = false;
    
    // Cache information for relink
    struct CacheInfo {
        std::string audioFile;
        std::string cacheFile;
        std::chrono::system_clock::time_point lastModified;
        size_t fileSize;
        std::string checksum;
    };
    std::vector<CacheInfo> audioCache;
    
    // User preferences
    std::unordered_map<std::string, std::string> preferences;
    
    // Validation
    bool isValid() const;
    std::vector<std::string> validate() const;
};

// Serialization results
struct SerializationResult {
    bool success;
    std::string errorMessage;
    std::string jsonData;
    size_t dataSize = 0;
    
    static SerializationResult success(const std::string& json) {
        SerializationResult result;
        result.success = true;
        result.jsonData = json;
        result.dataSize = json.size();
        return result;
    }
    
    static SerializationResult error(const std::string& error) {
        SerializationResult result;
        result.success = false;
        result.errorMessage = error;
        return result;
    }
};

struct DeserializationResult {
    bool success;
    std::string errorMessage;
    SessionData sessionData;
    std::vector<std::string> warnings;
    
    static DeserializationResult success(const SessionData& data) {
        DeserializationResult result;
        result.success = true;
        result.sessionData = data;
        return result;
    }
    
    static DeserializationResult error(const std::string& error) {
        DeserializationResult result;
        result.success = false;
        result.errorMessage = error;
        return result;
    }
};

// Main serializer class
class SessionSerializer {
public:
    SessionSerializer();
    ~SessionSerializer();
    
    // JSON Schema v1 serialization
    SerializationResult serialize(const SessionData& session);
    DeserializationResult deserialize(const std::string& jsonData);
    
    // File operations
    bool saveToFile(const SessionData& session, const std::string& filePath);
    DeserializationResult loadFromFile(const std::string& filePath);
    
    // Round-trip testing
    struct RoundTripResult {
        bool success;
        std::string errorMessage;
        SessionData originalData;
        SessionData roundTripData;
        std::vector<std::string> differences;
        
        double fidelity() const {
            if (!success) return 0.0;
            return differences.empty() ? 1.0 : 1.0 - (differences.size() * 0.1);
        }
    };
    
    RoundTripResult testRoundTrip(const SessionData& session);
    
    // Schema validation
    bool validateJsonSchema(const std::string& jsonData);
    std::string getJsonSchema() const;
    
    // Cache management
    struct CacheRelinkResult {
        bool success;
        std::vector<std::string> relinkResults;
        std::vector<std::string> missingFiles;
        std::vector<std::string> relocatedFiles;
    };
    
    CacheRelinkResult relinkMissingFiles(SessionData& session, const std::string& searchPath);
    
    // Version compatibility
    bool isVersionSupported(const std::string& version) const;
    DeserializationResult migrateFromVersion(const std::string& jsonData, const std::string& fromVersion);
    
    // Utility functions
    static std::string generateSessionId();
    static std::string generateTrackId();
    static std::string generatePluginId();
    static std::string calculateChecksum(const std::string& filePath);
    
    // Statistics
    struct SessionStats {
        int trackCount;
        int audioClipCount;
        int midiClipCount;
        int pluginCount;
        double totalDuration;
        size_t totalAudioFiles;
        size_t jsonSize;
        
        std::string summary() const;
    };
    
    SessionStats analyzeSession(const SessionData& session) const;
    
private:
    std::unique_ptr<nlohmann::json> jsonSchema_;
    
    // Internal serialization methods
    nlohmann::json serializeTrack(const TrackData& track);
    nlohmann::json serializePlugin(const PluginData& plugin);
    nlohmann::json serializeClip(const TrackData::ClipData& clip);
    nlohmann::json serializeMidi(const TrackData::MidiData& midi);
    
    // Internal deserialization methods
    TrackData deserializeTrack(const nlohmann::json& json);
    PluginData deserializePlugin(const nlohmann::json& json);
    TrackData::ClipData deserializeClip(const nlohmann::json& json);
    TrackData::MidiData deserializeMidi(const nlohmann::json& json);
    
    // Validation helpers
    bool validateTrackData(const TrackData& track, std::vector<std::string>& errors) const;
    bool validatePluginData(const PluginData& plugin, std::vector<std::string>& errors) const;
    
    // Utility helpers
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& str) const;
    
    // File path helpers
    std::string makePathRelative(const std::string& fullPath, const std::string& basePath) const;
    std::string makePathAbsolute(const std::string& relativePath, const std::string& basePath) const;
    
    // Error accumulation
    void addError(std::vector<std::string>& errors, const std::string& context, const std::string& error) const;
    void addWarning(std::vector<std::string>& warnings, const std::string& warning) const;
};

// Convenience functions
namespace session_io {
    
    // Quick save/load
    bool saveSession(const SessionData& session, const std::string& filePath);
    DeserializationResult loadSession(const std::string& filePath);
    
    // Session templates
    SessionData createEmptySession(const std::string& name = "New Session");
    SessionData createTemplateSession(const std::string& templateType); // "basic", "recording", "mixing"
    
    // Backup and recovery
    bool createBackup(const std::string& sessionFile, const std::string& backupDir);
    std::vector<std::string> findBackups(const std::string& sessionFile, const std::string& backupDir);
    
    // Session comparison
    std::vector<std::string> compareeSessions(const SessionData& session1, const SessionData& session2);
    
    // Export formats
    bool exportToMIDI(const SessionData& session, const std::string& midiFilePath);
    bool exportToAudio(const SessionData& session, const std::string& audioFilePath); // Bounce/render
}

} // namespace mixmind::session