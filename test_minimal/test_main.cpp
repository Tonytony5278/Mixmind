#include "result.h"
#include "async.h"
#include <iostream>
#include <chrono>
#include <thread>

// Test the core async and result systems
int main() {
    std::cout << "=== MixMind Core Systems Test ===" << std::endl;
    
    // Test 1: Basic Result<T> functionality
    std::cout << "\n1. Testing Result<T> system..." << std::endl;
    
    auto successResult = mixmind::core::Result<int>::success(42);
    auto errorResult = mixmind::core::Result<int>::error(
        mixmind::core::ErrorCode::Unknown,
        mixmind::core::ErrorCategory::general(),
        "Test error message"
    );
    
    if (successResult.isSuccess()) {
        std::cout << "   ✓ Success result works: " << successResult.value() << std::endl;
    } else {
        std::cout << "   ✗ Success result failed!" << std::endl;
        return 1;
    }
    
    if (errorResult.isError()) {
        std::cout << "   ✓ Error result works: " << errorResult.error().toString() << std::endl;
    } else {
        std::cout << "   ✗ Error result failed!" << std::endl;
        return 1;
    }
    
    // Test legacy API compatibility
    if (successResult.hasValue() && errorResult.getErrorMessage().find("Test error") != std::string::npos) {
        std::cout << "   ✓ Legacy API compatibility works" << std::endl;
    } else {
        std::cout << "   ✗ Legacy API compatibility failed!" << std::endl;
        return 1;
    }
    
    // Test 2: VoidResult functionality
    std::cout << "\n2. Testing VoidResult system..." << std::endl;
    
    auto voidSuccess = mixmind::core::VoidResult::success();
    auto voidError = mixmind::core::VoidResult::error(
        mixmind::core::ErrorCode::InvalidParameter,
        mixmind::core::ErrorCategory::general(),
        "Void test error"
    );
    
    if (voidSuccess.isSuccess() && voidError.isError()) {
        std::cout << "   ✓ VoidResult works correctly" << std::endl;
    } else {
        std::cout << "   ✗ VoidResult failed!" << std::endl;
        return 1;
    }
    
    // Test 3: Simple async execution
    std::cout << "\n3. Testing async execution..." << std::endl;
    
    auto asyncResult = mixmind::core::executeAsync<int>([]() -> mixmind::core::Result<int> {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        return mixmind::core::Result<int>::success(123);
    }, "test async operation");
    
    // Wait for result
    if (asyncResult.wait_for(std::chrono::milliseconds{2000}) == std::future_status::ready) {
        auto result = asyncResult.get();
        if (result.isSuccess() && result.value() == 123) {
            std::cout << "   ✓ Async execution works: " << result.value() << std::endl;
        } else {
            std::cout << "   ✗ Async execution returned wrong value!" << std::endl;
            return 1;
        }
    } else {
        std::cout << "   ✗ Async execution timed out!" << std::endl;
        return 1;
    }
    
    // Test 4: Async void execution
    std::cout << "\n4. Testing async void execution..." << std::endl;
    
    auto asyncVoidResult = mixmind::core::executeAsyncVoid([]() -> mixmind::core::VoidResult {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        return mixmind::core::VoidResult::success();
    }, "test async void operation");
    
    if (asyncVoidResult.wait_for(std::chrono::milliseconds{2000}) == std::future_status::ready) {
        auto result = asyncVoidResult.get();
        if (result.isSuccess()) {
            std::cout << "   ✓ Async void execution works" << std::endl;
        } else {
            std::cout << "   ✗ Async void execution failed: " << result.error().toString() << std::endl;
            return 1;
        }
    } else {
        std::cout << "   ✗ Async void execution timed out!" << std::endl;
        return 1;
    }
    
    // Test 5: Thread pool functionality
    std::cout << "\n5. Testing thread pool..." << std::endl;
    
    {
        mixmind::core::ThreadPool pool(2);
        
        auto poolResult = pool.executeAsync<std::string>([]() -> mixmind::core::Result<std::string> {
            return mixmind::core::Result<std::string>::success("Thread pool works!");
        }, "thread pool test");
        
        if (poolResult.wait_for(std::chrono::milliseconds{2000}) == std::future_status::ready) {
            auto result = poolResult.get();
            if (result.isSuccess()) {
                std::cout << "   ✓ Thread pool works: " << result.value() << std::endl;
            } else {
                std::cout << "   ✗ Thread pool failed: " << result.error().toString() << std::endl;
                return 1;
            }
        } else {
            std::cout << "   ✗ Thread pool timed out!" << std::endl;
            return 1;
        }
    } // ThreadPool destructor should clean up threads
    
    // Test 6: Global thread pool
    std::cout << "\n6. Testing global thread pool..." << std::endl;
    
    auto globalResult = mixmind::core::executeAsyncVoidGlobal([]() -> mixmind::core::VoidResult {
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
        return mixmind::core::VoidResult::success();
    }, "global thread pool test");
    
    if (globalResult.wait_for(std::chrono::milliseconds{2000}) == std::future_status::ready) {
        auto result = globalResult.get();
        if (result.isSuccess()) {
            std::cout << "   ✓ Global thread pool works" << std::endl;
        } else {
            std::cout << "   ✗ Global thread pool failed: " << result.error().toString() << std::endl;
            return 1;
        }
    } else {
        std::cout << "   ✗ Global thread pool timed out!" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== All Core Tests Passed! ===" << std::endl;
    std::cout << "MixMind async and result systems are working correctly." << std::endl;
    
    return 0;
}