#include "TEPluginAdapter.h"
#include <nlohmann/json.hpp>
#include <pluginterfaces/vst/ivstunits.h>
#include <pluginterfaces/vst/ivstmidicontrollers.h>
#include <public.sdk/source/vst/hosting/eventlist.h>
#include <public.sdk/source/vst/hosting/parameterchanges.h>
#include <base/source/fstring.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace mixmind::adapters::tracktion {

// ============================================================================
// PluginState Implementation
// ============================================================================

core::Result<std::string> PluginState::toJson() const {
    try {
        nlohmann::json j;
        j["pluginUID"] = pluginUID;
        j["pluginName"] = pluginName;
        j["bypassState"] = bypassState;
        
        // Encode binary state as base64
        if (!processorState.empty()) {
            j["processorState"] = nlohmann::json::binary_t(processorState);
        }
        if (!controllerState.empty()) {
            j["controllerState"] = nlohmann::json::binary_t(controllerState);
        }
        
        // Store parameter values
        nlohmann::json params;
        for (const auto& [paramId, value] : parameterValues) {
            params[std::to_string(paramId)] = value;
        }
        j["parameters"] = params;
        
        return core::Result<std::string>::success(j.dump());
        
    } catch (const std::exception& e) {
        return core::Result<std::string>::error(
            core::ErrorCode::Unknown,
            core::ErrorCategory::general(),
            std::string("Failed to serialize plugin state: ") + e.what()
        );
    }
}

core::Result<PluginState> PluginState::fromJson(const std::string& json) {
    try {
        PluginState state;
        auto j = nlohmann::json::parse(json);
        
        state.pluginUID = j.value("pluginUID", "");
        state.pluginName = j.value("pluginName", "");
        state.bypassState = j.value("bypassState", false);
        
        // Decode binary state from base64
        if (j.contains("processorState") && j["processorState"].is_binary()) {
            auto binary = j["processorState"].get_binary();
            state.processorState.assign(binary.begin(), binary.end());
        }
        
        if (j.contains("controllerState") && j["controllerState"].is_binary()) {
            auto binary = j["controllerState"].get_binary();
            state.controllerState.assign(binary.begin(), binary.end());
        }
        
        // Restore parameter values
        if (j.contains("parameters") && j["parameters"].is_object()) {
            for (const auto& [key, value] : j["parameters"].items()) {
                try {
                    Steinberg::Vst::ParamID paramId = std::stoull(key);
                    state.parameterValues[paramId] = value.get<double>();
                } catch (...) {
                    // Skip invalid parameter entries
                }
            }
        }
        
        return core::Result<PluginState>::success(std::move(state));
        
    } catch (const std::exception& e) {
        return core::Result<PluginState>::error(
            core::ErrorCode::Unknown,
            core::ErrorCategory::general(),
            std::string("Failed to deserialize plugin state: ") + e.what()
        );
    }
}

// ============================================================================
// TEPluginInstance Implementation
// ============================================================================

TEPluginInstance::TEPluginInstance(const VST3PluginInfo& pluginInfo, 
                                   VST::Hosting::Module::Ptr module,
                                   Steinberg::IPtr<Steinberg::Vst::IComponent> component,
                                   Steinberg::IPtr<Steinberg::Vst::IEditController> controller)
    : pluginInfo_(pluginInfo), module_(module), component_(component), controller_(controller)
{
    // Generate unique instance ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    std::ostringstream oss;
    oss << std::hex << dis(gen);
    instanceId_ = pluginInfo.name + "_" + oss.str();
    
    // Get audio processor interface
    if (component_) {
        component_->queryInterface(Steinberg::Vst::IAudioProcessor::iid, (void**)&processor_);
    }
    
    // Initialize parameter information
    initializeParameters();
}

TEPluginInstance::~TEPluginInstance() {
    if (isActive_) {
        deactivate();
    }
    
    if (editorView_) {
        closeEditor();
    }
}

core::VoidResult TEPluginInstance::initialize(double sampleRate, int32_t maxBlockSize) {
    if (!component_ || !processor_) {
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Plugin component or processor not available"
        );
    }
    
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    
    // Setup process setup
    Steinberg::Vst::ProcessSetup processSetup;
    processSetup.processMode = Steinberg::Vst::ProcessModes::kRealtime;
    processSetup.symbolicSampleSize = Steinberg::Vst::kSample32;
    processSetup.maxSamplesPerBlock = maxBlockSize;
    processSetup.sampleRate = sampleRate;
    
    Steinberg::tresult result = processor_->setupProcessing(processSetup);
    if (result != Steinberg::kResultOk) {
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Failed to setup plugin processing"
        );
    }
    
    // Setup audio buses
    auto setupResult = setupAudioBuses();
    if (!setupResult) {
        return setupResult;
    }
    
    // Initialize process context
    processContext_.state = Steinberg::Vst::ProcessContext::kPlaying;
    processContext_.sampleRate = sampleRate;
    processContext_.projectTimeSamples = 0;
    processContext_.systemTime = 0;
    processContext_.continousTimeSamples = 0;
    processContext_.projectTimeMusic = 0.0;
    processContext_.barPositionMusic = 0.0;
    processContext_.cycleStartMusic = 0.0;
    processContext_.cycleEndMusic = 0.0;
    processContext_.tempo = 120.0;
    processContext_.timeSigNumerator = 4;
    processContext_.timeSigDenominator = 4;
    processContext_.chord = nullptr;
    processContext_.frameRate = {};
    processContext_.samplesToNextClock = 0;
    
    return core::VoidResult::success();
}

