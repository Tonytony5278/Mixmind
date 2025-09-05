#include "PluginHost.h"
#include "../core/Logger.h"
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <thread>
#include <random>

namespace mixmind::plugins {

// ============================================================================
// Plugin AI Implementation
// ============================================================================

PluginQuality PluginAI::analyzePluginQuality(const PluginInfo& info) {
    float score = 0.0f;
    
    // Base quality from AI analysis
    score += info.aiQualityScore * 0.4f;
    
    // Performance factors
    if (info.averageCpuUsage < 5.0) score += 0.2f;      // Low CPU usage
    else if (info.averageCpuUsage > 20.0) score -= 0.1f; // High CPU usage
    
    if (info.latencySamples < 64) score += 0.1f;        // Low latency
    else if (info.latencySamples > 512) score -= 0.1f;  // High latency
    
    // User feedback
    if (info.userRating >= 4.0f) score += 0.2f;
    else if (info.userRating <= 2.0f) score -= 0.2f;
    
    // Usage patterns
    if (info.usageCount > 100) score += 0.1f;           // Popular with user
    
    // Convert to quality enum
    if (score >= 0.9f) return PluginQuality::PROFESSIONAL;
    if (score >= 0.7f) return PluginQuality::EXCELLENT;
    if (score >= 0.5f) return PluginQuality::GOOD;
    if (score >= 0.3f) return PluginQuality::AVERAGE;
    if (score >= 0.1f) return PluginQuality::POOR;
    return PluginQuality::BROKEN;
}

std::string PluginAI::generateQualityReport(const PluginInfo& info) {
    PluginQuality quality = analyzePluginQuality(info);
    std::string report = "Plugin Quality Analysis: " + info.name + "\n\n";
    
    // Overall assessment
    switch (quality) {
        case PluginQuality::PROFESSIONAL:
            report += "✅ PROFESSIONAL GRADE - Industry standard quality\n";
            break;
        case PluginQuality::EXCELLENT:
            report += "🌟 EXCELLENT - Very high quality, highly recommended\n";
            break;
        case PluginQuality::GOOD:
            report += "👍 GOOD - Solid choice for most applications\n";
            break;
        case PluginQuality::AVERAGE:
            report += "⚠️ AVERAGE - Acceptable quality with some limitations\n";
            break;
        case PluginQuality::POOR:
            report += "❌ POOR - Consider alternatives if available\n";
            break;
        case PluginQuality::BROKEN:
            report += "🚫 BROKEN - Non-functional or severely compromised\n";
            break;
    }
    
    // Performance analysis
    report += "\nPerformance Metrics:\n";
    report += "  CPU Usage: " + std::to_string(info.averageCpuUsage) + "% average\n";
    report += "  Latency: " + std::to_string(info.latencySamples) + " samples\n";
    report += "  Real-time Capable: " + std::string(info.isRealTimeCapable ? "Yes" : "No") + "\n";
    
    // User feedback
    if (info.userRating > 0.0f) {
        report += "\nUser Rating: " + std::to_string(info.userRating) + "/5.0 stars\n";
        report += "Usage Count: " + std::to_string(info.usageCount) + " sessions\n";
    }
    
    // AI insights
    if (!info.aiAnalysis.empty()) {
        report += "\nAI Analysis:\n" + info.aiAnalysis + "\n";
    }
    
    if (!info.aiRecommendations.empty()) {
        report += "\nRecommendations:\n" + info.aiRecommendations + "\n";
    }
    
    return report;
}

float PluginAI::calculateQualityScore(const PluginInfo& info) {
    return static_cast<float>(static_cast<int>(analyzePluginQuality(info))) / 5.0f;
}

std::vector<PluginAI::ParameterMapping> PluginAI::generateParameterMappings(
    const PluginInfo& sourcePlugin,
    const PluginInfo& targetPlugin) {
    
    std::vector<ParameterMapping> mappings;
    
    // This would use AI to intelligently map similar parameters
    // For now, provide some common mappings based on parameter names
    
    std::vector<std::pair<std::string, std::string>> commonMappings = {
        {"gain", "volume"},
        {"drive", "input"},
        {"mix", "wet"},
        {"cutoff", "frequency"},
        {"resonance", "q"},
        {"attack", "attack"},
        {"release", "release"},
        {"threshold", "threshold"},
        {"ratio", "ratio"}
    };
    
    for (const auto& [source, target] : commonMappings) {
        ParameterMapping mapping;
        mapping.sourceParam = source;
        mapping.targetParam = target;
        mapping.mappingCurve = 1.0f;
        mapping.confidenceScore = 0.8f;
        mapping.reasoning = "Common parameter name match";
        mappings.push_back(mapping);
    }
    
    return mappings;
}

std::vector<PluginAI::PluginRecommendation> PluginAI::recommendPlugins(
    PluginCategory category,
    const std::string& context,
    int maxRecommendations) {
    
    std::vector<PluginRecommendation> recommendations;
    
    // This would use AI to provide intelligent recommendations
    // For now, provide some example recommendations based on category
    
    switch (category) {
        case PluginCategory::EQ: {
            PluginRecommendation rec;
            rec.pluginUid = "fabfilter_pro_q3";
            rec.relevanceScore = 0.95f;
            rec.qualityScore = 0.98f;
            rec.reasoning = "Industry standard EQ with surgical precision and excellent workflow";
            rec.useCases = {"Mixing", "Mastering", "Creative filtering"};
            recommendations.push_back(rec);
            break;
        }
        case PluginCategory::DYNAMICS: {
            PluginRecommendation rec;
            rec.pluginUid = "waves_ssl_comp";
            rec.relevanceScore = 0.90f;
            rec.qualityScore = 0.92f;
            rec.reasoning = "Classic SSL compressor character with modern workflow";
            rec.useCases = {"Mix bus compression", "Drum processing", "Vocal treatment"};
            recommendations.push_back(rec);
            break;
        }
        default:
            break;
    }
    
    return recommendations;
}

std::vector<std::string> PluginAI::optimizePluginChain(const PluginChain& chain) {
    std::vector<std::string> suggestions;
    
    // Analyze chain for optimization opportunities
    if (chain.slots.size() > 8) {
        suggestions.push_back("Consider reducing chain length for better CPU performance");
    }
    
    // Check for redundant plugins
    std::unordered_map<PluginCategory, int> categoryCount;
    for (const auto& slot : chain.slots) {
        if (slot.plugin) {
            PluginCategory category = slot.plugin->getInfo().category;
            categoryCount[category]++;
        }
    }
    
    for (const auto& [category, count] : categoryCount) {
        if (count > 2) {
            suggestions.push_back("Multiple " + std::to_string(count) + " plugins of same category detected - consider consolidation");
        }
    }
    
    suggestions.push_back("Consider A/B testing different plugin orders for optimal sound");
    
    return suggestions;
}

PluginChain PluginAI::generateOptimalChain(const std::string& goal, const std::vector<PluginInfo>& availablePlugins) {
    PluginChain chain;
    chain.chainId = "ai_generated_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    chain.name = "AI Generated: " + goal;
    
    // This would use AI to build an optimal chain for the specified goal
    // For now, create a basic template based on common mixing practices
    
    if (goal.find("vocal") != std::string::npos) {
        chain.aiAnalysis = "AI-generated vocal processing chain";
        // Would add EQ, compressor, de-esser, reverb based on available plugins
    } else if (goal.find("master") != std::string::npos) {
        chain.aiAnalysis = "AI-generated mastering chain";
        // Would add EQ, compressor, limiter, stereo enhancer
    }
    
    return chain;
}

std::vector<PluginAI::PluginRecommendation> PluginAI::getStylePlugins(const std::string& musicalStyle) {
    std::vector<PluginRecommendation> recommendations;
    
    if (musicalStyle == "Nirvana" || musicalStyle == "Grunge") {
        PluginRecommendation amp;
        amp.pluginUid = "neural_dsp_archetype_plini";
        amp.relevanceScore = 0.92f;
        amp.qualityScore = 0.95f;
        amp.reasoning = "High-gain amp simulation perfect for grunge tones";
        amp.useCases = {"Rhythm guitar", "Lead guitar", "Power chords"};
        recommendations.push_back(amp);
        
        PluginRecommendation chorus;
        chorus.pluginUid = "boss_ce2_chorus";
        chorus.relevanceScore = 0.88f;
        chorus.qualityScore = 0.90f;
        chorus.reasoning = "Classic chorus effect used extensively in 90s grunge";
        chorus.useCases = {"Clean guitar", "Atmospheric textures"};
        recommendations.push_back(chorus);
    }
    
    return recommendations;
}

PluginChain PluginAI::generateStyleChain(const std::string& style, const std::vector<PluginInfo>& availablePlugins) {
    PluginChain chain;
    chain.chainId = "style_" + style + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    chain.name = style + " Style Chain";
    chain.aiAnalysis = "AI-generated chain optimized for " + style + " style";
    
    // This would analyze the style and build appropriate processing chains
    
    return chain;
}

// ============================================================================
// Plugin Host Implementation
// ============================================================================

class PluginHost::Impl {
public:
    struct LoadedPlugin {
        std::shared_ptr<PluginInstance> instance;
        PluginInfo info;
        std::chrono::steady_clock::time_point loadTime;
        bool isActive = false;
        double cpuUsage = 0.0;
    };
    
