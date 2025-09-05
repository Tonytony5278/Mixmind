#include "SandboxedPlugin.h"
#include "../audio/LockFreeBuffer.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <random>

namespace mixmind::plugins {

// ============================================================================
// SandboxedPlugin Implementation
// ============================================================================

SandboxedPlugin::SandboxedPlugin(const std::string& pluginPath) 
    : pluginPath_(pluginPath) {
}

SandboxedPlugin::~SandboxedPlugin() {
    unloadPlugin();
}

core::AsyncResult<void> SandboxedPlugin::loadPlugin() {
    return core::executeAsyncGlobal<void>([this]() -> core::Result<void> {
        if (state_.load() != PluginState::UNLOADED) {
            return core::Result<void>::failure("Plugin already loaded or in invalid state");
        }
        
        state_.store(PluginState::LOADING);
        
        try {
            // Create shared memory and synchronization objects
            if (!createSharedMemory()) {
                state_.store(PluginState::SANDBOXED_ERROR);
                return core::Result<void>::failure("Failed to create shared memory");
            }
            
            if (!createSyncEvents()) {
                destroySharedMemory();
                state_.store(PluginState::SANDBOXED_ERROR);
                return core::Result<void>::failure("Failed to create synchronization events");
            }
            
            // Start the sandboxed plugin process
            if (!startSandboxProcess()) {
                destroySyncEvents();
                destroySharedMemory();
                state_.store(PluginState::SANDBOXED_ERROR);
                return core::Result<void>::failure("Failed to start sandboxed plugin process");
            }
            
            // Start monitoring thread
            startProcessMonitoringThread();
            
            // Wait for plugin initialization (with timeout)
            auto startTime = std::chrono::steady_clock::now();
            while (state_.load() == PluginState::LOADING) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed > std::chrono::seconds(30)) { // 30 second timeout
                    handlePluginTimeout();
                    return core::Result<void>::failure("Plugin initialization timeout");
                }
                
                if (!isSandboxProcessRunning()) {
                    handlePluginCrash("Process terminated during initialization");
                    return core::Result<void>::failure("Plugin process crashed during initialization");
                }
            }
            
            if (state_.load() == PluginState::LOADED) {
                std::cout << "✅ Sandboxed plugin loaded successfully: " << pluginPath_ << std::endl;
                return core::Result<void>::success();
            } else {
                return core::Result<void>::failure("Plugin failed to initialize properly");
            }
            
        } catch (const std::exception& e) {
            state_.store(PluginState::SANDBOXED_ERROR);
            return core::Result<void>::failure("Exception during plugin loading: " + std::string(e.what()));
        }
    });
}

void SandboxedPlugin::unloadPlugin() {
    if (state_.load() == PluginState::UNLOADED) {
        return;
    }
    
    // Stop monitoring
    stopProcessMonitoringThread();
    
    // Terminate sandbox process
    terminateSandboxProcess();
    
    // Cleanup resources
    destroySyncEvents();
    destroySharedMemory();
    
    state_.store(PluginState::UNLOADED);
    
    std::cout << "🗑️ Sandboxed plugin unloaded: " << pluginPath_ << std::endl;
}