core::VoidResult TEPluginInstance::activate() {
    if (!component_ || !processor_) {
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Plugin component or processor not available"
        );
    }
    
    if (isActive_) {
        return core::VoidResult::success();
    }
    
    // Activate component
    Steinberg::tresult result = component_->setActive(true);
    if (result != Steinberg::kResultOk) {
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Failed to activate plugin component"
        );
    }
    
    // Set processing active
    result = processor_->setProcessing(true);
    if (result != Steinberg::kResultOk) {
        component_->setActive(false);
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Failed to activate plugin processing"
        );
    }
    
    isActive_ = true;
    return core::VoidResult::success();
}

core::VoidResult TEPluginInstance::deactivate() {
    if (!isActive_) {
        return core::VoidResult::success();
    }
    
    if (processor_) {
        processor_->setProcessing(false);
    }
    
    if (component_) {
        component_->setActive(false);
    }
    
    isActive_ = false;
    return core::VoidResult::success();
}

core::VoidResult TEPluginInstance::setBypassed(bool bypassed) {
    isBypassed_ = bypassed;
    
    // Set bypass parameter if available
    for (const auto& [paramId, paramInfo] : parameters_) {
        if (paramInfo.isBypassParameter) {
            return setParameter(paramId, bypassed ? 1.0 : 0.0);
        }
    }
    
    return core::VoidResult::success();
}

core::VoidResult TEPluginInstance::processAudio(Steinberg::Vst::ProcessData& data) {
    if (!isActive_ || !processor_) {
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Plugin not active or processor unavailable"
        );
    }
    
    if (isBypassed_) {
        // Simple bypass - copy inputs to outputs
        for (int32_t channel = 0; channel < data.numOutputs && channel < data.numInputs; ++channel) {
            if (data.outputs && data.inputs) {
                std::memcpy(data.outputs[0].channelBuffers32[channel], 
                           data.inputs[0].channelBuffers32[channel], 
                           data.numSamples * sizeof(float));
            }
        }
        return core::VoidResult::success();
    }
    
    // Set process context
    data.processContext = &processContext_;
    
    // Update process context timing
    processContext_.projectTimeSamples += data.numSamples;
    processContext_.continousTimeSamples += data.numSamples;
    processContext_.systemTime = std::chrono::steady_clock::now().time_since_epoch().count();
    
    // Process audio through plugin
    Steinberg::tresult result = processor_->process(data);
    if (result != Steinberg::kResultOk) {
        return core::VoidResult::error(
            core::ErrorCode::AudioDeviceError,
            core::ErrorCategory::audio(),
            "Plugin audio processing failed"
        );
    }
    
    // Handle parameter changes from plugin
    if (data.outputParameterChanges) {
        handleParameterChanges(data.outputParameterChanges);
    }
    
    return core::VoidResult::success();
}

core::VoidResult TEPluginInstance::setProcessPrecision(Steinberg::Vst::ProcessPrecision precision) {
    if (!processor_) {
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Processor not available"
        );
    }
    
    Steinberg::tresult result = processor_->setProcessPrecision(precision);
    if (result != Steinberg::kResultOk) {
        return core::VoidResult::error(
            core::ErrorCode::NotSupported,
            core::ErrorCategory::plugin(),
            "Plugin does not support requested processing precision"
        );
    }
    
    return core::VoidResult::success();
}

std::vector<PluginParameterInfo> TEPluginInstance::getAllParameters() const {
    std::lock_guard<std::mutex> lock(parameterMutex_);
    
    std::vector<PluginParameterInfo> params;
    params.reserve(parameters_.size());
    
    for (const auto& [paramId, paramInfo] : parameters_) {
        params.push_back(paramInfo);
    }
    
    return params;
}

std::optional<PluginParameterInfo> TEPluginInstance::getParameter(Steinberg::Vst::ParamID paramId) const {
    std::lock_guard<std::mutex> lock(parameterMutex_);
    
    auto it = parameters_.find(paramId);
    if (it != parameters_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

core::VoidResult TEPluginInstance::setParameter(Steinberg::Vst::ParamID paramId, double normalizedValue) {
    if (!controller_) {
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Controller not available"
        );
    }
    
    // Clamp value to valid range
    normalizedValue = std::clamp(normalizedValue, 0.0, 1.0);
    
    // Set parameter in controller
    Steinberg::tresult result = controller_->setParamNormalized(paramId, normalizedValue);
    if (result != Steinberg::kResultOk) {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::plugin(),
            "Failed to set plugin parameter"
        );
    }
    
    // Update local parameter cache
    {
        std::lock_guard<std::mutex> lock(parameterMutex_);
        auto it = parameters_.find(paramId);
        if (it != parameters_.end()) {
            it->second.currentValue = normalizedValue;
        }
    }
    
    // Notify parameter change callback
    if (paramChangeCallback_) {
        paramChangeCallback_(paramId, normalizedValue);
    }
    
    return core::VoidResult::success();
}

core::Result<double> TEPluginInstance::getParameter(Steinberg::Vst::ParamID paramId) const {
    if (!controller_) {
        return core::Result<double>::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Controller not available"
        );
    }
    
    double value = controller_->getParamNormalized(paramId);
    
    // Update local cache
    {
        std::lock_guard<std::mutex> lock(parameterMutex_);
        auto it = parameters_.find(paramId);
        if (it != parameters_.end()) {
            it->second.currentValue = value;
        }
    }
    
    return core::Result<double>::success(value);
}

