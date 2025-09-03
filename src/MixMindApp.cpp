#include "MixMindApp.h"
#include "core/async.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif __linux__
#include <sys/resource.h>
#include <unistd.h>
#elif __APPLE__
#include <mach/mach.h>
#endif

namespace mixmind {

using json = nlohmann::json;

// ============================================================================
// MixMindApp Implementation
// ============================================================================

MixMindApp::MixMindApp()
    : startTime_(std::chrono::system_clock::now())
{
    // Initialize default configuration
    resetConfig();
}

MixMindApp::~MixMindApp() {
    if (isRunning()) {
        shutdown().get();
    }
}

// ========================================================================
// Application Lifecycle
// ========================================================================

core::AsyncResult<core::VoidResult> MixMindApp::initialize() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (isRunning()) {
                return core::VoidResult::failure("Application is already running");
            }
            
            emitEvent(AppEvent::Started, "Initializing MixMind application");
            
            // Create necessary directories
            std::filesystem::create_directories(config_.dataDirectory);
            std::filesystem::create_directories(config_.pluginsDirectory);
            std::filesystem::create_directories(config_.presetsDirectory);
            std::filesystem::create_directories(config_.modelsDirectory);
            std::filesystem::create_directories(config_.tempDirectory);
            std::filesystem::create_directories(config_.logDirectory);
            
            // Initialize components in dependency order
            auto coreResult = initializeCore().get();
            if (!coreResult.success) {
                return core::VoidResult::failure("Core initialization failed: " + coreResult.error);
            }
            
            auto teResult = initializeTracktionAdapters().get();
            if (!teResult.success) {
                return core::VoidResult::failure("Tracktion Engine initialization failed: " + teResult.error);
            }
            
            auto ossResult = initializeOSSServices().get();
            if (!ossResult.success) {
                return core::VoidResult::failure("OSS services initialization failed: " + ossResult.error);
            }
            
            auto apiResult = initializeActionAPI().get();
            if (!apiResult.success) {
                return core::VoidResult::failure("Action API initialization failed: " + apiResult.error);
            }
            
            auto serversResult = initializeServers().get();
            if (!serversResult.success) {
                return core::VoidResult::failure("Servers initialization failed: " + serversResult.error);
            }
            
            // Setup event connections
            setupEventConnections();
            
            // Start background monitoring
            if (config_.enablePerformanceMonitoring) {
                startBackgroundMonitoring();
            }
            
            isRunning_.store(true);
            emitEvent(AppEvent::Started, "MixMind application started successfully");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Application initialization failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> MixMindApp::shutdown() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (!isRunning()) {
                return core::VoidResult::success();
            }
            
            emitEvent(AppEvent::Stopped, "Shutting down MixMind application");
            
            // Stop background monitoring
            stopBackgroundMonitoring();
            
            // Stop servers
            auto serversResult = stopServers().get();
            if (!serversResult.success) {
                emitEvent(AppEvent::ComponentError, "Failed to stop servers: " + serversResult.error);
            }
            
            // Shutdown OSS services
            if (ossServices_) {
                try {
                    ossServices_->shutdownAll().get();
                } catch (const std::exception& e) {
                    emitEvent(AppEvent::ComponentError, "OSS services shutdown error: " + std::string(e.what()));
                }
            }
            
            // Shutdown Tracktion Engine adapters
            if (teSession_) {
                try {
                    teSession_->shutdown().get();
                } catch (const std::exception& e) {
                    emitEvent(AppEvent::ComponentError, "TE session shutdown error: " + std::string(e.what()));
                }
            }
            
            // Cleanup temp files
            cleanupTempFiles();
            
            isRunning_.store(false);
            emitEvent(AppEvent::Stopped, "MixMind application shut down successfully");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Application shutdown failed: " + std::string(e.what()));
        }
    });
}

bool MixMindApp::isRunning() const {
    return isRunning_.load();
}

std::string MixMindApp::getVersion() const {
    return APP_VERSION;
}

