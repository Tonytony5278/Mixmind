#include "../TestFramework.h"
#include "../../core/async.h"
#include <chrono>
#include <future>
#include <thread>
#include <vector>

namespace mixmind::tests {

class AsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test environment if needed
    }
    
    void TearDown() override {
        // Cleanup after tests
    }
};

// ============================================================================
// Basic Async Execution Tests
// ============================================================================

MIXMIND_TEST_F(AsyncTest, ExecuteAsyncSuccess) {
    // Test basic async execution with successful result
    auto asyncResult = core::executeAsync<int>([]() -> core::Result<int> {
        return core::Result<int>::success(42);
    }, "test async success");
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value(), 42);
}

MIXMIND_TEST_F(AsyncTest, ExecuteAsyncError) {
    // Test async execution with error result
    auto asyncResult = core::executeAsync<int>([]() -> core::Result<int> {
        return core::Result<int>::error(
            core::ErrorCode::Unknown,
            core::ErrorCategory::general(),
            "Test error message"
        );
    }, "test async error");
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.error().code, core::ErrorCode::Unknown);
    EXPECT_NE(result.error().message.find("Test error message"), std::string::npos);
}

MIXMIND_TEST_F(AsyncTest, ExecuteAsyncVoidSuccess) {
    // Test async void execution with success
    auto asyncResult = core::executeAsyncVoid([]() -> core::VoidResult {
        return core::VoidResult::success();
    }, "test async void success");
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isSuccess());
}

MIXMIND_TEST_F(AsyncTest, ExecuteAsyncVoidError) {
    // Test async void execution with error
    auto asyncResult = core::executeAsyncVoid([]() -> core::VoidResult {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::general(),
            "Test void error"
        );
    }, "test async void error");
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.error().code, core::ErrorCode::InvalidParameter);
}

MIXMIND_TEST_F(AsyncTest, ExecuteAsyncException) {
    // Test async execution with exception
    auto asyncResult = core::executeAsync<int>([]() -> core::Result<int> {
        throw std::runtime_error("Test exception");
        return core::Result<int>::success(42);
    }, "test async exception");
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("Test exception"), std::string::npos);
}

// ============================================================================
// Thread Pool Tests
// ============================================================================

MIXMIND_TEST_F(AsyncTest, ThreadPoolBasic) {
    // Test thread pool basic functionality
    core::ThreadPool pool(2);
    
    auto asyncResult = pool.executeAsync<int>([]() -> core::Result<int> {
        return core::Result<int>::success(100);
    }, "thread pool test");
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value(), 100);
}

MIXMIND_TEST_F(AsyncTest, ThreadPoolMultipleTasks) {
    // Test thread pool with multiple concurrent tasks
    core::ThreadPool pool(4);
    
    std::vector<core::AsyncResult<core::Result<int>>> futures;
    
    // Start 10 tasks
    for (int i = 0; i < 10; ++i) {
        auto asyncResult = pool.executeAsync<int>([i]() -> core::Result<int> {
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            return core::Result<int>::success(i * 2);
        }, "multiple tasks test");
        
        futures.push_back(std::move(asyncResult));
    }
    
    // Wait for all tasks and verify results
    for (size_t i = 0; i < futures.size(); ++i) {
        ASSERT_TRUE(TestUtils::waitForResult(futures[i], std::chrono::milliseconds{2000}));
        
        auto result = futures[i].get();
        EXPECT_TRUE(result.isSuccess());
        EXPECT_EQ(result.value(), static_cast<int>(i) * 2);
    }
}

MIXMIND_TEST_F(AsyncTest, GlobalThreadPoolUsage) {
    // Test global thread pool functionality
    auto asyncResult = core::executeAsyncVoidGlobal([]() -> core::VoidResult {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        return core::VoidResult::success();
    }, "global thread pool test");
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isSuccess());
}

// ============================================================================
// Cancellation Token Tests
// ============================================================================

MIXMIND_TEST_F(AsyncTest, CancellationTokenBasic) {
    // Test cancellation token basic functionality
    core::CancellationToken token;
    
    EXPECT_FALSE(token.isCancelled());
    
    token.cancel();
    EXPECT_TRUE(token.isCancelled());
}

MIXMIND_TEST_F(AsyncTest, ExecuteWithTimeout) {
    // Test async execution with timeout
    auto asyncResult = core::executeAsyncWithTimeout<int>(
        []() -> core::Result<int> {
            // This should timeout
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
            return core::Result<int>::success(42);
        },
        std::chrono::milliseconds{50}, // Short timeout
        nullptr,
        "timeout test"
    );
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("timed out"), std::string::npos);
}

MIXMIND_TEST_F(AsyncTest, ExecuteWithCancellation) {
    // Test async execution with cancellation
    core::CancellationToken token;
    
    auto asyncResult = core::executeAsyncWithTimeout<int>(
        [&token]() -> core::Result<int> {
            // Check for cancellation during work
            for (int i = 0; i < 100; ++i) {
                if (token.isCancelled()) {
                    return core::Result<int>::error(
                        core::ErrorCode::OperationCancelled,
                        core::ErrorCategory::general(),
                        "Operation was cancelled"
                    );
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{5});
            }
            return core::Result<int>::success(42);
        },
        std::chrono::milliseconds{5000}, // Long timeout
        &token,
        "cancellation test"
    );
    
    // Cancel after a short delay
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    token.cancel();
    
    ASSERT_TRUE(TestUtils::waitForResult(asyncResult, std::chrono::milliseconds{1000}));
    
    auto result = asyncResult.get();
    EXPECT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("cancelled"), std::string::npos);
}

} // namespace mixmind::tests