#include "../TestFramework.h"
#include "../../core/types.h"
#include "../../core/result.h"

namespace mixmind::tests {

// ============================================================================
// Core Types Tests
// ============================================================================

class CoreTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        testEnv = &getTestEnvironment();
    }
    
    TestEnvironment* testEnv;
};

// ============================================================================
// StrongID Tests
// ============================================================================

MIXMIND_TEST_F(CoreTypesTest, StrongIDCreation) {
    // Test TrackID creation and comparison
    auto track1 = core::TrackID{1};
    auto track2 = core::TrackID{2};
    auto track1Copy = core::TrackID{1};
    
    EXPECT_EQ(track1.value(), 1);
    EXPECT_NE(track1, track2);
    EXPECT_EQ(track1, track1Copy);
    
    // Test invalid ID
    auto invalidTrack = core::TrackID{};
    EXPECT_FALSE(invalidTrack.isValid());
    EXPECT_TRUE(track1.isValid());
}

MIXMIND_TEST_F(CoreTypesTest, StrongIDHashing) {
    auto track1 = core::TrackID{1};
    auto track2 = core::TrackID{1};
    auto track3 = core::TrackID{2};
    
    std::hash<core::TrackID> hasher;
    EXPECT_EQ(hasher(track1), hasher(track2));
    EXPECT_NE(hasher(track1), hasher(track3));
    
    // Test in unordered containers
    std::unordered_map<core::TrackID, std::string> trackMap;
    trackMap[track1] = "Track 1";
    trackMap[track3] = "Track 2";
    
    EXPECT_EQ(trackMap[track2], "Track 1");
    EXPECT_EQ(trackMap.size(), 2);
}

// ============================================================================
// Result Type Tests
// ============================================================================

MIXMIND_TEST_F(CoreTypesTest, ResultSuccess) {
    auto result = core::Result<int>::success(42);
    
    EXPECT_TRUE(result.hasValue());
    EXPECT_FALSE(result.hasError());
    EXPECT_EQ(result.getValue(), 42);
    EXPECT_NO_THROW(result.getValue());
}

MIXMIND_TEST_F(CoreTypesTest, ResultFailure) {
    auto result = core::Result<int>::failure("Test error");
    
    EXPECT_FALSE(result.hasValue());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.getErrorMessage(), "Test error");
    EXPECT_THROW(result.getValue(), std::runtime_error);
}

MIXMIND_TEST_F(CoreTypesTest, ResultMonadicOperations) {
    auto result = core::Result<int>::success(5);
    
    // Test map
    auto mapped = result.map<double>([](int value) {
        return value * 2.5;
    });
    
    EXPECT_TRUE(mapped.hasValue());
    EXPECT_DOUBLE_EQ(mapped.getValue(), 12.5);
    
    // Test flatMap
    auto flatMapped = result.flatMap<std::string>([](int value) -> core::Result<std::string> {
        return core::Result<std::string>::success("Number: " + std::to_string(value));
    });
    
    EXPECT_TRUE(flatMapped.hasValue());
    EXPECT_EQ(flatMapped.getValue(), "Number: 5");
    
    // Test error propagation
    auto errorResult = core::Result<int>::failure("Original error");
    auto mappedError = errorResult.map<double>([](int value) {
        return value * 2.0;
    });
    
    EXPECT_FALSE(mappedError.hasValue());
    EXPECT_EQ(mappedError.getErrorMessage(), "Original error");
}

MIXMIND_TEST_F(CoreTypesTest, VoidResult) {
    auto success = core::VoidResult::success();
    auto failure = core::VoidResult::failure("Something went wrong");
    
    EXPECT_TRUE(success.hasValue());
    EXPECT_FALSE(failure.hasValue());
    EXPECT_EQ(failure.getErrorMessage(), "Something went wrong");
}

// ============================================================================
// Audio Buffer Tests
// ============================================================================

MIXMIND_TEST_F(CoreTypesTest, FloatAudioBuffer) {
    core::FloatAudioBuffer buffer(2, 1024); // 2 channels, 1024 samples
    
    EXPECT_EQ(buffer.getNumChannels(), 2);
    EXPECT_EQ(buffer.getNumSamples(), 1024);
    EXPECT_NE(buffer.getReadPointer(0), nullptr);
    EXPECT_NE(buffer.getWritePointer(0), nullptr);
    
    // Test buffer manipulation
    buffer.clear();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            EXPECT_FLOAT_EQ_TOL(buffer.getSample(ch, i), 0.0f, 1e-6f);
        }
    }
    
    // Test setting samples
    buffer.setSample(0, 100, 0.5f);
    buffer.setSample(1, 200, -0.3f);
    
    EXPECT_FLOAT_EQ_TOL(buffer.getSample(0, 100), 0.5f, 1e-6f);
    EXPECT_FLOAT_EQ_TOL(buffer.getSample(1, 200), -0.3f, 1e-6f);
}