MixMindApp::BuildInfo MixMindApp::getBuildInfo() const {
    BuildInfo info;
    info.version = APP_VERSION;
    info.buildDate = BUILD_DATE;
    
#ifdef _DEBUG
    info.buildType = "Debug";
#else
    info.buildType = "Release";
#endif
    
#ifdef _MSC_VER
    info.compiler = "MSVC " + std::to_string(_MSC_VER);
#elif defined(__GNUC__)
    info.compiler = "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(__clang__)
    info.compiler = "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#else
    info.compiler = "Unknown";
#endif
    
#ifdef _WIN32
    info.platform = "Windows";
#elif defined(__linux__)
    info.platform = "Linux";
#elif defined(__APPLE__)
    info.platform = "macOS";
#else
    info.platform = "Unknown";
#endif
    
    // Add feature list
    info.features = {
        "Tracktion Engine",
        "OSS Services",
        "Action API",
        "REST Server",
        "WebSocket Server",
        "LUFS Analysis",
        "FFT Analysis", 
        "Audio Metadata",
        "OSC Control",
        "Time Stretching",
        "ML/AI Integration"
    };
    
    return info;
}

// ========================================================================
// Component Access
// ========================================================================

std::shared_ptr<core::ISession> MixMindApp::getSession() const {
    return session_;
}

std::shared_ptr<core::ITransport> MixMindApp::getTransport() const {
    return transport_;
}

std::shared_ptr<core::ITrack> MixMindApp::getTrackManager() const {
    return trackManager_;
}

std::shared_ptr<core::IClip> MixMindApp::getClipManager() const {
    return clipManager_;
}

std::shared_ptr<core::IPluginHost> MixMindApp::getPluginHost() const {
    return pluginHost_;
}

std::shared_ptr<core::IAutomation> MixMindApp::getAutomation() const {
    return automation_;
}

std::shared_ptr<core::IRenderService> MixMindApp::getRenderService() const {
    return renderService_;
}

std::shared_ptr<core::IMediaLibrary> MixMindApp::getMediaLibrary() const {
    return mediaLibrary_;
}

std::shared_ptr<core::IAudioProcessor> MixMindApp::getAudioProcessor() const {
    return audioProcessor_;
}

std::shared_ptr<core::IAsyncService> MixMindApp::getAsyncService() const {
    return asyncService_;
}

std::shared_ptr<services::OSSServiceRegistry> MixMindApp::getOSSServices() const {
    return ossServices_;
}

std::shared_ptr<services::LibEBU128Service> MixMindApp::getLUFSService() const {
    return lufsService_;
}

std::shared_ptr<services::KissFFTService> MixMindApp::getFFTService() const {
    return fftService_;
}

std::shared_ptr<services::TagLibService> MixMindApp::getTagLibService() const {
    return tagLibService_;
}

std::shared_ptr<services::OSCService> MixMindApp::getOSCService() const {
    return oscService_;
}

std::shared_ptr<services::TimeStretchService> MixMindApp::getTimeStretchService() const {
    return timeStretchService_;
}

std::shared_ptr<services::ONNXService> MixMindApp::getONNXService() const {
    return onnxService_;
}

std::shared_ptr<api::ActionAPI> MixMindApp::getActionAPI() const {
    return actionAPI_;
}

std::shared_ptr<api::RESTServer> MixMindApp::getRESTServer() const {
    return restServer_;
}

std::shared_ptr<api::WebSocketServer> MixMindApp::getWebSocketServer() const {
    return wsServer_;
}

// ========================================================================
// Configuration Management
// ========================================================================

