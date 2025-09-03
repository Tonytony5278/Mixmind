#include "../TestFramework.h"
#include "../../adapters/tracktion/TESession.h"
#include "../../adapters/tracktion/TEAdapter.h"
#include <memory>

namespace mixmind::tests {

class TEIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Tracktion Engine for testing
        auto& env = getTestEnvironment();
        ASSERT_TRUE(env.initialize());
        engine_ = &env.getTracktionEngine();
    }
    
    void TearDown() override {
        // Cleanup after tests
    }

protected:
    te::Engine* engine_ = nullptr;
};

// ============================================================================
// Basic TE Session Tests
// ============================================================================

MIXMIND_TEST_F(TEIntegrationTest, CreateTESession) {
    // Test basic TE session creation
    ASSERT_NE(engine_, nullptr);
    
    auto session = std::make_unique<mixmind::adapters::tracktion::TESession>(*engine_);
    EXPECT_NE(session, nullptr);
}

MIXMIND_TEST_F(TEIntegrationTest, CreateNewSessionAsync) {
    // Test async session creation
    ASSERT_NE(engine_, nullptr);
    
    auto session = std::make_unique<mixmind::adapters::tracktion::TESession>(*engine_);
    
    core::SessionConfig config;
    config.name = "TestSession";
    config.sampleRate = 48000;
    config.bitDepth = 24;
    config.tempo = 120.0f;
    config.timeSignature = {4, 4};
    
    auto asyncResult = session->createNewSession(config);
    
    // Wait for async operation to complete
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{5000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isSuccess()) << "Session creation failed: " << result.error().toString();
}

MIXMIND_TEST_F(TEIntegrationTest, CreateAudioTrackAsync) {
    // Test async audio track creation
    ASSERT_NE(engine_, nullptr);
    
    auto session = std::make_unique<mixmind::adapters::tracktion::TESession>(*engine_);
    
    // First create a session
    core::SessionConfig config;
    config.name = "TestSession";
    config.sampleRate = 48000;
    config.bitDepth = 24;
    
    auto sessionResult = session->createNewSession(config);
    ASSERT_TRUE(TestUtils::waitForResult(sessionResult, std::chrono::milliseconds{5000}));
    auto sessionCreateResult = sessionResult.get();
    ASSERT_TRUE(sessionCreateResult.isSuccess());
    
    // Now create an audio track
    auto trackResult = session->createAudioTrack("Test Audio Track");
    
    ASSERT_TRUE(TestUtils::waitForResult(trackResult, std::chrono::milliseconds{5000}));
    auto trackCreateResult = trackResult.get();
    EXPECT_TRUE(trackCreateResult.isSuccess()) << "Track creation failed: " << trackCreateResult.error().toString();
    
    if (trackCreateResult.isSuccess()) {
        core::TrackID trackId = trackCreateResult.value();
        EXPECT_GT(trackId.id, 0); // Track ID should be valid
    }
}

MIXMIND_TEST_F(TEIntegrationTest, SetTempoAsync) {
    // Test async tempo setting
    ASSERT_NE(engine_, nullptr);
    
    auto session = std::make_unique<mixmind::adapters::tracktion::TESession>(*engine_);
    
    // First create a session
    core::SessionConfig config;
    config.name = "TestSession";
    config.sampleRate = 48000;
    
    auto sessionResult = session->createNewSession(config);
    ASSERT_TRUE(TestUtils::waitForResult(sessionResult, std::chrono::milliseconds{5000}));
    auto sessionCreateResult = sessionResult.get();
    ASSERT_TRUE(sessionCreateResult.isSuccess());
    
    // Set tempo
    auto tempoResult = session->setTempo(140.0f);
    
    ASSERT_TRUE(TestUtils::waitForResult(tempoResult, std::chrono::milliseconds{2000}));
    auto tempoSetResult = tempoResult.get();
    EXPECT_TRUE(tempoSetResult.isSuccess()) << "Tempo setting failed: " << tempoSetResult.error().toString();
}

MIXMIND_TEST_F(TEIntegrationTest, SessionSaveLoadAsync) {
    // Test async session save/load
    ASSERT_NE(engine_, nullptr);
    
    auto session = std::make_unique<mixmind::adapters::tracktion::TESession>(*engine_);
    
    // Create a test session
    core::SessionConfig config;
    config.name = "SaveLoadTest";
    config.sampleRate = 48000;
    config.tempo = 125.0f;
    
    auto sessionResult = session->createNewSession(config);
    ASSERT_TRUE(TestUtils::waitForResult(sessionResult, std::chrono::milliseconds{5000}));
    ASSERT_TRUE(sessionResult.get().isSuccess());
    
    // Save the session
    std::string testPath = TestUtils::createTempFile(".tracktionedit");
    auto saveResult = session->saveSessionAs(testPath);
    
    ASSERT_TRUE(TestUtils::waitForResult(saveResult, std::chrono::milliseconds{5000}));
    auto saveComplete = saveResult.get();
    EXPECT_TRUE(saveComplete.isSuccess()) << "Session save failed: " << saveComplete.error().toString();
    
    // Create new session and load the saved file
    auto session2 = std::make_unique<mixmind::adapters::tracktion::TESession>(*engine_);
    auto loadResult = session2->loadSession(testPath);
    
    ASSERT_TRUE(TestUtils::waitForResult(loadResult, std::chrono::milliseconds{5000}));
    auto loadComplete = loadResult.get();
    EXPECT_TRUE(loadComplete.isSuccess()) << "Session load failed: " << loadComplete.error().toString();
    
    // Clean up temp file
    TestUtils::cleanupTempFiles();
}

// ============================================================================
// Performance Tests
// ============================================================================

MIXMIND_TEST_F(TEIntegrationTest, AsyncPerformanceBaseline) {
    // Performance test for async operations
    ASSERT_NE(engine_, nullptr);
    
    auto session = std::make_unique<mixmind::adapters::tracktion::TESession>(*engine_);
    
    // Create session once
    core::SessionConfig config;
    config.name = "PerformanceTest";
    config.sampleRate = 48000;
    
    auto sessionResult = session->createNewSession(config);
    ASSERT_TRUE(TestUtils::waitForResult(sessionResult));
    ASSERT_TRUE(sessionResult.get().isSuccess());
    
    // Benchmark track creation
    BENCHMARK_TEST("Track Creation", {
        auto trackResult = session->createAudioTrack("Perf Track");
        ASSERT_TRUE(TestUtils::waitForResult(trackResult, std::chrono::milliseconds{1000}));
        ASSERT_TRUE(trackResult.get().isSuccess());
    }, 10);
    
    // Benchmark tempo changes
    BENCHMARK_TEST("Tempo Changes", {
        auto tempoResult = session->setTempo(120.0f + (rand() % 60));
        ASSERT_TRUE(TestUtils::waitForResult(tempoResult, std::chrono::milliseconds{500}));
        ASSERT_TRUE(tempoResult.get().isSuccess());
    }, 50);
}

} // namespace mixmind::tests