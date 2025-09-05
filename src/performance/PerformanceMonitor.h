#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <chrono>
#include <thread>

namespace mixmind::performance {

// ============================================================================
// System Performance Monitoring
// ============================================================================

struct SystemMetrics {
    // CPU metrics
    double cpuUsagePercent = 0.0;
    double audioThreadCpuPercent = 0.0;
    double uiThreadCpuPercent = 0.0;
    int activeCoreCount = 0;
    int totalCoreCount = 0;
    std::vector<double> perCoreCpuUsage;
    
    // Memory metrics
    size_t totalMemoryMB = 0;
    size_t availableMemoryMB = 0;
    size_t usedMemoryMB = 0;
    size_t audioBufferMemoryMB = 0;
    size_t pluginMemoryMB = 0;
    double memoryUsagePercent = 0.0;
    
    // Audio metrics
    double audioLatencyMs = 0.0;
    double audioDropoutRate = 0.0;
    int audioXrunCount = 0;
    int audioBufferUnderuns = 0;
    int audioBufferOverruns = 0;
    double audioEngineLoad = 0.0;
    
    // Disk I/O metrics
    double diskReadMBps = 0.0;
    double diskWriteMBps = 0.0;
    size_t diskQueueDepth = 0;
    double diskLatencyMs = 0.0;
    
    // Network metrics (for collaboration)
    double networkLatencyMs = 0.0;
    double networkBandwidthMbps = 0.0;
    int networkDroppedPackets = 0;
    
    // GPU metrics (for visualization)
    double gpuUsagePercent = 0.0;
    size_t gpuMemoryMB = 0;
    double gpuTemperature = 0.0;
    
    // Power metrics
    double powerConsumptionWatts = 0.0;
    double cpuTemperature = 0.0;
    int thermalThrottlingEvents = 0;
    
    std::chrono::steady_clock::time_point timestamp;
    
    SystemMetrics() : timestamp(std::chrono::steady_clock::now()) {}
};

struct ProcessMetrics {
    std::string processName;
    uint32_t processId = 0;
    double cpuUsagePercent = 0.0;
    size_t memoryUsageMB = 0;
    size_t virtualMemoryMB = 0;
    int threadCount = 0;
    int handleCount = 0;
    double diskReadMBps = 0.0;
    double diskWriteMBps = 0.0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::milliseconds uptime{0};
    
    // Audio-specific metrics
    int audioBuffersProcessed = 0;
    int audioBuffersDropped = 0;
    double audioProcessingTimeMs = 0.0;
    double audioLatencyMs = 0.0;
};

// ============================================================================
// Audio Engine Performance Monitoring
// ============================================================================

struct AudioEngineMetrics {
    // Real-time processing metrics
    double currentCpuLoad = 0.0;
    double averageCpuLoad = 0.0;
    double peakCpuLoad = 0.0;
    double processingTimeMs = 0.0;
    double availableTimeMs = 0.0;
    double headroomPercent = 0.0;
    
    // Timing metrics
    double inputLatencyMs = 0.0;
    double outputLatencyMs = 0.0;
    double roundTripLatencyMs = 0.0;
    double jitter = 0.0;
    
    // Buffer metrics
    int bufferSize = 512;
    double sampleRate = 44100.0;
    int inputChannels = 2;
    int outputChannels = 2;
    int buffersProcessed = 0;
    int buffersDropped = 0;
    int xrunCount = 0;
    
    // Plugin metrics
    int totalPlugins = 0;
    int activePlugins = 0;
    double pluginsCpuLoad = 0.0;
    int pluginLatencySamples = 0;
    
    // Device metrics
    std::string audioDriver;
    std::string inputDevice;
    std::string outputDevice;
    bool exclusiveMode = false;
    std::string deviceStatus;
    
    std::chrono::steady_clock::time_point timestamp;
    
