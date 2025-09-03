#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../api/ActionAPI.h"
#include "../api/RESTServer.h"
#include "../api/WebSocketServer.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>

namespace mixmind::ui {

using json = nlohmann::json;

// ============================================================================
// Modern Web UI - React-based frontend with C++ backend integration
// ============================================================================

class WebUI {
public:
    WebUI(
        std::shared_ptr<api::ActionAPI> actionAPI,
        std::shared_ptr<api::RESTServer> restServer,
        std::shared_ptr<api::WebSocketServer> wsServer
    );
    ~WebUI();
    
    // Non-copyable, movable
    WebUI(const WebUI&) = delete;
    WebUI& operator=(const WebUI&) = delete;
    WebUI(WebUI&&) = default;
    WebUI& operator=(WebUI&&) = default;
    
    // ========================================================================
    // UI Server Management
    // ========================================================================
    
    /// UI server configuration
    struct UIConfig {
        std::string host = "localhost";
        int port = 3000;
        std::string staticFilesPath = "./web/build";
        bool enableHotReload = false;
        bool enableDevMode = false;
        std::string theme = "dark";
        std::string primaryColor = "#6366F1";
        std::string accentColor = "#EC4899";
        bool enableAnalytics = false;
        std::string analyticsId;
        std::vector<std::string> allowedOrigins = {"http://localhost:3000"};
    };
    
    /// Start the web UI server
    core::AsyncResult<core::VoidResult> start(const UIConfig& config = UIConfig{});
    
    /// Stop the web UI server
    core::AsyncResult<core::VoidResult> stop();
    
    /// Check if UI server is running
    bool isRunning() const;
    
    /// Get UI server URL
    std::string getServerURL() const;
    
    /// Update UI configuration
    core::VoidResult updateConfig(const UIConfig& config);
    
    /// Get current configuration
    UIConfig getConfig() const;
    
    // ========================================================================
    // UI Component Management
    // ========================================================================
    
    /// UI component types
    enum class ComponentType {
        Transport,
        TrackList,
        Mixer,
        Timeline,
        PluginRack,
        Browser,
        Chat,
        Analyzer,
        Settings,
        Custom
    };
    
    /// UI component configuration
    struct ComponentConfig {
        ComponentType type;
        std::string id;
        std::string title;
        json props = json::object();
        json state = json::object();
        bool visible = true;
        bool resizable = true;
        bool draggable = true;
        struct {
            int x = 0, y = 0;
            int width = 300, height = 200;
            int minWidth = 200, minHeight = 150;
            int maxWidth = -1, maxHeight = -1;
        } layout;
    };
    
    /// Register UI component
    core::VoidResult registerComponent(const ComponentConfig& config);
    
    /// Update component state
    core::VoidResult updateComponentState(const std::string& componentId, const json& state);
    
    /// Update component props
    core::VoidResult updateComponentProps(const std::string& componentId, const json& props);
    
    /// Show/hide component
    core::VoidResult setComponentVisible(const std::string& componentId, bool visible);
    
    /// Get component configuration
    std::optional<ComponentConfig> getComponent(const std::string& componentId) const;
    
    /// Get all components
    std::vector<ComponentConfig> getAllComponents() const;
    
    /// Remove component
    core::VoidResult removeComponent(const std::string& componentId);
    
    // ========================================================================
    // Layout Management
    // ========================================================================
    
    /// UI layout types
    enum class LayoutType {
        Grid,
        Flex,
        Tabs,
        Panels,
        Dock,
        Custom
    };
    
    /// Layout configuration
    struct LayoutConfig {
        LayoutType type;
        std::string name;
        json configuration;
        std::vector<std::string> componentIds;
        bool isDefault = false;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point lastModified;
    };
    
    /// Save current layout
    core::VoidResult saveLayout(const std::string& layoutName);
    
    /// Load layout
    core::VoidResult loadLayout(const std::string& layoutName);
    
    /// Get available layouts
    std::vector<LayoutConfig> getAvailableLayouts() const;
    
    /// Delete layout
    core::VoidResult deleteLayout(const std::string& layoutName);
    
    /// Set default layout
    core::VoidResult setDefaultLayout(const std::string& layoutName);
    
    /// Reset to default layout
    core::VoidResult resetToDefaultLayout();
    
    // ========================================================================
    // Theme and Styling
    // ========================================================================
    
    /// UI theme configuration
    struct ThemeConfig {
        std::string name;
        std::string displayName;
        struct {
            std::string background = "#0F0F0F";
            std::string surface = "#1A1A1A";
            std::string primary = "#6366F1";
            std::string secondary = "#EC4899";
            std::string accent = "#F59E0B";
            std::string text = "#FFFFFF";
            std::string textSecondary = "#9CA3AF";
            std::string border = "#374151";
            std::string success = "#10B981";
            std::string warning = "#F59E0B";
            std::string error = "#EF4444";
            std::string info = "#3B82F6";
        } colors;
        
        struct {
            std::string primary = "'Inter', sans-serif";
            std::string monospace = "'JetBrains Mono', monospace";
        } fonts;
        
