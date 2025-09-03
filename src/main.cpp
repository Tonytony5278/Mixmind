#include "MixMindApp.h"
#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>
#include <chrono>

// Global application instance for signal handling
std::shared_ptr<mixmind::MixMindApp> g_app;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    
    if (g_app) {
        auto future = g_app->shutdown();
        auto result = future.get();
        if (result.isSuccess()) {
            std::cout << "Application shutdown completed successfully." << std::endl;
        } else {
            std::cout << "Application shutdown failed: " << result.error().toString() << std::endl;
        }
    }
    
    exit(0);
}

void printBanner() {
    std::cout << R"(
    ███╗   ███╗██╗██╗  ██╗███╗   ███╗██╗███╗   ██╗██████╗ 
    ████╗ ████║██║╚██╗██╔╝████╗ ████║██║████╗  ██║██╔══██╗
    ██╔████╔██║██║ ╚███╔╝ ██╔████╔██║██║██╔██╗ ██║██║  ██║
    ██║╚██╔╝██║██║ ██╔██╗ ██║╚██╔╝██║██║██║╚██╗██║██║  ██║
    ██║ ╚═╝ ██║██║██╔╝ ██╗██║ ╚═╝ ██║██║██║ ╚████║██████╔╝
    ╚═╝     ╚═╝╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═════╝ 
    
    AI-Powered Digital Audio Workstation
    Production-grade C++ implementation with Tracktion Engine
    )" << std::endl;
}

void printStatus(const mixmind::MixMindApp& app) {
    auto buildInfo = app.getBuildInfo();
    auto urls = app.getServerURLs();
    
    std::cout << "=== MixMind Status ===" << std::endl;
    std::cout << "Version: " << buildInfo.version << std::endl;
    std::cout << "Build: " << buildInfo.buildDate << " (" << buildInfo.buildType << ")" << std::endl;
    std::cout << "Platform: " << buildInfo.platform << std::endl;
    
    std::cout << "\n=== API Endpoints ===" << std::endl;
    if (urls.restActive) {
        std::cout << "REST API: " << urls.restURL << std::endl;
    }
    if (urls.webSocketActive) {
        std::cout << "WebSocket: " << urls.webSocketURL << std::endl;
    }
    
    // Print available features
    std::cout << "\n=== Available Features ===" << std::endl;
    for (const auto& feature : buildInfo.features) {
        std::cout << "  ✓ " << feature << std::endl;
    }
    
    std::cout << std::endl;
}

