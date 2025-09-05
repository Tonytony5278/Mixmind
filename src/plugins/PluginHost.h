#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include "../audio/LockFreeBuffer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>

namespace mixmind::plugins {

// ============================================================================
// Plugin Types and Formats
// ============================================================================

enum class PluginFormat {
    VST3,
    AU,           // Audio Units (macOS)
    LADSPA,       // Linux Audio Developer's Simple Plugin API
    LV2,          // LADSPA Version 2
    CLAP,         // CLever Audio Plug-in
    AAX           // Avid Audio eXtension (Pro Tools)
};

enum class PluginCategory {
    INSTRUMENT,           // Synthesizers, samplers
    EFFECT,              // Audio effects
    DYNAMICS,            // Compressors, limiters, gates
    EQ,                  // Equalizers, filters
    REVERB,              // Reverb and spatial effects
    DELAY,               // Delay, echo effects
    MODULATION,          // Chorus, flanger, phaser
    DISTORTION,          // Saturation, overdrive, fuzz
    ANALYZER,            // Spectrum analyzers, meters
    UTILITY,             // Gain, phase, stereo tools
    MASTERING,           // Mastering chain processors
    RESTORATION,         // Noise reduction, restoration
    VINTAGE,             // Vintage hardware emulations
    MODERN,              // Modern digital processors
    CREATIVE,            // Creative/experimental effects
    UNKNOWN
};

enum class PluginQuality {
    PROFESSIONAL = 5,    // Industry standard, flawless
    EXCELLENT = 4,       // Extremely high quality
    GOOD = 3,           // Solid, reliable quality
    AVERAGE = 2,        // Acceptable quality
    POOR = 1,           // Low quality, issues
    BROKEN = 0          // Non-functional
};

// ============================================================================
// Plugin Parameter System
// ============================================================================

struct PluginParameter {
    std::string id;
    std::string name;
    std::string displayName;
    std::string units;
    float value = 0.0f;
    float defaultValue = 0.0f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    bool isAutomatable = true;
    bool isReadOnly = false;
    std::vector<std::string> valueStrings; // For discrete parameters
    
    // AI enhancement data
    std::string aiDescription;       // AI-generated parameter description
    float aiImportanceScore = 0.5f; // How important this parameter is (0.0-1.0)
    std::vector<std::string> aiTags; // AI-generated tags for smart mapping
};

struct ParameterChange {
    std::string parameterId;
    float newValue;
    int sampleOffset = 0;      // Sample-accurate timing
    bool isFromUser = true;    // vs automation
    std::string source = "manual"; // "manual", "automation", "ai", "voice"
};

// ============================================================================
// Plugin Information and Metadata
// ============================================================================

struct PluginInfo {
    std::string uid;               // Unique identifier
    std::string name;
    std::string manufacturer;
    std::string version;
    std::string description;
    std::string filePath;
    PluginFormat format;
    PluginCategory category;
    PluginQuality quality = PluginQuality::UNKNOWN;
    
    // Audio configuration
    int numInputChannels = 2;
    int numOutputChannels = 2;
    bool acceptsMidi = false;
    bool producesMidi = false;
    bool isInstrument = false;
    bool isSynth = false;
    
    // Performance characteristics
    double averageCpuUsage = 0.0;     // Measured CPU usage %
    double peakCpuUsage = 0.0;        // Peak CPU usage %
    int latencySamples = 0;           // Introduced latency
    bool isRealTimeCapable = true;    // Can run in real-time
    
    // AI analysis
    std::string aiAnalysis;           // AI assessment of plugin capabilities
    std::vector<std::string> aiTags;  // AI-generated tags
    float aiQualityScore = 0.5f;      // AI quality assessment (0.0-1.0)
    std::string aiRecommendations;    // AI usage recommendations
    
    // User data
    float userRating = 0.0f;          // User rating (1-5 stars)
    int usageCount = 0;               // How often user loads this plugin
    std::string userNotes;            // User comments/notes
    std::vector<std::string> userPresets; // User-created presets
};

// ============================================================================
// Plugin Instance Management
// ============================================================================

class PluginInstance {
public:
    virtual ~PluginInstance() = default;
    
