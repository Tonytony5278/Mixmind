#pragma once

#include "core/types.h"
#include "core/result.h"

// Core interfaces
#include "core/ISession.h"
#include "core/ITransport.h"
#include "core/ITrack.h"
#include "core/IClip.h"
#include "core/IPluginHost.h"
#include "core/IPluginInstance.h"
#include "core/IAutomation.h"
#include "core/IRenderService.h"
#include "core/IMediaLibrary.h"
#include "core/IAudioProcessor.h"
#include "core/IAsyncService.h"

// Tracktion Engine adapters
#include "adapters/tracktion/TESession.h"
#include "adapters/tracktion/TETransport.h"
#include "adapters/tracktion/TETrack.h"

// OSS services
#include "services/OSSServiceRegistry.h"
#include "services/LibEBU128Service.h"
#include "services/KissFFTService.h"
#include "services/TagLibService.h"
#include "services/OSCService.h"
#include "services/TimeStretchService.h"
#include "services/ONNXService.h"

// AI Action API
#include "api/ActionAPI.h"
#include "api/RESTServer.h"
#include "api/WebSocketServer.h"

#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>

namespace mixmind {

// ============================================================================
// MixMind Application - Main application class integrating all components
// ============================================================================

class MixMindApp {
public:
    MixMindApp();
    ~MixMindApp();
    
    // Non-copyable, movable
    MixMindApp(const MixMindApp&) = delete;
    MixMindApp& operator=(const MixMindApp&) = delete;
    MixMindApp(MixMindApp&&) = default;
    MixMindApp& operator=(MixMindApp&&) = default;
    
    // ========================================================================
    // Application Lifecycle
    // ========================================================================
    
    /// Initialize the application
    core::AsyncResult<core::VoidResult> initialize();
    
    /// Shutdown the application
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if application is running
    bool isRunning() const;
    
    /// Get application version
    std::string getVersion() const;
    
    /// Get build information
    struct BuildInfo {
        std::string version;
        std::string buildDate;
        std::string buildType;
        std::string compiler;
        std::string platform;
        std::vector<std::string> features;
    };
    
    BuildInfo getBuildInfo() const;
    
    // ========================================================================
    // Component Access
    // ========================================================================
    
    /// Get core interfaces
    std::shared_ptr<core::ISession> getSession() const;
    std::shared_ptr<core::ITransport> getTransport() const;
    std::shared_ptr<core::ITrack> getTrackManager() const;
    std::shared_ptr<core::IClip> getClipManager() const;
    std::shared_ptr<core::IPluginHost> getPluginHost() const;
    std::shared_ptr<core::IAutomation> getAutomation() const;
    std::shared_ptr<core::IRenderService> getRenderService() const;
    std::shared_ptr<core::IMediaLibrary> getMediaLibrary() const;
    std::shared_ptr<core::IAudioProcessor> getAudioProcessor() const;
    std::shared_ptr<core::IAsyncService> getAsyncService() const;
    
    /// Get OSS service registry
    std::shared_ptr<services::OSSServiceRegistry> getOSSServices() const;
    
    /// Get specific OSS services
    std::shared_ptr<services::LibEBU128Service> getLUFSService() const;
    std::shared_ptr<services::KissFFTService> getFFTService() const;
    std::shared_ptr<services::TagLibService> getTagLibService() const;
    std::shared_ptr<services::OSCService> getOSCService() const;
    std::shared_ptr<services::TimeStretchService> getTimeStretchService() const;
    std::shared_ptr<services::ONNXService> getONNXService() const;
    
    /// Get AI Action API
    std::shared_ptr<api::ActionAPI> getActionAPI() const;
    
    /// Get REST server
    std::shared_ptr<api::RESTServer> getRESTServer() const;
    
    /// Get WebSocket server
    std::shared_ptr<api::WebSocketServer> getWebSocketServer() const;
    
    // ========================================================================
    // Configuration Management
    // ========================================================================
    
    /// Application configuration
    struct AppConfig {
        // Core settings
        std::string dataDirectory = "./data";
        std::string pluginsDirectory = "./plugins";
        std::string presetsDirectory = "./presets";
        std::string modelsDirectory = "./models";
        std::string tempDirectory = "./temp";
        
        // Audio settings
        core::SampleRate defaultSampleRate = 48000;
        int defaultBitDepth = 24;
        int bufferSize = 512;
        std::string audioDeviceName;
        
        // OSS services
        bool enableLUFSService = true;
        bool enableFFTService = true;
        bool enableTagLibService = true;
        bool enableOSCService = true;
        bool enableTimeStretchService = true;
        bool enableONNXService = true;
        
        // API servers
        bool enableRESTServer = true;
        std::string restHost = "localhost";
        int restPort = 8080;
        
        bool enableWebSocketServer = true;
        std::string wsHost = "localhost";
        int wsPort = 8081;
        
        // Authentication
        std::string apiToken;
        
        // Logging
        bool enableLogging = true;
        std::string logLevel = "INFO";
        std::string logDirectory = "./logs";
        
        // Performance
        int maxThreads = 0; // 0 = auto-detect
        bool enablePerformanceMonitoring = true;
    };
    
    /// Load configuration from file
    core::VoidResult loadConfig(const std::string& configPath);
    
    /// Save configuration to file
    core::VoidResult saveConfig(const std::string& configPath) const;
    
    /// Get current configuration
    AppConfig getConfig() const;
    
    /// Update configuration
    core::VoidResult updateConfig(const AppConfig& config);
    
    /// Reset to default configuration
    void resetConfig();
    
    // ========================================================================
    // Server Management
    // ========================================================================
    
    /// Start all enabled servers
    core::AsyncResult<core::VoidResult> startServers();
    