core::Result<std::string> TEPluginInstance::getParameterStringValue(Steinberg::Vst::ParamID paramId, double normalizedValue) const {
    if (!controller_) {
        return core::Result<std::string>::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Controller not available"
        );
    }
    
    Steinberg::Vst::String128 stringValue;
    Steinberg::tresult result = controller_->getParamStringByValue(paramId, normalizedValue, stringValue);
    
    if (result != Steinberg::kResultOk) {
        // Fallback to numeric representation
        auto paramInfo = getParameter(paramId);
        if (paramInfo) {
            double realValue = normalizedValue * (paramInfo->maxValue - paramInfo->minValue) + paramInfo->minValue;
            return core::Result<std::string>::success(std::to_string(realValue) + " " + paramInfo->units);
        } else {
            return core::Result<std::string>::success(std::to_string(normalizedValue));
        }
    }
    
    // Convert Steinberg string to std::string
    std::string result_str;
    result_str.reserve(128);
    for (int i = 0; i < 128 && stringValue[i] != 0; ++i) {
        result_str.push_back(static_cast<char>(stringValue[i]));
    }
    
    return core::Result<std::string>::success(result_str);
}

core::Result<double> TEPluginInstance::getParameterNormalizedValue(Steinberg::Vst::ParamID paramId, const std::string& stringValue) const {
    if (!controller_) {
        return core::Result<double>::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Controller not available"
        );
    }
    
    // Convert string to Steinberg format
    Steinberg::Vst::String128 vstString;
    std::fill(vstString, vstString + 128, 0);
    
    size_t copyLength = std::min(stringValue.length(), static_cast<size_t>(127));
    for (size_t i = 0; i < copyLength; ++i) {
        vstString[i] = stringValue[i];
    }
    
    Steinberg::Vst::ParamValue normalizedValue;
    Steinberg::tresult result = controller_->getParamValueByString(paramId, vstString, normalizedValue);
    
    if (result != Steinberg::kResultOk) {
        // Try to parse as numeric value
        try {
            double value = std::stod(stringValue);
            auto paramInfo = getParameter(paramId);
            if (paramInfo) {
                // Convert real value to normalized
                double normalized = (value - paramInfo->minValue) / (paramInfo->maxValue - paramInfo->minValue);
                return core::Result<double>::success(std::clamp(normalized, 0.0, 1.0));
            } else {
                return core::Result<double>::success(std::clamp(value, 0.0, 1.0));
            }
        } catch (...) {
            return core::Result<double>::error(
                core::ErrorCode::InvalidParameter,
                core::ErrorCategory::plugin(),
                "Cannot convert string to parameter value"
            );
        }
    }
    
    return core::Result<double>::success(normalizedValue);
}

core::Result<PluginState> TEPluginInstance::getState() const {
    PluginState state;
    state.pluginUID = pluginInfo_.uid;
    state.pluginName = pluginInfo_.name;
    state.bypassState = isBypassed_;
    
    // Get processor state
    if (component_) {
        Steinberg::IBStream* stream = nullptr;
        // Note: In real implementation, you'd create a memory stream
        // For now, we'll skip the binary state implementation
    }
    
    // Get controller state
    if (controller_) {
        // Similar implementation for controller state
    }
    
    // Get current parameter values
    {
        std::lock_guard<std::mutex> lock(parameterMutex_);
        for (const auto& [paramId, paramInfo] : parameters_) {
            state.parameterValues[paramId] = paramInfo.currentValue;
        }
    }
    
    return core::Result<PluginState>::success(std::move(state));
}

core::VoidResult TEPluginInstance::setState(const PluginState& state) {
    if (state.pluginUID != pluginInfo_.uid) {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::plugin(),
            "Plugin state UID mismatch"
        );
    }
    
    // Restore bypass state
    setBypassed(state.bypassState);
    
    // Restore parameter values
    for (const auto& [paramId, value] : state.parameterValues) {
        setParameter(paramId, value);
    }
    
    // Restore binary states would be implemented here with proper memory streams
    
    return core::VoidResult::success();
}

std::vector<std::string> TEPluginInstance::getPresetList() const {
    std::vector<std::string> presets;
    
    if (!controller_) {
        return presets;
    }
    
    // Query for unit info (presets)
    Steinberg::IPtr<Steinberg::Vst::IUnitInfo> unitInfo;
    if (controller_->queryInterface(Steinberg::Vst::IUnitInfo::iid, (void**)&unitInfo) == Steinberg::kResultOk) {
        int32_t programListCount = unitInfo->getProgramListCount();
        
        for (int32_t i = 0; i < programListCount; ++i) {
            Steinberg::Vst::ProgramListInfo listInfo;
            if (unitInfo->getProgramListInfo(i, listInfo) == Steinberg::kResultOk) {
                for (int32_t j = 0; j < listInfo.programCount; ++j) {
                    Steinberg::Vst::String128 programName;
                    if (unitInfo->getProgramName(listInfo.id, j, programName) == Steinberg::kResultOk) {
                        std::string name;
                        for (int k = 0; k < 128 && programName[k] != 0; ++k) {
                            name.push_back(static_cast<char>(programName[k]));
                        }
                        presets.push_back(name);
                    }
                }
            }
        }
    }
    
    return presets;
}

