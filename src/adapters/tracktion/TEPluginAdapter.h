#pragma once

#include "../../core/types.h"
#include "../../core/result.h"
#include "../../core/async.h"
#include "TEVSTScanner.h"
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace mixmind::adapters::tracktion {

// Forward declarations
class TETrackAdapter;
class TESessionAdapter;

// ============================================================================
// Plugin Parameter Information
// ============================================================================

struct PluginParameterInfo {
    Steinberg::Vst::ParamID id;
    std::string title;
    std::string shortTitle;
    std::string units;
    double defaultValue = 0.0;
    double currentValue = 0.0;
    double minValue = 0.0;
    double maxValue = 1.0;
    int32_t stepCount = 0;  // 0 = continuous, >0 = discrete steps
    int32_t flags = 0;      // Steinberg::Vst::ParameterInfo::ParameterFlags
    bool isAutomatable = true;
    bool isBypassParameter = false;
    
    bool isContinuous() const { return stepCount == 0; }
    bool isDiscrete() const { return stepCount > 0; }
    std::string toString() const {
        return title + " = " + std::to_string(currentValue) + " " + units;
    }
};

// ============================================================================
// Plugin State Management
// ============================================================================

struct PluginState {
    std::string pluginUID;
    std::string pluginName;
    std::vector<uint8_t> processorState;
    std::vector<uint8_t> controllerState;
    std::unordered_map<Steinberg::Vst::ParamID, double> parameterValues;
    bool bypassState = false;
    
    // Serialization
    core::Result<std::string> toJson() const;
    static core::Result<PluginState> fromJson(const std::string& json);
    
    bool isEmpty() const {
        return processorState.empty() && controllerState.empty() && parameterValues.empty();
    }
    
    size_t getDataSize() const {
        return processorState.size() + controllerState.size() + (parameterValues.size() * sizeof(double));
    }
};

// ============================================================================
// Plugin Instance - Represents a loaded VST3 plugin
// ============================================================================

class TEPluginInstance {
public:
    using ParamChangeCallback = std::function<void(Steinberg::Vst::ParamID, double)>;
    using StateChangeCallback = std::function<void(const PluginState&)>;
    
    TEPluginInstance(const VST3PluginInfo& pluginInfo, 
                     VST::Hosting::Module::Ptr module,
                     Steinberg::IPtr<Steinberg::Vst::IComponent> component,
                     Steinberg::IPtr<Steinberg::Vst::IEditController> controller);
    ~TEPluginInstance();
    
    // Non-copyable, movable
    TEPluginInstance(const TEPluginInstance&) = delete;
    TEPluginInstance& operator=(const TEPluginInstance&) = delete;
    TEPluginInstance(TEPluginInstance&&) = default;
    TEPluginInstance& operator=(TEPluginInstance&&) = default;
    
    // ========================================================================
    // Plugin Information
    // ========================================================================
    
    const VST3PluginInfo& getPluginInfo() const { return pluginInfo_; }
    std::string getInstanceId() const { return instanceId_; }
    bool isActive() const { return isActive_; }
    bool isBypassed() const { return isBypassed_; }
    
    // ========================================================================
    // Lifecycle Management
    // ========================================================================
    
    /// Initialize plugin with audio setup
    core::VoidResult initialize(double sampleRate, int32_t maxBlockSize);
    
    /// Activate plugin for processing
    core::VoidResult activate();
    
    /// Deactivate plugin processing
    core::VoidResult deactivate();
    
    /// Set bypass state
    core::VoidResult setBypassed(bool bypassed);
    
    // ========================================================================
    // Audio Processing
    // ========================================================================
    
    /// Process audio block
    core::VoidResult processAudio(Steinberg::Vst::ProcessData& data);
    
    /// Set processing precision (32-bit or 64-bit)
    core::VoidResult setProcessPrecision(Steinberg::Vst::ProcessPrecision precision);
    
    // ========================================================================
    // Parameter Management
    // ========================================================================
    
    /// Get all plugin parameters
    std::vector<PluginParameterInfo> getAllParameters() const;
    
    /// Get parameter by ID
    std::optional<PluginParameterInfo> getParameter(Steinberg::Vst::ParamID paramId) const;
    
    /// Set parameter value (normalized 0.0-1.0)
    core::VoidResult setParameter(Steinberg::Vst::ParamID paramId, double normalizedValue);
    
    /// Get parameter value (normalized 0.0-1.0)
    core::Result<double> getParameter(Steinberg::Vst::ParamID paramId) const;
    
    /// Convert normalized value to string representation
    core::Result<std::string> getParameterStringValue(Steinberg::Vst::ParamID paramId, double normalizedValue) const;
    
    /// Convert string to normalized parameter value
    core::Result<double> getParameterNormalizedValue(Steinberg::Vst::ParamID paramId, const std::string& stringValue) const;
    
    // ========================================================================
    // State Management
    // ========================================================================
    
    /// Get current plugin state
    core::Result<PluginState> getState() const;
    
    /// Restore plugin state
    core::VoidResult setState(const PluginState& state);
    
    /// Get preset information
    std::vector<std::string> getPresetList() const;
    
    /// Load preset by name
    core::VoidResult loadPreset(const std::string& presetName);
    
    // ========================================================================
    // Editor Support
    // ========================================================================
    
    /// Check if plugin has editor
    bool hasEditor() const;
    
    /// Create plugin editor view
    core::Result<void*> createEditor(void* parentWindow);
    
    /// Close plugin editor
    core::VoidResult closeEditor();
    
    /// Get editor size
    core::Result<core::Size> getEditorSize() const;
    
    // ========================================================================
    // Event Callbacks
    // ========================================================================
    
    /// Set parameter change callback
    void setParameterChangeCallback(ParamChangeCallback callback);
    
    /// Set state change callback  
    void setStateChangeCallback(StateChangeCallback callback);
    
    // ========================================================================
    // MIDI Support
    // ========================================================================
    
    /// Send MIDI event to plugin
    core::VoidResult sendMidiEvent(const core::MidiEvent& midiEvent);
    
    /// Process MIDI events
    core::VoidResult processMidiEvents(const std::vector<core::MidiEvent>& events);

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    void initializeParameters();
    core::VoidResult setupAudioBuses();
    void handleParameterChanges(Steinberg::Vst::IParameterChanges* paramChanges);
    void notifyParameterChange(Steinberg::Vst::ParamID paramId, double value);
    
    // Core plugin objects
    std::string instanceId_;
    VST3PluginInfo pluginInfo_;
    VST::Hosting::Module::Ptr module_;
    Steinberg::IPtr<Steinberg::Vst::IComponent> component_;
    Steinberg::IPtr<Steinberg::Vst::IEditController> controller_;
    Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> processor_;
    
    // State
    std::atomic<bool> isActive_{false};
    std::atomic<bool> isBypassed_{false};
    double sampleRate_ = 44100.0;
    int32_t maxBlockSize_ = 512;
    
    // Parameters
    mutable std::mutex parameterMutex_;
    std::unordered_map<Steinberg::Vst::ParamID, PluginParameterInfo> parameters_;
    
    // Callbacks
    ParamChangeCallback paramChangeCallback_;
    StateChangeCallback stateChangeCallback_;
    
    // Editor
    void* editorView_ = nullptr;
    
    // Processing context
    Steinberg::Vst::ProcessContext processContext_;
};

using TEPluginInstancePtr = std::unique_ptr<TEPluginInstance>;

// ============================================================================
// Plugin Adapter - Manages VST3 plugin lifecycle and integration
// ============================================================================

class TEPluginAdapter {
public:
    TEPluginAdapter();
    ~TEPluginAdapter();
    
    // Non-copyable
    TEPluginAdapter(const TEPluginAdapter&) = delete;
    TEPluginAdapter& operator=(const TEPluginAdapter&) = delete;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /// Initialize plugin adapter with audio settings
    core::VoidResult initialize(double sampleRate, int32_t maxBlockSize);
    
    /// Shutdown plugin adapter
    core::VoidResult shutdown();
    
    // ========================================================================
    // Plugin Discovery and Loading
    // ========================================================================
    
    /// Scan for available plugins
    core::AsyncResult<core::VoidResult> scanPlugins();
    
    /// Get all available plugins
    std::vector<VST3PluginInfo> getAvailablePlugins() const;
    
    /// Find plugins by category
    std::vector<VST3PluginInfo> getPluginsByCategory(const std::string& category) const;
    
    /// Load plugin by UID
    core::AsyncResult<core::Result<TEPluginInstancePtr>> loadPlugin(const std::string& pluginUID);
    
    /// Unload plugin instance
    core::VoidResult unloadPlugin(const std::string& instanceId);
    
    /// Get loaded plugin instance
    TEPluginInstance* getPluginInstance(const std::string& instanceId) const;
    
    // ========================================================================
    // Track Integration
    // ========================================================================
    
    /// Insert plugin on track at specific position
    core::AsyncResult<core::Result<std::string>> insertPluginOnTrack(
        const std::string& trackId, 
        const std::string& pluginUID, 
        int insertIndex = -1  // -1 = append at end
    );
    
    /// Remove plugin from track
    core::VoidResult removePluginFromTrack(const std::string& trackId, const std::string& instanceId);
    
    /// Move plugin position on track
    core::VoidResult movePluginOnTrack(const std::string& trackId, const std::string& instanceId, int newIndex);
    
    /// Get all plugins on track
    std::vector<std::string> getTrackPlugins(const std::string& trackId) const;
    
    // ========================================================================
    // Session Integration
    // ========================================================================
    
    /// Save all plugin states to session
    core::Result<std::unordered_map<std::string, PluginState>> savePluginStates() const;
    
    /// Restore plugin states from session
    core::AsyncResult<core::VoidResult> restorePluginStates(
        const std::unordered_map<std::string, PluginState>& pluginStates
    );
    
    /// Get plugin state by instance ID
    core::Result<PluginState> getPluginState(const std::string& instanceId) const;
    
    /// Set plugin state by instance ID
    core::AsyncResult<core::VoidResult> setPluginState(const std::string& instanceId, const PluginState& state);
    
    // ========================================================================
    // Plugin Chain Processing
    // ========================================================================
    
    /// Process audio through track plugin chain
    core::VoidResult processTrackPlugins(
        const std::string& trackId,
        float** audioInputs,
        float** audioOutputs, 
        int32_t numChannels,
        int32_t numSamples,
        const std::vector<core::MidiEvent>& midiEvents = {}
    );
    
    /// Bypass/unbypass plugin
    core::VoidResult setPluginBypassed(const std::string& instanceId, bool bypassed);
    
    // ========================================================================
    // Preset Management
    // ========================================================================
    
    /// Get available presets for plugin
    core::Result<std::vector<std::string>> getPluginPresets(const std::string& instanceId) const;
    
    /// Load preset for plugin
    core::AsyncResult<core::VoidResult> loadPluginPreset(const std::string& instanceId, const std::string& presetName);
    
    /// Save current state as preset
    core::VoidResult savePluginPreset(const std::string& instanceId, const std::string& presetName);
    
    // ========================================================================
    // Automation Support
    // ========================================================================
    
    /// Set parameter automation value at sample time
    core::VoidResult setAutomationValue(
        const std::string& instanceId, 
        Steinberg::Vst::ParamID paramId, 
        double normalizedValue,
        int32_t sampleOffset = 0
    );
    
    /// Get automatable parameters for plugin
    core::Result<std::vector<PluginParameterInfo>> getAutomatableParameters(const std::string& instanceId) const;
    
    // ========================================================================
    // Statistics and Monitoring
    // ========================================================================
    
    /// Get performance statistics
    struct PluginStats {
        int totalPluginsLoaded = 0;
        int activePlugins = 0;
        int bypassedPlugins = 0;
        double averageLoadTime = 0.0;
        double totalCpuUsage = 0.0;
        size_t totalMemoryUsage = 0;
    };
    
    PluginStats getPluginStats() const;
    
    /// Get detailed plugin information
    core::Result<std::string> getPluginDiagnosticInfo(const std::string& instanceId) const;

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Generate unique instance ID
    std::string generateInstanceId() const;
    
    /// Load VST3 module
    core::Result<VST::Hosting::Module::Ptr> loadVST3Module(const std::string& pluginPath) const;
    
    /// Create plugin components
    core::Result<std::pair<Steinberg::IPtr<Steinberg::Vst::IComponent>, 
                          Steinberg::IPtr<Steinberg::Vst::IEditController>>> 
    createPluginComponents(VST::Hosting::Module::Ptr module, const std::string& pluginUID) const;
    
    /// Setup audio buses for plugin
    core::VoidResult setupPluginAudioBuses(TEPluginInstance* plugin) const;
    
    /// Thread-safe plugin registry access
    mutable std::recursive_mutex pluginMutex_;
    
    /// Plugin scanner
    std::unique_ptr<TEVSTScanner> scanner_;
    
    /// Loaded plugin instances
    std::unordered_map<std::string, TEPluginInstancePtr> loadedPlugins_;
    
    /// Track to plugin mapping
    std::unordered_map<std::string, std::vector<std::string>> trackPlugins_;
    
    /// Audio settings
    double sampleRate_ = 44100.0;
    int32_t maxBlockSize_ = 512;
    bool isInitialized_ = false;
    
    /// Performance tracking
    mutable std::atomic<double> totalCpuUsage_{0.0};
    mutable std::atomic<size_t> totalMemoryUsage_{0};
};

// ============================================================================
// Global Plugin Adapter Instance
// ============================================================================

/// Get the global plugin adapter instance
TEPluginAdapter& getGlobalPluginAdapter();

} // namespace mixmind::adapters::tracktion