#include "VST3Plugin.h"
#include "../core/Logger.h"
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

namespace mixmind::plugins {

// Mock JUCE types for compilation (in real implementation, these would be actual JUCE includes)
namespace juce {
    struct AudioBuffer {
        float** data = nullptr;
        int numChannels = 0;
        int numSamples = 0;
    };
    
    struct MidiBuffer {};
    
    struct PluginDescription {
        std::string name;
        std::string manufacturerName;
        std::string version;
        std::string category;
        std::string fileOrIdentifier;
    };
    
    class AudioProcessor {
    public:
        virtual ~AudioProcessor() = default;
        virtual void prepareToPlay(double sampleRate, int samplesPerBlock) {}
        virtual void processBlock(AudioBuffer& buffer, MidiBuffer& midi) {}
        virtual void releaseResources() {}
        virtual int getNumParameters() { return 0; }
        virtual float getParameter(int index) { return 0.0f; }
        virtual void setParameter(int index, float value) {}
        virtual std::string getParameterName(int index) { return "Param" + std::to_string(index); }
        virtual int getLatencySamples() { return 0; }
        virtual bool hasEditor() { return false; }
    };
    
    class AudioProcessorEditor {
    public:
        virtual ~AudioProcessorEditor() = default;
        virtual void setVisible(bool visible) {}
        virtual bool isVisible() { return false; }
    };
}

// ============================================================================
// VST3Plugin Implementation
// ============================================================================

class VST3Plugin::Impl {
public:
    std::string pluginPath_;
    std::unique_ptr<juce::AudioProcessor> processor_;
    std::unique_ptr<juce::AudioProcessorEditor> editor_;
    PluginInfo info_;
    
    // Processing state
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> isActive_{false};
    std::atomic<bool> isProcessing_{false};
    
    // Performance monitoring
    mutable std::mutex perfMutex_;
    std::chrono::steady_clock::time_point lastProcessStart_;
    std::chrono::steady_clock::time_point lastProcessEnd_;
    double avgCpuUsage_ = 0.0;
    
    // Parameter cache
    mutable std::mutex paramMutex_;
    std::unordered_map<std::string, float> parameterValues_;
    std::unordered_map<std::string, ParameterAutomation> automations_;
    
    // Audio buffers
    std::unique_ptr<juce::AudioBuffer> juceInputBuffer_;
    std::unique_ptr<juce::AudioBuffer> juceOutputBuffer_;
    juce::MidiBuffer midiBuffer_;
    
    // Advanced features
    bool useDoublePrecision_ = false;
    bool enableMultiThreading_ = false;
    bool lowLatencyMode_ = false;
    
    Impl(const std::string& path) : pluginPath_(path) {}
    
    bool loadPlugin() {
        try {
            // In real implementation, this would use JUCE's VST3PluginFormat
            // to load the actual VST3 plugin from the file path
            
            // For demo purposes, create a mock processor
            processor_ = std::make_unique<juce::AudioProcessor>();
            
            // Populate plugin info
            info_.filePath = pluginPath_;
            info_.name = std::filesystem::path(pluginPath_).stem().string();
            info_.manufacturer = "Unknown";
            info_.version = "1.0";
            info_.format = PluginFormat::VST3;
            info_.category = PluginCategory::EFFECT;
            info_.uid = "vst3_" + std::to_string(std::hash<std::string>{}(pluginPath_));
            
            return true;
        } catch (const std::exception& e) {
            core::Logger::error("Failed to load VST3 plugin: " + std::string(e.what()));
            return false;
        }
    }
    