    /// Stop all servers
    core::AsyncResult<core::VoidResult> stopServers();
    
    /// Check if servers are running
    bool areServersRunning() const;
    
    /// Get server URLs
    struct ServerURLs {
        std::string restURL;
        std::string webSocketURL;
        bool restActive = false;
        bool webSocketActive = false;
    };
    
    ServerURLs getServerURLs() const;
    
    // ========================================================================
    // Health and Status
    // ========================================================================
    
    /// Application health status
    struct HealthStatus {
        bool healthy = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        std::unordered_map<std::string, bool> componentStatus;
        double uptime = 0.0; // seconds
        std::chrono::system_clock::time_point startTime;
    };
    
    /// Get application health status
    HealthStatus getHealthStatus() const;
    
    /// Run self-test on all components
    core::AsyncResult<HealthStatus> runSelfTest();
    
    /// Get performance metrics
    struct PerformanceMetrics {
        double cpuUsage = 0.0;          // percentage
        double memoryUsage = 0.0;       // MB
        double diskUsage = 0.0;         // MB
        int activeConnections = 0;
        int totalRequests = 0;
        double averageResponseTime = 0.0; // ms
    };
    
    PerformanceMetrics getPerformanceMetrics() const;
    
    // ========================================================================
    // Event System
    // ========================================================================
    
    /// Application event types
    enum class AppEvent {
        Started,
        Stopped,
        ConfigChanged,
        ComponentError,
        ServerStarted,
        ServerStopped,
        ClientConnected,
        ClientDisconnected
    };
    
    /// Event callback type
    using EventCallback = std::function<void(AppEvent event, const std::string& details)>;
    
    /// Register event callback
    void addEventListener(EventCallback callback);
    
    /// Remove event callbacks
    void clearEventListeners();
    
    // ========================================================================
    // Utility Functions
    // ========================================================================
    
    /// Create default session
    core::AsyncResult<core::VoidResult> createDefaultSession();
    
    /// Load session from file
    core::AsyncResult<core::VoidResult> loadSession(const std::string& filePath);
    
    /// Export application logs
    core::VoidResult exportLogs(const std::string& outputPath) const;
    
    /// Clean up temporary files
    core::VoidResult cleanupTempFiles();
    
    /// Get data directory usage
    struct DirectoryUsage {
        std::string path;
        size_t totalSize = 0;  // bytes
        int fileCount = 0;
        std::unordered_map<std::string, size_t> categorySizes;
    };
    
    DirectoryUsage getDataDirectoryUsage() const;

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize core components
    core::AsyncResult<core::VoidResult> initializeCore();
    
    /// Initialize Tracktion Engine adapters
    core::AsyncResult<core::VoidResult> initializeTracktionAdapters();
    
    /// Initialize OSS services
    core::AsyncResult<core::VoidResult> initializeOSSServices();
    
    /// Initialize Action API
    core::AsyncResult<core::VoidResult> initializeActionAPI();
    
    /// Initialize servers
    core::AsyncResult<core::VoidResult> initializeServers();
    
    /// Setup event connections
    void setupEventConnections();
    
    /// Emit application event
    void emitEvent(AppEvent event, const std::string& details);
    
    /// Update performance metrics
    void updatePerformanceMetrics();
    
    /// Start background monitoring
    void startBackgroundMonitoring();
    
    /// Stop background monitoring
    void stopBackgroundMonitoring();

private:
    // Core interfaces (will be implemented by TE adapters)
    std::shared_ptr<core::ISession> session_;
    std::shared_ptr<core::ITransport> transport_;
    std::shared_ptr<core::ITrack> trackManager_;
    std::shared_ptr<core::IClip> clipManager_;
    std::shared_ptr<core::IPluginHost> pluginHost_;
    std::shared_ptr<core::IAutomation> automation_;
    std::shared_ptr<core::IRenderService> renderService_;
    std::shared_ptr<core::IMediaLibrary> mediaLibrary_;
    std::shared_ptr<core::IAudioProcessor> audioProcessor_;
    std::shared_ptr<core::IAsyncService> asyncService_;
    
    // Tracktion Engine adapters
    std::shared_ptr<adapters::tracktion::TESession> teSession_;
    std::shared_ptr<adapters::tracktion::TETransport> teTransport_;
    std::shared_ptr<adapters::tracktion::TETrack> teTrack_;
    
    // OSS services
    std::shared_ptr<services::OSSServiceRegistry> ossServices_;
    std::shared_ptr<services::LibEBU128Service> lufsService_;
    std::shared_ptr<services::KissFFTService> fftService_;
    std::shared_ptr<services::TagLibService> tagLibService_;
    std::shared_ptr<services::OSCService> oscService_;
    std::shared_ptr<services::TimeStretchService> timeStretchService_;
    std::shared_ptr<services::ONNXService> onnxService_;
    
    // AI Action API
    std::shared_ptr<api::ActionAPI> actionAPI_;
    std::shared_ptr<api::RESTServer> restServer_;
    std::shared_ptr<api::WebSocketServer> wsServer_;
    
    // Configuration
    AppConfig config_;
    mutable std::mutex configMutex_;
    
    // State
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> serversRunning_{false};
    std::chrono::system_clock::time_point startTime_;
    
    // Performance monitoring
    mutable PerformanceMetrics performanceMetrics_;
    std::thread monitoringThread_;
    std::atomic<bool> shouldStopMonitoring_{false};
    mutable std::mutex metricsMutex_;
    
    // Event system
    std::vector<EventCallback> eventListeners_;
    mutable std::mutex eventMutex_;
    
    // Constants
    static constexpr const char* APP_VERSION = "1.0.0";
    static constexpr const char* BUILD_DATE = __DATE__ " " __TIME__;
};

} // namespace mixmind