core::VoidResult MixMindApp::loadConfig(const std::string& configPath) {
    try {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        if (!std::filesystem::exists(configPath)) {
            return core::VoidResult::failure("Configuration file not found: " + configPath);
        }
        
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return core::VoidResult::failure("Cannot open configuration file: " + configPath);
        }
        
        json configJson;
        file >> configJson;
        
        // Parse configuration
        if (configJson.contains("dataDirectory")) {
            config_.dataDirectory = configJson["dataDirectory"];
        }
        if (configJson.contains("pluginsDirectory")) {
            config_.pluginsDirectory = configJson["pluginsDirectory"];
        }
        if (configJson.contains("presetsDirectory")) {
            config_.presetsDirectory = configJson["presetsDirectory"];
        }
        if (configJson.contains("modelsDirectory")) {
            config_.modelsDirectory = configJson["modelsDirectory"];
        }
        if (configJson.contains("tempDirectory")) {
            config_.tempDirectory = configJson["tempDirectory"];
        }
        
        // Audio settings
        if (configJson.contains("audio")) {
            auto audio = configJson["audio"];
            if (audio.contains("sampleRate")) {
                config_.defaultSampleRate = audio["sampleRate"];
            }
            if (audio.contains("bitDepth")) {
                config_.defaultBitDepth = audio["bitDepth"];
            }
            if (audio.contains("bufferSize")) {
                config_.bufferSize = audio["bufferSize"];
            }
            if (audio.contains("deviceName")) {
                config_.audioDeviceName = audio["deviceName"];
            }
        }
        
        // OSS services
        if (configJson.contains("services")) {
            auto services = configJson["services"];
            if (services.contains("enableLUFSService")) {
                config_.enableLUFSService = services["enableLUFSService"];
            }
            if (services.contains("enableFFTService")) {
                config_.enableFFTService = services["enableFFTService"];
            }
            if (services.contains("enableTagLibService")) {
                config_.enableTagLibService = services["enableTagLibService"];
            }
            if (services.contains("enableOSCService")) {
                config_.enableOSCService = services["enableOSCService"];
            }
            if (services.contains("enableTimeStretchService")) {
                config_.enableTimeStretchService = services["enableTimeStretchService"];
            }
            if (services.contains("enableONNXService")) {
                config_.enableONNXService = services["enableONNXService"];
            }
        }
        
        // API servers
        if (configJson.contains("servers")) {
            auto servers = configJson["servers"];
            if (servers.contains("rest")) {
                auto rest = servers["rest"];
                if (rest.contains("enabled")) {
                    config_.enableRESTServer = rest["enabled"];
                }
                if (rest.contains("host")) {
                    config_.restHost = rest["host"];
                }
                if (rest.contains("port")) {
                    config_.restPort = rest["port"];
                }
            }
            if (servers.contains("websocket")) {
                auto ws = servers["websocket"];
                if (ws.contains("enabled")) {
                    config_.enableWebSocketServer = ws["enabled"];
                }
                if (ws.contains("host")) {
                    config_.wsHost = ws["host"];
                }
                if (ws.contains("port")) {
                    config_.wsPort = ws["port"];
                }
            }
        }
        
        // Authentication
        if (configJson.contains("auth")) {
            auto auth = configJson["auth"];
            if (auth.contains("token")) {
                config_.apiToken = auth["token"];
            }
        }
        
        // Logging
        if (configJson.contains("logging")) {
            auto logging = configJson["logging"];
            if (logging.contains("enabled")) {
                config_.enableLogging = logging["enabled"];
            }
            if (logging.contains("level")) {
                config_.logLevel = logging["level"];
            }
            if (logging.contains("directory")) {
                config_.logDirectory = logging["directory"];
            }
        }
        
        // Performance
        if (configJson.contains("performance")) {
            auto perf = configJson["performance"];
            if (perf.contains("maxThreads")) {
                config_.maxThreads = perf["maxThreads"];
            }
            if (perf.contains("enableMonitoring")) {
                config_.enablePerformanceMonitoring = perf["enableMonitoring"];
            }
        }
        
        emitEvent(AppEvent::ConfigChanged, "Configuration loaded from: " + configPath);
        return core::VoidResult::success();
        
    } catch (const json::parse_error& e) {
        return core::VoidResult::failure("Configuration parse error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Load configuration failed: " + std::string(e.what()));
    }
}

