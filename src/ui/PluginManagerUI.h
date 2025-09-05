#pragma once

#include "../plugins/PluginHost.h"
#include "../plugins/PluginIntelligence.h"
#include "MainWindow.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// Forward declare ImGui types
struct ImVec2;
struct ImVec4;

namespace mixmind::ui {

// ============================================================================
// Plugin Browser Panel - Browse and Load Plugins
// ============================================================================

class PluginBrowserPanel : public UIPanel {
public:
    PluginBrowserPanel();
    ~PluginBrowserPanel() override = default;
    
    // UIPanel interface
    void render() override;
    const char* getName() const override { return "Plugin Browser"; }
    bool isVisible() const override { return visible_; }
    void setVisible(bool visible) override { visible_ = visible; }
    
    // Plugin browser specific
    void refreshPluginList();
    void scanForNewPlugins();
    
private:
    bool visible_ = true;
    
    // Plugin management
    std::vector<plugins::PluginInfo> availablePlugins_;
    std::vector<plugins::PluginInfo> filteredPlugins_;
    std::string searchFilter_;
    plugins::PluginCategory selectedCategory_ = plugins::PluginCategory::EFFECT;
    plugins::PluginQuality minQuality_ = plugins::PluginQuality::POOR;
    
    // UI state
    bool showOnlyFavorites_ = false;
    bool showAIRecommendations_ = true;
    int selectedPluginIndex_ = -1;
    bool scanInProgress_ = false;
    
    // Rendering helpers
    void renderSearchBar();
    void renderCategoryFilter();
    void renderQualityFilter();
    void renderPluginList();
    void renderPluginDetails(const plugins::PluginInfo& plugin);
    void renderPluginActions(const plugins::PluginInfo& plugin);
    void renderScanProgress();
    
    // Actions
    void loadPlugin(const plugins::PluginInfo& plugin);
    void toggleFavorite(const plugins::PluginInfo& plugin);
    void openPluginFolder(const plugins::PluginInfo& plugin);
    
    // Filtering
    void applyFilters();
    bool matchesFilter(const plugins::PluginInfo& plugin) const;
};

// ============================================================================
// Plugin Chain Panel - Manage Plugin Chains
// ============================================================================

class PluginChainPanel : public UIPanel {
public:
    PluginChainPanel();
    ~PluginChainPanel() override = default;
    
    // UIPanel interface
    void render() override;
    const char* getName() const override { return "Plugin Chain"; }
    bool isVisible() const override { return visible_; }
    void setVisible(bool visible) override { visible_ = visible; }
    
    // Chain management
    void setCurrentChain(const std::string& chainId);
    void createNewChain();
    void duplicateChain(const std::string& chainId);
    void deleteChain(const std::string& chainId);
    
private:
    bool visible_ = true;
    
    // Chain state
    std::string currentChainId_;
    plugins::PluginChain currentChain_;
    std::vector<std::string> availableChains_;
    
    // UI state
    int selectedSlotIndex_ = -1;
    bool showChainSettings_ = false;
    bool showPerformanceMetrics_ = true;
    
    // Drag and drop
    int draggedSlotIndex_ = -1;
    bool isDragging_ = false;
    
    // Rendering helpers
    void renderChainSelector();
    void renderChainControls();
    void renderPluginSlots();
    void renderSlot(const plugins::PluginSlot& slot, int index);
    void renderSlotControls(plugins::PluginSlot& slot, int index);
    void renderChainSettings();
    void renderPerformanceMetrics();
    void renderAIOptimizations();
    
    // Slot actions
    void moveSlot(int fromIndex, int toIndex);
    void removeSlot(int index);
    void bypassSlot(int index, bool bypass);
    void adjustSlotGain(int index, float inputGain, float outputGain);
    void adjustWetDryMix(int index, float mix);
    
    // Chain operations
    void optimizeChain();
    void analyzeChain();
    void exportChain();
    void importChain();
};

// ============================================================================
// Plugin Control Panel - Real-time Plugin Parameter Control
// ============================================================================

class PluginControlPanel : public UIPanel {
public:
    PluginControlPanel();
    ~PluginControlPanel() override = default;
    
    // UIPanel interface  
    void render() override;
    const char* getName() const override { return "Plugin Control"; }
    bool isVisible() const override { return visible_; }
    void setVisible(bool visible) override { visible_ = visible; }
    
    // Plugin selection
    void setSelectedPlugin(std::shared_ptr<plugins::PluginInstance> plugin);
    
private:
    bool visible_ = true;
    
