#include "PerformanceMonitor.h"
#include "../core/logging.h"
#include "../audio/RealtimeAudioEngine.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#elif __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#elif __linux__
#include <unistd.h>
#include <sys/sysinfo.h>
#include <fstream>
#endif

using json = nlohmann::json;

namespace mixmind::performance {

// ============================================================================
// Platform-Specific System Metrics Collection
// ============================================================================

class SystemMetricsCollector {
public:
    SystemMetricsCollector() {
        initializePlatformSpecific();
    }
    
    ~SystemMetricsCollector() {
        cleanupPlatformSpecific();
    }
    
    SystemMetrics collectMetrics() {
        SystemMetrics metrics;
        
        collectCpuMetrics(metrics);
        collectMemoryMetrics(metrics);
        collectDiskMetrics(metrics);
        collectNetworkMetrics(metrics);
        collectGpuMetrics(metrics);
        collectPowerMetrics(metrics);
        
        return metrics;
    }
    
private:
    void initializePlatformSpecific() {
#ifdef _WIN32
        // Initialize Windows performance counters
        if (PdhOpenQuery(nullptr, 0, &hQuery_) != ERROR_SUCCESS) {
            MIXMIND_LOG_WARNING("Failed to initialize Windows performance counters");
        }
        
        // Add CPU usage counter
        PdhAddCounterA(hQuery_, "\\Processor(_Total)\\% Processor Time", 0, &hCpuCounter_);
        
        // Add memory counters
        PdhAddCounterA(hQuery_, "\\Memory\\Available MBytes", 0, &hMemoryAvailableCounter_);
        PdhAddCounterA(hQuery_, "\\Memory\\Committed Bytes", 0, &hMemoryUsedCounter_);
        
        PdhCollectQueryData(hQuery_); // Initial collection
        
#elif __APPLE__
        // macOS-specific initialization
        mach_port_t hostPort = mach_host_self();
        host_page_size(hostPort, &pageSize_);
        
#elif __linux__
        // Linux-specific initialization
        pageSize_ = getpagesize();
        lastCpuTime_ = getCpuTime();
#endif
    }
    
    void cleanupPlatformSpecific() {
#ifdef _WIN32
        if (hQuery_) {
            PdhCloseQuery(hQuery_);
        }
#endif
    }
    
    void collectCpuMetrics(SystemMetrics& metrics) {
#ifdef _WIN32
        PDH_FMT_COUNTERVALUE counterValue;
        if (PdhCollectQueryData(hQuery_) == ERROR_SUCCESS) {
            if (PdhGetFormattedCounterValue(hCpuCounter_, PDH_FMT_DOUBLE, nullptr, &counterValue) == ERROR_SUCCESS) {
                metrics.cpuUsagePercent = counterValue.doubleValue;
            }
        }
        
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        metrics.totalCoreCount = sysInfo.dwNumberOfProcessors;
        metrics.activeCoreCount = metrics.totalCoreCount;
        
#elif __APPLE__
        host_cpu_load_info_data_t cpuinfo;
        mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
        
        if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, 
                          reinterpret_cast<host_info_t>(&cpuinfo), &count) == KERN_SUCCESS) {
            
            unsigned long totalTicks = 0;
            for (int i = 0; i < CPU_STATE_MAX; i++) {
                totalTicks += cpuinfo.cpu_ticks[i];
            }
            
            if (totalTicks > 0) {
                metrics.cpuUsagePercent = 100.0 * (1.0 - static_cast<double>(cpuinfo.cpu_ticks[CPU_STATE_IDLE]) / totalTicks);
            }
        }
        
        int mib[2] = {CTL_HW, HW_NCPU};
        size_t len = sizeof(metrics.totalCoreCount);
        sysctl(mib, 2, &metrics.totalCoreCount, &len, nullptr, 0);
        metrics.activeCoreCount = metrics.totalCoreCount;
        
#elif __linux__
        // Read /proc/stat for CPU usage
        std::ifstream statFile("/proc/stat");
        std::string line;
        