        struct {
            int small = 4;
            int medium = 8;
            int large = 16;
            int xlarge = 24;
        } spacing;
        
        struct {
            int small = 4;
            int medium = 8;
            int large = 16;
        } borderRadius;
        
        json customProperties = json::object();
    };
    
    /// Get current theme
    ThemeConfig getCurrentTheme() const;
    
    /// Set theme
    core::VoidResult setTheme(const std::string& themeName);
    
    /// Get available themes
    std::vector<ThemeConfig> getAvailableThemes() const;
    
    /// Create custom theme
    core::VoidResult createCustomTheme(const ThemeConfig& theme);
    
    /// Update theme colors
    core::VoidResult updateThemeColors(const json& colorUpdates);
    
    // ========================================================================
    // Real-time Data Streaming
    // ========================================================================
    
    /// Data stream types
    enum class StreamType {
        Spectrum,
        Waveform,
        Meters,
        Transport,
        TrackStates,
        PluginParams,
        ChatMessages,
        Notifications
    };
    
    /// Start data stream
    core::VoidResult startDataStream(StreamType type, int32_t updateRate = 30); // Hz
    
    /// Stop data stream
    core::VoidResult stopDataStream(StreamType type);
    
    /// Check if stream is active
    bool isStreamActive(StreamType type) const;
    
    /// Set stream update rate
    core::VoidResult setStreamUpdateRate(StreamType type, int32_t rate);
    
    /// Broadcast data to all connected clients
    void broadcastData(StreamType type, const json& data);
    
    // ========================================================================
    // User Interface State
    // ========================================================================
    
    /// UI state management
    struct UIState {
        std::string currentView = "main";
        json viewStates = json::object();
        json userPreferences = json::object();
        json componentStates = json::object();
        std::string activeTheme = "dark";
        std::string currentLayout = "default";
        bool isFullscreen = false;
        json customState = json::object();
    };
    
    /// Get current UI state
    UIState getUIState() const;
    
    /// Update UI state
    core::VoidResult updateUIState(const UIState& state);
    
    /// Set user preference
    core::VoidResult setUserPreference(const std::string& key, const json& value);
    
    /// Get user preference
    json getUserPreference(const std::string& key, const json& defaultValue = json{}) const;
    
    /// Save UI state to file
    core::VoidResult saveUIState(const std::string& filePath = "");
    
    /// Load UI state from file
    core::VoidResult loadUIState(const std::string& filePath = "");
    
    // ========================================================================
    // Chat Interface Integration
    // ========================================================================
    
    /// Chat message for UI
    struct UIChatMessage {
        std::string id;
        std::string type; // user, assistant, system, error
        std::string content;
        json metadata = json::object();
        std::chrono::system_clock::time_point timestamp;
        bool isTyping = false;
        std::vector<std::string> attachments;
    };
    
    /// Send chat message from UI
    core::AsyncResult<api::ActionResult> sendChatMessage(
        const std::string& message,
        const json& context = json::object()
    );
    
    /// Get chat history
    std::vector<UIChatMessage> getChatHistory(int32_t maxMessages = 50) const;
    
    /// Clear chat history
    void clearChatHistory();
    
    /// Set typing indicator
    void setTypingIndicator(bool typing);
    
    // ========================================================================
    // Notification System
    // ========================================================================
    
    /// Notification types
    enum class NotificationType {
        Info,
        Success,
        Warning,
        Error,
        Progress
    };
    
    /// UI notification
    struct UINotification {
        std::string id;
        NotificationType type;
        std::string title;
        std::string message;
        json data = json::object();
        int32_t duration = 5000; // ms, -1 for persistent
        bool dismissible = true;
        std::vector<struct {
            std::string label;
            std::string action;
            json parameters;
        }> actions;
        std::chrono::system_clock::time_point timestamp;
    };
    
    /// Show notification
    std::string showNotification(const UINotification& notification);
    
    /// Dismiss notification
    core::VoidResult dismissNotification(const std::string& notificationId);
    
    /// Get active notifications
    std::vector<UINotification> getActiveNotifications() const;
    
    /// Clear all notifications
    void clearAllNotifications();
    
    // ========================================================================
    // Keyboard Shortcuts and Hotkeys
    // ========================================================================
    
    /// Keyboard shortcut
    struct KeyboardShortcut {
        std::string id;
        std::string keys; // "Ctrl+S", "Alt+Shift+P", etc.
        std::string action;
        json parameters = json::object();
        std::string context = "global"; // global, transport, mixer, etc.
        std::string description;
        bool enabled = true;
    };
    
    /// Register keyboard shortcut
    core::VoidResult registerShortcut(const KeyboardShortcut& shortcut);
    
    /// Update shortcut
    core::VoidResult updateShortcut(const std::string& shortcutId, const KeyboardShortcut& shortcut);
    
    /// Remove shortcut
    core::VoidResult removeShortcut(const std::string& shortcutId);
    
    /// Get all shortcuts
    std::vector<KeyboardShortcut> getAllShortcuts() const;
    
