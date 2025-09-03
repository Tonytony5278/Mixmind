#include <iostream>
#include <memory>

// Basic test to validate build system works
int main() {
    std::cout << "=== MixMind AI Core Test ===" << std::endl;
    std::cout << "Build system validated!" << std::endl;
    std::cout << "C++20 features: std::make_unique available" << std::endl;
    
    auto test_ptr = std::make_unique<int>(42);
    std::cout << "Smart pointer test: " << *test_ptr << std::endl;
    
    std::cout << "Core build successful!" << std::endl;
    return 0;
}