core::VoidResult MixMindApp::saveConfig(const std::string& configPath) const {
    try {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        json configJson;
        
        // Basic directories
        configJson["dataDirectory"] = config_.dataDirectory;
        configJson["pluginsDirectory"] = config_.pluginsDirectory;
        configJson["presetsDirectory"] = config_.presetsDirectory;
        configJson["modelsDirectory"] = config_.modelsDirectory;
        configJson["tempDirectory"] = config_.tempDirectory;
        
        // Audio settings
        configJson["audio"] = {
            {"sampleRate", config_.defaultSampleRate},
            {"bitDepth", config_.defaultBitDepth},
            {"bufferSize", config_.bufferSize},
            {"deviceName", config_.audioDeviceName}
        };
        
        // OSS services
        configJson["services"] = {
            {"enableLUFSService", config_.enableLUFSService},
            {"enableFFTService", config_.enableFFTService},
            {"enableTagLibService", config_.enableTagLibService},
            {"enableOSCService", config_.enableOSCService},
            {"enableTimeStretchService", config_.enableTimeStretchService},
            {"enableONNXService", config_.enableONNXService}
        };
        
        // API servers
        configJson["servers"] = {
            {"rest", {
                {"enabled", config_.enableRESTServer},
                {"host", config_.restHost},
                {"port", config_.restPort}
            }},
            {"websocket", {
                {"enabled", config_.enableWebSocketServer},
                {"host", config_.wsHost},
                {"port", config_.wsPort}
            }}
        };
        
        // Authentication
        configJson["auth"] = {
            {"token", config_.apiToken}
        };
        
        // Logging
        configJson["logging"] = {
            {"enabled", config_.enableLogging},
            {"level", config_.logLevel},
            {"directory", config_.logDirectory}
        };
        
        // Performance
        configJson["performance"] = {
            {"maxThreads", config_.maxThreads},
            {"enableMonitoring", config_.enablePerformanceMonitoring}
        };
        
        // Write to file
        std::ofstream file(configPath);
        if (!file.is_open()) {
            return core::VoidResult::failure("Cannot create configuration file: " + configPath);
        }
        
        file << configJson.dump(4);
        file.close();
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Save configuration failed: " + std::string(e.what()));
    }
}

MixMindApp::AppConfig MixMindApp::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

core::VoidResult MixMindApp::updateConfig(const AppConfig& config) {
    try {
        std::lock_guard<std::mutex> lock(configMutex_);
        config_ = config;
        emitEvent(AppEvent::ConfigChanged, "Configuration updated");
        return core::VoidResult::success();
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Update configuration failed: " + std::string(e.what()));
    }
}

void MixMindApp::resetConfig() {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = AppConfig{}; // Default configuration
}

// ========================================================================
// Server Management
// ========================================================================

core::AsyncResult<core::VoidResult> MixMindApp::startServers() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (serversRunning_.load()) {
                return core::VoidResult::success();
            }
            
            // Start REST server if enabled
            if (config_.enableRESTServer && restServer_) {
                auto result = restServer_->start(config_.restHost, config_.restPort).get();
                if (!result.success) {
                    return core::VoidResult::failure("REST server start failed: " + result.error);
                }
                emitEvent(AppEvent::ServerStarted, "REST server started on " + config_.restHost + ":" + std::to_string(config_.restPort));
            }
            
            // Start WebSocket server if enabled
            if (config_.enableWebSocketServer && wsServer_) {
                auto result = wsServer_->start(config_.wsHost, config_.wsPort).get();
                if (!result.success) {
                    return core::VoidResult::failure("WebSocket server start failed: " + result.error);
                }
                emitEvent(AppEvent::ServerStarted, "WebSocket server started on " + config_.wsHost + ":" + std::to_string(config_.wsPort));
            }
            
            serversRunning_.store(true);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Start servers failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> MixMindApp::stopServers() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (!serversRunning_.load()) {
                return core::VoidResult::success();
            }
            
            // Stop REST server
            if (restServer_ && restServer_->isRunning()) {
                auto result = restServer_->stop().get();
                if (!result.success) {
                    emitEvent(AppEvent::ComponentError, "REST server stop failed: " + result.error);
                } else {
                    emitEvent(AppEvent::ServerStopped, "REST server stopped");
                }
            }
            
            // Stop WebSocket server
            if (wsServer_ && wsServer_->isRunning()) {
                auto result = wsServer_->stop().get();
                if (!result.success) {
                    emitEvent(AppEvent::ComponentError, "WebSocket server stop failed: " + result.error);
                } else {
                    emitEvent(AppEvent::ServerStopped, "WebSocket server stopped");
                }
            }
            
            serversRunning_.store(false);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Stop servers failed: " + std::string(e.what()));
        }
    });
}