    // Basic plugin lifecycle
    virtual bool initialize(double sampleRate, int maxBufferSize) = 0;
    virtual bool activate() = 0;
    virtual void deactivate() = 0;
    virtual void cleanup() = 0;
    
    // Audio processing
    virtual void processAudio(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output) = 0;
    
    // Parameter control
    virtual std::vector<PluginParameter> getParameters() const = 0;
    virtual bool setParameter(const std::string& id, float value) = 0;
    virtual float getParameter(const std::string& id) const = 0;
    virtual void automateParameter(const std::string& id, const std::vector<std::pair<int, float>>& automation) = 0;
    
    // Preset management
    virtual std::vector<std::string> getPresets() const = 0;
    virtual bool loadPreset(const std::string& presetName) = 0;
    virtual bool savePreset(const std::string& presetName) = 0;
    virtual std::string getCurrentPreset() const = 0;
    
    // Plugin information
    virtual PluginInfo getInfo() const = 0;
    virtual std::string getStateData() const = 0;
    virtual bool setStateData(const std::string& data) = 0;
    
    // Performance monitoring
    virtual double getCurrentCpuUsage() const = 0;
    virtual int getCurrentLatency() const = 0;
    virtual bool isProcessing() const = 0;
    
    // UI integration
    virtual bool hasCustomUI() const = 0;
    virtual void showUI() = 0;
    virtual void hideUI() = 0;
    virtual bool isUIVisible() const = 0;
};

// ============================================================================
// Plugin Chain and Routing
// ============================================================================

struct PluginSlot {
    std::string slotId;
    std::shared_ptr<PluginInstance> plugin;
    bool isActive = true;
    bool isBypassed = false;
    float wetDryMix = 1.0f;        // 0.0 = dry, 1.0 = wet
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    
    // AI enhancements
    std::string aiOptimizationHints; // AI suggestions for this slot
    float aiEffectivenessScore = 0.5f; // How effective this plugin is in this context
};

struct PluginChain {
    std::string chainId;
    std::string name;
    std::vector<PluginSlot> slots;
    bool isActive = true;
    
    // Chain-level controls
    float masterInputGain = 1.0f;
    float masterOutputGain = 1.0f;
    bool isParallelProcessing = false;  // vs serial
    
    // AI chain analysis
    std::string aiAnalysis;             // AI assessment of the entire chain
    std::vector<std::string> aiSuggestions; // AI optimization suggestions
    float aiCoherenceScore = 0.5f;      // How well the plugins work together
};

// ============================================================================
// AI-Powered Plugin Intelligence
// ============================================================================

class PluginAI {
public:
    // Plugin quality analysis
    static PluginQuality analyzePluginQuality(const PluginInfo& info);
    static std::string generateQualityReport(const PluginInfo& info);
    static float calculateQualityScore(const PluginInfo& info);
    
    // Smart parameter mapping
    struct ParameterMapping {
        std::string sourceParam;
        std::string targetParam;
        float mappingCurve = 1.0f;     // Linear = 1.0, Exponential > 1.0, etc.
        float confidenceScore = 0.0f;  // AI confidence in this mapping
        std::string reasoning;         // Why AI chose this mapping
    };
    
    static std::vector<ParameterMapping> generateParameterMappings(
        const PluginInfo& sourcePlugin,
        const PluginInfo& targetPlugin);
    
    // Plugin recommendations
    struct PluginRecommendation {
        std::string pluginUid;
        float relevanceScore = 0.0f;   // How relevant to current context
        float qualityScore = 0.0f;     // Quality assessment
        std::string reasoning;         // Why recommended
        std::vector<std::string> useCases; // Suggested use cases
    };
    
    static std::vector<PluginRecommendation> recommendPlugins(
        PluginCategory category,
        const std::string& context = "",
        int maxRecommendations = 5);
    
    // Chain optimization
    static std::vector<std::string> optimizePluginChain(const PluginChain& chain);
    static PluginChain generateOptimalChain(const std::string& goal, const std::vector<PluginInfo>& availablePlugins);
    
