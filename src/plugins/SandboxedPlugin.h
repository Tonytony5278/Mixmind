#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace mixmind::plugins {

// ============================================================================
// CRITICAL SAFETY: Sandboxed Plugin Host
// Runs VST plugins in separate processes to isolate crashes
// Prevents a single buggy plugin from crashing the entire DAW
// ============================================================================

class SandboxedPlugin {
public:
    enum class PluginState {
        UNLOADED,
        LOADING,
        LOADED,
        PROCESSING,
        CRASHED,
        TIMEOUT,
        SANDBOXED_ERROR
    };
    
    struct PluginInfo {
        std::string name;
        std::string vendor;
        std::string path;
        std::string uniqueId;
        int numInputs = 0;
        int numOutputs = 0;
        int numParameters = 0;
        bool isInstrument = false;
        bool supportsMidi = false;
    };
    
    struct ProcessingConfig {
        int sampleRate = 48000;
        int maxBlockSize = 512;
        int numInputChannels = 2;
        int numOutputChannels = 2;
        bool enableMidi = false;
    };
    
    SandboxedPlugin(const std::string& pluginPath);
    ~SandboxedPlugin();
    
    // Non-copyable due to process management
    SandboxedPlugin(const SandboxedPlugin&) = delete;
    SandboxedPlugin& operator=(const SandboxedPlugin&) = delete;
    
    // Plugin lifecycle management
    core::AsyncResult<void> loadPlugin();
    void unloadPlugin();
    bool isLoaded() const { return state_.load() == PluginState::LOADED; }
    PluginState getState() const { return state_.load(); }
    
    // Configuration
    core::Result<void> configure(const ProcessingConfig& config);
    ProcessingConfig getConfig() const { return config_; }
    
    // Plugin information
    core::AsyncResult<PluginInfo> getPluginInfo();
    
    // Parameter control (thread-safe)
    core::Result<void> setParameter(int index, float value);
    core::Result<float> getParameter(int index);
    core::Result<std::string> getParameterName(int index);
    core::Result<std::string> getParameterText(int index);
    
    // Audio processing (real-time safe)
    bool processAudio(float* const* inputs, float* const* outputs, int numSamples);
    
    // MIDI support
    bool processMidi(const uint8_t* midiData, int midiDataSize);
    
    // Crash recovery
    bool hasCrashed() const { return state_.load() == PluginState::CRASHED; }
    core::AsyncResult<void> restartAfterCrash();
    
    // Performance monitoring
    struct PerformanceStats {
        uint64_t totalProcessCalls = 0;
        uint64_t successfulProcessCalls = 0;
        uint64_t timeoutCount = 0;
        uint64_t crashCount = 0;
        std::chrono::microseconds avgProcessingTime{0};
        std::chrono::microseconds maxProcessingTime{0};
        bool isHealthy = true;
    };
    
    PerformanceStats getPerformanceStats() const;
    void resetPerformanceStats();
    
    // Health monitoring
    bool isHealthy() const;
    void setProcessingTimeout(std::chrono::milliseconds timeout) { 
        processingTimeout_ = timeout; 
    }
    
    // Callback for crash notifications
    using CrashCallback = std::function<void(const std::string& pluginPath, const std::string& reason)>;
    void setCrashCallback(CrashCallback callback) { crashCallback_ = std::move(callback); }
    
private:
    // Process management
    bool startSandboxProcess();
    void terminateSandboxProcess();
    bool isSandboxProcessRunning();
    void monitorSandboxProcess();
    
    // Inter-process communication
    struct SharedAudioData {
        static constexpr size_t MAX_CHANNELS = 16;
        static constexpr size_t MAX_SAMPLES = 4096;
        
        std::atomic<bool> processingActive{false};
        std::atomic<bool> processingComplete{false};
        std::atomic<int> numSamples{0};
        std::atomic<int> numInputChannels{0};
        std::atomic<int> numOutputChannels{0};
        
        // Audio buffers
        float inputBuffers[MAX_CHANNELS][MAX_SAMPLES];
        float outputBuffers[MAX_CHANNELS][MAX_SAMPLES];
        
        // MIDI data
        std::atomic<int> midiDataSize{0};
        uint8_t midiData[1024];
        
        // Performance monitoring
        std::atomic<uint64_t> processStartTime{0};
        std::atomic<uint64_t> processEndTime{0};
    };
    
    bool createSharedMemory();
    void destroySharedMemory();
    
    // Communication events
    bool createSyncEvents();
    void destroySyncEvents();
    
    // Plugin process monitoring
    void startProcessMonitoringThread();
    void stopProcessMonitoringThread();
    void processMonitoringLoop();
    
    // Error handling
    void handlePluginCrash(const std::string& reason);
    void handlePluginTimeout();
    
    std::string pluginPath_;
    ProcessingConfig config_;
    std::atomic<PluginState> state_{PluginState::UNLOADED};
    