bool MixMindApp::areServersRunning() const {
    return serversRunning_.load();
}

MixMindApp::ServerURLs MixMindApp::getServerURLs() const {
    ServerURLs urls;
    
    if (restServer_) {
        urls.restURL = restServer_->getServerURL();
        urls.restActive = restServer_->isRunning();
    }
    
    if (wsServer_) {
        urls.webSocketURL = wsServer_->getServerURL();
        urls.webSocketActive = wsServer_->isRunning();
    }
    
    return urls;
}

// ========================================================================
// Health and Status
// ========================================================================

MixMindApp::HealthStatus MixMindApp::getHealthStatus() const {
    HealthStatus status;
    status.startTime = startTime_;
    status.uptime = std::chrono::duration<double>(std::chrono::system_clock::now() - startTime_).count();
    
    // Check component health
    if (ossServices_) {
        bool ossHealthy = true; // Would check actual OSS service health
        status.componentStatus["ossServices"] = ossHealthy;
        if (!ossHealthy) {
            status.healthy = false;
            status.errors.push_back("OSS services are not healthy");
        }
    }
    
    if (actionAPI_) {
        bool apiHealthy = true; // Would check actual API health
        status.componentStatus["actionAPI"] = apiHealthy;
        if (!apiHealthy) {
            status.healthy = false;
            status.errors.push_back("Action API is not healthy");
        }
    }
    
    if (restServer_) {
        bool restHealthy = restServer_->isRunning() || !config_.enableRESTServer;
        status.componentStatus["restServer"] = restHealthy;
        if (!restHealthy) {
            status.healthy = false;
            status.errors.push_back("REST server is not running");
        }
    }
    
    if (wsServer_) {
        bool wsHealthy = wsServer_->isRunning() || !config_.enableWebSocketServer;
        status.componentStatus["webSocketServer"] = wsHealthy;
        if (!wsHealthy) {
            status.healthy = false;
            status.errors.push_back("WebSocket server is not running");
        }
    }
    
    // Check disk space
    auto usage = getDataDirectoryUsage();
    if (usage.totalSize > 10LL * 1024 * 1024 * 1024) { // > 10GB
        status.warnings.push_back("Data directory is using over 10GB of disk space");
    }
    
    return status;
}

core::AsyncResult<MixMindApp::HealthStatus> MixMindApp::runSelfTest() {
    return executeAsync<HealthStatus>([this]() -> HealthStatus {
        HealthStatus status = getHealthStatus();
        
        // Run self-tests on components
        if (ossServices_) {
            // Would run actual OSS service self-tests
        }
        
        if (actionAPI_) {
            // Would run actual Action API self-tests
        }
        
        return status;
    });
}

MixMindApp::PerformanceMetrics MixMindApp::getPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return performanceMetrics_;
}

// ========================================================================
// Event System
// ========================================================================

void MixMindApp::addEventListener(EventCallback callback) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    eventListeners_.push_back(std::move(callback));
}

