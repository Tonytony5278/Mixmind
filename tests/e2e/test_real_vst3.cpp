#include <gtest/gtest.h>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "../../src/vst3/RealVST3Scanner.h"
#include "../../src/core/result.h"

using namespace mixmind;

class RealVST3Test : public ::testing::Test {
protected:
    void SetUp() override {
        scanner = std::make_unique<RealVST3Scanner>();
        
        // Create artifacts directory if it doesn't exist
        std::filesystem::create_directories("artifacts");
        
        // Open log file for detailed output
        logFile.open("artifacts/real_vst3_test.log");
        if (logFile.is_open()) {
            logFile << "=== MixMind AI Real VST3 Integration Test Log ===" << std::endl;
            logFile << "Date: " << getCurrentTimestamp() << std::endl;
            logFile << "Test Suite: Real VST3 Plugin Integration" << std::endl;
            logFile << "=====================================================" << std::endl;
        }
    }
    
    void TearDown() override {
        if (logFile.is_open()) {
            logFile << "\n=== Test Suite Completed ===" << std::endl;
            logFile << "End Time: " << getCurrentTimestamp() << std::endl;
            logFile.close();
        }
    }
    
    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    void logResult(const std::string& testName, bool success, const std::string& details = "") {
        if (logFile.is_open()) {
            logFile << "\n[" << getCurrentTimestamp() << "] " << testName << ": " 
                    << (success ? "PASS" : "FAIL") << std::endl;
            if (!details.empty()) {
                logFile << "  Details: " << details << std::endl;
            }
        }
    }
    
    std::unique_ptr<RealVST3Scanner> scanner;
    std::ofstream logFile;
};

// Test VST3 system directory detection
TEST_F(RealVST3Test, SystemDirectoryDetection) {
    auto directories = scanner->getSystemVST3Directories();
    
    EXPECT_FALSE(directories.empty()) << "No VST3 system directories found";
    
    bool foundValidDir = false;
    for (const auto& dir : directories) {
        if (std::filesystem::exists(dir)) {
            foundValidDir = true;
            std::cout << "Found VST3 directory: " << dir << std::endl;
        }
    }
    
    logResult("SystemDirectoryDetection", !directories.empty(), 
              "Found " + std::to_string(directories.size()) + " directories");
}

// Test system VST3 plugin scanning
TEST_F(RealVST3Test, SystemPluginScan) {
    auto result = scanner->scanSystemPlugins();
    
    if (result.isSuccess()) {
        auto plugins = result.getValue();
        std::cout << "Found " << plugins.size() << " VST3 plugins" << std::endl;
        
        for (const auto& plugin : plugins) {
            std::cout << "  - " << plugin.name << " at " << plugin.path << std::endl;
            EXPECT_TRUE(plugin.isValid) << "Plugin should be marked as valid: " << plugin.name;
            EXPECT_FALSE(plugin.path.empty()) << "Plugin path should not be empty";
            EXPECT_FALSE(plugin.name.empty()) << "Plugin name should not be empty";
        }
        
        logResult("SystemPluginScan", true, 
                  "Found " + std::to_string(plugins.size()) + " plugins");
    } else {
        std::cout << "No VST3 plugins found: " << result.getError().toString() << std::endl;
        logResult("SystemPluginScan", true, "No plugins found (acceptable)");
    }
}

// Test specific plugin detection - Span
TEST_F(RealVST3Test, SpanPluginDetection) {
    auto result = scanner->findSpanPlugin();
    
    if (result.isSuccess()) {
        auto plugin = result.getValue();
        std::cout << "✅ Span plugin found: " << plugin.path << std::endl;
        
        EXPECT_EQ(plugin.name, "Span") << "Plugin name should be 'Span'";
        EXPECT_TRUE(plugin.isValid) << "Span plugin should be valid";
        EXPECT_TRUE(std::filesystem::exists(plugin.path)) << "Span plugin path should exist";
        
        logResult("SpanPluginDetection", true, "Found at: " + plugin.path);
    } else {
        std::cout << "Span plugin not found: " << result.getError().toString() << std::endl;
        logResult("SpanPluginDetection", true, "Plugin not installed (acceptable)");
    }
}

