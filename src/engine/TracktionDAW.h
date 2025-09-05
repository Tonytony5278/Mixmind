#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

// Forward declarations for Tracktion Engine
#ifdef MIXMIND_TRACKTION_ENABLED
namespace tracktion {
    class Engine;
    class Edit;
    class AudioTrack;
    class Track;
    class Plugin;
}
#endif

namespace mixmind::engine {

// ============================================================================
// Smart Tracktion Integration - Using v3.2.0 stable release
// Foundation for professional DAW functionality
// ============================================================================

class TracktionDAW {
public:
    TracktionDAW();
    ~TracktionDAW();
    
    // Non-copyable
    TracktionDAW(const TracktionDAW&) = delete;
    TracktionDAW& operator=(const TracktionDAW&) = delete;
    
    // Core DAW initialization
    mixmind::core::Result<void> initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Project management
    mixmind::core::Result<void> newProject(const std::string& projectName);
    mixmind::core::Result<void> loadProject(const std::string& filePath);
    mixmind::core::Result<void> saveProject(const std::string& filePath);
    mixmind::core::Result<void> saveProjectAs(const std::string& filePath);
    
    // Track management - Tracktion handles all complexity
    std::string addAudioTrack(const std::string& name = "");
    std::string addMidiTrack(const std::string& name = "");
    std::string addFolderTrack(const std::string& name = "");
    std::string addAuxReturn(const std::string& name = "");
    
    mixmind::core::Result<void> removeTrack(const std::string& trackId);
    mixmind::core::Result<void> moveTrack(const std::string& trackId, int newPosition);
    mixmind::core::Result<void> setTrackName(const std::string& trackId, const std::string& name);
    mixmind::core::Result<void> setTrackColor(const std::string& trackId, uint32_t color);
    
    std::vector<std::string> getAllTracks() const;
    std::string getTrackName(const std::string& trackId) const;
    std::string getTrackType(const std::string& trackId) const;
    
    // Plugin management - Tracktion does the heavy lifting
    mixmind::core::Result<std::string> loadPlugin(const std::string& trackId, const std::string& pluginPath);
    mixmind::core::Result<void> unloadPlugin(const std::string& pluginId);
    mixmind::core::Result<void> bypassPlugin(const std::string& pluginId, bool bypass);
    mixmind::core::Result<void> movePlugin(const std::string& pluginId, int newPosition);
    
    std::vector<std::string> getTrackPlugins(const std::string& trackId) const;
    std::string getPluginName(const std::string& pluginId) const;
    
    // Parameter control
    mixmind::core::Result<void> setPluginParameter(const std::string& pluginId, int parameterIndex, float value);
    mixmind::core::Result<void> setPluginParameter(const std::string& pluginId, const std::string& parameterName, float value);
    mixmind::core::Result<float> getPluginParameter(const std::string& pluginId, int parameterIndex);
    mixmind::core::Result<float> getPluginParameter(const std::string& pluginId, const std::string& parameterName);
    
    // Transport control - Tracktion handles timing
    void play();
    void stop();
    void pause();
    void record();
    void rewind();
    void fastForward();
    
    bool isPlaying() const;
    bool isRecording() const;
    
    void setPosition(double seconds);
    double getPosition() const;
    
    void setLooping(bool enabled, double startTime = 0.0, double endTime = 0.0);
    bool isLooping() const;
    
    // Tempo and timing
    void setTempo(double bpm);
    double getTempo() const;
    void setTimeSignature(int numerator, int denominator);
    std::pair<int, int> getTimeSignature() const;
    
    // Audio recording - Tracktion handles all buffering
    mixmind::core::Result<void> armTrackForRecording(const std::string& trackId, bool armed);
    mixmind::core::Result<void> setRecordingInput(const std::string& trackId, const std::string& inputName);
    bool isTrackArmed(const std::string& trackId) const;
    
    // Mixing
    mixmind::core::Result<void> setTrackVolume(const std::string& trackId, float volumeDB);
    mixmind::core::Result<void> setTrackPan(const std::string& trackId, float pan);
    mixmind::core::Result<void> setTrackMute(const std::string& trackId, bool muted);
    mixmind::core::Result<void> setTrackSolo(const std::string& trackId, bool soloed);
    
    float getTrackVolume(const std::string& trackId) const;
    float getTrackPan(const std::string& trackId) const;
    bool isTrackMuted(const std::string& trackId) const;
    bool isTrackSoloed(const std::string& trackId) const;
    
    // Audio routing
    mixmind::core::Result<void> createSend(const std::string& fromTrackId, const std::string& toTrackId, float level);
    mixmind::core::Result<void> removeSend(const std::string& fromTrackId, const std::string& toTrackId);
    mixmind::core::Result<void> setSendLevel(const std::string& fromTrackId, const std::string& toTrackId, float level);
    
    // Export/Render - Tracktion does it all
    struct RenderSettings {
        std::string filePath;
        std::string format = "WAV";  // "WAV", "MP3", "FLAC"
        int sampleRate = 48000;
        int bitDepth = 24;
        bool normalize = false;
        float normalizeLevel = -0.1f;
        bool realTimeExport = false;
        double startTime = 0.0;
        double endTime = -1.0; // -1 = entire project
    };
    
    mixmind::core::AsyncResult<void> renderToFile(const RenderSettings& settings);
    void cancelRender();
    