core::VoidResult TEPluginInstance::loadPreset(const std::string& presetName) {
    // Implementation would query unit info and set the appropriate program
    // This is a simplified placeholder
    return core::VoidResult::success();
}

bool TEPluginInstance::hasEditor() const {
    return pluginInfo_.hasEditor && controller_;
}

core::Result<void*> TEPluginInstance::createEditor(void* parentWindow) {
    if (!hasEditor()) {
        return core::Result<void*>::error(
            core::ErrorCode::NotSupported,
            core::ErrorCategory::plugin(),
            "Plugin does not have an editor"
        );
    }
    
    // Plugin editor creation would be implemented here
    // This requires platform-specific window handling
    return core::Result<void*>::error(
        core::ErrorCode::NotSupported,
        core::ErrorCategory::plugin(),
        "Editor creation not implemented yet"
    );
}

core::VoidResult TEPluginInstance::closeEditor() {
    if (editorView_) {
        // Close editor implementation
        editorView_ = nullptr;
    }
    return core::VoidResult::success();
}

core::Result<core::Size> TEPluginInstance::getEditorSize() const {
    if (!hasEditor()) {
        return core::Result<core::Size>::error(
            core::ErrorCode::NotSupported,
            core::ErrorCategory::plugin(),
            "Plugin does not have an editor"
        );
    }
    
    // Return default size for now
    return core::Result<core::Size>::success({800, 600});
}

void TEPluginInstance::setParameterChangeCallback(ParamChangeCallback callback) {
    paramChangeCallback_ = callback;
}

void TEPluginInstance::setStateChangeCallback(StateChangeCallback callback) {
    stateChangeCallback_ = callback;
}

core::VoidResult TEPluginInstance::sendMidiEvent(const core::MidiEvent& midiEvent) {
    // MIDI event implementation would go here
    return core::VoidResult::success();
}

core::VoidResult TEPluginInstance::processMidiEvents(const std::vector<core::MidiEvent>& events) {
    // MIDI events processing would go here
    return core::VoidResult::success();
}

void TEPluginInstance::initializeParameters() {
    if (!controller_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(parameterMutex_);
    
    int32_t paramCount = controller_->getParameterCount();
    for (int32_t i = 0; i < paramCount; ++i) {
        Steinberg::Vst::ParameterInfo paramInfo;
        if (controller_->getParameterInfo(i, paramInfo) == Steinberg::kResultOk) {
            PluginParameterInfo info;
            info.id = paramInfo.id;
            
            // Convert Steinberg strings to std::string
            for (int j = 0; j < 128 && paramInfo.title[j] != 0; ++j) {
                info.title.push_back(static_cast<char>(paramInfo.title[j]));
            }
            for (int j = 0; j < 128 && paramInfo.shortTitle[j] != 0; ++j) {
                info.shortTitle.push_back(static_cast<char>(paramInfo.shortTitle[j]));
            }
            for (int j = 0; j < 128 && paramInfo.units[j] != 0; ++j) {
                info.units.push_back(static_cast<char>(paramInfo.units[j]));
            }
            
            info.defaultValue = paramInfo.defaultNormalizedValue;
            info.currentValue = controller_->getParamNormalized(paramInfo.id);
            info.stepCount = paramInfo.stepCount;
            info.flags = paramInfo.flags;
            info.isAutomatable = (paramInfo.flags & Steinberg::Vst::ParameterInfo::kCanAutomate) != 0;
            info.isBypassParameter = (paramInfo.flags & Steinberg::Vst::ParameterInfo::kIsBypass) != 0;
            
            parameters_[paramInfo.id] = info;
        }
    }
}

core::VoidResult TEPluginInstance::setupAudioBuses() {
    if (!component_) {
        return core::VoidResult::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Component not available"
        );
    }
    
    // Activate audio buses
    int32_t inputBusCount = component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
    int32_t outputBusCount = component_->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
    
    // Activate main buses
    if (inputBusCount > 0) {
        component_->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, 0, true);
    }
    if (outputBusCount > 0) {
        component_->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, 0, true);
    }
    
    return core::VoidResult::success();
}

void TEPluginInstance::handleParameterChanges(Steinberg::Vst::IParameterChanges* paramChanges) {
    if (!paramChanges) {
        return;
    }
    
    int32_t paramChangeCount = paramChanges->getParameterCount();
    for (int32_t i = 0; i < paramChangeCount; ++i) {
        Steinberg::Vst::IParamValueQueue* queue = paramChanges->getParameterData(i);
        if (!queue) {
            continue;
        }
        
        Steinberg::Vst::ParamID paramId = queue->getParameterId();
        int32_t pointCount = queue->getPointCount();
        
        if (pointCount > 0) {
            int32_t sampleOffset;
            Steinberg::Vst::ParamValue value;
            if (queue->getPoint(pointCount - 1, sampleOffset, value) == Steinberg::kResultOk) {
                notifyParameterChange(paramId, value);
            }
        }
    }
}

void TEPluginInstance::notifyParameterChange(Steinberg::Vst::ParamID paramId, double value) {
    // Update local cache
    {
        std::lock_guard<std::mutex> lock(parameterMutex_);
        auto it = parameters_.find(paramId);
        if (it != parameters_.end()) {
            it->second.currentValue = value;
        }
    }
    
    // Notify callback
    if (paramChangeCallback_) {
        paramChangeCallback_(paramId, value);
    }
}

// ============================================================================
// TEPluginAdapter Implementation
// ============================================================================