    // Plugin state
    std::shared_ptr<plugins::PluginInstance> selectedPlugin_;
    std::vector<plugins::PluginParameter> parameters_;
    std::unordered_map<std::string, float> parameterValues_;
    
    // UI state
    std::string selectedPreset_;
    bool showAdvancedControls_ = false;
    bool enableAIAssistance_ = true;
    std::string searchFilter_;
    
    // Automation
    std::string recordingParameterId_;
    bool isRecording_ = false;
    
    // Rendering helpers
    void renderPluginInfo();
    void renderPresetSelector();
    void renderParameterControls();
    void renderParameter(const plugins::PluginParameter& param);
    void renderAutomationControls();
    void renderAIAssistance();
    void renderPluginUI();
    
    // Parameter control widgets
    bool renderKnob(const char* label, float* value, float min, float max, const char* format = "%.2f");
    bool renderFader(const char* label, float* value, float min, float max, const ImVec2& size = ImVec2(30, 120));
    bool renderButton(const char* label, bool* value);
    bool renderDropdown(const char* label, int* value, const std::vector<std::string>& options);
    
    // Actions
    void loadPreset(const std::string& presetName);
    void savePreset(const std::string& presetName);
    void resetToDefault();
    void startParameterRecording(const std::string& parameterId);
    void stopParameterRecording();
};

// ============================================================================
// Plugin AI Assistant Panel - AI-Powered Plugin Recommendations and Analysis
// ============================================================================

class PluginAIAssistantPanel : public UIPanel {
public:
    PluginAIAssistantPanel();
    ~PluginAIAssistantPanel() override = default;
    
    // UIPanel interface
    void render() override;
    const char* getName() const override { return "AI Plugin Assistant"; }
    bool isVisible() const override { return visible_; }
    void setVisible(bool visible) override { visible_ = visible; }
    
    // AI assistance features
    void analyzeCurrentSetup();
    void generateRecommendations();
    void optimizePluginChain();
    
private:
    bool visible_ = true;
    
    // AI state
    std::vector<plugins::PluginAI::PluginRecommendation> recommendations_;
    std::string currentAnalysis_;
    std::vector<std::string> optimizationSuggestions_;
    bool analysisInProgress_ = false;
    
    // User interaction
    std::string userQuery_;
    std::string musicalStyle_;
    std::string currentGoal_;
    
    // Tone modification
    std::string sourceDescription_;
    std::string targetStyle_;
    plugins::ToneModificationEngine::ToneTransformation lastTransformation_;
    bool toneTransformationActive_ = false;
    
    // Rendering helpers
    void renderQueryInterface();
    void renderStyleSelection();
    void renderToneModification();
    void renderRecommendations();
    void renderAnalysisResults();
    void renderOptimizationSuggestions();
    void renderWorkflowTemplates();
    
    // AI actions
    void processUserQuery();
    void applyRecommendation(const plugins::PluginAI::PluginRecommendation& recommendation);
    void createToneTransformation();
    void applyToneTransformation();
    void generateWorkflowTemplate();
    
    // Nirvana-specific features
    void createNirvanaGuitar();
    void createNirvanaDrums();
    void createNirvanaVocals();
    void createFullNirvanaSetup();
};

// ============================================================================
// Plugin Performance Monitor Panel - Real-time Performance Analysis
// ============================================================================

class PluginPerformancePanel : public UIPanel {
public:
    PluginPerformancePanel();
    ~PluginPerformancePanel() override = default;
    
    // UIPanel interface
    void render() override;
    const char* getName() const override { return "Performance Monitor"; }
    bool isVisible() const override { return visible_; }
    void setVisible(bool visible) override { visible_ = visible; }
    
    // Performance monitoring
    void updateMetrics();
    void resetMetrics();
    void exportPerformanceReport();
    
private:
    bool visible_ = true;
    
    // Performance data
    plugins::PluginHost::PerformanceStats currentStats_;
    std::vector<double> cpuHistory_;
    std::vector<double> memoryHistory_;
    std::vector<int> latencyHistory_;
    
    // Display settings  
    int historyLength_ = 300; // 5 minutes at 60fps
    bool showCPUGraph_ = true;
    bool showMemoryGraph_ = true;
    bool showLatencyGraph_ = true;
    bool showPerPluginStats_ = true;
    
    // Alert thresholds
    double cpuWarningThreshold_ = 70.0;
    double cpuCriticalThreshold_ = 90.0;
    double memoryWarningThreshold_ = 80.0;
    int latencyWarningThreshold_ = 512;
    
    // Rendering helpers
    void renderOverallStats();
    void renderCPUGraph();
    void renderMemoryGraph();
    void renderLatencyGraph();
    void renderPerPluginStats();
    void renderAlerts();
    void renderOptimizationSuggestions();
    