        if (std::getline(statFile, line) && line.substr(0, 3) == "cpu") {
            std::istringstream ss(line);
            std::string cpu;
            long user, nice, system, idle, iowait, irq, softirq, steal;
            
            ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            long currentTotal = user + nice + system + idle + iowait + irq + softirq + steal;
            long currentIdle = idle + iowait;
            
            if (lastCpuTotal_ > 0) {
                long totalDelta = currentTotal - lastCpuTotal_;
                long idleDelta = currentIdle - lastCpuIdle_;
                
                if (totalDelta > 0) {
                    metrics.cpuUsagePercent = 100.0 * (1.0 - static_cast<double>(idleDelta) / totalDelta);
                }
            }
            
            lastCpuTotal_ = currentTotal;
            lastCpuIdle_ = currentIdle;
        }
        
        metrics.totalCoreCount = std::thread::hardware_concurrency();
        metrics.activeCoreCount = metrics.totalCoreCount;
#endif
    }
    
    void collectMemoryMetrics(SystemMetrics& metrics) {
#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        
        if (GlobalMemoryStatusEx(&memInfo)) {
            metrics.totalMemoryMB = memInfo.ullTotalPhys / (1024 * 1024);
            metrics.availableMemoryMB = memInfo.ullAvailPhys / (1024 * 1024);
            metrics.usedMemoryMB = metrics.totalMemoryMB - metrics.availableMemoryMB;
            metrics.memoryUsagePercent = (static_cast<double>(metrics.usedMemoryMB) / metrics.totalMemoryMB) * 100.0;
        }
        
#elif __APPLE__
        mach_port_t hostPort = mach_host_self();
        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        
        if (host_statistics64(hostPort, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmStats), &count) == KERN_SUCCESS) {
            uint64_t totalPages = vmStats.free_count + vmStats.active_count + vmStats.inactive_count + vmStats.wire_count;
            
            metrics.totalMemoryMB = (totalPages * pageSize_) / (1024 * 1024);
            metrics.availableMemoryMB = (vmStats.free_count * pageSize_) / (1024 * 1024);
            metrics.usedMemoryMB = metrics.totalMemoryMB - metrics.availableMemoryMB;
            metrics.memoryUsagePercent = (static_cast<double>(metrics.usedMemoryMB) / metrics.totalMemoryMB) * 100.0;
        }
        
#elif __linux__
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            metrics.totalMemoryMB = info.totalram / (1024 * 1024);
            metrics.availableMemoryMB = info.freeram / (1024 * 1024);
            metrics.usedMemoryMB = metrics.totalMemoryMB - metrics.availableMemoryMB;
            metrics.memoryUsagePercent = (static_cast<double>(metrics.usedMemoryMB) / metrics.totalMemoryMB) * 100.0;
        }
#endif
    }
    
    void collectDiskMetrics(SystemMetrics& metrics) {
        // Platform-specific disk I/O monitoring would be implemented here
        // For now, provide placeholder values
        metrics.diskReadMBps = 0.0;
        metrics.diskWriteMBps = 0.0;
        metrics.diskQueueDepth = 0;
        metrics.diskLatencyMs = 0.0;
    }
    
    void collectNetworkMetrics(SystemMetrics& metrics) {
        // Network metrics collection would be implemented here
        metrics.networkLatencyMs = 0.0;
        metrics.networkBandwidthMbps = 0.0;
        metrics.networkDroppedPackets = 0;
    }
    
    void collectGpuMetrics(SystemMetrics& metrics) {
        // GPU metrics would require platform-specific libraries (NVML, etc.)
        metrics.gpuUsagePercent = 0.0;
        metrics.gpuMemoryMB = 0;
        metrics.gpuTemperature = 0.0;
    }
    
    void collectPowerMetrics(SystemMetrics& metrics) {
        // Power and thermal metrics would be platform-specific
        metrics.powerConsumptionWatts = 0.0;
        metrics.cpuTemperature = 0.0;
        metrics.thermalThrottlingEvents = 0;
    }
    
#ifdef _WIN32
    PDH_HQUERY hQuery_ = nullptr;
    PDH_HCOUNTER hCpuCounter_ = nullptr;
    PDH_HCOUNTER hMemoryAvailableCounter_ = nullptr;
    PDH_HCOUNTER hMemoryUsedCounter_ = nullptr;
