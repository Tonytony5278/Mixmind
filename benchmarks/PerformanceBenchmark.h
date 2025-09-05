#pragma once

#include "../src/core/result.h"
#include <string>
#include <chrono>
#include <vector>
#include <map>
#include <memory>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace mixmind::benchmarks {

// ============================================================================
// PRODUCTION: Performance Benchmark Suite
// Validates system performance against professional audio standards
// ============================================================================

class PerformanceBenchmark {
public:
    struct BenchmarkResults {
        // Audio Performance (Critical for DAW)
        double audioLatencyMs = 0.0;          // Should be <3ms
        double audioCallbackLatencyMs = 0.0;   // Should be <1ms
        int audioDropoutCount = 0;             // Should be 0
        double audioThreadCPU = 0.0;           // Should be <50%
        
        // UI Responsiveness (Critical for User Experience)
        double uiResponseTimeMs = 0.0;         // Should be <16ms (60fps)
        double uiFrameRateAvg = 0.0;          // Should be >30fps
        int uiFrameDropCount = 0;             // Should be <5% of frames
        
        // System Performance
        double memoryUsageMB = 0.0;           // Should be <500MB idle
        double cpuUsageIdle = 0.0;            // Should be <10% idle
        double cpuUsageLoad = 0.0;            // Should be <80% under load
        
        // Scalability Tests
        int concurrentTracks = 0;             // Should handle 100+
        int vst3PluginsLoaded = 0;           // Should handle 50+
        int simultaneousVoiceCount = 0;       // Should handle 256+
        
        // AI Performance
        double aiResponseTimeMs = 0.0;        // Should be <2000ms
        int aiRequestsPerMinute = 0;          // Should handle 30+
        bool aiThreadIsolation = false;       // Must be true (no audio blocking)
        
        // File I/O Performance  
        double projectLoadTimeMs = 0.0;       // Should be <5000ms for large projects
        double audioFileLoadTimeMs = 0.0;     // Should be <1000ms per minute
        double projectSaveTimeMs = 0.0;       // Should be <3000ms
        
        // Network Performance (for AI features)
        double networkLatencyMs = 0.0;        // Should be <200ms
        bool offlineCapability = false;       // Must be true
        
        // Overall Scores
        double performanceScore = 0.0;        // 0-100 (>90 is excellent)
        bool passesMinimumRequirements = false;
        std::vector<std::string> failures;
        std::vector<std::string> warnings;
        
        // Test Environment
        std::string testSystem;
        std::string cpuInfo;
        std::string memoryInfo;
        std::string audioHardware;
        std::chrono::system_clock::time_point testDate;
        std::chrono::seconds testDuration{0};
    };
    
    enum class BenchmarkType {
        QUICK,          // 30 seconds - basic performance check
        STANDARD,       // 5 minutes - comprehensive testing
        STRESS,         // 30 minutes - long-term stability
        PRODUCTION      // 2 hours - full production simulation
    };
    
    using ProgressCallback = std::function<void(const std::string&, int)>;
    
    PerformanceBenchmark();
    ~PerformanceBenchmark();
    
    // Main benchmark execution
    core::Result<BenchmarkResults> runFullBenchmark(
        BenchmarkType type = BenchmarkType::STANDARD,
        ProgressCallback progressCallback = nullptr
    );
    
    // Individual benchmark tests
    core::Result<double> benchmarkAudioLatency();
    core::Result<double> benchmarkUIResponsiveness();
    core::Result<double> benchmarkMemoryUsage();
    core::Result<double> benchmarkCPUUsage();
    core::Result<int> benchmarkTrackScalability();
    core::Result<int> benchmarkPluginScalability();
    core::Result<double> benchmarkAIPerformance();
    core::Result<double> benchmarkFileIO();
    core::Result<bool> benchmarkOfflineCapability();
    
    // Stress testing
    core::Result<BenchmarkResults> stressTestAudioEngine(std::chrono::minutes duration);
    core::Result<BenchmarkResults> stressTestMemoryLeaks(std::chrono::minutes duration);
    core::Result<BenchmarkResults> stressTestConcurrentUsers(int userCount);
    
    // Real-time monitoring
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    BenchmarkResults getCurrentMetrics() const;
    
    // Comparison and analysis
    void setBaselineResults(const BenchmarkResults& baseline) { baseline_ = baseline; }
    BenchmarkResults compareWithBaseline(const BenchmarkResults& current) const;
    std::string generatePerformanceReport(const BenchmarkResults& results) const;
    
    // Configuration
    void setAudioConfig(int sampleRate, int bufferSize) {
        audioSampleRate_ = sampleRate;
        audioBufferSize_ = bufferSize;
    }
    
    void setExpectedRequirements(const BenchmarkResults& requirements) {
        minimumRequirements_ = requirements;
    }
    
private:
    // Audio system testing
    void initializeAudioSystem();
    void shutdownAudioSystem();
    double measureAudioCallbackLatency();
    int countAudioDropouts();
    void processAudioBlock(float* input, float* output, int frameCount);
    
    // UI system testing
    void initializeTestUI();
    void shutdownTestUI();
    double measureUIFrameTime();
    void renderTestUI();
    
