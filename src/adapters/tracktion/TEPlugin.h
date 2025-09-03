#pragma once

#include "TEAdapter.h"
#include "../../core/IPluginHost.h"
#include "../../core/IPluginInstance.h"
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
// TE Plugin Host Adapter - Tracktion Engine implementation of IPluginHost
// ============================================================================

class TEPluginHost : public TEAdapter, public core::IPluginHost {
public:
    explicit TEPluginHost(te::Engine& engine);
    ~TEPluginHost() override = default;
    
    // Non-copyable, movable
    TEPluginHost(const TEPluginHost&) = delete;
    TEPluginHost& operator=(const TEPluginHost&) = delete;
    TEPluginHost(TEPluginHost&&) = default;
    TEPluginHost& operator=(TEPluginHost&&) = default;
    
    // ========================================================================
    // IPluginHost Implementation
    // ========================================================================
    
    // Plugin Discovery and Management
    core::AsyncResult<core::VoidResult> scanForPlugins(
        const std::vector<std::string>& searchPaths = {},
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::Result<std::vector<AvailablePlugin>>> getAvailablePlugins(
        PluginFormat format = PluginFormat::All
    ) const override;
    
    core::AsyncResult<core::Result<AvailablePlugin>> findPlugin(
        const std::string& nameOrId,
        PluginFormat format = PluginFormat::All
    ) const override;
    
    // Plugin Loading and Instantiation
    core::AsyncResult<core::Result<core::PluginInstanceID>> loadPlugin(
        core::TrackID trackId,
        const std::string& pluginId,
        int32_t slotIndex = -1
    ) override;
    
    core::AsyncResult<core::Result<core::PluginInstanceID>> loadPluginFromFile(
        core::TrackID trackId,
        const std::string& filePath,
        int32_t slotIndex = -1
    ) override;
    
    core::AsyncResult<core::VoidResult> unloadPlugin(core::PluginInstanceID instanceId) override;
    
    core::AsyncResult<core::VoidResult> unloadAllPluginsFromTrack(core::TrackID trackId) override;
    
    // Plugin Chain Management
    core::AsyncResult<core::VoidResult> movePlugin(
        core::PluginInstanceID instanceId,
        int32_t newSlotIndex
    ) override;
    
    core::AsyncResult<core::VoidResult> movePluginToTrack(
        core::PluginInstanceID instanceId,
        core::TrackID targetTrackId,
        int32_t slotIndex = -1
    ) override;
    
    core::AsyncResult<core::Result<std::vector<core::PluginInstanceID>>> getPluginChain(
        core::TrackID trackId
    ) const override;
    
    core::AsyncResult<core::VoidResult> reorderPluginChain(
        core::TrackID trackId,
        const std::vector<core::PluginInstanceID>& newOrder
    ) override;
    
    // Plugin State and Presets
    core::AsyncResult<core::VoidResult> savePluginState(
        core::PluginInstanceID instanceId,
        const std::string& filePath
    ) override;
    
    core::AsyncResult<core::VoidResult> loadPluginState(
        core::PluginInstanceID instanceId,
        const std::string& filePath
    ) override;
    
    core::AsyncResult<core::VoidResult> savePluginPreset(
        core::PluginInstanceID instanceId,
        const std::string& presetName,
        const std::string& category = ""
    ) override;
    
    core::AsyncResult<core::VoidResult> loadPluginPreset(
        core::PluginInstanceID instanceId,
        const std::string& presetName
    ) override;
    
    core::AsyncResult<core::Result<std::vector<PresetInfo>>> getAvailablePresets(
        core::PluginInstanceID instanceId
    ) const override;
    
    // Bulk Operations
    core::AsyncResult<core::VoidResult> enableAllPlugins(core::TrackID trackId) override;
    core::AsyncResult<core::VoidResult> disableAllPlugins(core::TrackID trackId) override;
    core::AsyncResult<core::VoidResult> bypassAllPlugins(core::TrackID trackId, bool bypassed) override;
    
    // Plugin Information
    core::AsyncResult<core::Result<std::vector<PluginInstanceInfo>>> getAllPluginInstances() const override;
    core::AsyncResult<core::Result<PluginInstanceInfo>> getPluginInstance(
        core::PluginInstanceID instanceId
    ) const override;
    
    // Event Callbacks
    void setPluginEventCallback(PluginEventCallback callback) override;
    void clearPluginEventCallback() override;
    
    // Plugin Format Support
    std::vector<PluginFormat> getSupportedFormats() const override;
    bool isFormatSupported(PluginFormat format) const override;

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Get plugin manager from engine
    te::PluginManager& getPluginManager() const;
    
    /// Get known plugins list
    juce::KnownPluginList& getKnownPluginList() const;
    
    /// Get Tracktion Engine plugin by ID
    te::Plugin* getTEPlugin(core::PluginInstanceID instanceId) const;
    
    /// Get track by ID
    te::AudioTrack* getTrack(core::TrackID trackId) const;
    
    /// Convert TE plugin description to AvailablePlugin
    AvailablePlugin convertTEPluginDescription(const juce::PluginDescription& desc) const;
    
    /// Convert TE plugin to PluginInstanceInfo
    PluginInstanceInfo convertTEPluginToInfo(te::Plugin* plugin) const;
    
    /// Convert PluginFormat to TE format
    juce::AudioPluginFormat* convertPluginFormatToTE(PluginFormat format) const;
    
    /// Update plugin mapping
    void updatePluginMapping();
    
    /// Generate unique plugin instance ID
    core::PluginInstanceID generatePluginInstanceID();
    
    /// Emit plugin event
    void emitPluginEvent(PluginEventType eventType, core::PluginInstanceID instanceId, const std::string& details);

private:
    // Plugin instance mapping
    std::unordered_map<core::PluginInstanceID, te::Plugin*> pluginMap_;
    std::unordered_map<te::Plugin*, core::PluginInstanceID> reversePluginMap_;
    mutable std::shared_mutex pluginMapMutex_;
    
    // ID generation
    std::atomic<uint32_t> nextPluginInstanceId_{1};
    
    // Event callback
    PluginEventCallback pluginEventCallback_;
    std::mutex callbackMutex_;
    
    // Current edit reference
    mutable te::Edit* currentEdit_ = nullptr;
    mutable std::mutex editMutex_;
};

// ============================================================================
// TE Plugin Instance Adapter - Tracktion Engine implementation of IPluginInstance
// ============================================================================

class TEPluginInstance : public TEAdapter, public core::IPluginInstance {
public:
    TEPluginInstance(te::Engine& engine, core::PluginInstanceID instanceId);
    ~TEPluginInstance() override = default;
    
