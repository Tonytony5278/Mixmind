#pragma once

#include "result.h"
#include <future>
#include <memory>
#include <functional>
#include <thread>
#include <chrono>

namespace mixmind::core {

// ============================================================================
// Forward Declarations
// ============================================================================

class ThreadPool;
class CancellationToken;

// ============================================================================
// Thread Pool for Better Resource Management
// ============================================================================

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    /// Execute a function asynchronously using the thread pool
    template<typename T, typename Func>
    AsyncResult<T> executeAsync(Func&& operation, const std::string& description = "");
    
    /// Execute a void operation asynchronously using the thread pool
    template<typename Func>
    AsyncResult<void> executeAsyncVoid(Func&& operation, const std::string& description = "");
    
    /// Get the number of worker threads
    size_t getThreadCount() const;
    
    /// Get the number of pending tasks
    size_t getPendingTaskCount() const;
    
    /// Wait for all tasks to complete
    void waitForAll();
    
    /// Shutdown the thread pool
    void shutdown();

private:
    class ThreadPoolImpl;
    std::unique_ptr<ThreadPoolImpl> impl_;
};

// ============================================================================
// Global Thread Pool Instance
// ============================================================================

/// Get the global thread pool instance
ThreadPool& getGlobalThreadPool();

/// Execute using the global thread pool
template<typename T, typename Func>
AsyncResult<T> executeAsyncGlobal(Func&& operation, const std::string& description = "") {
    return getGlobalThreadPool().executeAsync<T>(std::forward<Func>(operation), description);
}

/// Execute void operation using the global thread pool
template<typename Func>
AsyncResult<void> executeAsyncVoidGlobal(Func&& operation, const std::string& description = "") {
    return getGlobalThreadPool().executeAsyncVoid(std::forward<Func>(operation), description);
}

// ============================================================================
// Simple Async Execution Functions
// ============================================================================

/// Execute a function asynchronously and return an AsyncResult
template<typename T, typename Func>
AsyncResult<T> executeAsync(Func&& operation, const std::string& description = "") {
    auto promise = std::make_shared<std::promise<Result<T>>>();
    auto future = promise->get_future();
    
    // Execute in a separate thread
    std::thread([promise, operation = std::forward<Func>(operation), description]() {
        try {
            auto result = operation();
            promise->set_value(std::move(result));
        } catch (const std::exception& e) {
            promise->set_value(Result<T>::error(
                ErrorCode::Unknown,
                ErrorCategory::general(),
                std::string("Async operation failed") + 
                (description.empty() ? "" : " (" + description + ")") +
                ": " + e.what()
            ));
        } catch (...) {
            promise->set_value(Result<T>::error(
                ErrorCode::Unknown,
                ErrorCategory::general(),
                std::string("Async operation failed with unknown error") +
                (description.empty() ? "" : " (" + description + ")")
            ));
        }
    }).detach();
    
    return AsyncResult<T>(std::move(future));
}

/// Execute a void operation asynchronously and return an AsyncResult<VoidResult>
template<typename Func>
AsyncResult<void> executeAsyncVoid(Func&& operation, const std::string& description = "") {
    auto promise = std::make_shared<std::promise<VoidResult>>();
    auto future = promise->get_future();
    
    // Execute in a separate thread
    std::thread([promise, operation = std::forward<Func>(operation), description]() {
        try {
            auto result = operation();
            promise->set_value(std::move(result));
        } catch (const std::exception& e) {
            promise->set_value(VoidResult::error(
                ErrorCode::Unknown,
                ErrorCategory::general(),
                std::string("Async operation failed") + 
                (description.empty() ? "" : " (" + description + ")") +
                ": " + e.what()
            ));
        } catch (...) {
            promise->set_value(VoidResult::error(
                ErrorCode::Unknown,
                ErrorCategory::general(),
                std::string("Async operation failed with unknown error") +
                (description.empty() ? "" : " (" + description + ")")
            ));
        }
    }).detach();
    
    return AsyncResult<void>(std::move(future));
}

/// Execute with timeout and cancellation support
template<typename T, typename Func>
AsyncResult<T> executeAsyncWithTimeout(
    Func&& operation,
    std::chrono::milliseconds timeout,
    CancellationToken* token = nullptr,
    const std::string& description = ""
) {
    auto promise = std::make_shared<std::promise<Result<T>>>();
    auto future = promise->get_future();
    
    std::thread([promise, operation = std::forward<Func>(operation), timeout, token, description]() {
        try {
            auto start = std::chrono::steady_clock::now();
            
            // Execute with periodic cancellation checks
            std::future<Result<T>> operationFuture = std::async(std::launch::async, [&operation]() {
                return operation();
            });
            
            while (operationFuture.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
                auto elapsed = std::chrono::steady_clock::now() - start;
                
                // Check timeout
                if (elapsed > timeout) {
                    promise->set_value(Result<T>::error(
                        ErrorCode::OperationCancelled,
                        ErrorCategory::general(),
                        std::string("Operation timed out") +
                        (description.empty() ? "" : " (" + description + ")")
                    ));
                    return;
                }
                
                // Check cancellation
                if (token && token->isCancelled()) {
                    promise->set_value(Result<T>::error(
                        ErrorCode::OperationCancelled,
                        ErrorCategory::general(),
                        std::string("Operation was cancelled") +
                        (description.empty() ? "" : " (" + description + ")")
                    ));
                    return;
                }
            }
            
            // Get the result
            promise->set_value(operationFuture.get());
            
        } catch (const std::exception& e) {
            promise->set_value(Result<T>::error(
                ErrorCode::Unknown,
                ErrorCategory::general(),
                std::string("Async operation failed") + 
                (description.empty() ? "" : " (" + description + ")") +
                ": " + e.what()
            ));
        }
    }).detach();
    
    return AsyncResult<T>(std::move(future));
}

// ============================================================================
// Progress Reporting 
// ============================================================================

// Note: ProgressCallback is defined in result.h

// TODO: executeAsyncWithProgress function temporarily removed due to compilation issues
// Will be re-implemented with correct ProgressInfo signature

} // namespace mixmind::core