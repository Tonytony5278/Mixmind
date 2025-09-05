#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <unordered_map>

// Forward declarations
#ifdef MIXMIND_JUCE_ENABLED
namespace juce {
    class AudioProcessor;
    class AudioPluginInstance;
}
#endif

namespace mixmind::plugins {

// ============================================================================
// Universal Plugin Bridge System - CRITICAL COMPONENT 1
// The key to controlling ALL professional plugins via AI
// ============================================================================

struct ParameterMapping {
    int index = -1;
    std::string name;
    std::string aiDescription;      // AI-friendly parameter description
    std::string musicalFunction;    // "gain", "frequency", "resonance", etc.
    float defaultValue = 0.0f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    std::string units;              // "dB", "Hz", "%", etc.
    bool isAutomatable = true;
    std::string category;           // "EQ", "Dynamics", "Time", "Modulation"
};

struct PresetData {
    std::string name;
    std::string category;
    std::map<std::string, float> parameterValues;
    std::string description;
    std::vector<std::string> tags;
};

struct PluginMetadata {
    std::string id;
    std::string name;
    std::string manufacturer;
    std::string category;
    std::string version;
    std::string pluginPath;
    
    std::vector<ParameterMapping> parameters;
    std::map<std::string, PresetData> presets;
    
    bool hasCustomUI = false;
    bool hasEditor = false;
    bool isSynth = false;
    bool acceptsMidi = false;
    bool producesMidi = false;
    
    int numInputs = 0;
    int numOutputs = 0;
    int latencySamples = 0;
    
    // AI-specific data
    std::vector<std::string> aiTags;
    std::string aiDescription;
    float aiRecommendationScore = 0.0f; // How often AI suggests this plugin
    
    // Performance data
    double averageCpuUsage = 0.0;
    size_t memoryUsage = 0;
};

// Plugin-specific mappings for professional integration
struct DrumMapping {
    int kick = 36;           // C1
    int snare = 38;          // D1
    int hihat_closed = 42;   // F#1
    int hihat_open = 46;     // A#1
    int crash = 49;          // C#2
    int ride = 51;           // D#2
    int tom_low = 43;        // G1
    int tom_mid = 47;        // B1
    int tom_high = 50;       // D2
};

struct AmpMapping {
    std::string gain;
    std::string bass;
    std::string mid;
    std::string treble;
    std::string presence;
    std::string volume;
    std::string drive;
    std::string master;
};

struct SynthMapping {
    std::string cutoff;
    std::string resonance;
    std::string attack;
    std::string decay;
    std::string sustain;
    std::string release;
    std::string lfoRate;
    std::string lfoAmount;
};

// AI command system for plugins
using AICommandCallback = std::function<void(const std::string& command, const std::map<std::string, float>& parameters)>;

class UniversalPluginBridge {
public:
    UniversalPluginBridge();
    ~UniversalPluginBridge();
    
    // Non-copyable
    UniversalPluginBridge(const UniversalPluginBridge&) = delete;
    UniversalPluginBridge& operator=(const UniversalPluginBridge&) = delete;
    
    // Plugin discovery and analysis
    mixmind::core::AsyncResult<void> scanAndAnalyzePlugins();
    void scanVST2Plugins(const std::string& path);
    void scanVST3Plugins(const std::string& path);
    void scanAUPlugins(const std::string& path);
    void scanCLAPPlugins(const std::string& path);
    
    // Plugin loading and management
    mixmind::core::Result<std::string> loadPlugin(const std::string& pluginPath);
    mixmind::core::Result<void> unloadPlugin(const std::string& pluginId);
    mixmind::core::Result<void> bypassPlugin(const std::string& pluginId, bool bypass);
    
    // Parameter control
    mixmind::core::Result<void> setParameter(const std::string& pluginId, const std::string& parameterName, float value);
    mixmind::core::Result<void> setParameter(const std::string& pluginId, int parameterIndex, float value);
    mixmind::core::Result<float> getParameter(const std::string& pluginId, const std::string& parameterName);
    mixmind::core::Result<float> getParameter(const std::string& pluginId, int parameterIndex);
    
    // Preset management
    mixmind::core::Result<void> loadPreset(const std::string& pluginId, const std::string& presetName);
    mixmind::core::Result<void> savePreset(const std::string& pluginId, const std::string& presetName);
    std::vector<std::string> getPresetList(const std::string& pluginId);
    
    // Plugin information
    PluginMetadata getPluginMetadata(const std::string& pluginId) const;
    std::vector<PluginMetadata> getAllPlugins() const;
    std::vector<PluginMetadata> getPluginsByCategory(const std::string& category) const;
    std::vector<PluginMetadata> searchPlugins(const std::string& query) const;
    
    // AI-powered plugin analysis
    void analyzePluginWithAI(PluginMetadata& metadata);
    std::string inferParameterPurpose(const std::string& parameterName);
    std::string categorizeParameter(const std::string& parameterName);
    std::vector<std::string> generateAITags(const PluginMetadata& metadata);
    
    // Professional plugin integrations
    void setupSuperiorDrummerIntegration(PluginMetadata& metadata);
    void setupNeuralDSPIntegration(PluginMetadata& metadata);
    void setupSerumIntegration(PluginMetadata& metadata);
    void setupOmnisphereIntegration(PluginMetadata& metadata);
    void setupFabFilterIntegration(PluginMetadata& metadata);
    void setupWavesIntegration(PluginMetadata& metadata);
    void setupiZotopeIntegration(PluginMetadata& metadata);
    
    // AI command registration
    void registerAICommand(const std::string& pluginId, const std::string& command, AICommandCallback callback);
    mixmind::core::Result<void> executeAICommand(const std::string& pluginId, const std::string& command, 
                                                 const std::map<std::string, std::string>& arguments);
    