    // Process management
#ifdef _WIN32
    HANDLE processHandle_ = nullptr;
    PROCESS_INFORMATION processInfo_ = {};
    HANDLE sharedMemoryHandle_ = nullptr;
    SharedAudioData* sharedData_ = nullptr;
    
    // Synchronization events
    HANDLE processStartEvent_ = nullptr;
    HANDLE processCompleteEvent_ = nullptr;
    HANDLE processShutdownEvent_ = nullptr;
#endif
    
    // Monitoring
    std::atomic<bool> monitoringActive_{false};
    std::unique_ptr<std::thread> monitoringThread_;
    
    // Performance tracking
    mutable std::mutex statsMutex_;
    PerformanceStats stats_;
    std::chrono::milliseconds processingTimeout_{100}; // 100ms timeout
    
    // Callbacks
    CrashCallback crashCallback_;
};

// ============================================================================
// Sandboxed Plugin Manager - Manages Multiple Sandboxed Plugins
// ============================================================================

class SandboxedPluginManager {
public:
    SandboxedPluginManager();
    ~SandboxedPluginManager();
    
    // Plugin management
    std::string loadPlugin(const std::string& pluginPath);
    bool unloadPlugin(const std::string& pluginId);
    
    SandboxedPlugin* getPlugin(const std::string& pluginId);
    std::vector<std::string> getLoadedPlugins() const;
    
    // Global configuration
    void setGlobalProcessingTimeout(std::chrono::milliseconds timeout);
    void setMaxSandboxedProcesses(int maxProcesses) { maxSandboxedProcesses_ = maxProcesses; }
    
    // Health monitoring
    struct ManagerStats {
        int totalPlugins = 0;
        int loadedPlugins = 0;
        int healthyPlugins = 0;
        int crashedPlugins = 0;
        uint64_t totalCrashes = 0;
        uint64_t totalRestarts = 0;
    };
    
    ManagerStats getManagerStats() const;
    
    // Automatic crash recovery
    void enableAutoRestart(bool enable) { autoRestartEnabled_.store(enable); }
    bool isAutoRestartEnabled() const { return autoRestartEnabled_.load(); }
    
    // Cleanup crashed processes
    void cleanupCrashedProcesses();
    
    // Emergency: Kill all sandboxed processes
    void emergencyShutdown();
    
private:
    void generatePluginId(const std::string& pluginPath, std::string& outId);
    void onPluginCrash(const std::string& pluginId, const std::string& reason);
    void autoRestartCrashedPlugin(const std::string& pluginId);
    
    mutable std::mutex pluginsMutex_;
    std::unordered_map<std::string, std::unique_ptr<SandboxedPlugin>> plugins_;
    
    std::atomic<bool> autoRestartEnabled_{true};
    std::atomic<int> maxSandboxedProcesses_{16};
    std::chrono::milliseconds globalTimeout_{100};
    
    // Statistics
    mutable std::mutex statsMutex_;
    std::atomic<uint64_t> totalCrashes_{0};
    std::atomic<uint64_t> totalRestarts_{0};
};

// ============================================================================
// Plugin Sandbox Process - Separate executable for running plugins
// This would be compiled as a separate executable (PluginSandbox.exe)
// ============================================================================

class PluginSandboxProcess {
public:
    static int main(int argc, char* argv[]);
    
private:
    static bool initializePlugin(const std::string& pluginPath);
    static void processAudioLoop();
    static void handleShutdown();
    static void reportError(const std::string& error);
    
    // Plugin wrapper
    static std::unique_ptr<void> pluginInstance_;
    static bool pluginInitialized_;
};

// ============================================================================
// Sandboxed VST3 Plugin Wrapper
// ============================================================================

class SandboxedVST3Plugin {
public:
    SandboxedVST3Plugin(const std::string& pluginPath);
    ~SandboxedVST3Plugin();
    
    // VST3-specific interface
    bool initialize();
    void terminate();
    
    bool setActive(bool state);
    bool setupProcessing(const ProcessingConfig& config);
    
    bool process(float* const* inputs, float* const* outputs, int numSamples);
    
    // VST3 parameter interface
    int getParameterCount() const;
    bool setParameterNormalized(int index, double value);
    double getParameterNormalized(int index) const;
    
    std::string getParameterInfo(int index) const;
    
private:
    std::unique_ptr<SandboxedPlugin> sandboxedPlugin_;
    std::string pluginPath_;
};

// ============================================================================
// Global Sandboxed Plugin System
// ============================================================================

SandboxedPluginManager& getGlobalSandboxedPluginManager();
void initializeSandboxedPluginSystem();
void shutdownSandboxedPluginSystem();

// Utility functions
bool isPluginSandboxingAvailable();
std::string getSandboxExecutablePath();

} // namespace mixmind::plugins