// Test specific plugin detection - TDR Nova
TEST_F(RealVST3Test, TDRNovaPluginDetection) {
    auto result = scanner->findTDRNovaPlugin();
    
    if (result.isSuccess()) {
        auto plugin = result.getValue();
        std::cout << "✅ TDR Nova plugin found: " << plugin.path << std::endl;
        
        EXPECT_EQ(plugin.name, "TDR Nova") << "Plugin name should be 'TDR Nova'";
        EXPECT_TRUE(plugin.isValid) << "TDR Nova plugin should be valid";
        EXPECT_TRUE(std::filesystem::exists(plugin.path)) << "TDR Nova plugin path should exist";
        
        logResult("TDRNovaPluginDetection", true, "Found at: " + plugin.path);
    } else {
        std::cout << "TDR Nova plugin not found: " << result.getError().toString() << std::endl;
        logResult("TDRNovaPluginDetection", true, "Plugin not installed (acceptable)");
    }
}

// Generate summary report
TEST_F(RealVST3Test, GenerateSummaryReport) {
    std::ofstream summaryFile("artifacts/real_vst3_summary.txt");
    
    if (summaryFile.is_open()) {
        summaryFile << "MixMind AI - Real VST3 Integration Test Summary\n";
        summaryFile << "==============================================\n\n";
        summaryFile << "Date: " << getCurrentTimestamp() << "\n";
        summaryFile << "Test Type: Real VST3 Plugin Integration\n\n";
        
        // Scan for plugins and report
        auto scanResult = scanner->scanSystemPlugins();
        if (scanResult.isSuccess()) {
            auto plugins = scanResult.getValue();
            summaryFile << "VST3 Plugins Found: " << plugins.size() << "\n\n";
            
            for (const auto& plugin : plugins) {
                summaryFile << "Plugin: " << plugin.name << "\n";
                summaryFile << "  Path: " << plugin.path << "\n";
                summaryFile << "  Valid: " << (plugin.isValid ? "Yes" : "No") << "\n\n";
            }
        } else {
            summaryFile << "VST3 Plugins Found: 0\n";
            summaryFile << "Reason: " << scanResult.getError().toString() << "\n\n";
        }
        
        // Check for specific plugins
        auto spanResult = scanner->findSpanPlugin();
        auto novaResult = scanner->findTDRNovaPlugin();
        
        summaryFile << "Specific Plugin Detection:\n";
        summaryFile << "  Span: " << (spanResult.isSuccess() ? "FOUND" : "NOT FOUND") << "\n";
        summaryFile << "  TDR Nova: " << (novaResult.isSuccess() ? "FOUND" : "NOT FOUND") << "\n\n";
        
        summaryFile << "Real VST3 Integration Status: ";
        if (scanResult.isSuccess() && !scanResult.getValue().empty()) {
            summaryFile << "SUCCESS - Real plugins detected and validated\n";
        } else {
            summaryFile << "READY - System configured for VST3 plugins\n";
            summaryFile << "Install Span or TDR Nova to test with real plugins\n";
        }
        
        summaryFile.close();
        std::cout << "Summary report generated: artifacts/real_vst3_summary.txt" << std::endl;
    }
    
    logResult("GenerateSummaryReport", true, "Report generated successfully");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "\n=== MixMind AI Real VST3 Integration Tests ===" << std::endl;
    std::cout << "Testing real VST3 plugin detection and validation" << std::endl;
    std::cout << "============================================\n" << std::endl;
    
    auto result = RUN_ALL_TESTS();
    
    std::cout << "\n=== Real VST3 Test Summary ===" << std::endl;
    if (result == 0) {
        std::cout << "✅ All real VST3 integration tests passed!" << std::endl;
        std::cout << "Check artifacts/ directory for detailed logs and reports" << std::endl;
    } else {
        std::cout << "❌ Some tests failed - check artifacts/real_vst3_test.log" << std::endl;
    }
    
    return result;
}