void MixMindApp::clearEventListeners() {
    std::lock_guard<std::mutex> lock(eventMutex_);
    eventListeners_.clear();
}

// ========================================================================
// Utility Functions
// ========================================================================

core::AsyncResult<core::VoidResult> MixMindApp::createDefaultSession() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (!session_) {
                return core::VoidResult::failure("Session manager not initialized");
            }
            
            // Create default session with basic settings
            core::SessionCreateParameters params;
            params.sampleRate = config_.defaultSampleRate;
            params.bitDepth = config_.defaultBitDepth;
            params.name = "Default Session";
            params.directory = config_.dataDirectory + "/sessions";
            
            auto result = session_->createSession(params).get();
            if (!result.success) {
                return core::VoidResult::failure("Create default session failed: " + result.error);
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Create default session failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> MixMindApp::loadSession(const std::string& filePath) {
    return executeAsync<core::VoidResult>([this, filePath]() -> core::VoidResult {
        try {
            if (!session_) {
                return core::VoidResult::failure("Session manager not initialized");
            }
            
            auto result = session_->loadSession(filePath).get();
            if (!result.success) {
                return core::VoidResult::failure("Load session failed: " + result.error);
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Load session failed: " + std::string(e.what()));
        }
    });
}

core::VoidResult MixMindApp::exportLogs(const std::string& outputPath) const {
    try {
        // This would collect logs from all components and export them
        std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());
        
        std::ofstream logFile(outputPath);
        if (!logFile.is_open()) {
            return core::VoidResult::failure("Cannot create log export file: " + outputPath);
        }
        
        logFile << "MixMind Application Log Export" << std::endl;
        logFile << "Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
        logFile << "Version: " << APP_VERSION << std::endl;
        logFile << std::endl;
        
        // Add health status
        auto health = getHealthStatus();
        logFile << "Health Status: " << (health.healthy ? "HEALTHY" : "UNHEALTHY") << std::endl;
        logFile << "Uptime: " << health.uptime << " seconds" << std::endl;
        
        for (const auto& warning : health.warnings) {
            logFile << "WARNING: " << warning << std::endl;
        }
        
        for (const auto& error : health.errors) {
            logFile << "ERROR: " << error << std::endl;
        }
        
        logFile.close();
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Export logs failed: " + std::string(e.what()));
    }
}

core::VoidResult MixMindApp::cleanupTempFiles() {
    try {
        if (!std::filesystem::exists(config_.tempDirectory)) {
            return core::VoidResult::success();
        }
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(config_.tempDirectory)) {
            if (entry.is_regular_file()) {
                std::filesystem::remove(entry.path());
            }
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Cleanup temp files failed: " + std::string(e.what()));
    }
}

MixMindApp::DirectoryUsage MixMindApp::getDataDirectoryUsage() const {
    DirectoryUsage usage;
    usage.path = config_.dataDirectory;
    
    try {
        if (!std::filesystem::exists(config_.dataDirectory)) {
            return usage;
        }
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(config_.dataDirectory)) {
            if (entry.is_regular_file()) {
                size_t fileSize = entry.file_size();
                usage.totalSize += fileSize;
                usage.fileCount++;
                
                // Categorize by file extension
                std::string extension = entry.path().extension().string();
                usage.categorySizes[extension] += fileSize;
            }
        }
        
    } catch (const std::exception& e) {
        // Silently ignore errors in usage calculation
    }
    
    return usage;
}