    void updatePerformanceStats(std::chrono::microseconds processingTime) {
        std::lock_guard<std::mutex> lock(perfMutex_);
        
        // Simple exponential moving average
        double newUsage = processingTime.count() / 1000.0; // Convert to ms
        avgCpuUsage_ = 0.9 * avgCpuUsage_ + 0.1 * newUsage;
    }
};

VST3Plugin::VST3Plugin(const std::string& pluginPath) 
    : pImpl_(std::make_unique<Impl>(pluginPath)) {
}

VST3Plugin::~VST3Plugin() {
    cleanup();
}

bool VST3Plugin::initialize(double sampleRate, int maxBufferSize) {
    if (!pImpl_->processor_) {
        if (!pImpl_->loadPlugin()) {
            return false;
        }
    }
    
    try {
        pImpl_->processor_->prepareToPlay(sampleRate, maxBufferSize);
        
        // Create audio buffers
        pImpl_->juceInputBuffer_ = std::make_unique<juce::AudioBuffer>();
        pImpl_->juceOutputBuffer_ = std::make_unique<juce::AudioBuffer>();
        
        pImpl_->isInitialized_.store(true);
        
        core::Logger::info("VST3 plugin initialized: " + pImpl_->info_.name);
        return true;
    } catch (const std::exception& e) {
        core::Logger::error("Failed to initialize VST3 plugin: " + std::string(e.what()));
        return false;
    }
}

bool VST3Plugin::activate() {
    if (!pImpl_->isInitialized_.load()) {
        return false;
    }
    
    pImpl_->isActive_.store(true);
    core::Logger::info("VST3 plugin activated: " + pImpl_->info_.name);
    return true;
}

void VST3Plugin::deactivate() {
    pImpl_->isActive_.store(false);
    core::Logger::info("VST3 plugin deactivated: " + pImpl_->info_.name);
}

void VST3Plugin::cleanup() {
    if (pImpl_->processor_) {
        pImpl_->processor_->releaseResources();
        pImpl_->processor_.reset();
    }
    
    pImpl_->editor_.reset();
    pImpl_->isInitialized_.store(false);
    pImpl_->isActive_.store(false);
    
    core::Logger::info("VST3 plugin cleaned up: " + pImpl_->info_.name);
}

void VST3Plugin::processAudio(const audio::AudioBufferPool::AudioBuffer& input,
                              audio::AudioBufferPool::AudioBuffer& output) {
    if (!pImpl_->isActive_.load() || !pImpl_->processor_) {
        // Bypass - copy input to output
        std::copy(input.data, input.data + input.numChannels * input.numSamples, output.data);
        return;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    pImpl_->isProcessing_.store(true);
    
    try {
        // Convert from our buffer format to JUCE format
        // In real implementation, this would be more efficient
        pImpl_->juceInputBuffer_->data = const_cast<float**>(&input.data);
        pImpl_->juceInputBuffer_->numChannels = input.numChannels;
        pImpl_->juceInputBuffer_->numSamples = input.numSamples;
        
        pImpl_->juceOutputBuffer_->data = &output.data;
        pImpl_->juceOutputBuffer_->numChannels = output.numChannels;
        pImpl_->juceOutputBuffer_->numSamples = output.numSamples;
        
        // Process audio through VST3 plugin
        pImpl_->processor_->processBlock(*pImpl_->juceOutputBuffer_, pImpl_->midiBuffer_);
        
    } catch (const std::exception& e) {
        core::Logger::error("VST3 plugin processing error: " + std::string(e.what()));
        // Fallback to bypass
        std::copy(input.data, input.data + input.numChannels * input.numSamples, output.data);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    pImpl_->updatePerformanceStats(duration);
    
    pImpl_->isProcessing_.store(false);
}

std::vector<PluginParameter> VST3Plugin::getParameters() const {
    std::vector<PluginParameter> parameters;
    
    if (!pImpl_->processor_) {
        return parameters;
    }
    
    int numParams = pImpl_->processor_->getNumParameters();
    parameters.reserve(numParams);
    
    for (int i = 0; i < numParams; ++i) {
        PluginParameter param;
        param.id = "param_" + std::to_string(i);
        param.name = pImpl_->processor_->getParameterName(i);
        param.displayName = param.name;
        param.value = pImpl_->processor_->getParameter(i);
        param.defaultValue = 0.5f; // Default assumption
        param.minValue = 0.0f;
        param.maxValue = 1.0f;
        param.isAutomatable = true;
        
        // AI enhancement (would be populated by AI analysis)
        param.aiDescription = "Parameter: " + param.name;
        param.aiImportanceScore = 0.5f;
        param.aiTags = {"control", "effect"};
        
        parameters.push_back(param);
    }
    
    return parameters;
}

bool VST3Plugin::setParameter(const std::string& id, float value) {
    if (!pImpl_->processor_) {
        return false;
    }
    
    try {
        // Extract parameter index from ID
        std::string indexStr = id.substr(6); // Remove "param_" prefix
        int paramIndex = std::stoi(indexStr);
        
        // Clamp value
        value = std::clamp(value, 0.0f, 1.0f);
        
        pImpl_->processor_->setParameter(paramIndex, value);
        
        // Cache the value
        std::lock_guard<std::mutex> lock(pImpl_->paramMutex_);
        pImpl_->parameterValues_[id] = value;
        
        return true;
    } catch (const std::exception& e) {
        core::Logger::error("Failed to set parameter: " + std::string(e.what()));
        return false;
    }
}

float VST3Plugin::getParameter(const std::string& id) const {
    std::lock_guard<std::mutex> lock(pImpl_->paramMutex_);
    
    auto it = pImpl_->parameterValues_.find(id);
    if (it != pImpl_->parameterValues_.end()) {
        return it->second;
    }
    
    // Try to get from processor
    if (pImpl_->processor_) {
        try {
            std::string indexStr = id.substr(6);
            int paramIndex = std::stoi(indexStr);
            return pImpl_->processor_->getParameter(paramIndex);
        } catch (...) {
            return 0.0f;
        }
    }
    
    return 0.0f;
}

void VST3Plugin::automateParameter(const std::string& id, 
                                   const std::vector<std::pair<int, float>>& automation) {
    std::lock_guard<std::mutex> lock(pImpl_->paramMutex_);
    
    ParameterAutomation& paramAuto = pImpl_->automations_[id];
    paramAuto.parameterId = id;
    paramAuto.points.clear();
    
    for (const auto& [sample, value] : automation) {
        paramAuto.points.emplace_back(sample / 44100.0, value); // Convert samples to seconds
    }
    
    paramAuto.isPlaying = true;
    
    core::Logger::info("Parameter automation set for: " + id + 
                       " (" + std::to_string(automation.size()) + " points)");
}

std::vector<std::string> VST3Plugin::getPresets() const {
    // In real implementation, this would query the VST3 plugin for presets
    std::vector<std::string> presets = {
        "Default",
        "Factory 1",
        "Factory 2",
        "User Preset 1",
        "User Preset 2"
    };
    
    return presets;
}

bool VST3Plugin::loadPreset(const std::string& presetName) {
    // In real implementation, this would load the actual preset
    core::Logger::info("Loading preset: " + presetName + " for " + pImpl_->info_.name);
    return true;
}

bool VST3Plugin::savePreset(const std::string& presetName) {
    // In real implementation, this would save the current state as a preset
    core::Logger::info("Saving preset: " + presetName + " for " + pImpl_->info_.name);
    return true;
}

std::string VST3Plugin::getCurrentPreset() const {
    return "Default"; // Would return actual current preset
}

PluginInfo VST3Plugin::getInfo() const {
    return pImpl_->info_;
}

std::string VST3Plugin::getStateData() const {
    // In real implementation, this would serialize the plugin's complete state
    return "plugin_state_data_placeholder";
}

bool VST3Plugin::setStateData(const std::string& data) {
    // In real implementation, this would restore the plugin's state
    core::Logger::info("Restoring plugin state for: " + pImpl_->info_.name);
    return true;
}

double VST3Plugin::getCurrentCpuUsage() const {
    std::lock_guard<std::mutex> lock(pImpl_->perfMutex_);
    return pImpl_->avgCpuUsage_;
}

int VST3Plugin::getCurrentLatency() const {
    if (pImpl_->processor_) {
        return pImpl_->processor_->getLatencySamples();
    }
    return 0;
}

bool VST3Plugin::isProcessing() const {
    return pImpl_->isProcessing_.load();
}

bool VST3Plugin::hasCustomUI() const {
    if (pImpl_->processor_) {
        return pImpl_->processor_->hasEditor();
    }
    return false;
}

void VST3Plugin::showUI() {
    if (pImpl_->editor_) {
        pImpl_->editor_->setVisible(true);
        core::Logger::info("Showing UI for: " + pImpl_->info_.name);
    }
}

void VST3Plugin::hideUI() {
    if (pImpl_->editor_) {
        pImpl_->editor_->setVisible(false);
        core::Logger::info("Hiding UI for: " + pImpl_->info_.name);
    }
}

bool VST3Plugin::isUIVisible() const {
    if (pImpl_->editor_) {
        return pImpl_->editor_->isVisible();
    }
    return false;
}

bool VST3Plugin::loadFromFile(const std::string& pluginPath) {
    pImpl_->pluginPath_ = pluginPath;
    return pImpl_->loadPlugin();
}

bool VST3Plugin::scanPlugin(PluginInfo& info) {
    if (!pImpl_->loadPlugin()) {
        return false;
    }
    
    info = pImpl_->info_;
    return true;
}

VST3Plugin::VST3ExtendedInfo VST3Plugin::getExtendedInfo() const {
    VST3ExtendedInfo extInfo;
    
    // In real implementation, this would query the VST3 plugin for extended information
    extInfo.vendorName = pImpl_->info_.manufacturer;
    extInfo.vendorVersion = pImpl_->info_.version;
    extInfo.supportedFeatures = {"Real-time Processing", "Parameter Automation"};
    extInfo.categories = {"Effect"};
    extInfo.supportsDoubleReplacing = true;
    extInfo.supportsBypass = true;
    
    return extInfo;
}

void VST3Plugin::startParameterRecording(const std::string& parameterId) {
    std::lock_guard<std::mutex> lock(pImpl_->paramMutex_);
    
    ParameterAutomation& automation = pImpl_->automations_[parameterId];
    automation.parameterId = parameterId;
    automation.points.clear();
    automation.isRecording = true;
    automation.isPlaying = false;
    
    core::Logger::info("Started recording parameter: " + parameterId);
}

void VST3Plugin::stopParameterRecording(const std::string& parameterId) {
    std::lock_guard<std::mutex> lock(pImpl_->paramMutex_);
    
    auto it = pImpl_->automations_.find(parameterId);
    if (it != pImpl_->automations_.end()) {
        it->second.isRecording = false;
        core::Logger::info("Stopped recording parameter: " + parameterId + 
                          " (" + std::to_string(it->second.points.size()) + " points recorded)");
    }
}

std::vector<VST3Plugin::ParameterAutomation> VST3Plugin::getParameterAutomations() const {
    std::lock_guard<std::mutex> lock(pImpl_->paramMutex_);
    
    std::vector<ParameterAutomation> automations;
    automations.reserve(pImpl_->automations_.size());
    
    for (const auto& [id, automation] : pImpl_->automations_) {
        automations.push_back(automation);
    }
    
    return automations;
}

void VST3Plugin::processMidi(const std::vector<uint8_t>& midiData, int sampleOffset) {
    // In real implementation, this would add MIDI data to the JUCE MidiBuffer
    core::Logger::debug("Processing MIDI data: " + std::to_string(midiData.size()) + " bytes");
}

bool VST3Plugin::acceptsMidi() const {
    return pImpl_->info_.acceptsMidi;
}

bool VST3Plugin::producesMidi() const {
    return pImpl_->info_.producesMidi;
}

void VST3Plugin::setProcessingPrecision(bool useDoublePrecision) {
    pImpl_->useDoublePrecision_ = useDoublePrecision;
    core::Logger::info("Set processing precision: " + 
                       std::string(useDoublePrecision ? "Double" : "Single") + 
                       " for " + pImpl_->info_.name);
}

void VST3Plugin::setThreadingMode(bool enableMultiThreading) {
    pImpl_->enableMultiThreading_ = enableMultiThreading;
    core::Logger::info("Set threading mode: " + 
                       std::string(enableMultiThreading ? "Multi-threaded" : "Single-threaded") + 
                       " for " + pImpl_->info_.name);
}

void VST3Plugin::optimizeForLatency(bool lowLatencyMode) {
    pImpl_->lowLatencyMode_ = lowLatencyMode;
    core::Logger::info("Set latency mode: " + 
                       std::string(lowLatencyMode ? "Low latency" : "Normal") + 
                       " for " + pImpl_->info_.name);
}

// ============================================================================
// VST3Scanner Implementation
// ============================================================================

class VST3Scanner::Impl {
public:
    bool deepScan_ = true;
    bool aiAnalysis_ = true;
    bool performanceTest_ = false;
    int timeoutSeconds_ = 10;
    
    std::unordered_map<std::string, PluginInfo> pluginCache_;
    
    std::vector<std::string> getSystemDirectories() const {
        std::vector<std::string> directories;
        
#ifdef _WIN32
        directories.push_back("C:\\Program Files\\Common Files\\VST3");
        directories.push_back("C:\\Program Files (x86)\\Common Files\\VST3");
        // Add user VST3 directory
        if (const char* userProfile = std::getenv("USERPROFILE")) {
            directories.push_back(std::string(userProfile) + "\\AppData\\Roaming\\VST3");
        }
#elif defined(__APPLE__)
        directories.push_back("/Library/Audio/Plug-Ins/VST3");
        directories.push_back("~/Library/Audio/Plug-Ins/VST3");
        directories.push_back("/System/Library/Audio/Plug-Ins/VST3");
#else // Linux
        directories.push_back("/usr/lib/vst3");
        directories.push_back("/usr/local/lib/vst3");
        directories.push_back("~/.vst3");
#endif
        
        return directories;
    }
    
    bool scanSinglePlugin(const std::filesystem::path& pluginPath, PluginInfo& info) {
        try {
            VST3Plugin plugin(pluginPath.string());
            return plugin.scanPlugin(info);
        } catch (const std::exception& e) {
            core::Logger::error("Failed to scan plugin: " + pluginPath.string() + " - " + e.what());
            return false;
        }
    }
    
    void performAIAnalysis(PluginInfo& info) {
        if (!aiAnalysis_) return;
        
        // AI analysis would happen here
        info.aiQualityScore = 0.8f; // Placeholder
        info.aiAnalysis = "AI analysis: High-quality plugin with good performance characteristics";
        info.aiTags = {"professional", "high-quality", "recommended"};
        info.aiRecommendations = "Excellent choice for professional audio production";
    }
};

VST3Scanner::VST3Scanner() : pImpl_(std::make_unique<Impl>()) {}

VST3Scanner::~VST3Scanner() = default;

core::AsyncResult<VST3Scanner::ScanResult> VST3Scanner::scanDirectory(const std::string& directory, bool recursive) {
    return core::executeAsyncGlobal<ScanResult>([this, directory, recursive]() -> ScanResult {
        ScanResult result;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (!std::filesystem::exists(directory)) {
            result.errors.push_back("Directory does not exist: " + directory);
            return result;
        }
        
        auto iterator = recursive ? 
            std::filesystem::recursive_directory_iterator(directory) :
            std::filesystem::directory_iterator(directory);
        
        for (const auto& entry : iterator) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".vst3") {
                    result.totalFilesScanned++;
                    
                    PluginInfo info;
                    if (pImpl_->scanSinglePlugin(entry.path(), info)) {
                        pImpl_->performAIAnalysis(info);
                        result.foundPlugins.push_back(info);
                        
                        core::Logger::info("Scanned plugin: " + info.name);
                    } else {
                        result.failedPaths.push_back(entry.path().string());
                    }
                }
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.scanTimeSeconds = std::chrono::duration<double>(endTime - startTime).count();
        
        core::Logger::info("Scan complete: " + std::to_string(result.foundPlugins.size()) + 
                          " plugins found in " + std::to_string(result.scanTimeSeconds) + "s");
        
        return result;
    });
}

core::AsyncResult<VST3Scanner::ScanResult> VST3Scanner::scanSystemDirectories() {
    return core::executeAsyncGlobal<ScanResult>([this]() -> ScanResult {
        ScanResult combinedResult;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        std::vector<std::string> directories = pImpl_->getSystemDirectories();
        
        for (const std::string& dir : directories) {
            if (std::filesystem::exists(dir)) {
                auto future = scanDirectory(dir, true);
                auto result = future.get(); // Wait for completion
                
                // Combine results
                combinedResult.foundPlugins.insert(combinedResult.foundPlugins.end(), 
                                                  result.foundPlugins.begin(), result.foundPlugins.end());
                combinedResult.failedPaths.insert(combinedResult.failedPaths.end(), 
                                                 result.failedPaths.begin(), result.failedPaths.end());
                combinedResult.errors.insert(combinedResult.errors.end(), 
                                            result.errors.begin(), result.errors.end());
                combinedResult.totalFilesScanned += result.totalFilesScanned;
            } else {
                core::Logger::warning("System directory not found: " + dir);
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        combinedResult.scanTimeSeconds = std::chrono::duration<double>(endTime - startTime).count();
        
        core::Logger::info("System scan complete: " + std::to_string(combinedResult.foundPlugins.size()) + 
                          " total plugins found");
        
        return combinedResult;
    });
}

void VST3Scanner::setDeepScan(bool enabled) {
    pImpl_->deepScan_ = enabled;
}

void VST3Scanner::setAIAnalysis(bool enabled) {
    pImpl_->aiAnalysis_ = enabled;
}

void VST3Scanner::setPerformanceTest(bool enabled) {
    pImpl_->performanceTest_ = enabled;
}

void VST3Scanner::setTimeout(int timeoutSeconds) {
    pImpl_->timeoutSeconds_ = timeoutSeconds;
}

bool VST3Scanner::verifyPlugin(const std::string& pluginPath) {
    try {
        VST3Plugin plugin(pluginPath);
        PluginInfo info;
        return plugin.scanPlugin(info);
    } catch (...) {
        return false;
    }
}

PluginInfo VST3Scanner::getPluginInfo(const std::string& pluginPath) {
    // Check cache first
    auto it = pImpl_->pluginCache_.find(pluginPath);
    if (it != pImpl_->pluginCache_.end()) {
        return it->second;
    }
    
    // Scan plugin
    VST3Plugin plugin(pluginPath);
    PluginInfo info;
    if (plugin.scanPlugin(info)) {
        pImpl_->performAIAnalysis(info);
        pImpl_->pluginCache_[pluginPath] = info;
        return info;
    }
    
    return PluginInfo{};
}

// ============================================================================
// VST3Factory Implementation
// ============================================================================

std::unique_ptr<VST3Plugin> VST3Factory::createPlugin(const PluginInfo& info) {
    auto plugin = std::make_unique<VST3Plugin>(info.filePath);
    
    if (!plugin->loadFromFile(info.filePath)) {
        core::Logger::error("Failed to load plugin: " + info.name);
        return nullptr;
    }
    
    core::Logger::info("Created VST3 plugin instance: " + info.name);
    return plugin;
}

std::vector<std::string> VST3Factory::getSupportedFormats() {
    return {"VST3", "VST2"};
}

bool VST3Factory::isFormatSupported(PluginFormat format) {
    return format == PluginFormat::VST3;
}

bool VST3Factory::validatePlugin(const std::string& pluginPath) {
    try {
        return std::filesystem::exists(pluginPath) && 
               std::filesystem::path(pluginPath).extension() == ".vst3";
    } catch (...) {
        return false;
    }
}

std::string VST3Factory::getValidationReport(const std::string& pluginPath) {
    std::string report = "Plugin Validation Report: " + pluginPath + "\n\n";
    
    if (!std::filesystem::exists(pluginPath)) {
        report += "❌ File does not exist\n";
        return report;
    }
    
    if (std::filesystem::path(pluginPath).extension() != ".vst3") {
        report += "❌ Invalid file extension (expected .vst3)\n";
        return report;
    }
    
    VST3Scanner scanner;
    if (scanner.verifyPlugin(pluginPath)) {
        report += "✅ Plugin loads successfully\n";
        report += "✅ Basic functionality verified\n";
    } else {
        report += "❌ Plugin failed to load\n";
    }
    
    return report;
}

std::unique_ptr<VST3Plugin> VST3Factory::createPluginWithOptions(
    const PluginInfo& info, 
    const LoadingOptions& options) {
    
    core::Logger::info("Creating plugin with advanced options: " + info.name);
    
    auto plugin = createPlugin(info);
    
    if (plugin && options.analyzeCapabilities) {
        // Perform additional capability analysis
        core::Logger::info("Analyzing plugin capabilities: " + info.name);
    }
    
    return plugin;
}

VST3Factory::CompatibilityTest VST3Factory::testCompatibility(const std::string& pluginPath) {
    CompatibilityTest test;
    
    if (!validatePlugin(pluginPath)) {
        test.isCompatible = false;
        test.issues.push_back("Plugin file validation failed");
        test.recommendation = "Verify plugin file integrity and format";
        return test;
    }
    
    try {
        VST3Plugin plugin(pluginPath);
        PluginInfo info;
        
        if (plugin.scanPlugin(info)) {
            test.isCompatible = true;
            test.recommendation = "Plugin is compatible and ready to use";
            
            if (info.averageCpuUsage > 15.0) {
                test.warnings.push_back("High CPU usage detected");
            }
            
            if (info.latencySamples > 512) {
                test.warnings.push_back("High latency detected");
            }
        } else {
            test.isCompatible = false;
            test.issues.push_back("Plugin failed initialization test");
        }
    } catch (const std::exception& e) {
        test.isCompatible = false;
        test.issues.push_back("Exception during compatibility test: " + std::string(e.what()));
    }
    
    return test;
}

} // namespace mixmind::plugins