#include "PluginManagerUI.h"
#include "../plugins/PluginHost.h"
#include "../plugins/PluginIntelligence.h"
#include "../core/Logger.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace mixmind::ui {

// Mock ImGui types for compilation
struct ImVec2 { float x, y; ImVec2(float x = 0, float y = 0) : x(x), y(y) {} };
struct ImVec4 { float x, y, z, w; ImVec4(float x = 0, float y = 0, float z = 0, float w = 0) : x(x), y(y), z(z), w(w) {} };

// ============================================================================
// Plugin Browser Panel Implementation
// ============================================================================

PluginBrowserPanel::PluginBrowserPanel() {
    refreshPluginList();
}

void PluginBrowserPanel::render() {
    if (!visible_) return;
    
    if (ImGui::Begin("Plugin Browser", &visible_)) {
        renderSearchBar();
        ImGui::Separator();
        
        ImGui::Columns(2, "browser_columns", true);
        
        // Left column: Filters and categories
        ImGui::Text("Category");
        renderCategoryFilter();
        
        ImGui::Spacing();
        ImGui::Text("Quality");
        renderQualityFilter();
        
        ImGui::Spacing();
        ImGui::Checkbox("Favorites Only", &showOnlyFavorites_);
        ImGui::Checkbox("AI Recommendations", &showAIRecommendations_);
        
        if (ImGui::Button("Scan for Plugins")) {
            scanForNewPlugins();
        }
        
        renderScanProgress();
        
        ImGui::NextColumn();
        
        // Right column: Plugin list and details
        renderPluginList();
        
        if (selectedPluginIndex_ >= 0 && selectedPluginIndex_ < filteredPlugins_.size()) {
            ImGui::Separator();
            renderPluginDetails(filteredPlugins_[selectedPluginIndex_]);
            renderPluginActions(filteredPlugins_[selectedPluginIndex_]);
        }
        
        ImGui::Columns(1);
    }
    ImGui::End();
}

void PluginBrowserPanel::renderSearchBar() {
    ImGui::Text("Search:");
    ImGui::SameLine();
    
    char searchBuffer[256];
    strncpy(searchBuffer, searchFilter_.c_str(), sizeof(searchBuffer) - 1);
    searchBuffer[sizeof(searchBuffer) - 1] = '\0';
    
    if (ImGui::InputText("##search", searchBuffer, sizeof(searchBuffer))) {
        searchFilter_ = searchBuffer;
        applyFilters();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        searchFilter_.clear();
        applyFilters();
    }
}

void PluginBrowserPanel::renderCategoryFilter() {
    const char* categories[] = {
        "All", "Instrument", "Effect", "Dynamics", "EQ", "Reverb", 
        "Delay", "Modulation", "Distortion", "Analyzer", "Utility"
    };
    
    int currentCategory = static_cast<int>(selectedCategory_);
    if (ImGui::Combo("##category", &currentCategory, categories, IM_ARRAYSIZE(categories))) {
        selectedCategory_ = static_cast<plugins::PluginCategory>(currentCategory);
        applyFilters();
    }
}

void PluginBrowserPanel::renderQualityFilter() {
    const char* qualities[] = {"Any", "Poor", "Average", "Good", "Excellent", "Professional"};
    
    int currentQuality = static_cast<int>(minQuality_);
    if (ImGui::Combo("##quality", &currentQuality, qualities, IM_ARRAYSIZE(qualities))) {
        minQuality_ = static_cast<plugins::PluginQuality>(currentQuality);
        applyFilters();
    }
}

void PluginBrowserPanel::renderPluginList() {
    if (ImGui::BeginListBox("##plugins", ImVec2(-1, 200))) {
        for (size_t i = 0; i < filteredPlugins_.size(); ++i) {
            const auto& plugin = filteredPlugins_[i];
            
            // Create display name with quality indicator
            std::string displayName = plugin.name;
            if (plugin.quality == plugins::PluginQuality::PROFESSIONAL) {
                displayName = "⭐ " + displayName;
            } else if (plugin.quality == plugins::PluginQuality::EXCELLENT) {
                displayName = "✨ " + displayName;
            }
            
            bool isSelected = (selectedPluginIndex_ == i);
            if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                selectedPluginIndex_ = i;
            }
            
            // Double-click to load
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                loadPlugin(plugin);
            }
        }
        ImGui::EndListBox();
    }
}