// ========================================================================
// Internal Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> MixMindApp::initializeCore() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            // Initialize core async service first
            // This would be implemented with actual core components
            
            return core::VoidResult::success();
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Core initialization failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> MixMindApp::initializeTracktionAdapters() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            // Initialize Tracktion Engine adapters
            teSession_ = std::make_shared<adapters::tracktion::TESession>();
            auto sessionResult = teSession_->initialize().get();
            if (!sessionResult.success) {
                return core::VoidResult::failure("TE Session initialization failed: " + sessionResult.error);
            }
            session_ = teSession_;
            
            teTransport_ = std::make_shared<adapters::tracktion::TETransport>();
            auto transportResult = teTransport_->initialize().get();
            if (!transportResult.success) {
                return core::VoidResult::failure("TE Transport initialization failed: " + transportResult.error);
            }
            transport_ = teTransport_;
            
            teTrack_ = std::make_shared<adapters::tracktion::TETrack>();
            auto trackResult = teTrack_->initialize().get();
            if (!trackResult.success) {
                return core::VoidResult::failure("TE Track initialization failed: " + trackResult.error);
            }
            trackManager_ = teTrack_;
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Tracktion Engine initialization failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> MixMindApp::initializeOSSServices() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            // Create OSS service registry
            ossServices_ = std::make_shared<services::OSSServiceRegistry>();
            
            // Initialize individual services based on configuration
            if (config_.enableLUFSService) {
                lufsService_ = std::make_shared<services::LibEBU128Service>();
                auto result = lufsService_->initialize().get();
                if (result.success) {
                    ossServices_->registerService("LUFS", lufsService_);
                } else {
                    emitEvent(AppEvent::ComponentError, "LUFS service initialization failed: " + result.error);
                }
            }
            
            if (config_.enableFFTService) {
                fftService_ = std::make_shared<services::KissFFTService>();
                auto result = fftService_->initialize().get();
                if (result.success) {
                    ossServices_->registerService("FFT", fftService_);
                } else {
                    emitEvent(AppEvent::ComponentError, "FFT service initialization failed: " + result.error);
                }
            }
            
            if (config_.enableTagLibService) {
                tagLibService_ = std::make_shared<services::TagLibService>();
                auto result = tagLibService_->initialize().get();
                if (result.success) {
                    ossServices_->registerService("TagLib", tagLibService_);
                } else {
                    emitEvent(AppEvent::ComponentError, "TagLib service initialization failed: " + result.error);
                }
            }
            
            if (config_.enableOSCService) {
                oscService_ = std::make_shared<services::OSCService>();
                auto result = oscService_->initialize().get();
                if (result.success) {
                    ossServices_->registerService("OSC", oscService_);
                } else {
                    emitEvent(AppEvent::ComponentError, "OSC service initialization failed: " + result.error);
                }
            }
            
            if (config_.enableTimeStretchService) {
                timeStretchService_ = std::make_shared<services::TimeStretchService>();
                auto result = timeStretchService_->initialize().get();
                if (result.success) {
                    ossServices_->registerService("TimeStretch", timeStretchService_);
                } else {
                    emitEvent(AppEvent::ComponentError, "TimeStretch service initialization failed: " + result.error);
                }
            }
            
            if (config_.enableONNXService) {
                onnxService_ = std::make_shared<services::ONNXService>();
                auto result = onnxService_->initialize().get();
                if (result.success) {
                    ossServices_->registerService("ONNX", onnxService_);
                } else {
                    emitEvent(AppEvent::ComponentError, "ONNX service initialization failed: " + result.error);
                }
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("OSS services initialization failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> MixMindApp::initializeActionAPI() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            // Initialize Action API with all our services
            actionAPI_ = std::make_shared<api::ActionAPI>();
            
            // Register core services
            if (session_) actionAPI_->registerService("session", session_);
            if (transport_) actionAPI_->registerService("transport", transport_);
            if (trackManager_) actionAPI_->registerService("tracks", trackManager_);
            
            // Register OSS services
            if (lufsService_) actionAPI_->registerService("lufs", lufsService_);
            if (fftService_) actionAPI_->registerService("fft", fftService_);
            if (tagLibService_) actionAPI_->registerService("metadata", tagLibService_);
            if (oscService_) actionAPI_->registerService("osc", oscService_);
            
            auto result = actionAPI_->initialize().get();
            if (!result.success) {
                return core::VoidResult::failure("Action API initialization failed: " + result.error);
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Action API initialization failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> MixMindApp::initializeServers() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            // Initialize REST server
            if (config_.enableRESTServer) {
                restServer_ = std::make_shared<api::RESTServer>(actionAPI_);
                
                if (!config_.apiToken.empty()) {
                    restServer_->setAuthToken(config_.apiToken);
                }
            }
            
            // Initialize WebSocket server
            if (config_.enableWebSocketServer) {
                wsServer_ = std::make_shared<api::WebSocketServer>(actionAPI_);
                
                if (!config_.apiToken.empty()) {
                    wsServer_->setAuthToken(config_.apiToken);
                }
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Servers initialization failed: " + std::string(e.what()));
        }
    });
}