TEPluginAdapter::TEPluginAdapter() 
    : scanner_(std::make_unique<TEVSTScanner>())
{
}

TEPluginAdapter::~TEPluginAdapter() {
    shutdown();
}

core::VoidResult TEPluginAdapter::initialize(double sampleRate, int32_t maxBlockSize) {
    if (isInitialized_) {
        return core::VoidResult::success();
    }
    
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    isInitialized_ = true;
    
    return core::VoidResult::success();
}

core::VoidResult TEPluginAdapter::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
    
    // Deactivate and unload all plugins
    for (auto& [instanceId, plugin] : loadedPlugins_) {
        if (plugin) {
            plugin->deactivate();
        }
    }
    
    loadedPlugins_.clear();
    trackPlugins_.clear();
    isInitialized_ = false;
    
    return core::VoidResult::success();
}

core::AsyncResult<core::VoidResult> TEPluginAdapter::scanPlugins() {
    return scanner_->scanAllDirectories();
}

std::vector<VST3PluginInfo> TEPluginAdapter::getAvailablePlugins() const {
    return scanner_->getAllPlugins();
}

std::vector<VST3PluginInfo> TEPluginAdapter::getPluginsByCategory(const std::string& category) const {
    return scanner_->getPluginsByCategory(category);
}

core::AsyncResult<core::Result<TEPluginInstancePtr>> TEPluginAdapter::loadPlugin(const std::string& pluginUID) {
    return core::getGlobalThreadPool().executeAsync<core::Result<TEPluginInstancePtr>>(
        [this, pluginUID]() -> core::Result<TEPluginInstancePtr> {
            
            // Find plugin info
            auto pluginInfoOpt = scanner_->findPluginByUID(pluginUID);
            if (!pluginInfoOpt) {
                return core::Result<TEPluginInstancePtr>::error(
                    core::ErrorCode::PluginNotFound,
                    core::ErrorCategory::plugin(),
                    "Plugin with UID '" + pluginUID + "' not found"
                );
            }
            
            const auto& pluginInfo = *pluginInfoOpt;
            
            // Load VST3 module
            auto moduleResult = loadVST3Module(pluginInfo.filePath);
            if (!moduleResult) {
                return core::Result<TEPluginInstancePtr>::error(
                    moduleResult.error().code,
                    moduleResult.error().category,
                    moduleResult.error().message
                );
            }
            
            auto module = moduleResult.value();
            
            // Create plugin components
            auto componentsResult = createPluginComponents(module, pluginUID);
            if (!componentsResult) {
                return core::Result<TEPluginInstancePtr>::error(
                    componentsResult.error().code,
                    componentsResult.error().category,
                    componentsResult.error().message
                );
            }
            
            auto [component, controller] = componentsResult.value();
            
            // Create plugin instance
            auto pluginInstance = std::make_unique<TEPluginInstance>(
                pluginInfo, module, component, controller
            );
            
            // Initialize plugin
            auto initResult = pluginInstance->initialize(sampleRate_, maxBlockSize_);
            if (!initResult) {
                return core::Result<TEPluginInstancePtr>::error(
                    initResult.error().code,
                    initResult.error().category,
                    initResult.error().message
                );
            }
            
            // Store plugin instance
            std::string instanceId = pluginInstance->getInstanceId();
            {
                std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
                loadedPlugins_[instanceId] = std::move(pluginInstance);
            }
            
            return core::Result<TEPluginInstancePtr>::success(std::move(loadedPlugins_[instanceId]));
        },
        "Loading VST3 plugin: " + pluginUID
    );
}

core::VoidResult TEPluginAdapter::unloadPlugin(const std::string& instanceId) {
    std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
    
    auto it = loadedPlugins_.find(instanceId);
    if (it == loadedPlugins_.end()) {
        return core::VoidResult::error(
            core::ErrorCode::PluginNotFound,
            core::ErrorCategory::plugin(),
            "Plugin instance not found: " + instanceId
        );
    }
    
    // Remove from all tracks
    for (auto& [trackId, plugins] : trackPlugins_) {
        plugins.erase(std::remove(plugins.begin(), plugins.end(), instanceId), plugins.end());
    }
    
    // Deactivate and remove plugin
    if (it->second) {
        it->second->deactivate();
    }
    loadedPlugins_.erase(it);
    
    return core::VoidResult::success();
}