    /// Get shortcuts for context
    std::vector<KeyboardShortcut> getShortcutsForContext(const std::string& context) const;
    
    /// Reset to default shortcuts
    core::VoidResult resetToDefaultShortcuts();
    
    // ========================================================================
    // Plugin UI Integration
    // ========================================================================
    
    /// Plugin UI types
    enum class PluginUIType {
        Native,       // Native plugin UI
        Generic,      // Generic parameter controls
        Embedded,     // Embedded in MixMind UI
        WebBased     // Web-based plugin UI
    };
    
    /// Plugin UI configuration
    struct PluginUIConfig {
        core::PluginInstanceID pluginId;
        PluginUIType uiType;
        std::string containerElementId;
        json uiProperties = json::object();
        bool resizable = true;
        bool alwaysOnTop = false;
        struct {
            int width = 400;
            int height = 300;
        } defaultSize;
    };
    
    /// Show plugin UI
    core::VoidResult showPluginUI(const PluginUIConfig& config);
    
    /// Hide plugin UI
    core::VoidResult hidePluginUI(core::PluginInstanceID pluginId);
    
    /// Update plugin UI
    core::VoidResult updatePluginUI(core::PluginInstanceID pluginId, const json& update);
    
    /// Check if plugin UI is visible
    bool isPluginUIVisible(core::PluginInstanceID pluginId) const;
    
    // ========================================================================
    // Event Handling
    // ========================================================================
    
    /// UI events
    enum class UIEvent {
        ComponentUpdated,
        LayoutChanged,
        ThemeChanged,
        ShortcutTriggered,
        NotificationShown,
        ChatMessageSent,
        StreamStarted,
        StreamStopped
    };
    
    /// UI event callback type
    using UIEventCallback = std::function<void(UIEvent event, const json& data)>;
    
    /// Set UI event callback
    void setUIEventCallback(UIEventCallback callback);
    
    /// Clear UI event callback
    void clearUIEventCallback();

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize UI server
    core::VoidResult initializeUIServer();
    
    /// Setup static file serving
    void setupStaticFileServing();
    
    /// Setup API endpoints
    void setupUIEndpoints();
    
    /// Handle WebSocket connections for UI
    void handleUIWebSocket(const std::string& clientId, const json& message);
    
    /// Broadcast to all UI clients
    void broadcastToUIClients(const json& message);
    
    /// Load built-in themes
    void loadBuiltInThemes();
    
    /// Load default shortcuts
    void loadDefaultShortcuts();
    
    /// Process shortcut command
    void processShortcutCommand(const KeyboardShortcut& shortcut);
    
    /// Update data streams
    void updateDataStreams();
    
    /// Emit UI event
    void emitUIEvent(UIEvent event, const json& data);

private:
    // Service references
    std::shared_ptr<api::ActionAPI> actionAPI_;
    std::shared_ptr<api::RESTServer> restServer_;
    std::shared_ptr<api::WebSocketServer> wsServer_;
    
    // UI server
    std::unique_ptr<httplib::Server> uiServer_;
    std::thread uiServerThread_;
    std::atomic<bool> isRunning_{false};
    
    // Configuration
    UIConfig config_;
    mutable std::mutex configMutex_;
    
    // Components and layout
    std::unordered_map<std::string, ComponentConfig> components_;
    std::unordered_map<std::string, LayoutConfig> layouts_;
    mutable std::shared_mutex componentsMutex_;
    
    // Themes and styling
    std::unordered_map<std::string, ThemeConfig> themes_;
    std::string currentThemeName_ = "dark";
    mutable std::shared_mutex themesMutex_;
    
    // UI state
    UIState uiState_;
    mutable std::shared_mutex uiStateMutex_;
    
    // Chat integration
    std::vector<UIChatMessage> chatHistory_;
    mutable std::shared_mutex chatHistoryMutex_;
    
    // Notifications
    std::vector<UINotification> activeNotifications_;
    std::atomic<int32_t> nextNotificationId_{1};
    mutable std::shared_mutex notificationsMutex_;
    
    // Keyboard shortcuts
    std::unordered_map<std::string, KeyboardShortcut> shortcuts_;
    mutable std::shared_mutex shortcutsMutex_;
    
    // Plugin UI management
    std::unordered_map<core::PluginInstanceID, PluginUIConfig> pluginUIs_;
    mutable std::shared_mutex pluginUIsMutex_;
    
    // Data streaming
    std::unordered_map<StreamType, bool> activeStreams_;
    std::unordered_map<StreamType, int32_t> streamUpdateRates_;
    std::thread streamingThread_;
    std::atomic<bool> shouldStopStreaming_{false};
    
    // Event callback
    UIEventCallback uiEventCallback_;
    std::mutex callbackMutex_;
    
    // Constants
    static constexpr int32_t DEFAULT_UI_PORT = 3000;
    static constexpr int32_t MAX_CHAT_HISTORY = 100;
    static constexpr int32_t MAX_NOTIFICATIONS = 20;
};

} // namespace mixmind::ui