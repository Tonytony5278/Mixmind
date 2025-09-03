#include "result.h"
#include "async.h"
#include <iostream>
#include <chrono>
#include <thread>

// Extremely simple test to verify basic functionality
int main() {
    std::cout << "=== Simple MixMind Core Test ===" << std::endl;
    
    // Test 1: Basic Result<T> functionality
    std::cout << "\n1. Testing Result<T>..." << std::endl;
    
    auto successResult = mixmind::core::Result<int>::success(42);
    
    if (successResult.isSuccess()) {
        std::cout << "   SUCCESS: Result value = " << successResult.value() << std::endl;
    } else {
        std::cout << "   FAILED: Success result not working!" << std::endl;
        return 1;
    }
    
    // Test 2: VoidResult
    std::cout << "\n2. Testing VoidResult..." << std::endl;
    
    auto voidSuccess = mixmind::core::VoidResult::success();
    if (voidSuccess.isSuccess()) {
        std::cout << "   SUCCESS: VoidResult works" << std::endl;
    } else {
        std::cout << "   FAILED: VoidResult not working!" << std::endl;
        return 1;
    }
    
    // Test 3: Basic async - just test that it compiles and runs
    std::cout << "\n3. Testing basic async..." << std::endl;
    
    try {
        auto asyncResult = mixmind::core::executeAsync<int>([]() -> mixmind::core::Result<int> {
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
            return mixmind::core::Result<int>::success(100);
        });
        
        // Simple wait and get
        auto result = asyncResult.get();
        if (result.isSuccess()) {
            int val = result.value();
            std::cout << "   SUCCESS: Async execution works, value = " << val << std::endl;
        } else {
            std::cout << "   FAILED: Async execution failed!" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "   FAILED: Exception in async test: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    std::cout << "Core MixMind systems are working." << std::endl;
    
    return 0;
}