#elif __APPLE__
    vm_size_t pageSize_ = 0;
#elif __linux__
    long pageSize_ = 0;
    long lastCpuTotal_ = 0;
    long lastCpuIdle_ = 0;
    
    long getCpuTime() {
        // Helper function to get current CPU time
        return 0;
    }
#endif
};

// ============================================================================
// Performance Monitor Implementation
// ============================================================================

class PerformanceMonitor::Impl {
public:
    std::unique_ptr<SystemMetricsCollector> systemCollector_;
    
    // Monitoring state
    std::atomic<bool> isMonitoring_{false};
    std::thread monitoringThread_;
    std::chrono::milliseconds updateInterval_{100};
    
    // Metrics storage
    mutable std::mutex metricsMutex_;
    SystemMetrics currentSystemMetrics_;
    AudioEngineMetrics currentAudioMetrics_;
    ProcessMetrics currentProcessMetrics_;
    std::unordered_map<std::string, PluginMetrics> pluginMetrics_;
    
    // Historical data (ring buffers)
    static constexpr size_t MAX_HISTORY_SIZE = 3600; // 1 hour at 1Hz
    std::queue<SystemMetrics> systemMetricsHistory_;
    std::queue<AudioEngineMetrics> audioMetricsHistory_;
    
    // Callbacks
    SystemMetricsCallback systemCallback_;
    AudioMetricsCallback audioCallback_;
    PluginMetricsCallback pluginCallback_;
    AlertCallback alertCallback_;
    
    // Alert thresholds
    double cpuAlertThreshold_ = 80.0;
    double memoryAlertThreshold_ = 85.0;
    double latencyAlertThreshold_ = 20.0;
    int xrunAlertThreshold_ = 10;
    
    // Audio engine reference
    mixmind::audio::RealtimeAudioEngine* audioEngine_ = nullptr;
    
    // Profiling
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> profileStartTimes_;
    std::unordered_map<std::string, double> profilingResults_;
    mutable std::mutex profilingMutex_;
    
    Impl() {
        systemCollector_ = std::make_unique<SystemMetricsCollector>();
    }
    
    ~Impl() {
        if (isMonitoring_.load()) {
            stopMonitoring();
        }
    }
    
    void startMonitoring(std::chrono::milliseconds interval) {
        if (isMonitoring_.load()) {
            return;
        }
        
        updateInterval_ = interval;
        isMonitoring_.store(true);
        
        monitoringThread_ = std::thread([this]() {
            monitoringLoop();
        });
        
        MIXMIND_LOG_INFO("Performance monitoring started with {}ms interval", updateInterval_.count());
    }
    
    void stopMonitoring() {
        if (!isMonitoring_.load()) {
            return;
        }
        
        isMonitoring_.store(false);
        
        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }
        
