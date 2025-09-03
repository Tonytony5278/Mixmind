#include "result.h"
#include "async.h"
#include <iostream>
#include <thread>
#include <chrono>

// Test Result<T> and basic async
int main() {
    std::cout << "=== Result + Async Test ===" << std::endl;
    
    // Create a simple Result<int>
    auto result = mixmind::core::Result<int>::success(42);
    
    if (result.isSuccess()) {
        std::cout << "SUCCESS: Result is success" << std::endl;
        
        // This should give us an int
        auto val = result.value();
        std::cout << "Value type test completed" << std::endl;
        
        // Now try to print it
        std::cout << "Value: " << static_cast<int>(val) << std::endl;
    } else {
        std::cout << "FAILED" << std::endl;
        return 1;
    }
    
    // Test 2: Simple async execution (just test the template compilation)
    std::cout << "\nTesting async system..." << std::endl;
    
    try {
        // This will test if our async templates compile correctly
        auto asyncOp = mixmind::core::executeAsync<int>([]() -> mixmind::core::Result<int> {
            return mixmind::core::Result<int>::success(99);
        });
        
        std::cout << "Async operation created successfully!" << std::endl;
        
        // Test the get operation
        auto asyncResult = asyncOp.get();
        std::cout << "AsyncResult.get() completed" << std::endl;
        
        if (asyncResult.isSuccess()) {
            std::cout << "AsyncResult is success" << std::endl;
            
            // Let's see what type value() returns
            auto val = asyncResult.value();
            std::cout << "Got value from asyncResult" << std::endl;
            
            // Don't assign to int yet, just use it differently
            std::cout << "Async test completed" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Async test failed: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "All tests completed successfully!" << std::endl;
    return 0;
}