    // Utility functions
    void addDataPoint(std::vector<double>& history, double value);
    void addDataPoint(std::vector<int>& history, int value);
    bool hasPerformanceIssues() const;
    std::vector<std::string> getOptimizationSuggestions() const;
};

// ============================================================================
// Main Plugin Manager UI - Integrated Plugin Management Interface  
// ============================================================================

class PluginManagerUI {
public:
    PluginManagerUI();
    ~PluginManagerUI();
    
    // Non-copyable
    PluginManagerUI(const PluginManagerUI&) = delete;
    PluginManagerUI& operator=(const PluginManagerUI&) = delete;
    
    // Main interface
    bool initialize();
    void shutdown();
    void render();
    
    // Panel management
    void showPanel(const std::string& panelName, bool show = true);
    bool isPanelVisible(const std::string& panelName) const;
    void resetLayout();
    
    // Integration with main DAW
    void setMainWindow(MainWindow* mainWindow);
    void integrateWithTransport(TransportPanel* transport);
    void integrateWithMixer(MixerPanel* mixer);
    
    // Plugin operations
    void loadPlugin(const plugins::PluginInfo& plugin, const std::string& chainId = "");
    void unloadPlugin(const std::string& pluginUid);
    void openPluginUI(const std::string& pluginUid);
    
    // AI operations
    void executeAIWorkflow(const std::string& workflowName, const std::string& parameters = "");
    void createNirvanaWorkflow();
    void optimizeCurrentSetup();
    
    // Import/Export
    void exportPluginSetup(const std::string& filePath);
    void importPluginSetup(const std::string& filePath);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Plugin UI Utilities
// ============================================================================

namespace plugin_ui_utils {
    
    // ImGui extensions for plugin UI
    bool PluginKnob(const char* label, float* value, float min, float max, 
                   const ImVec2& size = ImVec2(50, 50), const char* format = "%.2f");
    
    bool PluginFader(const char* label, float* value, float min, float max,
                    const ImVec2& size = ImVec2(30, 120), bool vertical = true);
    
    bool PluginButton(const char* label, bool* pressed, const ImVec2& size = ImVec2(80, 30));
    
    bool PluginToggle(const char* label, bool* value);
    
    bool PluginMeter(const char* label, float level, float min, float max,
                    const ImVec2& size = ImVec2(20, 100), bool horizontal = false);
    
    void PluginSeparator();
    
    void PluginSpacing(float height = -1.0f);
    
    // Plugin-specific colors and styling
    struct PluginTheme {
        ImVec4 knobColor = ImVec4(0.2f, 0.7f, 1.0f, 1.0f);        // Blue
        ImVec4 faderColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);       // Green  
        ImVec4 buttonColor = ImVec4(0.8f, 0.4f, 0.2f, 1.0f);      // Orange
        ImVec4 meterColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);       // Yellow
        ImVec4 backgroundColor = ImVec4(0.15f, 0.15f, 0.15f, 1.0f); // Dark gray
        ImVec4 textColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);        // Light gray
        ImVec4 accentColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);      // Red accent
    };
    
    extern PluginTheme g_pluginTheme;
    
    void SetPluginTheme(const PluginTheme& theme);
    void ApplyPluginStyling();
    void ResetPluginStyling();
    
    // Layout helpers
    void BeginPluginGroup(const char* label);
    void EndPluginGroup();
    
    void BeginPluginColumns(int count, const char* id = nullptr);
    void NextPluginColumn();
    void EndPluginColumns();
    
    // Draw utilities
    void DrawPluginWaveform(const float* samples, int numSamples, const ImVec2& size);
    void DrawPluginSpectrum(const float* spectrum, int numBins, const ImVec2& size);
    void DrawPluginEnvelope(const std::vector<std::pair<float, float>>& points, const ImVec2& size);
    
    // Text utilities
    void TextPluginCentered(const char* text);
    void TextPluginColored(const ImVec4& color, const char* text);
    void TextPluginWithIcon(const char* icon, const char* text);
    
    // Tooltip system for plugins
    void SetPluginTooltip(const char* text);
    void SetPluginTooltipAdvanced(const char* title, const char* description, const char* hotkey = nullptr);
}

// ============================================================================
// Global Plugin UI Access
// ============================================================================

// Get global plugin manager UI (singleton)
PluginManagerUI& getGlobalPluginUI();

// Initialize plugin UI system (call at startup)
bool initializeGlobalPluginUI();

// Shutdown plugin UI system (call at exit)
void shutdownGlobalPluginUI();

} // namespace mixmind::ui