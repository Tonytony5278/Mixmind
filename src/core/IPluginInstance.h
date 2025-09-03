#pragma once

#include "types.h" 
#include "result.h"
#include <memory>
#include <unordered_map>

namespace mixmind::core {

// ============================================================================
// Plugin Instance Interface - Individual loaded plugin control
// ============================================================================

class IPluginInstance {
public:
    virtual ~IPluginInstance() = default;
    
    // ========================================================================
    // Instance Identity and Info
    // ========================================================================
    
    virtual PluginInstanceID getInstanceId() const = 0;
    virtual PluginInfo getPluginInfo() const = 0;
    virtual std::string getName() const = 0;
    virtual VoidResult setName(const std::string& name) = 0;
    virtual bool isValid() const = 0;
    
    // ========================================================================
    // Plugin State Management
    // ========================================================================
    
    virtual AsyncResult<VoidResult> initialize(SampleRate sampleRate, BufferSize maxBufferSize) = 0;
    virtual AsyncResult<VoidResult> release() = 0;
    virtual bool isInitialized() const = 0;
    
    // ========================================================================
    // Audio Processing
    // ========================================================================
    
    virtual void processBlock(FloatAudioBuffer& audioBuffer, MidiBuffer& midiBuffer) = 0;
    virtual bool canProcessAudio() const = 0;
    virtual bool canProcessMidi() const = 0;
    
    // ========================================================================
    // Parameter Control
    // ========================================================================
    
    struct ParameterInfo {
        ParamID id;
        std::string name;
        std::string label;  // units
        float minValue;
        float maxValue;
        float defaultValue;
        bool isAutomatable;
        bool isDiscrete;
        std::vector<std::string> discreteLabels;  // for discrete params
    };
    
    virtual std::vector<ParameterInfo> getParameters() const = 0;
    virtual std::optional<ParameterInfo> getParameterInfo(const ParamID& paramId) const = 0;
    
    virtual AsyncResult<VoidResult> setParameter(const ParamID& paramId, float value) = 0;
    virtual float getParameter(const ParamID& paramId) const = 0;
    virtual AsyncResult<VoidResult> setParameterNormalized(const ParamID& paramId, float normalizedValue) = 0;
    virtual float getParameterNormalized(const ParamID& paramId) const = 0;
    
    virtual std::string getParameterText(const ParamID& paramId) const = 0;
    virtual AsyncResult<VoidResult> setParameterFromText(const ParamID& paramId, const std::string& text) = 0;
    
    // ========================================================================
    // Editor/GUI Control
    // ========================================================================
    
    virtual bool hasEditor() const = 0;
    virtual AsyncResult<VoidResult> showEditor() = 0;
    virtual AsyncResult<VoidResult> hideEditor() = 0;
    virtual bool isEditorVisible() const = 0;
    
    virtual std::pair<int32_t, int32_t> getEditorSize() const = 0;
    virtual AsyncResult<VoidResult> setEditorSize(int32_t width, int32_t height) = 0;
    
    // ========================================================================
    // Preset Management
    // ========================================================================
    
    virtual AsyncResult<VoidResult> saveState(std::vector<uint8_t>& data) const = 0;
    virtual AsyncResult<VoidResult> loadState(const std::vector<uint8_t>& data) = 0;
    
    virtual std::vector<std::string> getFactoryPresets() const = 0;
    virtual AsyncResult<VoidResult> loadFactoryPreset(const std::string& presetName) = 0;
    virtual std::string getCurrentPresetName() const = 0;
    
    // ========================================================================
    // Processing Control
    // ========================================================================
    
    virtual VoidResult setBypass(bool bypassed) = 0;
    virtual bool isBypassed() const = 0;
    
    virtual VoidResult setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
    
    // ========================================================================
    // Performance Monitoring
    // ========================================================================
    
    virtual float getCPUUsage() const = 0;
    virtual size_t getMemoryUsage() const = 0;
    virtual int32_t getLatency() const = 0;  // samples
    
    // ========================================================================
    // Events
    // ========================================================================
    
    enum class PluginEvent {
        ParameterChanged,
        StateChanged,
        EditorOpened,
        EditorClosed,
        BypassChanged,
        LatencyChanged,
        CrashDetected
    };
    
    using PluginEventCallback = std::function<void(PluginEvent event, const std::string& details)>;
    virtual void addEventListener(PluginEventCallback callback) = 0;
    virtual void removeEventListener(PluginEventCallback callback) = 0;
};

} // namespace mixmind::core