void monitorHealth(mixmind::MixMindApp& app) {
    while (app.isRunning()) {
        std::this_thread::sleep_for(std::chrono::minutes(5));
        
        auto health = app.getHealthStatus();
        auto metrics = app.getPerformanceMetrics();
        
        std::cout << "[" << std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() << "] ";
        
        if (health.healthy) {
            std::cout << "✓ System healthy";
        } else {
            std::cout << "⚠ System issues detected";
            for (const auto& error : health.errors) {
                std::cout << " | Error: " << error;
            }
        }
        
        std::cout << " | CPU: " << metrics.cpuUsage << "%"
                  << " | Memory: " << metrics.memoryUsage << "MB"
                  << " | Connections: " << metrics.activeConnections
                  << " | Requests: " << metrics.totalRequests
                  << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Print banner
    printBanner();
    
    // Parse command line arguments (basic implementation)
    std::string configPath = "config.json";
    bool daemonMode = false;
    bool verbose = false;
    
    // VST3 scanning mode
    bool scanVST3 = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--daemon") {
            daemonMode = true;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--scan-vst3") {
            scanVST3 = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --config <path>    Configuration file path (default: config.json)" << std::endl;
            std::cout << "  --daemon           Run in daemon mode" << std::endl;
            std::cout << "  --verbose, -v      Enable verbose logging" << std::endl;
            std::cout << "  --scan-vst3        Scan for VST3 plugins and exit" << std::endl;
            std::cout << "  --help, -h         Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Handle VST3 scanning mode
    if (scanVST3) {
        std::cout << "=== VST3 Plugin Scanner ===" << std::endl;
        
        #include "vst3/RealVST3Scanner.h"
        mixmind::RealVST3Scanner scanner;
        
        // Look for specific plugins first
        auto spanResult = scanner.findSpanPlugin();
        auto novaResult = scanner.findTDRNovaPlugin();
        
        if (spanResult.isSuccess()) {
            std::cout << "\n✅ SPAN FOUND: " << spanResult.getValue().path << std::endl;
        } else {
            std::cout << "\n❌ Span not found: " << spanResult.getError().toString() << std::endl;
        }
        
        if (novaResult.isSuccess()) {
            std::cout << "✅ TDR NOVA FOUND: " << novaResult.getValue().path << std::endl;
        } else {
            std::cout << "❌ TDR Nova not found: " << novaResult.getError().toString() << std::endl;
        }
        
        // Scan all plugins
        auto allPluginsResult = scanner.scanSystemPlugins();
        if (allPluginsResult.isSuccess()) {
            auto plugins = allPluginsResult.getValue();
            std::cout << "\n📊 TOTAL VST3 PLUGINS FOUND: " << plugins.size() << std::endl;
            
            std::cout << "\nDetailed plugin list:" << std::endl;
            for (const auto& plugin : plugins) {
                std::cout << "  • " << plugin.name << " (" << plugin.path << ")" << std::endl;
            }
        } else {
            std::cout << "\n❌ No VST3 plugins found in system directories" << std::endl;
            scanner.printDownloadInstructions();
        }
        
        // Exit after scanning
        return (spanResult.isSuccess() || novaResult.isSuccess()) ? 0 : 1;
    }
    
    try {
        // Create application instance
        g_app = std::make_shared<mixmind::MixMindApp>();
        
        // Setup signal handlers for graceful shutdown
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        #ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN); // Ignore broken pipe signals
        #endif
        
        std::cout << "Initializing MixMind application..." << std::endl;
        
        // Load configuration
        if (!configPath.empty()) {
            auto configResult = g_app->loadConfig(configPath);
            if (!configResult.hasValue()) {
                std::cout << "Warning: Failed to load config from " << configPath 
                         << ": " << configResult.getErrorMessage() << std::endl;
                std::cout << "Using default configuration." << std::endl;
            } else {
                std::cout << "Configuration loaded from " << configPath << std::endl;
            }
        }
        
        // Initialize the application
        auto initFuture = g_app->initialize();
        auto initResult = initFuture.get();
        
        if (!initResult.hasValue()) {
            std::cerr << "Failed to initialize application: " << initResult.getErrorMessage() << std::endl;
            return 1;
        }
        
        std::cout << "Application initialized successfully." << std::endl;
        
        // Create a default session
        std::cout << "Creating default session..." << std::endl;
        auto sessionFuture = g_app->createDefaultSession();
        auto sessionResult = sessionFuture.get();
        
        if (!sessionResult.hasValue()) {
            std::cout << "Warning: Failed to create default session: " 
                     << sessionResult.getErrorMessage() << std::endl;
        } else {
            std::cout << "Default session created." << std::endl;
        }
        
        // Start servers
        std::cout << "Starting API servers..." << std::endl;
        auto serverFuture = g_app->startServers();
        auto serverResult = serverFuture.get();
        
        if (!serverResult.hasValue()) {
            std::cerr << "Failed to start servers: " << serverResult.getErrorMessage() << std::endl;
            return 1;
        }
        
        std::cout << "API servers started successfully." << std::endl;
        
        // Print status information
        printStatus(*g_app);
        
        // Run self-test
        std::cout << "Running system self-test..." << std::endl;
        auto testFuture = g_app->runSelfTest();
        auto testResult = testFuture.get();
        
        if (testResult.healthy) {
            std::cout << "✓ Self-test passed - all systems operational" << std::endl;
        } else {
            std::cout << "⚠ Self-test completed with warnings/errors:" << std::endl;
            for (const auto& warning : testResult.warnings) {
                std::cout << "  Warning: " << warning << std::endl;
            }
            for (const auto& error : testResult.errors) {
                std::cout << "  Error: " << error << std::endl;
            }
        }
        
        std::cout << "\n=== MixMind is ready! ===" << std::endl;
        std::cout << "The AI-powered DAW is running and ready to accept connections." << std::endl;
        std::cout << "Press Ctrl+C to shutdown gracefully." << std::endl;
        
        if (verbose) {
            std::cout << "\nVerbose mode enabled - starting health monitoring..." << std::endl;
        }
        
        // Start health monitoring thread if verbose mode
        std::thread healthThread;
        if (verbose) {
            healthThread = std::thread([&]() { monitorHealth(*g_app); });
        }
        
        // Main application loop
        while (g_app->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Check if servers are still running
            if (!g_app->areServersRunning()) {
                std::cout << "Servers stopped unexpectedly, shutting down..." << std::endl;
                break;
            }
        }
        
        // Wait for health monitoring thread to finish
        if (healthThread.joinable()) {
            healthThread.join();
        }
        
        std::cout << "MixMind application exiting." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
    
    return 0;
}