MIXMIND_TEST_F(CoreTypesTest, AudioBufferOperations) {
    auto testBuffer = TestUtils::generateTestAudio(1024, 2, 440.0f, 48000);
    auto silenceBuffer = TestUtils::generateSilence(1024, 2);
    
    EXPECT_GT(TestUtils::measureRMS(testBuffer), 0.0f);
    EXPECT_FLOAT_EQ_TOL(TestUtils::measureRMS(silenceBuffer), 0.0f, 1e-6f);
    
    EXPECT_GT(TestUtils::measurePeak(testBuffer), 0.0f);
    EXPECT_FLOAT_EQ_TOL(TestUtils::measurePeak(silenceBuffer), 0.0f, 1e-6f);
}

// ============================================================================
// Time Types Tests
// ============================================================================

MIXMIND_TEST_F(CoreTypesTest, TimePosition) {
    core::TimePosition pos1{5.5};
    core::TimePosition pos2{3.2};
    
    EXPECT_DOUBLE_EQ(pos1.count(), 5.5);
    EXPECT_GT(pos1, pos2);
    EXPECT_LT(pos2, pos1);
    
    auto sum = pos1 + core::TimeDuration{1.5};
    EXPECT_DOUBLE_EQ(sum.count(), 7.0);
    
    auto diff = pos1 - pos2;
    EXPECT_DOUBLE_EQ(diff.count(), 2.3);
}

MIXMIND_TEST_F(CoreTypesTest, SampleRate) {
    core::SampleRate sr1{48000};
    core::SampleRate sr2{44100};
    
    EXPECT_EQ(sr1, 48000);
    EXPECT_NE(sr1, sr2);
    EXPECT_GT(sr1, sr2);
    
    // Test sample/time conversions
    auto samples = sr1 * core::TimeDuration{1.0}; // 1 second worth of samples
    EXPECT_EQ(samples, 48000);
    
    auto duration = core::TimeDuration{48000.0 / sr1};
    EXPECT_DOUBLE_EQ(duration.count(), 1.0);
}

// ============================================================================
// Color Tests
// ============================================================================

MIXMIND_TEST_F(CoreTypesTest, ColorRGBA) {
    core::ColorRGBA red{255, 0, 0, 255};
    core::ColorRGBA transparent{0, 0, 0, 0};
    
    EXPECT_EQ(red.r, 255);
    EXPECT_EQ(red.g, 0);
    EXPECT_EQ(red.b, 0);
    EXPECT_EQ(red.a, 255);
    
    // Test color equality
    core::ColorRGBA redCopy{255, 0, 0, 255};
    EXPECT_EQ(red, redCopy);
    EXPECT_NE(red, transparent);
    
    // Test color utility functions
    auto packed = red.toPackedARGB();
    auto unpacked = core::ColorRGBA::fromPackedARGB(packed);
    EXPECT_EQ(red, unpacked);
}

// ============================================================================
// Performance Tests
// ============================================================================

MIXMIND_TEST_F(CoreTypesTest, StrongIDPerformance) {
    constexpr int32_t iterations = 100000;
    
    BENCHMARK_TEST("StrongID Creation", {
        core::TrackID id{i % 10000};
        (void)id;
    }, iterations);
    
    BENCHMARK_TEST("StrongID Hashing", {
        core::TrackID id{i % 1000};
        std::hash<core::TrackID> hasher;
        auto hash = hasher(id);
        (void)hash;
    }, iterations);
}

MIXMIND_TEST_F(CoreTypesTest, ResultPerformance) {
    constexpr int32_t iterations = 50000;
    
    BENCHMARK_TEST("Result Success Creation", {
        auto result = core::Result<int>::success(i);
        (void)result;
    }, iterations);
    
    BENCHMARK_TEST("Result Monadic Operations", {
        auto result = core::Result<int>::success(i);
        auto mapped = result.map<double>([](int v) { return v * 2.0; });
        (void)mapped;
    }, iterations);
}

MIXMIND_TEST_F(CoreTypesTest, AudioBufferPerformance) {
    constexpr int32_t bufferSize = 1024;
    constexpr int32_t iterations = 1000;
    
    BENCHMARK_TEST("AudioBuffer Creation", {
        core::FloatAudioBuffer buffer(2, bufferSize);
        (void)buffer;
    }, iterations);
    
    BENCHMARK_TEST("AudioBuffer Clear", {
        static core::FloatAudioBuffer buffer(2, bufferSize);
        buffer.clear();
    }, iterations * 10);
    
    BENCHMARK_TEST("AudioBuffer RMS Calculation", {
        static auto buffer = TestUtils::generateTestAudio(bufferSize, 2);
        auto rms = TestUtils::measureRMS(buffer);
        (void)rms;
    }, iterations);
}

} // namespace mixmind::tests