bool SandboxedPlugin::processAudio(float* const* inputs, float* const* outputs, int numSamples) {
    if (state_.load() != PluginState::LOADED || !sharedData_) {
        // Zero outputs to prevent garbage audio
        for (int ch = 0; ch < config_.numOutputChannels; ++ch) {
            std::memset(outputs[ch], 0, numSamples * sizeof(float));
        }
        return false;
    }
    
    if (numSamples > SharedAudioData::MAX_SAMPLES || 
        config_.numInputChannels > SharedAudioData::MAX_CHANNELS ||
        config_.numOutputChannels > SharedAudioData::MAX_CHANNELS) {
        
        // Buffer size exceeds limits
        RT_LOG_ERROR("Audio buffer size exceeds sandbox limits");
        return false;
    }
    
    auto processStartTime = std::chrono::high_resolution_clock::now();
    stats_.totalProcessCalls++;
    
    try {
        // Copy input audio to shared memory
        sharedData_->numSamples.store(numSamples);
        sharedData_->numInputChannels.store(config_.numInputChannels);
        sharedData_->numOutputChannels.store(config_.numOutputChannels);
        
        for (int ch = 0; ch < config_.numInputChannels; ++ch) {
            std::memcpy(sharedData_->inputBuffers[ch], inputs[ch], numSamples * sizeof(float));
        }
        
        // Signal the sandbox process to start processing
        sharedData_->processingActive.store(true);
        sharedData_->processingComplete.store(false);
        
#ifdef _WIN32
        SetEvent(processStartEvent_);
#endif
        
        // Wait for processing to complete (with timeout)
#ifdef _WIN32
        DWORD waitResult = WaitForSingleObject(processCompleteEvent_, static_cast<DWORD>(processingTimeout_.count()));
        
        if (waitResult == WAIT_TIMEOUT) {
            handlePluginTimeout();
            stats_.timeoutCount++;
            
            // Zero outputs
            for (int ch = 0; ch < config_.numOutputChannels; ++ch) {
                std::memset(outputs[ch], 0, numSamples * sizeof(float));
            }
            return false;
            
        } else if (waitResult != WAIT_OBJECT_0) {
            // Process crashed or other error
            handlePluginCrash("Process signaling error");
            
            // Zero outputs
            for (int ch = 0; ch < config_.numOutputChannels; ++ch) {
                std::memset(outputs[ch], 0, numSamples * sizeof(float));
            }
            return false;
        }
#endif
        
        // Copy output audio from shared memory
        for (int ch = 0; ch < config_.numOutputChannels; ++ch) {
            std::memcpy(outputs[ch], sharedData_->outputBuffers[ch], numSamples * sizeof(float));
        }
        
        // Update performance stats
        auto processEndTime = std::chrono::high_resolution_clock::now();
        auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(processEndTime - processStartTime);
        
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.successfulProcessCalls++;
            
            // Update average processing time (exponential moving average)
            if (stats_.avgProcessingTime.count() == 0) {
                stats_.avgProcessingTime = processingTime;
            } else {
                auto avgMicros = static_cast<int64_t>(stats_.avgProcessingTime.count() * 0.9 + processingTime.count() * 0.1);
                stats_.avgProcessingTime = std::chrono::microseconds(avgMicros);
            }
            
            // Update max processing time
            if (processingTime > stats_.maxProcessingTime) {
                stats_.maxProcessingTime = processingTime;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        RT_LOG_ERROR("Exception in processAudio");
        handlePluginCrash("Exception in processAudio: " + std::string(e.what()));
        
        // Zero outputs
        for (int ch = 0; ch < config_.numOutputChannels; ++ch) {
            std::memset(outputs[ch], 0, numSamples * sizeof(float));
        }
        return false;
    }
}

bool SandboxedPlugin::startSandboxProcess() {
#ifdef _WIN32
    // Get the sandbox executable path
    std::string sandboxExePath = getSandboxExecutablePath();
    
    // Create command line arguments
    std::ostringstream cmdLine;
    cmdLine << "\"" << sandboxExePath << "\" ";
    cmdLine << "\"" << pluginPath_ << "\" ";
    cmdLine << GetCurrentProcessId() << " "; // Parent process ID
    
    // Generate unique shared memory name
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    std::string sharedMemoryName = "MixMindPlugin_" + std::to_string(dis(gen));
    cmdLine << sharedMemoryName;
    
    std::string cmdLineStr = cmdLine.str();
    
    STARTUPINFOA startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    
    // Create the sandboxed process
    BOOL result = CreateProcessA(
        nullptr,                          // lpApplicationName
        const_cast<char*>(cmdLineStr.c_str()), // lpCommandLine
        nullptr,                          // lpProcessAttributes
        nullptr,                          // lpThreadAttributes
        FALSE,                           // bInheritHandles
        CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS, // dwCreationFlags
        nullptr,                          // lpEnvironment
        nullptr,                          // lpCurrentDirectory
        &startupInfo,                     // lpStartupInfo
        &processInfo_                     // lpProcessInformation
    );
    
    if (!result) {
        DWORD error = GetLastError();
        std::cerr << "❌ Failed to create sandboxed process: " << error << std::endl;
        return false;
    }
    
    processHandle_ = processInfo_.hProcess;
    
    std::cout << "🔒 Sandboxed plugin process created: PID " << processInfo_.dwProcessId << std::endl;
    return true;
    
#else
    // TODO: Implement for macOS/Linux using fork/exec
    std::cerr << "❌ Plugin sandboxing not implemented for this platform" << std::endl;
    return false;
#endif
}

void SandboxedPlugin::terminateSandboxProcess() {
#ifdef _WIN32
    if (processHandle_) {
        // Signal shutdown first
        if (processShutdownEvent_) {
            SetEvent(processShutdownEvent_);
        }
        
        // Wait briefly for graceful shutdown
        DWORD waitResult = WaitForSingleObject(processHandle_, 2000); // 2 second timeout
        
        if (waitResult == WAIT_TIMEOUT) {
            // Force terminate if not responding
            std::cout << "⚠️ Force terminating unresponsive plugin process" << std::endl;
            TerminateProcess(processHandle_, 1);
        }
        
        CloseHandle(processHandle_);
        processHandle_ = nullptr;
        
        if (processInfo_.hThread) {
            CloseHandle(processInfo_.hThread);
            processInfo_.hThread = nullptr;
        }
    }
#endif
}

bool SandboxedPlugin::isSandboxProcessRunning() {
#ifdef _WIN32
    if (!processHandle_) {
        return false;
    }
    
    DWORD exitCode;
    if (GetExitCodeProcess(processHandle_, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    
    return false;
#else
    return false;
#endif
}

bool SandboxedPlugin::createSharedMemory() {
#ifdef _WIN32
    // Create shared memory for audio data
    std::string sharedMemName = "MixMindSharedAudio_" + std::to_string(GetCurrentProcessId());
    
    sharedMemoryHandle_ = CreateFileMappingA(
        INVALID_HANDLE_VALUE,           // hFile
        nullptr,                        // lpFileMappingAttributes
        PAGE_READWRITE,                 // flProtect
        0,                              // dwMaximumSizeHigh
        sizeof(SharedAudioData),        // dwMaximumSizeLow
        sharedMemName.c_str()          // lpName
    );
    
    if (!sharedMemoryHandle_) {
        return false;
    }
    
    sharedData_ = static_cast<SharedAudioData*>(MapViewOfFile(
        sharedMemoryHandle_,           // hFileMappingObject
        FILE_MAP_ALL_ACCESS,           // dwDesiredAccess
        0,                             // dwFileOffsetHigh
        0,                             // dwFileOffsetLow
        sizeof(SharedAudioData)        // dwNumberOfBytesToMap
    ));
    
    if (!sharedData_) {
        CloseHandle(sharedMemoryHandle_);
        sharedMemoryHandle_ = nullptr;
        return false;
    }
    
    // Initialize shared data
    new (sharedData_) SharedAudioData();
    
    return true;
    
#else
    // TODO: Implement for macOS/Linux using mmap/shm_open
    return false;
#endif
}

void SandboxedPlugin::destroySharedMemory() {
#ifdef _WIN32
    if (sharedData_) {
        UnmapViewOfFile(sharedData_);
        sharedData_ = nullptr;
    }
    
    if (sharedMemoryHandle_) {
        CloseHandle(sharedMemoryHandle_);
        sharedMemoryHandle_ = nullptr;
    }
#endif
}

bool SandboxedPlugin::createSyncEvents() {
#ifdef _WIN32
    std::string baseName = "MixMindSync_" + std::to_string(GetCurrentProcessId());
    
    processStartEvent_ = CreateEventA(nullptr, FALSE, FALSE, (baseName + "_Start").c_str());
    processCompleteEvent_ = CreateEventA(nullptr, FALSE, FALSE, (baseName + "_Complete").c_str());
    processShutdownEvent_ = CreateEventA(nullptr, FALSE, FALSE, (baseName + "_Shutdown").c_str());
    
    return processStartEvent_ && processCompleteEvent_ && processShutdownEvent_;
    
#else
    return false;
#endif
}

void SandboxedPlugin::destroySyncEvents() {
#ifdef _WIN32
    if (processStartEvent_) {
        CloseHandle(processStartEvent_);
        processStartEvent_ = nullptr;
    }
    
    if (processCompleteEvent_) {
        CloseHandle(processCompleteEvent_);
        processCompleteEvent_ = nullptr;
    }
    
    if (processShutdownEvent_) {
        CloseHandle(processShutdownEvent_);
        processShutdownEvent_ = nullptr;
    }
#endif
}

void SandboxedPlugin::startProcessMonitoringThread() {
    monitoringActive_.store(true);
    monitoringThread_ = std::make_unique<std::thread>(&SandboxedPlugin::processMonitoringLoop, this);
}

void SandboxedPlugin::stopProcessMonitoringThread() {
    monitoringActive_.store(false);
    if (monitoringThread_ && monitoringThread_->joinable()) {
        monitoringThread_->join();
    }
    monitoringThread_.reset();
}

void SandboxedPlugin::processMonitoringLoop() {
    while (monitoringActive_.load()) {
        if (!isSandboxProcessRunning()) {
            if (state_.load() != PluginState::UNLOADED) {
                handlePluginCrash("Process terminated unexpectedly");
            }
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SandboxedPlugin::handlePluginCrash(const std::string& reason) {
    std::cout << "💥 Plugin crashed: " << pluginPath_ << " - " << reason << std::endl;
    
    state_.store(PluginState::CRASHED);
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.crashCount++;
    }
    
    if (crashCallback_) {
        crashCallback_(pluginPath_, reason);
    }
}

void SandboxedPlugin::handlePluginTimeout() {
    std::cout << "⏱️ Plugin timeout: " << pluginPath_ << std::endl;
    
    state_.store(PluginState::TIMEOUT);
    
    // Terminate the unresponsive process
    terminateSandboxProcess();
}

SandboxedPlugin::PerformanceStats SandboxedPlugin::getPerformanceStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    PerformanceStats stats = stats_;
    
    // Update health status
    double successRate = stats_.totalProcessCalls > 0 ? 
        static_cast<double>(stats_.successfulProcessCalls) / stats_.totalProcessCalls : 1.0;
    
    stats.isHealthy = (successRate > 0.95) && // > 95% success rate
                     (stats_.crashCount == 0) && 
                     (state_.load() == PluginState::LOADED);
    
    return stats;
}

bool SandboxedPlugin::isHealthy() const {
    return getPerformanceStats().isHealthy;
}

// ============================================================================
// Global Functions
// ============================================================================

std::string getSandboxExecutablePath() {
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    
    std::string exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash + 1);
    }
    
    return exeDir + "PluginSandbox.exe";
#else
    return "./PluginSandbox";
#endif
}

bool isPluginSandboxingAvailable() {
    std::string sandboxPath = getSandboxExecutablePath();
    
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(sandboxPath.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(sandboxPath.c_str(), X_OK) == 0;
#endif
}

// ============================================================================
// Global Manager
// ============================================================================

static std::unique_ptr<SandboxedPluginManager> g_sandboxedPluginManager;
static std::mutex g_managerMutex;

SandboxedPluginManager& getGlobalSandboxedPluginManager() {
    std::lock_guard<std::mutex> lock(g_managerMutex);
    if (!g_sandboxedPluginManager) {
        g_sandboxedPluginManager = std::make_unique<SandboxedPluginManager>();
    }
    return *g_sandboxedPluginManager;
}

void initializeSandboxedPluginSystem() {
    auto& manager = getGlobalSandboxedPluginManager();
    std::cout << "🔒 Sandboxed plugin system initialized" << std::endl;
}

void shutdownSandboxedPluginSystem() {
    std::lock_guard<std::mutex> lock(g_managerMutex);
    if (g_sandboxedPluginManager) {
        g_sandboxedPluginManager->emergencyShutdown();
        g_sandboxedPluginManager.reset();
    }
}

} // namespace mixmind::plugins