#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <tracktion_engine/tracktion_engine.h>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <random>
#include <filesystem>

namespace mixmind::tests {

// ============================================================================
// Test Framework - Comprehensive testing infrastructure for MixMind
// ============================================================================

/// Test result information
struct TestResult {
    bool passed = false;
    std::string name;
    std::string description;
    std::chrono::milliseconds executionTime{0};
    std::string errorMessage;
    std::vector<std::string> warnings;
    
    TestResult(const std::string& testName) : name(testName) {}
    
    static TestResult pass(const std::string& name, std::chrono::milliseconds time = std::chrono::milliseconds{0}) {
        TestResult result(name);
        result.passed = true;
        result.executionTime = time;
        return result;
    }
    
    static TestResult fail(const std::string& name, const std::string& error, std::chrono::milliseconds time = std::chrono::milliseconds{0}) {
        TestResult result(name);
        result.passed = false;
        result.errorMessage = error;
        result.executionTime = time;
        return result;
    }
};

/// Test suite results
struct TestSuiteResult {
    std::string suiteName;
    std::vector<TestResult> testResults;
    std::chrono::milliseconds totalTime{0};
    int32_t passedCount = 0;
    int32_t failedCount = 0;
    
    void addResult(const TestResult& result) {
        testResults.push_back(result);
        totalTime += result.executionTime;
        if (result.passed) {
            passedCount++;
        } else {
            failedCount++;
        }
    }
    
    bool allPassed() const {
        return failedCount == 0;
    }
};

// ============================================================================
// Test Environment Setup
// ============================================================================

class TestEnvironment {
public:
    TestEnvironment();
    ~TestEnvironment();
    
    /// Initialize test environment
    bool initialize();
    
    /// Cleanup test environment
    void cleanup();
    
    /// Get test data directory
    std::filesystem::path getTestDataDirectory() const;
    
    /// Get temporary directory for test outputs
    std::filesystem::path getTempDirectory() const;
    
    /// Create test audio file
    std::string createTestAudioFile(
        const std::string& filename,
        double duration = 5.0,
        core::SampleRate sampleRate = 48000,
        int32_t channels = 2,
        float frequency = 440.0f
    );
    
    /// Create test MIDI file
    std::string createTestMIDIFile(
        const std::string& filename,
        double duration = 4.0,
        int32_t noteCount = 16
    );
    
    /// Create test project/session
    std::string createTestProject(
        const std::string& projectName,
        int32_t trackCount = 4,
        bool withAudio = true,
        bool withMIDI = true
    );
    
    /// Get Tracktion Engine instance
    te::Engine& getTracktionEngine() { return *engine_; }
    
    /// Get test random generator
    std::mt19937& getRandomGenerator() { return randomGen_; }
    
private:
    std::unique_ptr<te::Engine> engine_;
    std::filesystem::path testDataDir_;
    std::filesystem::path tempDir_;
    std::mt19937 randomGen_;
    bool initialized_ = false;
};

// ============================================================================
// Test Utilities and Helpers
// ============================================================================

class TestUtils {
public:
    /// Compare floating point values with tolerance
    static bool floatEquals(float a, float b, float tolerance = 1e-6f);
    static bool doubleEquals(double a, double b, double tolerance = 1e-9);
    
    /// Compare audio buffers
    static bool audioBuffersEqual(
        const core::FloatAudioBuffer& buffer1,
        const core::FloatAudioBuffer& buffer2,
        float tolerance = 1e-6f
    );
    
    /// Generate test audio buffer
    static core::FloatAudioBuffer generateTestAudio(
        int32_t samples,
        int32_t channels,
        float frequency = 440.0f,
        core::SampleRate sampleRate = 48000
    );
    
    /// Generate silence buffer
    static core::FloatAudioBuffer generateSilence(int32_t samples, int32_t channels);
    
    /// Generate white noise buffer
    static core::FloatAudioBuffer generateWhiteNoise(
        int32_t samples,
        int32_t channels,
        float amplitude = 1.0f,
        std::mt19937& rng = getDefaultRNG()
    );
    