void PluginBrowserPanel::renderPluginDetails(const plugins::PluginInfo& plugin) {
    ImGui::Text("Plugin Details:");
    ImGui::Text("Name: %s", plugin.name.c_str());
    ImGui::Text("Manufacturer: %s", plugin.manufacturer.c_str());
    ImGui::Text("Version: %s", plugin.version.c_str());
    ImGui::Text("Category: %d", static_cast<int>(plugin.category));
    ImGui::Text("Quality: %d/5", static_cast<int>(plugin.quality));
    ImGui::Text("CPU Usage: %.1f%%", plugin.averageCpuUsage);
    ImGui::Text("Latency: %d samples", plugin.latencySamples);
    
    if (!plugin.description.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", plugin.description.c_str());
    }
    
    if (!plugin.aiAnalysis.empty()) {
        ImGui::Separator();
        ImGui::Text("AI Analysis:");
        ImGui::TextWrapped("%s", plugin.aiAnalysis.c_str());
    }
}

void PluginBrowserPanel::renderPluginActions(const plugins::PluginInfo& plugin) {
    ImGui::Separator();
    
    if (ImGui::Button("Load Plugin")) {
        loadPlugin(plugin);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Add to Favorites")) {
        toggleFavorite(plugin);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Open Folder")) {
        openPluginFolder(plugin);
    }
}

void PluginBrowserPanel::renderScanProgress() {
    if (scanInProgress_) {
        ImGui::Text("Scanning...");
        // In real implementation, show progress bar
        ImGui::ProgressBar(0.5f, ImVec2(-1, 0), "Scanning plugins...");
    }
}

void PluginBrowserPanel::refreshPluginList() {
    auto& pluginHost = plugins::getGlobalPluginHost();
    availablePlugins_ = pluginHost.getAvailablePlugins();
    applyFilters();
}

void PluginBrowserPanel::scanForNewPlugins() {
    scanInProgress_ = true;
    
    auto& pluginHost = plugins::getGlobalPluginHost();
    auto scanFuture = pluginHost.scanForPlugins();
    
    // In real implementation, this would be handled asynchronously
    // For demo, just refresh the list
    refreshPluginList();
    scanInProgress_ = false;
}

void PluginBrowserPanel::loadPlugin(const plugins::PluginInfo& plugin) {
    auto& pluginHost = plugins::getGlobalPluginHost();
    auto instance = pluginHost.loadPlugin(plugin.uid);
    
    if (instance) {
        core::Logger::info("Plugin loaded from browser: " + plugin.name);
        
        // Add to default chain
        auto chainId = pluginHost.createPluginChain("Main Chain");
        pluginHost.addPluginToChain(chainId, instance);
    }
}

void PluginBrowserPanel::applyFilters() {
    filteredPlugins_.clear();
    
    for (const auto& plugin : availablePlugins_) {
        if (matchesFilter(plugin)) {
            filteredPlugins_.push_back(plugin);
        }
    }
    
    // Sort by quality and name
    std::sort(filteredPlugins_.begin(), filteredPlugins_.end(),
        [](const plugins::PluginInfo& a, const plugins::PluginInfo& b) {
            if (a.quality != b.quality) {
                return a.quality > b.quality; // Higher quality first
            }
            return a.name < b.name;
        });
}