void MixMindApp::setupEventConnections() {
    // Setup WebSocket server event callbacks
    if (wsServer_) {
        wsServer_->setOnClientConnected([this](const std::string& clientId) {
            emitEvent(AppEvent::ClientConnected, "Client connected: " + clientId);
        });
        
        wsServer_->setOnClientDisconnected([this](const std::string& clientId) {
            emitEvent(AppEvent::ClientDisconnected, "Client disconnected: " + clientId);
        });
        
        wsServer_->setOnError([this](const std::string& clientId, const std::string& error) {
            emitEvent(AppEvent::ComponentError, "WebSocket error for " + clientId + ": " + error);
        });
    }
}

void MixMindApp::emitEvent(AppEvent event, const std::string& details) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    for (const auto& listener : eventListeners_) {
        try {
            listener(event, details);
        } catch (const std::exception& e) {
            // Silently ignore callback errors
        }
    }
}

void MixMindApp::updatePerformanceMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    // Update CPU usage
#ifdef _WIN32
    // Windows implementation
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        // Calculate CPU usage (simplified)
        performanceMetrics_.cpuUsage = 0.0; // Would implement actual calculation
    }
#elif __linux__
    // Linux implementation
    performanceMetrics_.cpuUsage = 0.0; // Would implement actual calculation
#elif __APPLE__
    // macOS implementation
    performanceMetrics_.cpuUsage = 0.0; // Would implement actual calculation
#endif
    
    // Update memory usage
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS memInfo;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &memInfo, sizeof(memInfo))) {
        performanceMetrics_.memoryUsage = static_cast<double>(memInfo.WorkingSetSize) / (1024.0 * 1024.0);
    }
#elif __linux__
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        performanceMetrics_.memoryUsage = static_cast<double>(usage.ru_maxrss) / 1024.0;
    }
#elif __APPLE__
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        performanceMetrics_.memoryUsage = static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
    }
#endif
    
    // Update disk usage
    auto usage = getDataDirectoryUsage();
    performanceMetrics_.diskUsage = static_cast<double>(usage.totalSize) / (1024.0 * 1024.0);
    
    // Update connection counts
    if (wsServer_) {
        performanceMetrics_.activeConnections = static_cast<int>(wsServer_->getConnectedClientsCount());
    }
    
    // Update request counts and response times from servers
    if (restServer_) {
        auto stats = restServer_->getStatistics();
        performanceMetrics_.totalRequests = static_cast<int>(stats.totalRequests);
        performanceMetrics_.averageResponseTime = stats.averageResponseTimeMs;
    }
}

void MixMindApp::startBackgroundMonitoring() {
    shouldStopMonitoring_.store(false);
    monitoringThread_ = std::thread([this]() {
        while (!shouldStopMonitoring_.load()) {
            try {
                updatePerformanceMetrics();
                std::this_thread::sleep_for(std::chrono::seconds(10)); // Update every 10 seconds
            } catch (const std::exception& e) {
                emitEvent(AppEvent::ComponentError, "Performance monitoring error: " + std::string(e.what()));
                std::this_thread::sleep_for(std::chrono::seconds(30)); // Wait longer on error
            }
        }
    });
}

void MixMindApp::stopBackgroundMonitoring() {
    shouldStopMonitoring_.store(true);
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
}

} // namespace mixmind