TEPluginInstance* TEPluginAdapter::getPluginInstance(const std::string& instanceId) const {
    std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
    
    auto it = loadedPlugins_.find(instanceId);
    if (it != loadedPlugins_.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

core::AsyncResult<core::Result<std::string>> TEPluginAdapter::insertPluginOnTrack(
    const std::string& trackId, 
    const std::string& pluginUID, 
    int insertIndex) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<std::string>>(
        [this, trackId, pluginUID, insertIndex]() -> core::Result<std::string> {
            
            // Load plugin
            auto loadResult = loadPlugin(pluginUID);
            auto pluginResult = loadResult.get();
            
            if (!pluginResult) {
                return core::Result<std::string>::error(
                    pluginResult.error().code,
                    pluginResult.error().category,
                    pluginResult.error().message
                );
            }
            
            auto plugin = pluginResult.value();
            std::string instanceId = plugin->getInstanceId();
            
            // Activate plugin
            auto activateResult = plugin->activate();
            if (!activateResult) {
                unloadPlugin(instanceId);
                return core::Result<std::string>::error(
                    activateResult.error().code,
                    activateResult.error().category,
                    activateResult.error().message
                );
            }
            
            // Add to track plugin chain
            {
                std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
                auto& plugins = trackPlugins_[trackId];
                
                if (insertIndex < 0 || insertIndex >= static_cast<int>(plugins.size())) {
                    plugins.push_back(instanceId);
                } else {
                    plugins.insert(plugins.begin() + insertIndex, instanceId);
                }
            }
            
            return core::Result<std::string>::success(instanceId);
        },
        "Inserting plugin on track: " + trackId
    );
}

core::VoidResult TEPluginAdapter::removePluginFromTrack(const std::string& trackId, const std::string& instanceId) {
    std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
    
    auto trackIt = trackPlugins_.find(trackId);
    if (trackIt == trackPlugins_.end()) {
        return core::VoidResult::error(
            core::ErrorCode::TrackNotFound,
            core::ErrorCategory::session(),
            "Track not found: " + trackId
        );
    }
    
    auto& plugins = trackIt->second;
    auto pluginIt = std::find(plugins.begin(), plugins.end(), instanceId);
    if (pluginIt != plugins.end()) {
        plugins.erase(pluginIt);
    }
    
    // Optionally unload plugin if not used elsewhere
    unloadPlugin(instanceId);
    
    return core::VoidResult::success();
}

core::VoidResult TEPluginAdapter::movePluginOnTrack(const std::string& trackId, const std::string& instanceId, int newIndex) {
    std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
    
    auto trackIt = trackPlugins_.find(trackId);
    if (trackIt == trackPlugins_.end()) {
        return core::VoidResult::error(
            core::ErrorCode::TrackNotFound,
            core::ErrorCategory::session(),
            "Track not found: " + trackId
        );
    }
    
    auto& plugins = trackIt->second;
    auto pluginIt = std::find(plugins.begin(), plugins.end(), instanceId);
    if (pluginIt == plugins.end()) {
        return core::VoidResult::error(
            core::ErrorCode::PluginNotFound,
            core::ErrorCategory::plugin(),
            "Plugin not found on track: " + instanceId
        );
    }
    
    // Remove from current position
    plugins.erase(pluginIt);
    
    // Insert at new position
    if (newIndex < 0 || newIndex >= static_cast<int>(plugins.size())) {
        plugins.push_back(instanceId);
    } else {
        plugins.insert(plugins.begin() + newIndex, instanceId);
    }
    
    return core::VoidResult::success();
}

std::vector<std::string> TEPluginAdapter::getTrackPlugins(const std::string& trackId) const {
    std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
    
    auto it = trackPlugins_.find(trackId);
    if (it != trackPlugins_.end()) {
        return it->second;
    }
    
    return {};
}

core::Result<std::unordered_map<std::string, PluginState>> TEPluginAdapter::savePluginStates() const {
    std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
    
    std::unordered_map<std::string, PluginState> states;
    
    for (const auto& [instanceId, plugin] : loadedPlugins_) {
        if (plugin) {
            auto stateResult = plugin->getState();
            if (stateResult) {
                states[instanceId] = stateResult.value();
            }
        }
    }
    
    return core::Result<std::unordered_map<std::string, PluginState>>::success(std::move(states));
}

core::AsyncResult<core::VoidResult> TEPluginAdapter::restorePluginStates(
    const std::unordered_map<std::string, PluginState>& pluginStates) {
    
    return core::getGlobalThreadPool().executeAsyncVoid(
        [this, pluginStates]() -> core::VoidResult {
            for (const auto& [instanceId, state] : pluginStates) {
                auto plugin = getPluginInstance(instanceId);
                if (plugin) {
                    plugin->setState(state);
                }
            }
            return core::VoidResult::success();
        },
        "Restoring plugin states"
    );
}

core::Result<PluginState> TEPluginAdapter::getPluginState(const std::string& instanceId) const {
    auto plugin = getPluginInstance(instanceId);
    if (!plugin) {
        return core::Result<PluginState>::error(
            core::ErrorCode::PluginNotFound,
            core::ErrorCategory::plugin(),
            "Plugin instance not found: " + instanceId
        );
    }
    
    return plugin->getState();
}

core::AsyncResult<core::VoidResult> TEPluginAdapter::setPluginState(const std::string& instanceId, const PluginState& state) {
    return core::getGlobalThreadPool().executeAsyncVoid(
        [this, instanceId, state]() -> core::VoidResult {
            auto plugin = getPluginInstance(instanceId);
            if (!plugin) {
                return core::VoidResult::error(
                    core::ErrorCode::PluginNotFound,
                    core::ErrorCategory::plugin(),
                    "Plugin instance not found: " + instanceId
                );
            }
            
            return plugin->setState(state);
        },
        "Setting plugin state: " + instanceId
    );
}

core::VoidResult TEPluginAdapter::processTrackPlugins(
    const std::string& trackId,
    float** audioInputs,
    float** audioOutputs, 
    int32_t numChannels,
    int32_t numSamples,
    const std::vector<core::MidiEvent>& midiEvents) {
    
    auto plugins = getTrackPlugins(trackId);
    if (plugins.empty()) {
        // No plugins - copy inputs to outputs
        for (int32_t channel = 0; channel < numChannels; ++channel) {
            std::memcpy(audioOutputs[channel], audioInputs[channel], numSamples * sizeof(float));
        }
        return core::VoidResult::success();
    }
    
    // Process through plugin chain
    // This is a simplified implementation - would need proper buffer management
    for (const auto& instanceId : plugins) {
        auto plugin = getPluginInstance(instanceId);
        if (!plugin || !plugin->isActive()) {
            continue;
        }
        
        // Setup process data
        Steinberg::Vst::ProcessData processData;
        processData.processMode = Steinberg::Vst::ProcessModes::kRealtime;
        processData.symbolicSampleSize = Steinberg::Vst::kSample32;
        processData.numSamples = numSamples;
        processData.numInputs = numChannels > 0 ? 1 : 0;
        processData.numOutputs = numChannels > 0 ? 1 : 0;
        
        // Setup audio buses (simplified)
        Steinberg::Vst::AudioBusBuffers inputBus;
        Steinberg::Vst::AudioBusBuffers outputBus;
        
        if (numChannels > 0) {
            inputBus.numChannels = numChannels;
            inputBus.channelBuffers32 = audioInputs;
            processData.inputs = &inputBus;
            
            outputBus.numChannels = numChannels;  
            outputBus.channelBuffers32 = audioOutputs;
            processData.outputs = &outputBus;
        }
        
        // Process MIDI events
        if (!midiEvents.empty()) {
            plugin->processMidiEvents(midiEvents);
        }
        
        // Process audio
        auto result = plugin->processAudio(processData);
        if (!result) {
            return result;
        }
        
        // Copy outputs to inputs for next plugin in chain
        if (numChannels > 0) {
            for (int32_t channel = 0; channel < numChannels; ++channel) {
                audioInputs[channel] = audioOutputs[channel];
            }
        }
    }
    
    return core::VoidResult::success();
}

core::VoidResult TEPluginAdapter::setPluginBypassed(const std::string& instanceId, bool bypassed) {
    auto plugin = getPluginInstance(instanceId);
    if (!plugin) {
        return core::VoidResult::error(
            core::ErrorCode::PluginNotFound,
            core::ErrorCategory::plugin(),
            "Plugin instance not found: " + instanceId
        );
    }
    
    return plugin->setBypassed(bypassed);
}

core::Result<std::vector<std::string>> TEPluginAdapter::getPluginPresets(const std::string& instanceId) const {
    auto plugin = getPluginInstance(instanceId);
    if (!plugin) {
        return core::Result<std::vector<std::string>>::error(
            core::ErrorCode::PluginNotFound,
            core::ErrorCategory::plugin(),
            "Plugin instance not found: " + instanceId
        );
    }
    
    auto presets = plugin->getPresetList();
    return core::Result<std::vector<std::string>>::success(presets);
}

core::AsyncResult<core::VoidResult> TEPluginAdapter::loadPluginPreset(const std::string& instanceId, const std::string& presetName) {
    return core::getGlobalThreadPool().executeAsyncVoid(
        [this, instanceId, presetName]() -> core::VoidResult {
            auto plugin = getPluginInstance(instanceId);
            if (!plugin) {
                return core::VoidResult::error(
                    core::ErrorCode::PluginNotFound,
                    core::ErrorCategory::plugin(),
                    "Plugin instance not found: " + instanceId
                );
            }
            
            return plugin->loadPreset(presetName);
        },
        "Loading plugin preset: " + presetName
    );
}

core::VoidResult TEPluginAdapter::savePluginPreset(const std::string& instanceId, const std::string& presetName) {
    // Preset saving implementation would go here
    return core::VoidResult::success();
}

core::VoidResult TEPluginAdapter::setAutomationValue(
    const std::string& instanceId, 
    Steinberg::Vst::ParamID paramId, 
    double normalizedValue,
    int32_t sampleOffset) {
    
    auto plugin = getPluginInstance(instanceId);
    if (!plugin) {
        return core::VoidResult::error(
            core::ErrorCode::PluginNotFound,
            core::ErrorCategory::plugin(),
            "Plugin instance not found: " + instanceId
        );
    }
    
    return plugin->setParameter(paramId, normalizedValue);
}

core::Result<std::vector<PluginParameterInfo>> TEPluginAdapter::getAutomatableParameters(const std::string& instanceId) const {
    auto plugin = getPluginInstance(instanceId);
    if (!plugin) {
        return core::Result<std::vector<PluginParameterInfo>>::error(
            core::ErrorCode::PluginNotFound,
            core::ErrorCategory::plugin(),
            "Plugin instance not found: " + instanceId
        );
    }
    
    auto allParams = plugin->getAllParameters();
    std::vector<PluginParameterInfo> automatableParams;
    
    std::copy_if(allParams.begin(), allParams.end(), 
                 std::back_inserter(automatableParams),
                 [](const PluginParameterInfo& param) { return param.isAutomatable; });
    
    return core::Result<std::vector<PluginParameterInfo>>::success(automatableParams);
}

TEPluginAdapter::PluginStats TEPluginAdapter::getPluginStats() const {
    std::lock_guard<std::recursive_mutex> lock(pluginMutex_);
    
    PluginStats stats;
    stats.totalPluginsLoaded = static_cast<int>(loadedPlugins_.size());
    
    for (const auto& [instanceId, plugin] : loadedPlugins_) {
        if (plugin) {
            if (plugin->isActive()) {
                stats.activePlugins++;
            }
            if (plugin->isBypassed()) {
                stats.bypassedPlugins++;
            }
        }
    }
    
    stats.totalCpuUsage = totalCpuUsage_.load();
    stats.totalMemoryUsage = totalMemoryUsage_.load();
    
    return stats;
}

core::Result<std::string> TEPluginAdapter::getPluginDiagnosticInfo(const std::string& instanceId) const {
    auto plugin = getPluginInstance(instanceId);
    if (!plugin) {
        return core::Result<std::string>::error(
            core::ErrorCode::PluginNotFound,
            core::ErrorCategory::plugin(),
            "Plugin instance not found: " + instanceId
        );
    }
    
    std::ostringstream info;
    const auto& pluginInfo = plugin->getPluginInfo();
    
    info << "Plugin: " << pluginInfo.name << " (" << pluginInfo.vendor << ")\n";
    info << "Version: " << pluginInfo.version << "\n";
    info << "UID: " << pluginInfo.uid << "\n";
    info << "File: " << pluginInfo.filePath << "\n";
    info << "Active: " << (plugin->isActive() ? "Yes" : "No") << "\n";
    info << "Bypassed: " << (plugin->isBypassed() ? "Yes" : "No") << "\n";
    info << "Has Editor: " << (plugin->hasEditor() ? "Yes" : "No") << "\n";
    info << "Audio I/O: " << pluginInfo.numAudioInputs << " -> " << pluginInfo.numAudioOutputs << "\n";
    info << "MIDI I/O: " << pluginInfo.numMidiInputs << " -> " << pluginInfo.numMidiOutputs << "\n";
    
    auto params = plugin->getAllParameters();
    info << "Parameters: " << params.size() << "\n";
    
    return core::Result<std::string>::success(info.str());
}

std::string TEPluginAdapter::generateInstanceId() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    std::ostringstream oss;
    oss << "plugin_" << std::hex << dis(gen);
    return oss.str();
}