    // Non-copyable, movable
    TEPluginInstance(const TEPluginInstance&) = delete;
    TEPluginInstance& operator=(const TEPluginInstance&) = delete;
    TEPluginInstance(TEPluginInstance&&) = default;
    TEPluginInstance& operator=(TEPluginInstance&&) = default;
    
    // ========================================================================
    // IPluginInstance Implementation
    // ========================================================================
    
    // Plugin Identity and Information
    core::PluginInstanceID getInstanceID() const override;
    core::AsyncResult<core::Result<PluginInstanceInfo>> getPluginInfo() const override;
    core::AsyncResult<core::Result<PluginCapabilities>> getCapabilities() const override;
    
    // Plugin State Control
    core::AsyncResult<core::VoidResult> setEnabled(bool enabled) override;
    core::AsyncResult<core::VoidResult> setBypassed(bool bypassed) override;
    bool isEnabled() const override;
    bool isBypassed() const override;
    
    // Parameter Management
    core::AsyncResult<core::Result<std::vector<ParameterInfo>>> getParameters() const override;
    core::AsyncResult<core::Result<ParameterInfo>> getParameter(int32_t parameterId) const override;
    core::AsyncResult<core::Result<ParameterInfo>> getParameterByName(const std::string& parameterName) const override;
    
    core::AsyncResult<core::VoidResult> setParameter(int32_t parameterId, float value) override;
    core::AsyncResult<core::VoidResult> setParameterByName(const std::string& parameterName, float value) override;
    core::AsyncResult<core::Result<float>> getParameterValue(int32_t parameterId) const override;
    core::AsyncResult<core::Result<float>> getParameterValueByName(const std::string& parameterName) const override;
    
    core::AsyncResult<core::VoidResult> setMultipleParameters(
        const std::vector<std::pair<int32_t, float>>& parameterValues
    ) override;
    
    core::AsyncResult<core::VoidResult> resetParametersToDefault() override;
    core::AsyncResult<core::VoidResult> resetParameter(int32_t parameterId) override;
    