    /// Measure audio buffer RMS level
    static float measureRMS(const core::FloatAudioBuffer& buffer);
    
    /// Measure audio buffer peak level
    static float measurePeak(const core::FloatAudioBuffer& buffer);
    
    /// Wait for async result with timeout
    template<typename T>
    static bool waitForResult(
        core::AsyncResult<T>& asyncResult,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}
    ) {
        return asyncResult.wait_for(timeout) == std::future_status::ready;
    }
    
    /// Get default random number generator
    static std::mt19937& getDefaultRNG() {
        static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        return rng;
    }
    
    /// Create temporary file with unique name
    static std::string createTempFile(const std::string& extension = ".tmp");
    
    /// Cleanup temporary files
    static void cleanupTempFiles();
    
private:
    static std::vector<std::string> tempFiles_;
    static std::mutex tempFilesMutex_;
};

// ============================================================================
// Performance Testing Framework
// ============================================================================

class PerformanceTest {
public:
    struct BenchmarkResult {
        std::string testName;
        std::chrono::microseconds averageTime{0};
        std::chrono::microseconds minTime{std::chrono::microseconds::max()};
        std::chrono::microseconds maxTime{0};
        int32_t iterations = 0;
        double throughput = 0.0; // operations per second
        std::vector<std::chrono::microseconds> allTimes;
    };
    
    /// Run performance benchmark
    template<typename Func>
    static BenchmarkResult benchmark(
        const std::string& testName,
        Func function,
        int32_t iterations = 1000,
        int32_t warmupIterations = 100
    ) {
        BenchmarkResult result;
        result.testName = testName;
        result.iterations = iterations;
        result.allTimes.reserve(iterations);
        
        // Warmup
        for (int32_t i = 0; i < warmupIterations; ++i) {
            function();
        }
        
        // Actual benchmark
        auto totalTime = std::chrono::microseconds{0};
        
        for (int32_t i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            function();
            auto end = std::chrono::high_resolution_clock::now();
            
            auto iterationTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            result.allTimes.push_back(iterationTime);
            totalTime += iterationTime;
            
            if (iterationTime < result.minTime) result.minTime = iterationTime;
            if (iterationTime > result.maxTime) result.maxTime = iterationTime;
        }
        
        result.averageTime = totalTime / iterations;
        result.throughput = 1000000.0 / result.averageTime.count(); // ops per second
        
        return result;
    }
    
    /// Print benchmark results
    static void printBenchmarkResult(const BenchmarkResult& result);
    
    /// Memory usage measurement
    struct MemoryUsage {
        size_t peakMemoryMB = 0;
        size_t currentMemoryMB = 0;
        size_t allocations = 0;
        size_t deallocations = 0;
    };
    
    /// Measure memory usage during function execution
    template<typename Func>
    static MemoryUsage measureMemoryUsage(Func function) {
        MemoryUsage usage;
        
        // Get initial memory
        auto initialMemory = getCurrentMemoryUsage();
        
        function();
        
        // Get final memory
        auto finalMemory = getCurrentMemoryUsage();
        usage.currentMemoryMB = finalMemory;
        usage.peakMemoryMB = std::max(initialMemory, finalMemory);
        
        return usage;
    }
    
private:
    static size_t getCurrentMemoryUsage();
};

// ============================================================================
// Mock Objects for Testing
// ============================================================================

// Mock Progress Callback
class MockProgressCallback {
public:
    MOCK_METHOD(bool, call, (double progress, const std::string& message));
    
    core::ProgressCallback getCallback() {
        return [this](double progress, const std::string& message) -> bool {
            return call(progress, message);
        };
    }
};

// Mock Audio Device
class MockAudioDevice {
public:
    MOCK_METHOD(bool, isAvailable, (), (const));
    MOCK_METHOD(void, start, ());
    MOCK_METHOD(void, stop, ());
    MOCK_METHOD(void, processAudio, (const core::FloatAudioBuffer& input, core::FloatAudioBuffer& output));
};