    // Audio analysis
    struct AudioAnalysis {
        float peakLevelL = -96.0f;
        float peakLevelR = -96.0f;
        float rmsLevelL = -96.0f;
        float rmsLevelR = -96.0f;
        float lufsLevel = -23.0f;
        std::vector<float> spectrum; // FFT data
    };
    
    AudioAnalysis getTrackAnalysis(const std::string& trackId) const;
    AudioAnalysis getMasterAnalysis() const;
    
    // MIDI functionality
    struct MidiNote {
        int note;
        int velocity;
        double startTime;
        double duration;
        int channel = 1;
    };
    
    mixmind::core::Result<void> addMidiNotes(const std::string& trackId, const std::vector<MidiNote>& notes);
    mixmind::core::Result<void> clearMidiTrack(const std::string& trackId);
    std::vector<MidiNote> getMidiNotes(const std::string& trackId) const;
    
    // Project information
    struct ProjectInfo {
        std::string name;
        std::string filePath;
        int trackCount;
        double lengthSeconds;
        double sampleRate;
        int bitDepth;
        bool hasUnsavedChanges;
        std::chrono::system_clock::time_point lastSaved;
    };
    
    ProjectInfo getProjectInfo() const;
    
    // Callbacks for real-time updates
    using PlaybackCallback = std::function<void(bool isPlaying, double position)>;
    using TrackCallback = std::function<void(const std::string& trackId, const std::string& action)>;
    using LevelCallback = std::function<void(const std::string& trackId, float levelL, float levelR)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    void setPlaybackCallback(PlaybackCallback callback);
    void setTrackCallback(TrackCallback callback);
    void setLevelCallback(LevelCallback callback);
    void setErrorCallback(ErrorCallback callback);
    
    // Advanced features
    
    // Automation - Tracktion has built-in automation
    mixmind::core::Result<void> enableAutomation(const std::string& trackId, const std::string& parameterName);
    mixmind::core::Result<void> addAutomationPoint(const std::string& trackId, const std::string& parameterName, 
                                                   double time, float value);
    mixmind::core::Result<void> clearAutomation(const std::string& trackId, const std::string& parameterName);
    
    // Bounce/Freeze tracks
    mixmind::core::AsyncResult<void> freezeTrack(const std::string& trackId);
    mixmind::core::Result<void> unfreezeTrack(const std::string& trackId);
    bool isTrackFrozen(const std::string& trackId) const;
    
    // Template system
    mixmind::core::Result<void> saveAsTemplate(const std::string& templateName, const std::string& description);
    mixmind::core::Result<void> loadTemplate(const std::string& templateName);
    std::vector<std::string> getAvailableTemplates() const;
    
    // Audio device integration
    struct AudioDeviceInfo {
        std::string name;
        std::string driverName;
        int numInputs;
        int numOutputs;
        std::vector<double> supportedSampleRates;
        std::vector<int> supportedBufferSizes;
    };
    
    std::vector<AudioDeviceInfo> getAvailableAudioDevices() const;
    mixmind::core::Result<void> setAudioDevice(const std::string& deviceName, double sampleRate, int bufferSize);
    AudioDeviceInfo getCurrentAudioDevice() const;
    
    // Plugin scanning
    mixmind::core::AsyncResult<void> scanPlugins(const std::vector<std::string>& searchPaths);
    std::vector<std::string> getAvailablePlugins() const;
    
    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    std::string getUndoDescription() const;
    std::string getRedoDescription() const;
    
    // Performance monitoring
    struct PerformanceStats {
        double cpuUsage;
        int xrunCount;
        double latencyMs;
        int activePlugins;
        size_t memoryUsage;
    };
    
    PerformanceStats getPerformanceStats() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// AI Controller - Bridges AI commands to Tracktion
// ============================================================================

class AITracktionController {
public:
    AITracktionController();
    ~AITracktionController();
    
    // Set the Tracktion DAW instance
    void setTracktionDAW(std::shared_ptr<TracktionDAW> daw);
    
    // AI command processing
    mixmind::core::Result<std::string> processAICommand(const std::string& command);
    mixmind::core::Result<void> executeNaturalLanguageRequest(const std::string& request);
    
    // High-level AI operations
    mixmind::core::Result<void> createTrackForInstrument(const std::string& instrumentName, const std::string& genre);
    mixmind::core::Result<void> setupVocalChain(const std::string& voiceType, const std::string& genre);
    mixmind::core::Result<void> createDrumBus();
    mixmind::core::Result<void> organizeTracksByInstrument();
    mixmind::core::Result<void> balanceMix();
    mixmind::core::Result<void> applySuggestedSettings(const std::string& genre);
    
    // Smart templates
    mixmind::core::Result<void> createProjectFromDescription(const std::string& description);
    mixmind::core::Result<void> suggestPluginChain(const std::string& trackType, const std::string& genre);
    
    // AI-powered mixing
    mixmind::core::Result<void> autoEQ(const std::string& trackId, const std::string& targetSound);
    mixmind::core::Result<void> autoCompression(const std::string& trackId, const std::string& style);
    mixmind::core::Result<void> spatialPlacement(const std::string& trackId, const std::string& position);
    
    // Voice control integration
    void enableVoiceControl(bool enabled);
    bool isVoiceControlEnabled() const;
    void processVoiceCommand(const std::string& voiceInput);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace mixmind::engine