bool PluginBrowserPanel::matchesFilter(const plugins::PluginInfo& plugin) const {
    // Search filter
    if (!searchFilter_.empty()) {
        std::string searchLower = searchFilter_;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
        
        std::string nameLower = plugin.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        
        if (nameLower.find(searchLower) == std::string::npos) {
            return false;
        }
    }
    
    // Category filter
    if (selectedCategory_ != plugins::PluginCategory::EFFECT && plugin.category != selectedCategory_) {
        return false;
    }
    
    // Quality filter
    if (plugin.quality < minQuality_) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Plugin Chain Panel Implementation
// ============================================================================

PluginChainPanel::PluginChainPanel() {
    // Initialize with default chain
    auto& pluginHost = plugins::getGlobalPluginHost();
    currentChainId_ = pluginHost.createPluginChain("Main Chain");
    currentChain_ = pluginHost.getPluginChain(currentChainId_);
}

void PluginChainPanel::render() {
    if (!visible_) return;
    
    if (ImGui::Begin("Plugin Chain", &visible_)) {
        renderChainSelector();
        ImGui::Separator();
        
        renderChainControls();
        ImGui::Separator();
        
        renderPluginSlots();
        
        if (showChainSettings_) {
            ImGui::Separator();
            renderChainSettings();
        }
        
        if (showPerformanceMetrics_) {
            ImGui::Separator();
            renderPerformanceMetrics();
        }
        
        renderAIOptimizations();
    }
    ImGui::End();
}

void PluginChainPanel::renderChainSelector() {
    ImGui::Text("Chain:");
    ImGui::SameLine();
    
    // Get available chains
    auto& pluginHost = plugins::getGlobalPluginHost();
    auto chains = pluginHost.getAllChains();
    
    if (ImGui::BeginCombo("##chain", currentChain_.name.c_str())) {
        for (const auto& chain : chains) {
            bool isSelected = (chain.chainId == currentChainId_);
            if (ImGui::Selectable(chain.name.c_str(), isSelected)) {
                currentChainId_ = chain.chainId;
                currentChain_ = chain;
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("New")) {
        createNewChain();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        duplicateChain(currentChainId_);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        deleteChain(currentChainId_);
    }
}

void PluginChainPanel::renderChainControls() {
    ImGui::Checkbox("Chain Active", &currentChain_.isActive);
    
    ImGui::SameLine();
    ImGui::Checkbox("Settings", &showChainSettings_);
    
    ImGui::SameLine();
    ImGui::Checkbox("Performance", &showPerformanceMetrics_);
    
    // Master controls
    ImGui::Text("Master Input:");
    ImGui::SameLine();
    ImGui::SliderFloat("##master_input", &currentChain_.masterInputGain, 0.0f, 2.0f, "%.2f");
    
    ImGui::Text("Master Output:");
    ImGui::SameLine();
    ImGui::SliderFloat("##master_output", &currentChain_.masterOutputGain, 0.0f, 2.0f, "%.2f");
}

void PluginChainPanel::renderPluginSlots() {
    ImGui::Text("Plugin Slots (%d):", static_cast<int>(currentChain_.slots.size()));
    
    // Render each slot
    for (size_t i = 0; i < currentChain_.slots.size(); ++i) {
        ImGui::PushID(i);
        renderSlot(currentChain_.slots[i], i);
        ImGui::PopID();
    }
    
    // Add slot button
    if (ImGui::Button("+ Add Plugin Slot")) {
        // Open plugin browser or create empty slot
        plugins::PluginSlot newSlot;
        newSlot.slotId = "slot_" + std::to_string(currentChain_.slots.size());
        currentChain_.slots.push_back(newSlot);
    }
}

void PluginChainPanel::renderSlot(const plugins::PluginSlot& slot, int index) {
    ImGui::BeginChild(("slot_" + std::to_string(index)).c_str(), ImVec2(0, 80), true);
    
    // Slot header
    std::string slotTitle = slot.plugin ? slot.plugin->getInfo().name : "Empty Slot";
    ImGui::Text("%d. %s", index + 1, slotTitle.c_str());
    
    // Controls
    ImGui::Columns(4, nullptr, false);
    
    // Active/Bypass
    bool isActive = slot.isActive;
    bool isBypassed = slot.isBypassed;
    
    if (ImGui::Checkbox("Active", &isActive)) {
        // Update slot state
    }
    
    ImGui::NextColumn();
    if (ImGui::Checkbox("Bypass", &isBypassed)) {
        // Update bypass state
    }
    
    ImGui::NextColumn();
    
    // Wet/Dry mix
    float wetDryMix = slot.wetDryMix;
    ImGui::Text("Mix");
    if (ImGui::SliderFloat("##mix", &wetDryMix, 0.0f, 1.0f, "%.2f")) {
        // Update mix
    }
    
    ImGui::NextColumn();
    
    // Actions
    if (ImGui::Button("Edit")) {
        selectedSlotIndex_ = index;
    }
    
    if (ImGui::Button("Remove")) {
        removeSlot(index);
    }
    
    ImGui::Columns(1);
    ImGui::EndChild();
}

void PluginChainPanel::renderChainSettings() {
    ImGui::Text("Chain Settings:");
    ImGui::Checkbox("Parallel Processing", &currentChain_.isParallelProcessing);
    
    // Chain name editing
    char nameBuffer[256];
    strncpy(nameBuffer, currentChain_.name.c_str(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
    
    if (ImGui::InputText("Chain Name", nameBuffer, sizeof(nameBuffer))) {
        currentChain_.name = nameBuffer;
    }
}

void PluginChainPanel::renderPerformanceMetrics() {
    auto& pluginHost = plugins::getGlobalPluginHost();
    auto stats = pluginHost.getPerformanceStats();
    
    ImGui::Text("Performance Metrics:");
    ImGui::Text("Total CPU: %.1f%%", stats.totalCpuUsage);
    ImGui::Text("Peak CPU: %.1f%%", stats.peakCpuUsage);
    ImGui::Text("Total Latency: %d samples", stats.totalLatency);
    ImGui::Text("Active Plugins: %d", stats.activePluginCount);
    
    // Per-plugin breakdown
    if (!stats.pluginCpuUsage.empty()) {
        ImGui::Separator();
        ImGui::Text("Per-Plugin CPU Usage:");
        for (const auto& [uid, usage] : stats.pluginCpuUsage) {
            ImGui::Text("%s: %.1f%%", uid.c_str(), usage);
        }
    }
}

void PluginChainPanel::renderAIOptimizations() {
    if (ImGui::CollapsingHeader("AI Optimizations")) {
        if (ImGui::Button("Optimize Chain")) {
            optimizeChain();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Analyze Chain")) {
            analyzeChain();
        }
        
        // Show AI suggestions if available
        if (!currentChain_.aiSuggestions.empty()) {
            ImGui::Separator();
            ImGui::Text("AI Suggestions:");
            for (const auto& suggestion : currentChain_.aiSuggestions) {
                ImGui::BulletText("%s", suggestion.c_str());
            }
        }
    }
}

void PluginChainPanel::createNewChain() {
    auto& pluginHost = plugins::getGlobalPluginHost();
    std::string newChainId = pluginHost.createPluginChain("New Chain");
    currentChainId_ = newChainId;
    currentChain_ = pluginHost.getPluginChain(newChainId);
}

void PluginChainPanel::optimizeChain() {
    auto& intelligenceSystem = plugins::PluginIntelligenceSystem::getInstance();
    auto& chainOptimizer = intelligenceSystem.getChainOptimizer();
    
    // Perform chain analysis and optimization
    auto analysisFuture = chainOptimizer.analyzeChain(currentChain_);
    // In real implementation, this would be handled asynchronously
    
    core::Logger::info("Chain optimization requested for: " + currentChain_.name);
}

// ============================================================================
// Plugin Control Panel Implementation
// ============================================================================

PluginControlPanel::PluginControlPanel() {}

void PluginControlPanel::render() {
    if (!visible_) return;
    
    if (ImGui::Begin("Plugin Control", &visible_)) {
        if (!selectedPlugin_) {
            ImGui::Text("No plugin selected");
            ImGui::Text("Select a plugin from the chain to control it");
            return;
        }
        
        renderPluginInfo();
        ImGui::Separator();
        
        renderPresetSelector();
        ImGui::Separator();
        
        renderParameterControls();
        ImGui::Separator();
        
        renderAutomationControls();
        
        if (enableAIAssistance_) {
            ImGui::Separator();
            renderAIAssistance();
        }
    }
    ImGui::End();
}

void PluginControlPanel::renderPluginInfo() {
    auto info = selectedPlugin_->getInfo();
    
    ImGui::Text("Plugin: %s", info.name.c_str());
    ImGui::Text("Manufacturer: %s", info.manufacturer.c_str());
    ImGui::Text("Category: %d", static_cast<int>(info.category));
    
    // Real-time stats
    ImGui::Text("CPU Usage: %.1f%%", selectedPlugin_->getCurrentCpuUsage());
    ImGui::Text("Latency: %d samples", selectedPlugin_->getCurrentLatency());
    
    bool isProcessing = selectedPlugin_->isProcessing();
    ImGui::Text("Status: %s", isProcessing ? "Processing" : "Idle");
}

void PluginControlPanel::renderPresetSelector() {
    ImGui::Text("Presets:");
    
    auto presets = selectedPlugin_->getPresets();
    
    if (ImGui::BeginCombo("##preset", selectedPreset_.c_str())) {
        for (const auto& preset : presets) {
            bool isSelected = (preset == selectedPreset_);
            if (ImGui::Selectable(preset.c_str(), isSelected)) {
                loadPreset(preset);
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        // Open save preset dialog
        selectedPreset_ = "New Preset";
        savePreset(selectedPreset_);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        resetToDefault();
    }
}

void PluginControlPanel::renderParameterControls() {
    ImGui::Text("Parameters:");
    
    // Search filter for parameters
    char searchBuffer[256];
    strncpy(searchBuffer, searchFilter_.c_str(), sizeof(searchBuffer) - 1);
    searchBuffer[sizeof(searchBuffer) - 1] = '\0';
    
    if (ImGui::InputText("Search##params", searchBuffer, sizeof(searchBuffer))) {
        searchFilter_ = searchBuffer;
    }
    
    // Get parameters
    parameters_ = selectedPlugin_->getParameters();
    
    // Render parameters
    for (const auto& param : parameters_) {
        // Skip if doesn't match search
        if (!searchFilter_.empty()) {
            std::string paramName = param.name;
            std::transform(paramName.begin(), paramName.end(), paramName.begin(), ::tolower);
            std::string search = searchFilter_;
            std::transform(search.begin(), search.end(), search.begin(), ::tolower);
            
            if (paramName.find(search) == std::string::npos) {
                continue;
            }
        }
        
        renderParameter(param);
    }
}

void PluginControlPanel::renderParameter(const plugins::PluginParameter& param) {
    ImGui::PushID(param.id.c_str());
    
    float currentValue = selectedPlugin_->getParameter(param.id);
    float newValue = currentValue;
    
    // Choose appropriate control based on parameter type
    if (!param.valueStrings.empty()) {
        // Discrete parameter (dropdown)
        int currentIndex = static_cast<int>(currentValue * (param.valueStrings.size() - 1));
        if (ImGui::Combo(param.displayName.c_str(), &currentIndex, 
                        [](void* data, int idx, const char** out_text) -> bool {
                            auto* strings = static_cast<const std::vector<std::string>*>(data);
                            if (idx < 0 || idx >= strings->size()) return false;
                            *out_text = (*strings)[idx].c_str();
                            return true;
                        }, 
                        const_cast<void*>(static_cast<const void*>(&param.valueStrings)), 
                        param.valueStrings.size())) {
            newValue = static_cast<float>(currentIndex) / (param.valueStrings.size() - 1);
        }
    } else {
        // Continuous parameter
        if (param.name.find("gain") != std::string::npos || 
            param.name.find("level") != std::string::npos ||
            param.name.find("volume") != std::string::npos) {
            // Use fader for gain-like parameters
            renderFader(param.displayName.c_str(), &newValue, param.minValue, param.maxValue);
        } else {
            // Use knob for other parameters
            renderKnob(param.displayName.c_str(), &newValue, param.minValue, param.maxValue);
        }
    }
    
    // Update parameter if changed
    if (newValue != currentValue) {
        selectedPlugin_->setParameter(param.id, newValue);
        parameterValues_[param.id] = newValue;
    }
    
    // Tooltip with parameter info
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", param.aiDescription.c_str());
        if (!param.units.empty()) {
            ImGui::Text("Units: %s", param.units.c_str());
        }
        ImGui::Text("Range: %.3f - %.3f", param.minValue, param.maxValue);
        ImGui::Text("Default: %.3f", param.defaultValue);
        ImGui::EndTooltip();
    }
    
    ImGui::PopID();
}

bool PluginControlPanel::renderKnob(const char* label, float* value, float min, float max, const char* format) {
    // Simple knob implementation using ImGui::SliderFloat
    return ImGui::SliderFloat(label, value, min, max, format);
}

bool PluginControlPanel::renderFader(const char* label, float* value, float min, float max, const ImVec2& size) {
    // Simple vertical fader using ImGui::VSliderFloat
    return ImGui::VSliderFloat(label, size, value, min, max);
}

void PluginControlPanel::renderAutomationControls() {
    ImGui::Text("Automation:");
    
    ImGui::Checkbox("Enable Recording", &isRecording_);
    
    if (isRecording_ && !recordingParameterId_.empty()) {
        ImGui::Text("Recording: %s", recordingParameterId_.c_str());
        if (ImGui::Button("Stop Recording")) {
            stopParameterRecording();
        }
    }
}

void PluginControlPanel::renderAIAssistance() {
    if (ImGui::CollapsingHeader("AI Assistance")) {
        ImGui::Text("AI parameter suggestions and automation assistance");
        
        if (ImGui::Button("Get AI Suggestions")) {
            // Request AI parameter suggestions
            core::Logger::info("AI assistance requested for: " + selectedPlugin_->getInfo().name);
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Optimize Parameters")) {
            // AI parameter optimization
            core::Logger::info("Parameter optimization requested");
        }
    }
}

void PluginControlPanel::setSelectedPlugin(std::shared_ptr<plugins::PluginInstance> plugin) {
    selectedPlugin_ = plugin;
    if (plugin) {
        parameters_ = plugin->getParameters();
        selectedPreset_ = plugin->getCurrentPreset();
        core::Logger::info("Plugin selected for control: " + plugin->getInfo().name);
    }
}

// ============================================================================
// Plugin AI Assistant Panel Implementation
// ============================================================================

PluginAIAssistantPanel::PluginAIAssistantPanel() {}

void PluginAIAssistantPanel::render() {
    if (!visible_) return;
    
    if (ImGui::Begin("AI Plugin Assistant", &visible_)) {
        renderQueryInterface();
        ImGui::Separator();
        
        renderStyleSelection();
        ImGui::Separator();
        
        renderToneModification();
        ImGui::Separator();
        
        renderRecommendations();
        
        if (!recommendations_.empty()) {
            ImGui::Separator();
            renderAnalysisResults();
        }
        
        renderWorkflowTemplates();
    }
    ImGui::End();
}

void PluginAIAssistantPanel::renderQueryInterface() {
    ImGui::Text("Ask the AI Assistant:");
    
    char queryBuffer[1024];
    strncpy(queryBuffer, userQuery_.c_str(), sizeof(queryBuffer) - 1);
    queryBuffer[sizeof(queryBuffer) - 1] = '\0';
    
    if (ImGui::InputTextMultiline("##query", queryBuffer, sizeof(queryBuffer), ImVec2(-1, 60))) {
        userQuery_ = queryBuffer;
    }
    
    if (ImGui::Button("Ask AI")) {
        processUserQuery();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Create Nirvana Setup")) {
        createFullNirvanaSetup();
    }
}

void PluginAIAssistantPanel::renderStyleSelection() {
    ImGui::Text("Musical Style:");
    
    const char* styles[] = {
        "Rock", "Pop", "Electronic", "Hip Hop", "Jazz", "Classical",
        "Blues", "Country", "Metal", "Reggae", "Funk", "Ambient",
        "Grunge", "Nirvana", "Alternative Rock", "90s Alternative"
    };
    
    static int selectedStyle = 0;
    if (ImGui::Combo("##style", &selectedStyle, styles, IM_ARRAYSIZE(styles))) {
        musicalStyle_ = styles[selectedStyle];
    }
}

void PluginAIAssistantPanel::renderToneModification() {
    if (ImGui::CollapsingHeader("Tone Modification")) {
        ImGui::Text("Transform your sound:");
        
        char sourceBuffer[256];
        strncpy(sourceBuffer, sourceDescription_.c_str(), sizeof(sourceBuffer) - 1);
        sourceBuffer[sizeof(sourceBuffer) - 1] = '\0';
        
        ImGui::Text("Source Description:");
        if (ImGui::InputText("##source", sourceBuffer, sizeof(sourceBuffer))) {
            sourceDescription_ = sourceBuffer;
        }
        
        char targetBuffer[256];
        strncpy(targetBuffer, targetStyle_.c_str(), sizeof(targetBuffer) - 1);
        targetBuffer[sizeof(targetBuffer) - 1] = '\0';
        
        ImGui::Text("Target Style:");
        if (ImGui::InputText("##target", targetBuffer, sizeof(targetBuffer))) {
            targetStyle_ = targetBuffer;
        }
        
        if (ImGui::Button("Create Tone Transformation")) {
            createToneTransformation();
        }
        
        if (toneTransformationActive_) {
            ImGui::SameLine();
            if (ImGui::Button("Apply Transformation")) {
                applyToneTransformation();
            }
            
            // Show transformation details
            ImGui::Separator();
            ImGui::Text("Transformation Analysis:");
            ImGui::TextWrapped("%s", lastTransformation_.analysis.c_str());
            ImGui::Text("Confidence: %.1f%%", lastTransformation_.confidenceScore * 100);
        }
    }
}

void PluginAIAssistantPanel::renderRecommendations() {
    if (ImGui::CollapsingHeader("Plugin Recommendations")) {
        if (ImGui::Button("Generate Recommendations")) {
            generateRecommendations();
        }
        
        // Show recommendations
        for (size_t i = 0; i < recommendations_.size(); ++i) {
            const auto& rec = recommendations_[i];
            
            ImGui::Separator();
            ImGui::Text("%d. %s", static_cast<int>(i + 1), rec.plugin.name.c_str());
            ImGui::Text("Relevance: %.1f%%", rec.relevanceScore * 100);
            ImGui::Text("Quality: %.1f%%", rec.qualityScore * 100);
            
            if (!rec.reasons.empty()) {
                ImGui::Text("Why recommended:");
                for (const auto& reason : rec.reasons) {
                    ImGui::BulletText("%s", reason.c_str());
                }
            }
            
            if (ImGui::Button(("Apply##" + std::to_string(i)).c_str())) {
                applyRecommendation(rec);
            }
        }
    }
}

void PluginAIAssistantPanel::renderAnalysisResults() {
    if (ImGui::CollapsingHeader("Analysis Results")) {
        if (!currentAnalysis_.empty()) {
            ImGui::TextWrapped("%s", currentAnalysis_.c_str());
        }
        
        if (!optimizationSuggestions_.empty()) {
            ImGui::Separator();
            ImGui::Text("Optimization Suggestions:");
            for (const auto& suggestion : optimizationSuggestions_) {
                ImGui::BulletText("%s", suggestion.c_str());
            }
        }
    }
}

void PluginAIAssistantPanel::renderWorkflowTemplates() {
    if (ImGui::CollapsingHeader("Workflow Templates")) {
        if (ImGui::Button("Nirvana Guitar")) {
            createNirvanaGuitar();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Nirvana Drums")) {
            createNirvanaDrums();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Nirvana Vocals")) {
            createNirvanaVocals();
        }
    }
}

void PluginAIAssistantPanel::createFullNirvanaSetup() {
    auto& intelligenceSystem = plugins::PluginIntelligenceSystem::getInstance();
    
    // Execute the Nirvana workflow
    auto workflowFuture = intelligenceSystem.executeWorkflow("nirvana_guitar", "full_setup");
    
    // In real implementation, this would be handled asynchronously
    core::Logger::info("Creating full Nirvana setup...");
    
    currentAnalysis_ = "Creating authentic Nirvana sound setup with guitar, drums, and vocal processing chains...";
    toneTransformationActive_ = true;
}

void PluginAIAssistantPanel::createNirvanaGuitar() {
    sourceDescription_ = "Clean electric guitar";
    targetStyle_ = "Nirvana grunge guitar tone";
    createToneTransformation();
}

void PluginAIAssistantPanel::createToneTransformation() {
    if (sourceDescription_.empty() || targetStyle_.empty()) {
        return;
    }
    
    auto& intelligenceSystem = plugins::PluginIntelligenceSystem::getInstance();
    auto& toneEngine = intelligenceSystem.getToneEngine();
    
    // Create tone target
    plugins::ToneModificationEngine::ToneTarget target;
    target.styleName = targetStyle_;
    target.instructions = "Create " + targetStyle_ + " from " + sourceDescription_;
    
    // Get available plugins
    auto& pluginHost = plugins::getGlobalPluginHost();
    auto availablePlugins = pluginHost.getAvailablePlugins();
    
    // Create transformation
    auto transformationFuture = toneEngine.createToneTransformation(
        sourceDescription_, target, availablePlugins);
    
    // In real implementation, this would be handled asynchronously
    // For demo, simulate the result
    lastTransformation_.analysis = "AI Tone Transformation:\n\n";
    lastTransformation_.analysis += "Converting '" + sourceDescription_ + "' to '" + targetStyle_ + "':\n";
    lastTransformation_.analysis += "• High-gain amplification for aggressive character\n";
    lastTransformation_.analysis += "• Midrange focus with controlled high frequencies\n"; 
    lastTransformation_.analysis += "• Dynamic compression for punch and sustain\n";
    lastTransformation_.analysis += "• EQ shaping for authentic grunge characteristics\n";
    
    lastTransformation_.confidenceScore = 0.88f;
    toneTransformationActive_ = true;
    
    core::Logger::info("Tone transformation created: " + sourceDescription_ + " -> " + targetStyle_);
}

void PluginAIAssistantPanel::processUserQuery() {
    if (userQuery_.empty()) return;
    
    analysisInProgress_ = true;
    
    // Simulate AI processing
    currentAnalysis_ = "Processing query: '" + userQuery_ + "'\n\n";
    currentAnalysis_ += "AI Response: Based on your query, I recommend the following approach...\n";
    
    core::Logger::info("Processing AI query: " + userQuery_);
    analysisInProgress_ = false;
}

// ============================================================================
// Plugin Manager UI Implementation
// ============================================================================

class PluginManagerUI::Impl {
public:
    std::unique_ptr<PluginBrowserPanel> browserPanel_;
    std::unique_ptr<PluginChainPanel> chainPanel_;
    std::unique_ptr<PluginControlPanel> controlPanel_;
    std::unique_ptr<PluginAIAssistantPanel> aiPanel_;
    std::unique_ptr<PluginPerformancePanel> performancePanel_;
    
    MainWindow* mainWindow_ = nullptr;
    bool initialized_ = false;
    
    bool initialize() {
        try {
            browserPanel_ = std::make_unique<PluginBrowserPanel>();
            chainPanel_ = std::make_unique<PluginChainPanel>();
            controlPanel_ = std::make_unique<PluginControlPanel>();
            aiPanel_ = std::make_unique<PluginAIAssistantPanel>();
            performancePanel_ = std::make_unique<PluginPerformancePanel>();
            
            initialized_ = true;
            return true;
        } catch (const std::exception& e) {
            core::Logger::error("Failed to initialize Plugin Manager UI: " + std::string(e.what()));
            return false;
        }
    }
};

PluginManagerUI::PluginManagerUI() : pImpl_(std::make_unique<Impl>()) {}
PluginManagerUI::~PluginManagerUI() = default;

bool PluginManagerUI::initialize() {
    return pImpl_->initialize();
}

void PluginManagerUI::render() {
    if (!pImpl_->initialized_) return;
    
    pImpl_->browserPanel_->render();
    pImpl_->chainPanel_->render();
    pImpl_->controlPanel_->render();
    pImpl_->aiPanel_->render();
    pImpl_->performancePanel_->render();
}

void PluginManagerUI::executeAIWorkflow(const std::string& workflowName, const std::string& parameters) {
    auto& intelligenceSystem = plugins::PluginIntelligenceSystem::getInstance();
    auto workflowFuture = intelligenceSystem.executeWorkflow(workflowName, parameters);
    
    core::Logger::info("Executing AI workflow: " + workflowName);
}

void PluginManagerUI::createNirvanaWorkflow() {
    executeAIWorkflow("nirvana_guitar", "full_setup");
}

// ============================================================================
// Plugin Performance Panel Stub Implementation
// ============================================================================

PluginPerformancePanel::PluginPerformancePanel() {}

void PluginPerformancePanel::render() {
    if (!visible_) return;
    
    if (ImGui::Begin("Plugin Performance", &visible_)) {
        updateMetrics();
        renderOverallStats();
        renderPerPluginStats();
    }
    ImGui::End();
}

void PluginPerformancePanel::renderOverallStats() {
    ImGui::Text("Overall Performance:");
    ImGui::Text("Total CPU: %.1f%%", currentStats_.totalCpuUsage);
    ImGui::Text("Peak CPU: %.1f%%", currentStats_.peakCpuUsage);
    ImGui::Text("Active Plugins: %d", currentStats_.activePluginCount);
}

void PluginPerformancePanel::renderPerPluginStats() {
    if (showPerPluginStats_ && !currentStats_.pluginCpuUsage.empty()) {
        ImGui::Separator();
        ImGui::Text("Per-Plugin Statistics:");
        
        for (const auto& [uid, usage] : currentStats_.pluginCpuUsage) {
            ImGui::Text("%s: %.1f%% CPU", uid.c_str(), usage);
        }
    }
}

void PluginPerformancePanel::updateMetrics() {
    auto& pluginHost = plugins::getGlobalPluginHost();
    currentStats_ = pluginHost.getPerformanceStats();
}

// ============================================================================
// Global Plugin UI Access
// ============================================================================

static std::unique_ptr<PluginManagerUI> g_pluginManagerUI;

PluginManagerUI& getGlobalPluginUI() {
    if (!g_pluginManagerUI) {
        g_pluginManagerUI = std::make_unique<PluginManagerUI>();
    }
    return *g_pluginManagerUI;
}

bool initializeGlobalPluginUI() {
    auto& ui = getGlobalPluginUI();
    bool success = ui.initialize();
    
    if (success) {
        core::Logger::info("Global Plugin UI initialized successfully");
    } else {
        core::Logger::error("Failed to initialize Global Plugin UI");
    }
    
    return success;
}

void shutdownGlobalPluginUI() {
    if (g_pluginManagerUI) {
        g_pluginManagerUI->shutdown();
        g_pluginManagerUI.reset();
        core::Logger::info("Global Plugin UI shut down");
    }
}

} // namespace mixmind::ui