    // Parameter Automation
    core::AsyncResult<core::VoidResult> automateParameter(
        int32_t parameterId,
        float startValue,
        float endValue,
        core::TimeDuration duration,
        AutomationCurve curve = AutomationCurve::Linear
    ) override;
    
    core::AsyncResult<core::VoidResult> recordParameterAutomation(
        int32_t parameterId,
        bool recording
    ) override;
    
    core::AsyncResult<core::VoidResult> clearParameterAutomation(int32_t parameterId) override;
    
    // Preset Management
    core::AsyncResult<core::Result<std::vector<PresetInfo>>> getFactoryPresets() const override;
    core::AsyncResult<core::Result<std::vector<PresetInfo>>> getUserPresets() const override;
    core::AsyncResult<core::VoidResult> loadPreset(const std::string& presetName) override;
    core::AsyncResult<core::VoidResult> savePreset(const std::string& presetName, bool userPreset = true) override;
    core::AsyncResult<core::VoidResult> deletePreset(const std::string& presetName) override;
    
    // Plugin Programs (for VST2 compatibility)
    core::AsyncResult<core::Result<std::vector<std::string>>> getPrograms() const override;
    core::AsyncResult<core::VoidResult> setCurrentProgram(int32_t programIndex) override;
    core::AsyncResult<core::Result<int32_t>> getCurrentProgram() const override;
    
    // State Management
    core::AsyncResult<core::Result<std::vector<uint8_t>>> getState() const override;
    core::AsyncResult<core::VoidResult> setState(const std::vector<uint8_t>& state) override;
    core::AsyncResult<core::Result<std::string>> getStateAsString() const override;
    core::AsyncResult<core::VoidResult> setStateFromString(const std::string& state) override;
    
    // MIDI and Audio Processing
    core::AsyncResult<core::VoidResult> sendMIDIEvent(const MIDIEvent& event) override;
    core::AsyncResult<core::VoidResult> sendMIDICC(int32_t controller, int32_t value, int32_t channel = 1) override;
    core::AsyncResult<core::VoidResult> sendMIDINote(
        int32_t note,
        int32_t velocity,
        int32_t channel = 1,
        bool noteOn = true
    ) override;
    
    // Plugin GUI Management
    core::AsyncResult<core::VoidResult> showEditor(bool show = true) override;
    core::AsyncResult<core::VoidResult> hideEditor() override;
    bool isEditorVisible() const override;
    core::AsyncResult<core::Result<WindowInfo>> getEditorSize() const override;
    core::AsyncResult<core::VoidResult> setEditorSize(int32_t width, int32_t height) override;
    
    // Performance and Monitoring
    core::AsyncResult<core::Result<PerformanceStats>> getPerformanceStats() const override;
    core::AsyncResult<core::VoidResult> resetPerformanceStats() override;
    double getCPUUsage() const override;
    int32_t getLatencySamples() const override;
    
    // Event Callbacks
    void setParameterChangeCallback(ParameterChangeCallback callback) override;
    void setStateChangeCallback(StateChangeCallback callback) override;
    void clearParameterChangeCallback() override;
    void clearStateChangeCallback() override;

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Get the underlying TE Plugin
    te::Plugin* getTEPlugin() const;
    
    /// Convert TE parameter to ParameterInfo
    ParameterInfo convertTEParameterToInfo(te::AutomatableParameter* param, int32_t index) const;
    
    /// Convert automation curve to TE curve type
    te::CurveSource::Type convertAutomationCurveToTE(AutomationCurve curve) const;
    
    /// Update parameter mappings
    void updateParameterMappings();
    
    /// Emit parameter change event
    void emitParameterChangeEvent(int32_t parameterId, float newValue);
    
    /// Emit state change event
    void emitStateChangeEvent();

private:
    // Plugin instance reference
    core::PluginInstanceID instanceId_;
    mutable te::Plugin* plugin_ = nullptr;
    mutable std::mutex pluginMutex_;
    
    // Parameter mappings
    std::unordered_map<int32_t, te::AutomatableParameter*> parameterMap_;
    std::unordered_map<std::string, te::AutomatableParameter*> parameterNameMap_;
    mutable std::shared_mutex parameterMapMutex_;
    
    // Event callbacks
    ParameterChangeCallback parameterChangeCallback_;
    StateChangeCallback stateChangeCallback_;
    std::mutex callbackMutex_;
    
    // Performance tracking
    mutable PerformanceStats performanceStats_;
    mutable std::mutex perfMutex_;
};

} // namespace mixmind::adapters::tracktion