#pragma once

#include "../core/result.h"
#include "../audio/RealtimeAudioEngine.h"
#include "../plugins/RealVST3Plugin.h"
#include "../automation/ParameterAutomation.h"
#include "../performance/PerformanceMonitor.h"
#include "../services/RealOpenAIService.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

// Forward declarations for Dear ImGui
struct ImDrawData;
struct ImGuiIO;

namespace mixmind::ui {

// ============================================================================
// Professional DAW Main Window Interface
// ============================================================================

class MixMindMainWindow {
public:
    MixMindMainWindow();
    ~MixMindMainWindow();
    
    // Non-copyable
    MixMindMainWindow(const MixMindMainWindow&) = delete;
    MixMindMainWindow& operator=(const MixMindMainWindow&) = delete;
    
    // Window lifecycle
    mixmind::core::Result<void> initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Main render loop
    void render();
    void handleEvents();
    bool shouldClose() const;
    
    // Engine integration
    void setAudioEngine(std::shared_ptr<audio::RealtimeAudioEngine> engine);
    void setAutomationManager(std::shared_ptr<automation::ParameterAutomationManager> automation);
    void setPerformanceMonitor(std::shared_ptr<performance::PerformanceMonitor> monitor);
    void setOpenAIService(std::shared_ptr<services::RealOpenAIService> aiService);
    
private:
    // UI Components
    void renderMainMenuBar();
    void renderTransportControls();
    void renderMixerPanel();
    void renderTrackPanel();
    void renderPluginRack();
    void renderAutomationEditor();
    void renderPerformanceMonitor();
    void renderAIAssistant();
    void renderProjectBrowser();
    void renderStatusBar();
    
    // Plugin Management UI
    void renderPluginBrowser();
    void renderPluginEditor(const std::string& pluginId);
    void renderPluginChain();
    
    // Audio Device UI
    void renderAudioSettings();
    void renderDeviceSelector();
    void renderLatencyAnalyzer();
    
    // Theme and styling
    void setupDarkTheme();
    void setupProfessionalColors();
    void renderThemeSelector();
    
    // Window management
    void setupDocking();
    void saveLayout();
    void loadLayout();
    
    // Real-time data visualization
    void renderAudioLevels();
    void renderSpectrumAnalyzer();
    void renderPhaseScope();
    void renderPerformanceGraphs();
    
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Professional Transport Bar
// ============================================================================

class TransportBar {
public:
    TransportBar();
    ~TransportBar();
    
    struct TransportState {
        bool isPlaying = false;
        bool isRecording = false;
        bool isLooping = false;
        double currentTimeSeconds = 0.0;
        double songLengthSeconds = 0.0;
        double tempo = 120.0;
        int timeSignatureNumerator = 4;
        int timeSignatureDenominator = 4;
        std::string timecodeFormat = "MMM:SS:FFF"; // Minutes:Seconds:Frames
    };
    
    void render();
    void setTransportState(const TransportState& state);
    
    // Transport callbacks
    using TransportCallback = std::function<void()>;
    void setPlayCallback(TransportCallback callback);
    void setStopCallback(TransportCallback callback);
    void setRecordCallback(TransportCallback callback);
    void setRewindCallback(TransportCallback callback);
    void setFastForwardCallback(TransportCallback callback);
    void setLoopCallback(TransportCallback callback);
    
    // Position callbacks  
    using PositionCallback = std::function<void(double seconds)>;
    void setPositionCallback(PositionCallback callback);
    