    AudioEngineMetrics() : timestamp(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// Plugin Performance Monitoring  
// ============================================================================

struct PluginMetrics {
    std::string pluginId;
    std::string pluginName;
    std::string manufacturer;
    std::string format; // VST3, AU, etc.
    
    // Processing metrics
    double cpuUsagePercent = 0.0;
    double averageCpuUsage = 0.0;
    double peakCpuUsage = 0.0;
    double processingTimeUs = 0.0;
    int latencySamples = 0;
    double latencyMs = 0.0;
    
    // Memory metrics
    size_t memoryUsageMB = 0;
    size_t peakMemoryUsageMB = 0;
    int memoryAllocations = 0;
    
    // Processing statistics
    int buffersProcessed = 0;
    int buffersSkipped = 0;
    int processingErrors = 0;
    bool isProcessing = false;
    bool isBypassed = false;
    bool isActive = true;
    
    // Parameter metrics
    int totalParameters = 0;
    int automatedParameters = 0;
    int parameterChanges = 0;
    
    // Quality metrics
    double snrDb = 0.0;
    double thdPercent = 0.0;
    bool hasNaN = false;
    bool hasInf = false;
    bool hasDC = false;
    
    std::chrono::steady_clock::time_point lastProcessTime;
    std::chrono::steady_clock::time_point startTime;
    
    PluginMetrics() : lastProcessTime(std::chrono::steady_clock::now()),
                     startTime(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// Performance Monitor
// ============================================================================

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();
    
    // Non-copyable
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
    
    // Monitoring control
    void startMonitoring(std::chrono::milliseconds updateInterval = std::chrono::milliseconds(100));
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Data collection
    SystemMetrics getSystemMetrics() const;
    ProcessMetrics getProcessMetrics() const;
    AudioEngineMetrics getAudioEngineMetrics() const;
    std::vector<PluginMetrics> getPluginMetrics() const;
    PluginMetrics getPluginMetrics(const std::string& pluginId) const;
    
    // Historical data
    std::vector<SystemMetrics> getSystemMetricsHistory(std::chrono::seconds duration) const;
    std::vector<AudioEngineMetrics> getAudioEngineMetricsHistory(std::chrono::seconds duration) const;
    
    // Callbacks for real-time updates
    using SystemMetricsCallback = std::function<void(const SystemMetrics&)>;
    using AudioMetricsCallback = std::function<void(const AudioEngineMetrics&)>;
    using PluginMetricsCallback = std::function<void(const std::vector<PluginMetrics>&)>;
    using AlertCallback = std::function<void(const std::string& alertType, const std::string& message, int severity)>;
    
    void setSystemMetricsCallback(SystemMetricsCallback callback);
    void setAudioMetricsCallback(AudioMetricsCallback callback);
    void setPluginMetricsCallback(PluginMetricsCallback callback);
    void setAlertCallback(AlertCallback callback);
    
    // Plugin monitoring registration
    void registerPlugin(const std::string& pluginId, const std::string& pluginName, const std::string& manufacturer);
    void unregisterPlugin(const std::string& pluginId);
    void updatePluginMetrics(const std::string& pluginId, const PluginMetrics& metrics);
    
    // Audio engine integration
    void setAudioEngine(void* audioEngine); // Pointer to RealtimeAudioEngine
    void updateAudioEngineMetrics(const AudioEngineMetrics& metrics);
    
    // Alert thresholds
    void setCpuAlertThreshold(double percentThreshold);
    void setMemoryAlertThreshold(double percentThreshold);
    void setLatencyAlertThreshold(double milliseconds);
    void setXrunAlertThreshold(int count);
    
    // Performance optimization suggestions
    struct OptimizationSuggestion {
        enum Type {
            CPU_OPTIMIZATION,
            MEMORY_OPTIMIZATION,
            LATENCY_OPTIMIZATION,
            DISK_OPTIMIZATION,
            PLUGIN_OPTIMIZATION,
            SYSTEM_CONFIGURATION
        };
        
        Type type;
        std::string title;
        std::string description;
        std::string recommendation;
        int priority; // 1-5, higher = more important
        double potentialImprovementPercent = 0.0;
        std::vector<std::string> steps;
    };
    
    std::vector<OptimizationSuggestion> getOptimizationSuggestions() const;
    
    // Profiling and benchmarking
    void startProfiling(const std::string& profileName);
    void endProfiling(const std::string& profileName);
    double getProfilingResult(const std::string& profileName) const;
    std::unordered_map<std::string, double> getAllProfilingResults() const;
    void clearProfilingResults();
    
    // Export and reporting
    struct PerformanceReport {
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
        std::chrono::seconds duration;
        
        SystemMetrics averageSystemMetrics;
        AudioEngineMetrics averageAudioMetrics;
        std::vector<PluginMetrics> pluginSummary;
        
        std::vector<std::string> alerts;
        std::vector<OptimizationSuggestion> suggestions;
        
        double overallPerformanceScore = 0.0; // 0-100
        std::string performanceGrade; // A+, A, B+, B, C+, C, D, F
    };
    
    PerformanceReport generateReport(std::chrono::seconds duration) const;
    mixmind::core::Result<void> exportReport(const PerformanceReport& report, const std::string& filePath) const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Performance Optimizer
// ============================================================================

class PerformanceOptimizer {
public:
    PerformanceOptimizer();
    ~PerformanceOptimizer();
    
    // Optimization modes
    enum class OptimizationMode {
        BALANCED,        // Balance between performance and quality
        LOW_LATENCY,     // Optimize for lowest latency
        HIGH_QUALITY,    // Optimize for best quality
        POWER_SAVE,      // Optimize for battery life
        MAXIMUM_PLUGINS  // Optimize for maximum plugin count
    };
    
    // System optimization
    void optimizeSystem(OptimizationMode mode);
    void optimizeAudioSettings(OptimizationMode mode);
    void optimizePluginChain(OptimizationMode mode);
    void optimizeMemoryUsage();
    void optimizeCpuAffinity();
    void optimizeDiskAccess();
    
    // Automatic optimization
    void enableAutoOptimization(bool enabled);
    void setAutoOptimizationInterval(std::chrono::seconds interval);
    bool isAutoOptimizationEnabled() const;
    
    // Optimization results
    struct OptimizationResult {
        std::string optimizationType;
        bool successful = false;
        double improvementPercent = 0.0;
        std::string description;
        std::vector<std::string> changes;
        std::chrono::milliseconds executionTime{0};
    };
    
    std::vector<OptimizationResult> getOptimizationHistory() const;
    OptimizationResult getLastOptimizationResult() const;
    
    // Manual optimizations
    bool adjustBufferSize(int newBufferSize);
    bool adjustSampleRate(double newSampleRate);
    bool adjustThreadPriority(int priority);
    bool adjustPluginOrder(const std::vector<std::string>& newOrder);
    
    // Performance testing
    struct BenchmarkResult {
        std::string testName;
        double score = 0.0;
        std::string units;
        std::chrono::milliseconds duration{0};
        std::unordered_map<std::string, double> detailedMetrics;
    };
    
    BenchmarkResult runCpuBenchmark();
    BenchmarkResult runAudioLatencyBenchmark();
    BenchmarkResult runPluginPerformanceBenchmark();
    BenchmarkResult runMemoryBenchmark();
    BenchmarkResult runDiskBenchmark();
    
    // Resource management
    void enableResourceLimits(bool enabled);
    void setMaxCpuUsage(double percent);
    void setMaxMemoryUsage(size_t megabytes);
    void setPluginTimeout(std::chrono::milliseconds timeout);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Real-Time Performance Profiler
// ============================================================================

class RealTimeProfiler {
public:
    RealTimeProfiler();
    ~RealTimeProfiler();
    
    // Profiling scopes (RAII-based)
    class ProfileScope {
    public:
        ProfileScope(RealTimeProfiler& profiler, const std::string& name);
        ~ProfileScope();
        
    private:
        RealTimeProfiler& profiler_;
        std::string name_;
        std::chrono::high_resolution_clock::time_point startTime_;
    };
    
    // Manual profiling
    void beginProfile(const std::string& name);
    void endProfile(const std::string& name);
    
    // Results
    struct ProfileResult {
        std::string name;
        double totalTimeMs = 0.0;
        double averageTimeMs = 0.0;
        double minTimeMs = 0.0;
        double maxTimeMs = 0.0;
        int callCount = 0;
        double percentageOfTotal = 0.0;
    };
    
    std::vector<ProfileResult> getResults() const;
    ProfileResult getResult(const std::string& name) const;
    void clearResults();
    
    // Real-time safe operations
    void enableRealTimeMode(bool enabled);
    bool isRealTimeMode() const;
    
    // Export results
    std::string exportResultsAsJSON() const;
    std::string exportResultsAsCSV() const;
    void exportResultsToFile(const std::string& filePath) const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// Convenience macro for scoped profiling
#define MIXMIND_PROFILE_SCOPE(profiler, name) \
    mixmind::performance::RealTimeProfiler::ProfileScope _profile_scope(profiler, name)

} // namespace mixmind::performance