        MIXMIND_LOG_INFO("Performance monitoring stopped");
    }
    
    void monitoringLoop() {
        auto nextUpdate = std::chrono::steady_clock::now();
        
        while (isMonitoring_.load()) {
            // Collect system metrics
            SystemMetrics systemMetrics = systemCollector_->collectMetrics();
            
            // Collect process metrics
            ProcessMetrics processMetrics = collectProcessMetrics();
            
            // Collect audio engine metrics if available
            AudioEngineMetrics audioMetrics;
            if (audioEngine_) {
                audioMetrics = collectAudioEngineMetrics();
            }
            
            // Store metrics
            {
                std::lock_guard<std::mutex> lock(metricsMutex_);
                
                currentSystemMetrics_ = systemMetrics;
                currentProcessMetrics_ = processMetrics;
                currentAudioMetrics_ = audioMetrics;
                
                // Add to history
                systemMetricsHistory_.push(systemMetrics);
                if (systemMetricsHistory_.size() > MAX_HISTORY_SIZE) {
                    systemMetricsHistory_.pop();
                }
                
                audioMetricsHistory_.push(audioMetrics);
                if (audioMetricsHistory_.size() > MAX_HISTORY_SIZE) {
                    audioMetricsHistory_.pop();
                }
            }
            
            // Check for alerts
            checkAlerts(systemMetrics, audioMetrics);
            
            // Call callbacks
            if (systemCallback_) {
                systemCallback_(systemMetrics);
            }
            
            if (audioCallback_) {
                audioCallback_(audioMetrics);
            }
            
            if (pluginCallback_) {
                std::vector<PluginMetrics> plugins;
                {
                    std::lock_guard<std::mutex> lock(metricsMutex_);
                    plugins.reserve(pluginMetrics_.size());
                    for (const auto& [id, metrics] : pluginMetrics_) {
                        plugins.push_back(metrics);
                    }
                }
                pluginCallback_(plugins);
            }
            
            // Wait for next update
            nextUpdate += updateInterval_;
            std::this_thread::sleep_until(nextUpdate);
        }
    }
    
    ProcessMetrics collectProcessMetrics() {
        ProcessMetrics metrics;
        
#ifdef _WIN32
        metrics.processId = GetCurrentProcessId();
        metrics.processName = "MixMindAI";
        
        HANDLE process = GetCurrentProcess();
        PROCESS_MEMORY_COUNTERS_EX memCounters;
        
        if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memCounters), sizeof(memCounters))) {
            metrics.memoryUsageMB = memCounters.WorkingSetSize / (1024 * 1024);
            metrics.virtualMemoryMB = memCounters.PagefileUsage / (1024 * 1024);
        }
        
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime)) {
            ULARGE_INTEGER creation;
            creation.LowPart = creationTime.dwLowDateTime;
            creation.HighPart = creationTime.dwHighDateTime;
            
            auto now = std::chrono::steady_clock::now();
            auto startTimePoint = std::chrono::steady_clock::time_point{
                std::chrono::microseconds{creation.QuadPart / 10}
            };
            
            metrics.startTime = startTimePoint;
            metrics.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTimePoint);
        }
        
#elif __APPLE__ || __linux__
        metrics.processId = getpid();
        metrics.processName = "MixMindAI";
        
        // Platform-specific process metrics collection would be implemented here