    using TempoCallback = std::function<void(double bpm)>;
    void setTempoCallback(TempoCallback callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Professional Mixer Panel
// ============================================================================

class MixerPanel {
public:
    MixerPanel();
    ~MixerPanel();
    
    struct ChannelStrip {
        std::string channelId;
        std::string channelName;
        float volume = 0.0f; // dB
        float pan = 0.0f;    // -1.0 to 1.0
        bool mute = false;
        bool solo = false;
        bool record = false;
        bool monitor = false;
        
        // Metering
        float peakLevelL = -96.0f; // dB
        float peakLevelR = -96.0f; // dB
        float rmsLevelL = -96.0f;
        float rmsLevelR = -96.0f;
        float lufsLevel = -23.0f;
        
        // EQ
        struct EQBand {
            bool enabled = true;
            float frequency = 1000.0f;
            float gain = 0.0f;
            float q = 1.0f;
            enum Type { HIGHPASS, LOWSHELF, BELL, HIGHSHELF, LOWPASS } type = BELL;
        };
        std::vector<EQBand> eqBands;
        
        // Send levels
        std::unordered_map<std::string, float> sendLevels;
    };
    
    void render();
    void setChannelCount(int count);
    void updateChannelStrip(int channelIndex, const ChannelStrip& strip);
    
    // Mixer callbacks
    using VolumeCallback = std::function<void(int channel, float volume)>;
    using PanCallback = std::function<void(int channel, float pan)>;
    using MuteCallback = std::function<void(int channel, bool mute)>;
    using SoloCallback = std::function<void(int channel, bool solo)>;
    using EQCallback = std::function<void(int channel, int band, const ChannelStrip::EQBand& eq)>;
    
    void setVolumeCallback(VolumeCallback callback);
    void setPanCallback(PanCallback callback);
    void setMuteCallback(MuteCallback callback);
    void setSoloCallback(SoloCallback callback);
    void setEQCallback(EQCallback callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Professional Plugin Rack Interface
// ============================================================================

class PluginRackPanel {
public:
    PluginRackPanel();
    ~PluginRackPanel();
    
    struct PluginSlot {
        std::string slotId;
        std::string pluginId;
        std::string pluginName;
        std::string manufacturer;
        bool isLoaded = false;
        bool isBypassed = false;
        bool isEnabled = true;
        float cpuUsage = 0.0f;
        int latencySamples = 0;
        
        // Plugin editor state
        bool editorOpen = false;
        int editorWidth = 400;
        int editorHeight = 300;
    };
    
    void render();
    void setPluginSlots(const std::vector<PluginSlot>& slots);
    void updatePluginSlot(int slotIndex, const PluginSlot& slot);
    
    // Plugin management callbacks
    using LoadPluginCallback = std::function<void(int slot, const std::string& pluginPath)>;
    using UnloadPluginCallback = std::function<void(int slot)>;
    using BypassPluginCallback = std::function<void(int slot, bool bypass)>;
    using OpenEditorCallback = std::function<void(int slot)>;
    using CloseEditorCallback = std::function<void(int slot)>;
    using MovePluginCallback = std::function<void(int fromSlot, int toSlot)>;
    
    void setLoadPluginCallback(LoadPluginCallback callback);
    void setUnloadPluginCallback(UnloadPluginCallback callback);
    void setBypassPluginCallback(BypassPluginCallback callback);
    void setOpenEditorCallback(OpenEditorCallback callback);
    void setCloseEditorCallback(CloseEditorCallback callback);
    void setMovePluginCallback(MovePluginCallback callback);
    
private:
    void renderPluginBrowser();
    void renderPluginSlot(int index, PluginSlot& slot);
    void renderDragDropTarget(int slotIndex);
    
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Professional Automation Editor
// ============================================================================

class AutomationEditor {
public:
    AutomationEditor();
    ~AutomationEditor();
    
    struct AutomationLaneView {
        std::string laneId;
        std::string parameterName;
        std::string targetName;
        bool isVisible = true;
        bool isSelected = false;
        float height = 100.0f;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        std::string units;
        
        // Visual properties
        uint32_t color = 0xFF4CAF50; // RGBA
        float lineWidth = 2.0f;
        bool showPoints = true;
        bool showGrid = true;
    };
    
    void render();
    void setTimeRange(double startSeconds, double endSeconds);
    void setZoom(float horizontalZoom, float verticalZoom);
    void addAutomationLane(const AutomationLaneView& lane);
    void removeAutomationLane(const std::string& laneId);
    void updateAutomationLane(const std::string& laneId, const AutomationLaneView& lane);
    
    // Editing callbacks
    using AddPointCallback = std::function<void(const std::string& laneId, double time, float value)>;
    using RemovePointCallback = std::function<void(const std::string& laneId, int pointIndex)>;
    using MovePointCallback = std::function<void(const std::string& laneId, int pointIndex, double time, float value)>;
    using SelectPointCallback = std::function<void(const std::string& laneId, int pointIndex, bool selected)>;
    
    void setAddPointCallback(AddPointCallback callback);
    void setRemovePointCallback(RemovePointCallback callback);
    void setMovePointCallback(MovePointCallback callback);
    void setSelectPointCallback(SelectPointCallback callback);
    
    // Tools and modes
    enum class EditMode {
        SELECT,
        PENCIL,
        LINE,
        CURVE,
        ERASE
    };
    
    void setEditMode(EditMode mode);
    EditMode getEditMode() const;
    
private:
    void renderTimeline();
    void renderAutomationLane(const AutomationLaneView& lane);
    void renderAutomationPoints(const std::string& laneId);
    void renderAutomationCurves(const std::string& laneId);
    void handleMouseInput();
    void handleKeyboardInput();
    
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// AI Assistant Panel
// ============================================================================

class AIAssistantPanel {
public:
    AIAssistantPanel();
    ~AIAssistantPanel();
    
    struct ChatMessage {
        enum Type { USER, ASSISTANT, SYSTEM } type;
        std::string content;
        std::chrono::system_clock::time_point timestamp;
        std::string metadata; // For structured data
    };
    
    struct AICapability {
        std::string name;
        std::string description;
        bool isAvailable = false;
        float confidence = 0.0f;
    };
    
    void render();
    void addMessage(const ChatMessage& message);
    void clearMessages();
    
    // AI service integration
    void setAvailableCapabilities(const std::vector<AICapability>& capabilities);
    void setProcessingState(bool isProcessing);
    
    // AI callbacks
    using SendMessageCallback = std::function<void(const std::string& message)>;
    using AnalyzeProjectCallback = std::function<void()>;
    using SuggestPluginsCallback = std::function<void(const std::string& genre, const std::string& mood)>;
    using GenerateMelodyCallback = std::function<void(const std::string& style, const std::string& key)>;
    
    void setSendMessageCallback(SendMessageCallback callback);
    void setAnalyzeProjectCallback(AnalyzeProjectCallback callback);
    void setSuggestPluginsCallback(SuggestPluginsCallback callback);
    void setGenerateMelodyCallback(GenerateMelodyCallback callback);
    
private:
    void renderChatArea();
    void renderInputArea();
    void renderCapabilitiesPanel();
    void renderQuickActions();
    
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Performance Monitor Panel
// ============================================================================

class PerformanceMonitorPanel {
public:
    PerformanceMonitorPanel();
    ~PerformanceMonitorPanel();
    
    void render();
    void updateMetrics(const performance::SystemMetrics& system,
                      const performance::AudioEngineMetrics& audio);
    void updatePluginMetrics(const std::vector<performance::PluginMetrics>& plugins);
    
    // Display options
    void setUpdateInterval(std::chrono::milliseconds interval);
    void setHistoryDuration(std::chrono::seconds duration);
    void setAlertThresholds(float cpuThreshold, float memoryThreshold, float latencyThreshold);
    
private:
    void renderSystemMetrics();
    void renderAudioEngineMetrics();
    void renderPluginPerformance();
    void renderPerformanceGraphs();
    void renderOptimizationSuggestions();
    void renderAlerts();
    
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace mixmind::ui