    // System metrics
    double getCurrentCPUUsage();
    double getCurrentMemoryUsage();
    std::string getSystemInfo();
    std::string getCPUInfo();
    std::string getMemoryInfo();
    std::string getAudioHardwareInfo();
    
    // Load generation
    void simulateAudioLoad(int trackCount, int pluginCount);
    void simulateUILoad();
    void simulateAIRequests(int requestCount);
    void simulateFileIOLoad();
    
    // Test projects and data
    void createTestProject(int trackCount, int pluginCount);
    void loadTestAudioFiles();
    void generateSyntheticLoad();
    
    // Performance monitoring
    struct MonitoringData {
        std::chrono::system_clock::time_point timestamp;
        double cpuUsage;
        double memoryUsage;
        double audioLatency;
        int activeVoices;
        double uiFrameRate;
    };
    
    std::vector<MonitoringData> monitoringHistory_;
    std::atomic<bool> monitoringActive_{false};
    std::unique_ptr<std::thread> monitoringThread_;
    
    void monitoringLoop();
    void recordMonitoringData();
    
    // Test configuration
    int audioSampleRate_ = 48000;
    int audioBufferSize_ = 256;
    BenchmarkResults minimumRequirements_;
    BenchmarkResults baseline_;
    
    // Test state
    mutable std::mutex benchmarkMutex_;
    std::atomic<bool> testRunning_{false};
    ProgressCallback currentProgressCallback_;
    
    // Audio test infrastructure
    void* audioDevice_ = nullptr;
    std::atomic<double> measuredLatency_{0.0};
    std::atomic<int> audioCallbackCount_{0};
    std::atomic<int> dropoutCount_{0};
};

// ============================================================================
// Automated Test Suite - Runs Benchmarks Automatically
// ============================================================================

class AutomatedTestSuite {
public:
    enum class TestSchedule {
        ON_STARTUP,     // Run quick test on app startup
        DAILY,          // Run standard test daily
        WEEKLY,         // Run stress test weekly  
        ON_DEMAND       // Manual execution
    };
    
    struct TestConfig {
        TestSchedule schedule = TestSchedule::ON_STARTUP;
        PerformanceBenchmark::BenchmarkType type = PerformanceBenchmark::BenchmarkType::QUICK;
        bool uploadResults = true;
        bool alertOnRegression = true;
        double regressionThreshold = 0.1; // 10% performance drop
    };
    
    AutomatedTestSuite();
    ~AutomatedTestSuite();
    
    void scheduleTest(const TestConfig& config);
    void runScheduledTests();
    
    // Results management
    void saveTestResults(const PerformanceBenchmark::BenchmarkResults& results);
    std::vector<PerformanceBenchmark::BenchmarkResults> getTestHistory() const;
    void uploadResultsToServer(const PerformanceBenchmark::BenchmarkResults& results);
    
    // Regression detection
    bool detectRegression(const PerformanceBenchmark::BenchmarkResults& current) const;
    void alertOnRegression(const PerformanceBenchmark::BenchmarkResults& current);
    
private:
    std::vector<TestConfig> scheduledTests_;
    std::vector<PerformanceBenchmark::BenchmarkResults> testHistory_;
    std::unique_ptr<PerformanceBenchmark> benchmark_;
    
    mutable std::mutex testHistoryMutex_;
    static constexpr int MAX_HISTORY_ENTRIES = 1000;
};

// ============================================================================
// Performance Regression Detection
// ============================================================================

class RegressionDetector {
public:
    struct RegressionAlert {
        std::string metric;
        double previousValue;
        double currentValue;
        double changePercent;
        std::string severity; // "warning", "critical"
        std::string description;
    };
    
    static std::vector<RegressionAlert> detectRegressions(
        const PerformanceBenchmark::BenchmarkResults& baseline,
        const PerformanceBenchmark::BenchmarkResults& current,
        double warningThreshold = 0.1,   // 10%
        double criticalThreshold = 0.25  // 25%
    );
    
    static std::string formatRegressionReport(const std::vector<RegressionAlert>& alerts);
};

// ============================================================================
// Benchmark Data Export
// ============================================================================

class BenchmarkExporter {
public:
    static void exportToCSV(
        const std::vector<PerformanceBenchmark::BenchmarkResults>& results,
        const std::string& filename
    );
    
    static void exportToJSON(
        const PerformanceBenchmark::BenchmarkResults& results,
        const std::string& filename
    );
    
    static std::string generateHTMLReport(
        const PerformanceBenchmark::BenchmarkResults& results
    );
};

// ============================================================================
// Global Benchmark System
// ============================================================================

PerformanceBenchmark& getGlobalBenchmark();
void initializeBenchmarkSystem();
void shutdownBenchmarkSystem();

// Quick benchmark macros for development
#define BENCHMARK_QUICK() getGlobalBenchmark().runFullBenchmark(PerformanceBenchmark::BenchmarkType::QUICK)
#define BENCHMARK_AUDIO_LATENCY() getGlobalBenchmark().benchmarkAudioLatency()
#define BENCHMARK_MEMORY() getGlobalBenchmark().benchmarkMemoryUsage()

} // namespace mixmind::benchmarks