#endif
        
        return metrics;
    }
    
    AudioEngineMetrics collectAudioEngineMetrics() {
        AudioEngineMetrics metrics;
        
        if (!audioEngine_) {
            return metrics;
        }
        
        // Get performance stats from audio engine
        const auto& stats = audioEngine_->getPerformanceStats();
        
        metrics.currentCpuLoad = stats.currentCpuUsage.load();
        metrics.averageCpuLoad = stats.averageCpuUsage.load();
        metrics.peakCpuLoad = stats.peakCpuUsage.load();
        metrics.inputLatencyMs = stats.inputLatencyMs.load();
        metrics.outputLatencyMs = stats.outputLatencyMs.load();
        metrics.roundTripLatencyMs = stats.roundTripLatencyMs.load();
        metrics.xrunCount = stats.xrunCount.load();
        metrics.buffersProcessed = stats.processedBuffers.load();
        metrics.buffersDropped = stats.droppedBuffers.load();
        
        // Get configuration
        auto config = audioEngine_->getConfiguration();
        metrics.bufferSize = config.bufferSize;
        metrics.sampleRate = config.sampleRate;
        metrics.inputChannels = config.inputChannels;
        metrics.outputChannels = config.outputChannels;
        
        // Calculate derived metrics
        if (metrics.sampleRate > 0 && metrics.bufferSize > 0) {
            metrics.availableTimeMs = (metrics.bufferSize / metrics.sampleRate) * 1000.0;
            metrics.processingTimeMs = (metrics.currentCpuLoad / 100.0) * metrics.availableTimeMs;
            metrics.headroomPercent = std::max(0.0, 100.0 - metrics.currentCpuLoad);
        }
        
        // Get device information
        if (audioEngine_->isStreamOpen()) {
            // Would get actual device names from audio engine
            metrics.audioDriver = "PortAudio";
            metrics.inputDevice = "Default Input";
            metrics.outputDevice = "Default Output";
            metrics.deviceStatus = audioEngine_->isStreamRunning() ? "Running" : "Stopped";
        }
        
        return metrics;
    }
    
    void checkAlerts(const SystemMetrics& systemMetrics, const AudioEngineMetrics& audioMetrics) {
        if (!alertCallback_) {
            return;
        }
        
        // CPU usage alerts
        if (systemMetrics.cpuUsagePercent > cpuAlertThreshold_) {
            alertCallback_("CPU_HIGH", 
                         "High CPU usage: " + std::to_string(static_cast<int>(systemMetrics.cpuUsagePercent)) + "%",
                         systemMetrics.cpuUsagePercent > 95.0 ? 3 : 2);
        }
        
        // Memory usage alerts
        if (systemMetrics.memoryUsagePercent > memoryAlertThreshold_) {
            alertCallback_("MEMORY_HIGH",
                         "High memory usage: " + std::to_string(static_cast<int>(systemMetrics.memoryUsagePercent)) + "%",
                         systemMetrics.memoryUsagePercent > 95.0 ? 3 : 2);
        }
        
        // Audio latency alerts
        if (audioMetrics.roundTripLatencyMs > latencyAlertThreshold_) {
            alertCallback_("LATENCY_HIGH",
                         "High audio latency: " + std::to_string(static_cast<int>(audioMetrics.roundTripLatencyMs)) + "ms",
                         2);
        }
        
        // Xrun alerts
        static int lastXrunCount = 0;
        if (audioMetrics.xrunCount > lastXrunCount) {
            int newXruns = audioMetrics.xrunCount - lastXrunCount;
            if (newXruns >= xrunAlertThreshold_) {
                alertCallback_("XRUN_DETECTED",
                             "Audio dropouts detected: " + std::to_string(newXruns) + " xruns",
                             3);
            }
            lastXrunCount = audioMetrics.xrunCount;
        }
        
        // Audio engine overload
        if (audioMetrics.currentCpuLoad > 90.0) {
            alertCallback_("AUDIO_OVERLOAD",
                         "Audio engine overload: " + std::to_string(static_cast<int>(audioMetrics.currentCpuLoad)) + "%",
                         3);
        }
    }
    
    std::vector<PerformanceMonitor::OptimizationSuggestion> generateOptimizationSuggestions() const {
        std::vector<PerformanceMonitor::OptimizationSuggestion> suggestions;
        
        std::lock_guard<std::mutex> lock(metricsMutex_);
        
        // CPU optimization suggestions
        if (currentSystemMetrics_.cpuUsagePercent > 70.0) {
            PerformanceMonitor::OptimizationSuggestion suggestion;
            suggestion.type = PerformanceMonitor::OptimizationSuggestion::CPU_OPTIMIZATION;
            suggestion.title = "Reduce CPU Usage";
            suggestion.description = "System CPU usage is high (" + 
                                   std::to_string(static_cast<int>(currentSystemMetrics_.cpuUsagePercent)) + "%)";
            suggestion.recommendation = "Consider increasing buffer size or disabling unused plugins";
            suggestion.priority = currentSystemMetrics_.cpuUsagePercent > 90.0 ? 5 : 3;
            suggestion.potentialImprovementPercent = 15.0;
            suggestion.steps = {
                "Increase audio buffer size to 512 or 1024 samples",
                "Disable or bypass unused plugins",
                "Use plugin oversampling sparingly",
                "Close unnecessary applications"
            };
            suggestions.push_back(suggestion);
        }
        
        // Memory optimization suggestions
        if (currentSystemMetrics_.memoryUsagePercent > 80.0) {
            PerformanceMonitor::OptimizationSuggestion suggestion;
            suggestion.type = PerformanceMonitor::OptimizationSuggestion::MEMORY_OPTIMIZATION;
            suggestion.title = "Optimize Memory Usage";
            suggestion.description = "Memory usage is high (" + 
                                   std::to_string(static_cast<int>(currentSystemMetrics_.memoryUsagePercent)) + "%)";
            suggestion.recommendation = "Free up memory by closing unused applications or reducing buffer pools";
            suggestion.priority = currentSystemMetrics_.memoryUsagePercent > 95.0 ? 5 : 3;
            suggestion.potentialImprovementPercent = 20.0;
            suggestion.steps = {
                "Close unused applications",
                "Reduce audio buffer pool size",
                "Free sample libraries from memory when not in use",
                "Restart MixMind AI to clear memory leaks"
            };
            suggestions.push_back(suggestion);
        }
        
        // Latency optimization suggestions
        if (currentAudioMetrics_.roundTripLatencyMs > 15.0) {
            PerformanceMonitor::OptimizationSuggestion suggestion;
            suggestion.type = PerformanceMonitor::OptimizationSuggestion::LATENCY_OPTIMIZATION;
            suggestion.title = "Reduce Audio Latency";
            suggestion.description = "Audio latency is high (" + 
                                   std::to_string(static_cast<int>(currentAudioMetrics_.roundTripLatencyMs)) + "ms)";
            suggestion.recommendation = "Use ASIO drivers and reduce buffer size";
            suggestion.priority = 4;
            suggestion.potentialImprovementPercent = 50.0;
            suggestion.steps = {
                "Switch to ASIO audio driver",
                "Reduce buffer size to 128 or 256 samples",
                "Enable exclusive mode if available",
                "Disable Windows audio enhancements"
            };
            suggestions.push_back(suggestion);
        }
        
        return suggestions;
    }
};