    std::vector<PluginInfo> availablePlugins_;
    std::unordered_map<std::string, LoadedPlugin> loadedPlugins_;
    std::unordered_map<std::string, PluginChain> pluginChains_;
    
    double sampleRate_ = 44100.0;
    int maxBufferSize_ = 512;
    std::atomic<bool> isInitialized_{false};
    
    // Performance monitoring
    mutable std::mutex performanceMutex_;
    std::unordered_map<std::string, double> pluginCpuUsage_;
    double maxCpuUsage_ = 80.0;
    int maxLatency_ = 1024;
    
    // AI optimization
    bool aiOptimizationEnabled_ = true;
    std::mt19937 rng_{std::chrono::steady_clock::now().time_since_epoch().count()};
};

PluginHost::PluginHost() : pImpl_(std::make_unique<Impl>()) {}

PluginHost::~PluginHost() {
    shutdown();
}

bool PluginHost::initialize(double sampleRate, int maxBufferSize) {
    pImpl_->sampleRate_ = sampleRate;
    pImpl_->maxBufferSize_ = maxBufferSize;
    pImpl_->isInitialized_.store(true);
    
    core::Logger::info("PluginHost initialized - Sample Rate: " + std::to_string(sampleRate) + 
                       ", Buffer Size: " + std::to_string(maxBufferSize));
    
    return true;
}

void PluginHost::shutdown() {
    if (!pImpl_->isInitialized_.load()) return;
    
    // Unload all plugins
    for (auto& [uid, plugin] : pImpl_->loadedPlugins_) {
        if (plugin.instance) {
            plugin.instance->deactivate();
            plugin.instance->cleanup();
        }
    }
    
    pImpl_->loadedPlugins_.clear();
    pImpl_->pluginChains_.clear();
    pImpl_->isInitialized_.store(false);
    
    core::Logger::info("PluginHost shutdown complete");
}

core::AsyncResult<std::vector<PluginInfo>> PluginHost::scanForPlugins() {
    return core::executeAsyncGlobal<std::vector<PluginInfo>>([]() -> std::vector<PluginInfo> {
        std::vector<PluginInfo> plugins;
        
        // Simulate plugin scanning - in real implementation this would:
        // 1. Scan common VST3/AU directories
        // 2. Load and analyze each plugin
        // 3. Run AI quality analysis
        // 4. Cache results for faster subsequent scans
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate scan time
        
        // Example plugins
        PluginInfo eq;
        eq.uid = "fabfilter_pro_q3";
        eq.name = "FabFilter Pro-Q 3";
        eq.manufacturer = "FabFilter";
        eq.version = "3.19";
        eq.description = "High-quality equalizer with dynamic EQ, mid/side processing, and linear phase mode";
        eq.format = PluginFormat::VST3;
        eq.category = PluginCategory::EQ;
        eq.quality = PluginQuality::PROFESSIONAL;
        eq.numInputChannels = 2;
        eq.numOutputChannels = 2;
        eq.averageCpuUsage = 3.2;
        eq.latencySamples = 0;
        eq.isRealTimeCapable = true;
        eq.aiAnalysis = "Professional-grade EQ with exceptional sound quality and workflow. Ideal for surgical cuts and musical enhancements.";
        eq.aiTags = {"mixing", "mastering", "surgical", "musical", "transparent"};
        eq.aiQualityScore = 0.98f;
        eq.aiRecommendations = "Excellent for all mixing and mastering tasks. Use linear phase mode for mastering bus.";
        plugins.push_back(eq);
        
        PluginInfo comp;
        comp.uid = "waves_ssl_comp";
        comp.name = "Waves SSL G-Master Buss Compressor";
        comp.manufacturer = "Waves";
        comp.version = "12.0";
        comp.description = "Authentic SSL console compressor emulation for mix bus and master processing";
        comp.format = PluginFormat::VST3;
        comp.category = PluginCategory::DYNAMICS;
        comp.quality = PluginQuality::EXCELLENT;
        comp.numInputChannels = 2;
        comp.numOutputChannels = 2;
        comp.averageCpuUsage = 2.8;
        comp.latencySamples = 32;
        comp.isRealTimeCapable = true;
        comp.aiAnalysis = "Classic SSL console sound with excellent glue characteristics. Perfect for mix bus compression.";
        comp.aiTags = {"mix bus", "glue", "analog", "classic", "transparent"};
        comp.aiQualityScore = 0.92f;
        comp.aiRecommendations = "Use slow attack and auto-release for mix bus glue. Try 2:1 or 4:1 ratio.";
        plugins.push_back(comp);
        
        PluginInfo verb;
        verb.uid = "valhalla_vintage_verb";
        verb.name = "Valhalla VintageVerb";
        verb.manufacturer = "Valhalla DSP";
        verb.version = "3.1.0";
        verb.description = "Vintage digital reverb algorithms from the 1970s and 1980s";
        verb.format = PluginFormat::VST3;
        verb.category = PluginCategory::REVERB;
        verb.quality = PluginQuality::EXCELLENT;
        verb.numInputChannels = 2;
        verb.numOutputChannels = 2;
        verb.averageCpuUsage = 4.5;
        verb.latencySamples = 0;
        verb.isRealTimeCapable = true;
        verb.aiAnalysis = "Exceptional vintage reverb character with modern flexibility. Sounds musical in all settings.";
        verb.aiTags = {"vintage", "musical", "creative", "atmospheric", "warm"};
        verb.aiQualityScore = 0.94f;
        verb.aiRecommendations = "Excellent for vocals and instruments. Try Hall and Plate modes for different textures.";
        plugins.push_back(verb);
        
        core::Logger::info("Plugin scan complete - Found " + std::to_string(plugins.size()) + " plugins");
        return plugins;
    });
}

std::vector<PluginInfo> PluginHost::getAvailablePlugins() const {
    return pImpl_->availablePlugins_;
}

std::shared_ptr<PluginInstance> PluginHost::loadPlugin(const std::string& pluginUid) {
    auto it = pImpl_->loadedPlugins_.find(pluginUid);
    if (it != pImpl_->loadedPlugins_.end()) {
        return it->second.instance;
    }
    
    // Find plugin info
    auto pluginIt = std::find_if(pImpl_->availablePlugins_.begin(), pImpl_->availablePlugins_.end(),
        [&](const PluginInfo& info) { return info.uid == pluginUid; });
    
    if (pluginIt == pImpl_->availablePlugins_.end()) {
        core::Logger::error("Plugin not found: " + pluginUid);
        return nullptr;
    }
    
    // For this demo, we'll create a mock plugin instance
    // In real implementation, this would load the actual VST3/AU plugin
    
    // Create plugin instance based on format
    std::shared_ptr<PluginInstance> instance;
    // instance = createPluginInstance(pluginIt->format, pluginIt->filePath);
    
    if (!instance) {
        core::Logger::error("Failed to create plugin instance: " + pluginUid);
        return nullptr;
    }
    
    // Initialize plugin
    if (!instance->initialize(pImpl_->sampleRate_, pImpl_->maxBufferSize_)) {
        core::Logger::error("Failed to initialize plugin: " + pluginUid);
        return nullptr;
    }
    
    // Store loaded plugin
    Impl::LoadedPlugin loadedPlugin;
    loadedPlugin.instance = instance;
    loadedPlugin.info = *pluginIt;
    loadedPlugin.loadTime = std::chrono::steady_clock::now();
    
    pImpl_->loadedPlugins_[pluginUid] = loadedPlugin;
    
    if (onPluginLoaded) {
        onPluginLoaded(pluginUid);
    }
    
    core::Logger::info("Plugin loaded successfully: " + pluginIt->name);
    return instance;
}

void PluginHost::unloadPlugin(std::shared_ptr<PluginInstance> plugin) {
    if (!plugin) return;
    
    std::string pluginUid = plugin->getInfo().uid;
    
    auto it = pImpl_->loadedPlugins_.find(pluginUid);
    if (it != pImpl_->loadedPlugins_.end()) {
        plugin->deactivate();
        plugin->cleanup();
        pImpl_->loadedPlugins_.erase(it);
        
        if (onPluginUnloaded) {
            onPluginUnloaded(pluginUid);
        }
        
        core::Logger::info("Plugin unloaded: " + pluginUid);
    }
}

std::string PluginHost::createPluginChain(const std::string& name) {
    std::string chainId = "chain_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    
    PluginChain chain;
    chain.chainId = chainId;
    chain.name = name.empty() ? ("Chain " + chainId) : name;
    chain.isActive = true;
    
    pImpl_->pluginChains_[chainId] = chain;
    
    core::Logger::info("Created plugin chain: " + chain.name + " (" + chainId + ")");
    return chainId;
}

bool PluginHost::addPluginToChain(const std::string& chainId, std::shared_ptr<PluginInstance> plugin) {
    auto chainIt = pImpl_->pluginChains_.find(chainId);
    if (chainIt == pImpl_->pluginChains_.end() || !plugin) {
        return false;
    }
    
    PluginSlot slot;
    slot.slotId = "slot_" + std::to_string(chainIt->second.slots.size());
    slot.plugin = plugin;
    slot.isActive = true;
    slot.isBypassed = false;
    
    chainIt->second.slots.push_back(slot);
    
    core::Logger::info("Added plugin to chain: " + plugin->getInfo().name + " -> " + chainId);
    return true;
}

PluginChain PluginHost::getPluginChain(const std::string& chainId) const {
    auto it = pImpl_->pluginChains_.find(chainId);
    if (it != pImpl_->pluginChains_.end()) {
        return it->second;
    }
    return PluginChain{};
}

void PluginHost::processChain(const std::string& chainId,
                             const audio::AudioBufferPool::AudioBuffer& input,
                             audio::AudioBufferPool::AudioBuffer& output) {
    auto chainIt = pImpl_->pluginChains_.find(chainId);
    if (chainIt == pImpl_->pluginChains_.end() || !chainIt->second.isActive) {
        // Copy input to output if chain not found or inactive
        std::copy(input.data, input.data + input.numChannels * input.numSamples, output.data);
        return;
    }
    
    const PluginChain& chain = chainIt->second;
    
    // For parallel processing, we'd need multiple buffers
    // For now, implement serial processing
    audio::AudioBufferPool::AudioBuffer currentBuffer = input;
    
    for (const auto& slot : chain.slots) {
        if (!slot.plugin || !slot.isActive || slot.isBypassed) {
            continue;
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Process through plugin
        slot.plugin->processAudio(currentBuffer, output);
        
        // Measure CPU usage
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::lock_guard<std::mutex> lock(pImpl_->performanceMutex_);
        pImpl_->pluginCpuUsage_[slot.plugin->getInfo().uid] = duration.count() / 1000.0; // Convert to ms
        
        // Apply wet/dry mix if needed
        if (slot.wetDryMix < 1.0f) {
            float dry = 1.0f - slot.wetDryMix;
            float wet = slot.wetDryMix;
            
            for (int ch = 0; ch < output.numChannels; ++ch) {
                for (int i = 0; i < output.numSamples; ++i) {
                    int idx = ch * output.numSamples + i;
                    output.data[idx] = dry * currentBuffer.data[idx] + wet * output.data[idx];
                }
            }
        }
        
        // Prepare for next plugin in chain
        currentBuffer = output;
    }
}

PluginHost::PerformanceStats PluginHost::getPerformanceStats() const {
    std::lock_guard<std::mutex> lock(pImpl_->performanceMutex_);
    
    PerformanceStats stats;
    stats.activePluginCount = static_cast<int>(pImpl_->loadedPlugins_.size());
    
    double totalCpu = 0.0;
    double maxCpu = 0.0;
    
    for (const auto& [uid, usage] : pImpl_->pluginCpuUsage_) {
        totalCpu += usage;
        maxCpu = std::max(maxCpu, usage);
        stats.pluginCpuUsage.emplace_back(uid, usage);
    }
    
    stats.totalCpuUsage = totalCpu;
    stats.peakCpuUsage = maxCpu;
    
    return stats;
}

void PluginHost::enableAIOptimization(bool enable) {
    pImpl_->aiOptimizationEnabled_ = enable;
    core::Logger::info("AI optimization " + std::string(enable ? "enabled" : "disabled"));
}

std::vector<PluginAI::PluginRecommendation> PluginHost::getAIRecommendations(const std::string& context) {
    if (!pImpl_->aiOptimizationEnabled_) {
        return {};
    }
    
    // Use AI to generate recommendations based on current context
    return PluginAI::recommendPlugins(PluginCategory::EFFECT, context, 5);
}

core::AsyncResult<PluginQuality> PluginHost::analyzePluginQuality(const std::string& pluginUid) {
    return core::executeAsyncGlobal<PluginQuality>([pluginUid, this]() -> PluginQuality {
        auto it = std::find_if(pImpl_->availablePlugins_.begin(), pImpl_->availablePlugins_.end(),
            [&](const PluginInfo& info) { return info.uid == pluginUid; });
        
        if (it != pImpl_->availablePlugins_.end()) {
            return PluginAI::analyzePluginQuality(*it);
        }
        
        return PluginQuality::UNKNOWN;
    });
}

std::string PluginHost::generatePluginReport(const std::string& pluginUid) {
    auto it = std::find_if(pImpl_->availablePlugins_.begin(), pImpl_->availablePlugins_.end(),
        [&](const PluginInfo& info) { return info.uid == pluginUid; });
    
    if (it != pImpl_->availablePlugins_.end()) {
        return PluginAI::generateQualityReport(*it);
    }
    
    return "Plugin not found: " + pluginUid;
}

// ============================================================================
// Realtime Plugin Processor Implementation
// ============================================================================

class RealtimePluginProcessor::Impl {
public:
    std::vector<std::shared_ptr<PluginInstance>> activePlugins_;
    std::mutex pluginMutex_;
    
    // Parameter change queue (lock-free)
    static constexpr size_t PARAM_QUEUE_SIZE = 1024;
    std::array<ParameterChange, PARAM_QUEUE_SIZE> paramQueue_;
    std::atomic<size_t> paramQueueWrite_{0};
    std::atomic<size_t> paramQueueRead_{0};
    
    // Performance monitoring
    std::atomic<double> currentCpuLoad_{0.0};
    std::atomic<bool> hasXruns_{false};
};

RealtimePluginProcessor::RealtimePluginProcessor() : pImpl_(std::make_unique<Impl>()) {}

RealtimePluginProcessor::~RealtimePluginProcessor() = default;

void RealtimePluginProcessor::processBuffer(const audio::AudioBufferPool::AudioBuffer& input,
                                           audio::AudioBufferPool::AudioBuffer& output) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Process queued parameter changes
    size_t readPos = pImpl_->paramQueueRead_.load();
    size_t writePos = pImpl_->paramQueueWrite_.load();
    
    while (readPos != writePos) {
        const auto& change = pImpl_->paramQueue_[readPos];
        // Apply parameter change to appropriate plugin
        readPos = (readPos + 1) % Impl::PARAM_QUEUE_SIZE;
    }
    pImpl_->paramQueueRead_.store(readPos);
    
    // Process audio through plugin chain
    std::lock_guard<std::mutex> lock(pImpl_->pluginMutex_);
    
    audio::AudioBufferPool::AudioBuffer currentBuffer = input;
    
    for (auto& plugin : pImpl_->activePlugins_) {
        if (plugin) {
            plugin->processAudio(currentBuffer, output);
            currentBuffer = output;
        }
    }
    
    // Update CPU load measurement
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double cpuLoad = (duration.count() / 1000.0) / (input.numSamples / 44100.0 * 1000.0); // Rough estimate
    pImpl_->currentCpuLoad_.store(cpuLoad);
}

void RealtimePluginProcessor::addPlugin(std::shared_ptr<PluginInstance> plugin) {
    std::lock_guard<std::mutex> lock(pImpl_->pluginMutex_);
    pImpl_->activePlugins_.push_back(plugin);
}

void RealtimePluginProcessor::queueParameterChange(const ParameterChange& change) {
    size_t writePos = pImpl_->paramQueueWrite_.load();
    size_t nextWrite = (writePos + 1) % Impl::PARAM_QUEUE_SIZE;
    
    if (nextWrite != pImpl_->paramQueueRead_.load()) {
        pImpl_->paramQueue_[writePos] = change;
        pImpl_->paramQueueWrite_.store(nextWrite);
    } else {
        // Queue full - this is an xrun condition
        pImpl_->hasXruns_.store(true);
    }
}

double RealtimePluginProcessor::getCurrentCpuLoad() const {
    return pImpl_->currentCpuLoad_.load();
}

bool RealtimePluginProcessor::hasXruns() const {
    return pImpl_->hasXruns_.load();
}

void RealtimePluginProcessor::clearXruns() {
    pImpl_->hasXruns_.store(false);
}

// ============================================================================
// Global Access
// ============================================================================

static std::unique_ptr<PluginHost> g_pluginHost;

PluginHost& getGlobalPluginHost() {
    if (!g_pluginHost) {
        g_pluginHost = std::make_unique<PluginHost>();
    }
    return *g_pluginHost;
}

void shutdownGlobalPluginHost() {
    if (g_pluginHost) {
        g_pluginHost->shutdown();
        g_pluginHost.reset();
    }
}

} // namespace mixmind::plugins