    // Style-specific recommendations
    static std::vector<PluginRecommendation> getStylePlugins(const std::string& musicalStyle);
    static PluginChain generateStyleChain(const std::string& style, const std::vector<PluginInfo>& availablePlugins);
};

// ============================================================================
// Main Plugin Host System
// ============================================================================

class PluginHost {
public:
    PluginHost();
    ~PluginHost();
    
    // Non-copyable
    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;
    
    // Initialize plugin host system
    bool initialize(double sampleRate, int maxBufferSize);
    void shutdown();
    
    // Plugin management
    core::AsyncResult<std::vector<PluginInfo>> scanForPlugins();
    std::vector<PluginInfo> getAvailablePlugins() const;
    std::shared_ptr<PluginInstance> loadPlugin(const std::string& pluginUid);
    void unloadPlugin(std::shared_ptr<PluginInstance> plugin);
    
    // Plugin instance lifecycle
    bool activatePlugin(std::shared_ptr<PluginInstance> plugin);
    void deactivatePlugin(std::shared_ptr<PluginInstance> plugin);
    
    // Chain management
    std::string createPluginChain(const std::string& name = "");
    bool addPluginToChain(const std::string& chainId, std::shared_ptr<PluginInstance> plugin);
    bool removePluginFromChain(const std::string& chainId, const std::string& slotId);
    PluginChain getPluginChain(const std::string& chainId) const;
    std::vector<PluginChain> getAllChains() const;
    
    // Audio processing
    void processChain(const std::string& chainId,
                     const audio::AudioBufferPool::AudioBuffer& input,
                     audio::AudioBufferPool::AudioBuffer& output);
    
    // Parameter automation
    void automateParameter(const std::string& chainId, const std::string& slotId,
                          const std::string& parameterId, const std::vector<std::pair<int, float>>& automation);
    
    // AI-powered features
    void enableAIOptimization(bool enable);
    std::vector<PluginAI::PluginRecommendation> getAIRecommendations(const std::string& context = "");
    PluginChain generateAIChain(const std::string& goal);
    
    // Quality analysis
    core::AsyncResult<PluginQuality> analyzePluginQuality(const std::string& pluginUid);
    std::string generatePluginReport(const std::string& pluginUid);
    
    // Performance monitoring
    struct PerformanceStats {
        double totalCpuUsage = 0.0;
        double peakCpuUsage = 0.0;
        int totalLatency = 0;
        int activePluginCount = 0;
        std::vector<std::pair<std::string, double>> pluginCpuUsage;
    };
    
    PerformanceStats getPerformanceStats() const;
    void setPerformanceLimits(double maxCpuUsage, int maxLatency);
    
    // Event callbacks
    std::function<void(const std::string&)> onPluginLoaded;
    std::function<void(const std::string&)> onPluginUnloaded;
    std::function<void(const ParameterChange&)> onParameterChanged;
    std::function<void(const std::string&)> onPluginError;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Real-time Plugin Processor
// ============================================================================

class RealtimePluginProcessor {
public:
    RealtimePluginProcessor();
    ~RealtimePluginProcessor();
    
    // Real-time processing interface
    void processBuffer(const audio::AudioBufferPool::AudioBuffer& input,
                      audio::AudioBufferPool::AudioBuffer& output);
    
    // Thread-safe plugin management
    void addPlugin(std::shared_ptr<PluginInstance> plugin);
    void removePlugin(std::shared_ptr<PluginInstance> plugin);
    void bypassPlugin(std::shared_ptr<PluginInstance> plugin, bool bypass);
    
    // Real-time parameter changes
    void queueParameterChange(const ParameterChange& change);
    
    // Performance monitoring
    double getCurrentCpuLoad() const;
    bool hasXruns() const;
    void clearXruns();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Global Plugin Host Access
// ============================================================================

// Get global plugin host (singleton)
PluginHost& getGlobalPluginHost();

// Shutdown plugin host (call at app exit)
void shutdownGlobalPluginHost();

} // namespace mixmind::plugins