    // Smart parameter mapping
    ParameterMapping findParameterByFunction(const std::string& pluginId, const std::string& function);
    std::vector<ParameterMapping> getParametersByCategory(const std::string& pluginId, const std::string& category);
    
    // Plugin recommendation system
    struct PluginRecommendation {
        std::string pluginId;
        std::string reason;
        float confidence;
        std::vector<std::string> suggestedSettings;
    };
    
    std::vector<PluginRecommendation> recommendPluginsForTask(const std::string& task, const std::string& genre = "");
    std::vector<PluginRecommendation> recommendPluginsForInstrument(const std::string& instrument);
    
    // Plugin chain management
    struct PluginChain {
        std::string name;
        std::vector<std::string> pluginIds;
        std::map<std::string, std::map<std::string, float>> presetValues;
        std::string description;
    };
    
    void savePluginChain(const std::string& name, const std::vector<std::string>& pluginIds);
    mixmind::core::Result<void> loadPluginChain(const std::string& name);
    std::vector<std::string> getPluginChains() const;
    
    // Performance monitoring
    struct PluginPerformanceData {
        double cpuUsage;
        size_t memoryUsage;
        int latencySamples;
        bool hasErrors;
        std::string lastError;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    
    PluginPerformanceData getPluginPerformance(const std::string& pluginId) const;
    void updatePluginPerformance(const std::string& pluginId, const PluginPerformanceData& data);
    
    // Event callbacks
    using PluginLoadedCallback = std::function<void(const std::string& pluginId, const PluginMetadata& metadata)>;
    using PluginUnloadedCallback = std::function<void(const std::string& pluginId)>;
    using ParameterChangedCallback = std::function<void(const std::string& pluginId, int parameterIndex, float value)>;
    using PluginErrorCallback = std::function<void(const std::string& pluginId, const std::string& error)>;
    
    void setPluginLoadedCallback(PluginLoadedCallback callback);
    void setPluginUnloadedCallback(PluginUnloadedCallback callback);
    void setParameterChangedCallback(ParameterChangedCallback callback);
    void setPluginErrorCallback(PluginErrorCallback callback);
    
    // Specialized plugin interfaces
    
    // Superior Drummer 3 Interface
    class SuperiorDrummerInterface {
    public:
        explicit SuperiorDrummerInterface(UniversalPluginBridge* bridge, const std::string& pluginId);
        
        void changeDrumKit(const std::string& kitName);
        void setDrumMix(float kickLevel, float snareLevel, float hihatLevel, float overheadLevel);
        void setVelocityCurve(const std::string& piece, float curve);
        void loadMIDIGroove(const std::string& grooveName);
        
    private:
        UniversalPluginBridge* bridge_;
        std::string pluginId_;
        DrumMapping mapping_;
    };
    
    // Neural DSP Interface
    class NeuralDSPInterface {
    public:
        explicit NeuralDSPInterface(UniversalPluginBridge* bridge, const std::string& pluginId);
        
        void setGuitarTone(const std::string& toneType); // "clean", "crunch", "lead", "rhythm"
        void setAmpSettings(float gain, float bass, float mid, float treble, float presence);
        void setCabinetModel(const std::string& cabinetName);
        void setMicPosition(float position);
        
    private:
        UniversalPluginBridge* bridge_;
        std::string pluginId_;
        AmpMapping mapping_;
    };
    
    // Serum Interface
    class SerumInterface {
    public:
        explicit SerumInterface(UniversalPluginBridge* bridge, const std::string& pluginId);
        
        void loadWavetable(int oscillator, const std::string& wavetableName);
        void setFilterSettings(float cutoff, float resonance, const std::string& filterType);
        void setEnvelope(const std::string& envelope, float attack, float decay, float sustain, float release);
        void setLFO(int lfoIndex, float rate, float amount, const std::string& destination);
        
    private:
        UniversalPluginBridge* bridge_;
        std::string pluginId_;
        SynthMapping mapping_;
    };
    
    // Factory methods for specialized interfaces
    std::unique_ptr<SuperiorDrummerInterface> createSuperiorDrummerInterface(const std::string& pluginId);
    std::unique_ptr<NeuralDSPInterface> createNeuralDSPInterface(const std::string& pluginId);
    std::unique_ptr<SerumInterface> createSerumInterface(const std::string& pluginId);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Plugin Database for AI Understanding
// ============================================================================

class PluginDatabase {
public:
    static PluginDatabase& getInstance();
    
    // Known plugin identification
    bool isKnownPlugin(const std::string& pluginName) const;
    std::string getPluginCategory(const std::string& pluginName) const;
    std::vector<std::string> getPluginTags(const std::string& pluginName) const;
    
    // Parameter understanding
    std::string getParameterFunction(const std::string& pluginName, const std::string& parameterName) const;
    std::string getParameterDescription(const std::string& parameterName) const;
    
    // AI training data
    void addPluginKnowledge(const std::string& pluginName, const PluginMetadata& metadata);
    void updateParameterUsage(const std::string& pluginName, const std::string& parameter, float value);
    
    // Smart defaults
    float getSmartDefault(const std::string& pluginName, const std::string& parameter, const std::string& genre) const;
    std::map<std::string, float> getGenreDefaults(const std::string& pluginName, const std::string& genre) const;
    
private:
    PluginDatabase() = default;
    void initializeKnownPlugins();
    
    struct PluginKnowledge {
        std::string category;
        std::vector<std::string> tags;
        std::map<std::string, std::string> parameterFunctions;
        std::map<std::string, std::map<std::string, float>> genreDefaults;
    };
    
    std::unordered_map<std::string, PluginKnowledge> knownPlugins_;
};

} // namespace mixmind::plugins