PerformanceMonitor::PerformanceMonitor()
    : pImpl_(std::make_unique<Impl>()) {
}

PerformanceMonitor::~PerformanceMonitor() = default;

void PerformanceMonitor::startMonitoring(std::chrono::milliseconds updateInterval) {
    pImpl_->startMonitoring(updateInterval);
}

void PerformanceMonitor::stopMonitoring() {
    pImpl_->stopMonitoring();
}

bool PerformanceMonitor::isMonitoring() const {
    return pImpl_->isMonitoring_.load();
}

SystemMetrics PerformanceMonitor::getSystemMetrics() const {
    std::lock_guard<std::mutex> lock(pImpl_->metricsMutex_);
    return pImpl_->currentSystemMetrics_;
}

ProcessMetrics PerformanceMonitor::getProcessMetrics() const {
    std::lock_guard<std::mutex> lock(pImpl_->metricsMutex_);
    return pImpl_->currentProcessMetrics_;
}

AudioEngineMetrics PerformanceMonitor::getAudioEngineMetrics() const {
    std::lock_guard<std::mutex> lock(pImpl_->metricsMutex_);
    return pImpl_->currentAudioMetrics_;
}

std::vector<PluginMetrics> PerformanceMonitor::getPluginMetrics() const {
    std::lock_guard<std::mutex> lock(pImpl_->metricsMutex_);
    
    std::vector<PluginMetrics> result;
    result.reserve(pImpl_->pluginMetrics_.size());
    
    for (const auto& [id, metrics] : pImpl_->pluginMetrics_) {
        result.push_back(metrics);
    }
    
    return result;
}

void PerformanceMonitor::setSystemMetricsCallback(SystemMetricsCallback callback) {
    pImpl_->systemCallback_ = std::move(callback);
}

void PerformanceMonitor::setAudioMetricsCallback(AudioMetricsCallback callback) {
    pImpl_->audioCallback_ = std::move(callback);
}

void PerformanceMonitor::setPluginMetricsCallback(PluginMetricsCallback callback) {
    pImpl_->pluginCallback_ = std::move(callback);
}

void PerformanceMonitor::setAlertCallback(AlertCallback callback) {
    pImpl_->alertCallback_ = std::move(callback);
}

void PerformanceMonitor::registerPlugin(const std::string& pluginId, const std::string& pluginName, const std::string& manufacturer) {
    std::lock_guard<std::mutex> lock(pImpl_->metricsMutex_);
    
    PluginMetrics metrics;
    metrics.pluginId = pluginId;
    metrics.pluginName = pluginName;
    metrics.manufacturer = manufacturer;
    
    pImpl_->pluginMetrics_[pluginId] = metrics;
    
    MIXMIND_LOG_INFO("Registered plugin for monitoring: {} ({})", pluginName, pluginId);
}