// ============================================================================
// Test Data Generators
// ============================================================================

class TestDataGenerator {
public:
    /// Generate test session data
    struct SessionData {
        std::string name;
        core::SampleRate sampleRate;
        int32_t bitDepth;
        std::vector<std::string> audioFiles;
        std::vector<std::string> midiFiles;
        int32_t trackCount;
        double duration;
    };
    
    static SessionData generateSessionData(
        const std::string& baseName = "TestSession",
        bool includeAudio = true,
        bool includeMIDI = true
    );
    
    /// Generate test automation data
    struct AutomationData {
        std::vector<std::pair<double, float>> points; // time, value pairs
        core::IAutomation::CurveType curveType;
        float minValue;
        float maxValue;
    };
    
    static AutomationData generateAutomationData(
        double duration = 10.0,
        int32_t pointCount = 20,
        float minValue = 0.0f,
        float maxValue = 1.0f
    );
    
    /// Generate test MIDI data
    struct MIDIData {
        std::vector<core::IClip::MIDINote> notes;
        std::vector<core::IClip::MIDIController> controllers;
        double duration;
    };
    
    static MIDIData generateMIDIData(
        double duration = 4.0,
        int32_t noteCount = 16,
        int32_t controllerCount = 4
    );
};

// ============================================================================
// Test Runner and Reporting
// ============================================================================

class TestRunner {
public:
    /// Add test suite
    void addTestSuite(const std::string& suiteName, std::function<TestSuiteResult()> testFunction);
    
    /// Run all test suites
    std::vector<TestSuiteResult> runAllTests();
    
    /// Run specific test suite
    TestSuiteResult runTestSuite(const std::string& suiteName);
    
    /// Generate test report
    void generateReport(
        const std::vector<TestSuiteResult>& results,
        const std::string& outputPath = "test_report.html"
    );
    
    /// Print test summary to console
    void printSummary(const std::vector<TestSuiteResult>& results);
    
private:
    std::unordered_map<std::string, std::function<TestSuiteResult()>> testSuites_;
};

// ============================================================================
// Global Test Environment Instance
// ============================================================================

/// Get global test environment instance
TestEnvironment& getTestEnvironment();

/// Initialize global test environment
bool initializeTestEnvironment();

/// Cleanup global test environment
void cleanupTestEnvironment();

} // namespace mixmind::tests

// ============================================================================
// Test Macros for Convenience
// ============================================================================

#define MIXMIND_TEST(suite_name, test_name) \
    TEST(suite_name, test_name)

#define MIXMIND_TEST_F(fixture, test_name) \
    TEST_F(fixture, test_name)

#define EXPECT_FLOAT_EQ_TOL(expected, actual, tolerance) \
    EXPECT_TRUE(mixmind::tests::TestUtils::floatEquals(expected, actual, tolerance))

#define EXPECT_AUDIO_BUFFER_EQ(expected, actual) \
    EXPECT_TRUE(mixmind::tests::TestUtils::audioBuffersEqual(expected, actual))

#define EXPECT_ASYNC_SUCCESS(async_result) \
    do { \
        ASSERT_TRUE(mixmind::tests::TestUtils::waitForResult(async_result)); \
        auto result = async_result.get(); \
        EXPECT_TRUE(result.isSuccess()) << "Error: " << result.error().toString(); \
    } while(0)

#define EXPECT_ASYNC_FAILURE(async_result) \
    do { \
        ASSERT_TRUE(mixmind::tests::TestUtils::waitForResult(async_result)); \
        auto result = async_result.get(); \
        EXPECT_FALSE(result.isSuccess()); \
    } while(0)

#define BENCHMARK_TEST(name, code, iterations) \
    do { \
        auto result = mixmind::tests::PerformanceTest::benchmark( \
            name, \
            [&]() { code; }, \
            iterations \
        ); \
        mixmind::tests::PerformanceTest::printBenchmarkResult(result); \
    } while(0)