core::Result<VST::Hosting::Module::Ptr> TEPluginAdapter::loadVST3Module(const std::string& pluginPath) const {
    try {
        auto module = VST::Hosting::Module::create(pluginPath);
        if (!module) {
            return core::Result<VST::Hosting::Module::Ptr>::error(
                core::ErrorCode::PluginLoadFailed,
                core::ErrorCategory::plugin(),
                "Failed to load VST3 module: " + pluginPath
            );
        }
        
        return core::Result<VST::Hosting::Module::Ptr>::success(module);
        
    } catch (const std::exception& e) {
        return core::Result<VST::Hosting::Module::Ptr>::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            std::string("Exception loading VST3 module: ") + e.what()
        );
    }
}

core::Result<std::pair<Steinberg::IPtr<Steinberg::Vst::IComponent>, 
                      Steinberg::IPtr<Steinberg::Vst::IEditController>>> 
TEPluginAdapter::createPluginComponents(VST::Hosting::Module::Ptr module, const std::string& pluginUID) const {
    
    if (!module) {
        return core::Result<std::pair<Steinberg::IPtr<Steinberg::Vst::IComponent>, 
                                     Steinberg::IPtr<Steinberg::Vst::IEditController>>>::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Module is null"
        );
    }
    
    auto factory = module->getFactory();
    if (!factory) {
        return core::Result<std::pair<Steinberg::IPtr<Steinberg::Vst::IComponent>, 
                                     Steinberg::IPtr<Steinberg::Vst::IEditController>>>::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Failed to get plugin factory"
        );
    }
    
    // Find plugin by UID and create component
    // This is a simplified implementation
    Steinberg::IPtr<Steinberg::Vst::IComponent> component;
    Steinberg::IPtr<Steinberg::Vst::IEditController> controller;
    
    // In real implementation, you'd iterate through factory classes and find matching UID
    // For now, create first available plugin
    VST::Hosting::PluginFactory plugFactory(factory);
    
    for (auto& classInfo : plugFactory.classInfos()) {
        if (classInfo.category() == kVstAudioEffectClass) {
            component = plugFactory.createInstance<Steinberg::Vst::IComponent>(classInfo.ID());
            if (component) {
                // Try to get controller from component
                if (component->queryInterface(Steinberg::Vst::IEditController::iid, (void**)&controller) != Steinberg::kResultOk) {
                    // Create separate controller if needed
                    controller = plugFactory.createInstance<Steinberg::Vst::IEditController>(classInfo.ID());
                }
                break;
            }
        }
    }
    
    if (!component) {
        return core::Result<std::pair<Steinberg::IPtr<Steinberg::Vst::IComponent>, 
                                     Steinberg::IPtr<Steinberg::Vst::IEditController>>>::error(
            core::ErrorCode::PluginLoadFailed,
            core::ErrorCategory::plugin(),
            "Failed to create plugin component"
        );
    }
    
    return core::Result<std::pair<Steinberg::IPtr<Steinberg::Vst::IComponent>, 
                                 Steinberg::IPtr<Steinberg::Vst::IEditController>>>::success(
        std::make_pair(component, controller)
    );
}

core::VoidResult TEPluginAdapter::setupPluginAudioBuses(TEPluginInstance* plugin) const {
    if (!plugin) {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::plugin(),
            "Plugin instance is null"
        );
    }
    
    // Audio bus setup is handled in TEPluginInstance::setupAudioBuses()
    return core::VoidResult::success();
}

// ============================================================================
// Global Plugin Adapter Instance
// ============================================================================

TEPluginAdapter& getGlobalPluginAdapter() {
    static TEPluginAdapter instance;
    return instance;
}

} // namespace mixmind::adapters::tracktion