void PerformanceMonitor::setAudioEngine(void* audioEngine) {
    pImpl_->audioEngine_ = static_cast<mixmind::audio::RealtimeAudioEngine*>(audioEngine);
}

void PerformanceMonitor::setCpuAlertThreshold(double percentThreshold) {
    pImpl_->cpuAlertThreshold_ = std::clamp(percentThreshold, 0.0, 100.0);
}

void PerformanceMonitor::setMemoryAlertThreshold(double percentThreshold) {
    pImpl_->memoryAlertThreshold_ = std::clamp(percentThreshold, 0.0, 100.0);
}

void PerformanceMonitor::setLatencyAlertThreshold(double milliseconds) {
    pImpl_->latencyAlertThreshold_ = std::max(0.0, milliseconds);
}

void PerformanceMonitor::setXrunAlertThreshold(int count) {
    pImpl_->xrunAlertThreshold_ = std::max(1, count);
}

std::vector<PerformanceMonitor::OptimizationSuggestion> PerformanceMonitor::getOptimizationSuggestions() const {
    return pImpl_->generateOptimizationSuggestions();
}

void PerformanceMonitor::startProfiling(const std::string& profileName) {
    std::lock_guard<std::mutex> lock(pImpl_->profilingMutex_);
    pImpl_->profileStartTimes_[profileName] = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::endProfiling(const std::string& profileName) {
    auto endTime = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(pImpl_->profilingMutex_);
    
    auto it = pImpl_->profileStartTimes_.find(profileName);
    if (it != pImpl_->profileStartTimes_.end()) {
        auto duration = std::chrono::duration<double, std::milli>(endTime - it->second);
        pImpl_->profilingResults_[profileName] = duration.count();
        pImpl_->profileStartTimes_.erase(it);
    }
}

double PerformanceMonitor::getProfilingResult(const std::string& profileName) const {
    std::lock_guard<std::mutex> lock(pImpl_->profilingMutex_);
    
    auto it = pImpl_->profilingResults_.find(profileName);
    return (it != pImpl_->profilingResults_.end()) ? it->second : 0.0;
}

std::unordered_map<std::string, double> PerformanceMonitor::getAllProfilingResults() const {
    std::lock_guard<std::mutex> lock(pImpl_->profilingMutex_);
    return pImpl_->profilingResults_;
}

void PerformanceMonitor::clearProfilingResults() {
    std::lock_guard<std::mutex> lock(pImpl_->profilingMutex_);
    pImpl_->profilingResults_.clear();
    pImpl_->profileStartTimes_.clear();
}

PerformanceMonitor::PerformanceReport PerformanceMonitor::generateReport(std::chrono::seconds duration) const {
    PerformanceReport report;
    
    auto now = std::chrono::steady_clock::now();
    report.endTime = now;
    report.startTime = now - duration;
    report.duration = duration;
    
    // Get current metrics as averages (simplified)
    report.averageSystemMetrics = getSystemMetrics();
    report.averageAudioMetrics = getAudioEngineMetrics();
    report.pluginSummary = getPluginMetrics();
    
    // Get optimization suggestions
    report.suggestions = getOptimizationSuggestions();
    
    // Calculate overall performance score (0-100)
    double cpuScore = std::max(0.0, 100.0 - report.averageSystemMetrics.cpuUsagePercent);
    double memoryScore = std::max(0.0, 100.0 - report.averageSystemMetrics.memoryUsagePercent);
    double audioScore = std::max(0.0, 100.0 - report.averageAudioMetrics.currentCpuLoad);
    double latencyScore = std::max(0.0, 100.0 - (report.averageAudioMetrics.roundTripLatencyMs * 2.0));
    
    report.overallPerformanceScore = (cpuScore + memoryScore + audioScore + latencyScore) / 4.0;
    
    // Assign performance grade
    if (report.overallPerformanceScore >= 95.0) report.performanceGrade = "A+";
    else if (report.overallPerformanceScore >= 90.0) report.performanceGrade = "A";
    else if (report.overallPerformanceScore >= 85.0) report.performanceGrade = "B+";
    else if (report.overallPerformanceScore >= 80.0) report.performanceGrade = "B";
    else if (report.overallPerformanceScore >= 75.0) report.performanceGrade = "C+";
    else if (report.overallPerformanceScore >= 70.0) report.performanceGrade = "C";
    else if (report.overallPerformanceScore >= 60.0) report.performanceGrade = "D";
    else report.performanceGrade = "F";
    
    return report;
}

mixmind::core::Result<void> PerformanceMonitor::exportReport(const PerformanceReport& report, const std::string& filePath) const {
    try {
        json reportJson;
        
        // Basic report info
        reportJson["startTime"] = std::chrono::duration_cast<std::chrono::seconds>(report.startTime.time_since_epoch()).count();
        reportJson["endTime"] = std::chrono::duration_cast<std::chrono::seconds>(report.endTime.time_since_epoch()).count();
        reportJson["duration"] = report.duration.count();
        reportJson["overallPerformanceScore"] = report.overallPerformanceScore;
        reportJson["performanceGrade"] = report.performanceGrade;
        
        // System metrics
        auto& sysMetrics = reportJson["systemMetrics"];
        sysMetrics["cpuUsagePercent"] = report.averageSystemMetrics.cpuUsagePercent;
        sysMetrics["memoryUsagePercent"] = report.averageSystemMetrics.memoryUsagePercent;
        sysMetrics["totalMemoryMB"] = report.averageSystemMetrics.totalMemoryMB;
        sysMetrics["usedMemoryMB"] = report.averageSystemMetrics.usedMemoryMB;
        
        // Audio metrics
        auto& audioMetrics = reportJson["audioMetrics"];
        audioMetrics["currentCpuLoad"] = report.averageAudioMetrics.currentCpuLoad;
        audioMetrics["averageCpuLoad"] = report.averageAudioMetrics.averageCpuLoad;
        audioMetrics["roundTripLatencyMs"] = report.averageAudioMetrics.roundTripLatencyMs;
        audioMetrics["xrunCount"] = report.averageAudioMetrics.xrunCount;
        audioMetrics["bufferSize"] = report.averageAudioMetrics.bufferSize;
        audioMetrics["sampleRate"] = report.averageAudioMetrics.sampleRate;
        
        // Plugin summary
        auto& plugins = reportJson["plugins"];
        for (const auto& plugin : report.pluginSummary) {
            json pluginJson;
            pluginJson["name"] = plugin.pluginName;
            pluginJson["manufacturer"] = plugin.manufacturer;
            pluginJson["cpuUsagePercent"] = plugin.cpuUsagePercent;
            pluginJson["memoryUsageMB"] = plugin.memoryUsageMB;
            pluginJson["latencyMs"] = plugin.latencyMs;
            plugins.push_back(pluginJson);
        }
        
        // Optimization suggestions
        auto& suggestions = reportJson["optimizationSuggestions"];
        for (const auto& suggestion : report.suggestions) {
            json suggestionJson;
            suggestionJson["title"] = suggestion.title;
            suggestionJson["description"] = suggestion.description;
            suggestionJson["recommendation"] = suggestion.recommendation;
            suggestionJson["priority"] = suggestion.priority;
            suggestionJson["potentialImprovementPercent"] = suggestion.potentialImprovementPercent;
            suggestionJson["steps"] = suggestion.steps;
            suggestions.push_back(suggestionJson);
        }
        
        // Write to file
        std::ofstream file(filePath);
        if (!file) {
            return mixmind::core::Result<void>::error(
                mixmind::core::ErrorCode::FileAccessDenied,
                "file_export",
                "Unable to create report file: " + filePath
            );
        }
        
        file << reportJson.dump(2);
        file.close();
        
        MIXMIND_LOG_INFO("Performance report exported to: {}", filePath);
        
        return mixmind::core::Result<void>::success();
        
    } catch (const std::exception& e) {
        return mixmind::core::Result<void>::error(
            mixmind::core::ErrorCode::Unknown,
            "report_export",
            "Failed to export performance report: " + std::string(e.what())
        );
    }
}

} // namespace mixmind::performance