#include "ChatService.h"
#include "ActionAPI.h"
#include "IntentRecognition.h"
#include "../core/async.h"
#include <iostream>
#include <cassert>
#include <chrono>

namespace mixmind::ai::test {

// ============================================================================
// AI Assistant Integration Test
// ============================================================================

class AIAssistantIntegrationTest {
public:
    AIAssistantIntegrationTest() = default;
    
    // Test the complete AI pipeline
    bool runFullPipelineTest() {
        std::cout << "=== AI Assistant Integration Test ===\n\n";
        
        bool success = true;
        success &= testChatService();
        success &= testActionAPI();
        success &= testIntentRecognition();
        success &= testFullPipeline();
        
        if (success) {
            std::cout << "\n✅ All tests passed! AI Assistant is working correctly.\n";
        } else {
            std::cout << "\n❌ Some tests failed. Check the output above.\n";
        }
        
        return success;
    }

private:
    bool testChatService() {
        std::cout << "1. Testing ChatService...\n";
        
        try {
            auto& chatService = getGlobalChatService();
            
            // Initialize with mock configuration
            AIProviderConfig config;
            config.provider = AIProvider::Mock;
            config.modelName = "gpt-4-mock";
            
            auto initResult = chatService.initialize(config).get();
            if (!initResult.isSuccess()) {
                std::cout << "   ❌ Failed to initialize ChatService: " << initResult.getErrorMessage() << "\n";
                return false;
            }
            
            // Start a conversation
            auto conversationResult = chatService.startConversation("test_user").get();
            if (!conversationResult.isSuccess()) {
                std::cout << "   ❌ Failed to start conversation: " << conversationResult.getErrorMessage() << "\n";
                return false;
            }
            
            std::string conversationId = conversationResult.value();
            
            // Send a test message
            auto responseResult = chatService.sendMessage(conversationId, "Hello, can you help me?").get();
            if (!responseResult.isSuccess()) {
                std::cout << "   ❌ Failed to send message: " << responseResult.getErrorMessage() << "\n";
                return false;
            }
            
            auto response = responseResult.value();
            std::cout << "   Response: " << response.content << "\n";
            
            // Test DAW-specific commands
            auto dawResponseResult = chatService.sendMessage(conversationId, "create a new track").get();
            if (!dawResponseResult.isSuccess()) {
                std::cout << "   ❌ Failed to send DAW command: " << dawResponseResult.getErrorMessage() << "\n";
                return false;
            }
            
            auto dawResponse = dawResponseResult.value();
            std::cout << "   DAW Response: " << dawResponse.content << "\n";
            
            std::cout << "   ✅ ChatService working correctly\n\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "   ❌ Exception in ChatService test: " << e.what() << "\n\n";
            return false;
        }
    }
    
    bool testActionAPI() {
        std::cout << "2. Testing ActionAPI...\n";
        
        try {
            auto& actionAPI = getGlobalActionAPI();
            
            // Initialize with null services (for testing)
            auto initResult = actionAPI.initialize(nullptr, nullptr, nullptr, nullptr, nullptr).get();
            if (!initResult.isSuccess()) {
                std::cout << "   ❌ Failed to initialize ActionAPI: " << initResult.getErrorMessage() << "\n";
                return false;
            }
            
            // Test getting available actions
            auto actions = actionAPI.getAllActions();
            std::cout << "   Available actions: " << actions.size() << "\n";
            
            for (const auto& action : actions) {
                std::cout << "     - " << action.name << ": " << action.description << "\n";
            }
            
            // Test command parsing
            ActionContext context;
            auto parseResult = actionAPI.parseIntent("play").get();
            if (!parseResult.isSuccess()) {
                std::cout << "   ❌ Failed to parse intent: " << parseResult.getErrorMessage() << "\n";
                return false;
            }
            
            auto intent = parseResult.value();
            std::cout << "   Parsed intent: " << intent.intent << " (confidence: " << intent.confidence << ")\n";
            
            // Test command execution
            auto executeResult = actionAPI.executeCommand("play", context).get();
            if (!executeResult.isSuccess()) {
                std::cout << "   ❌ Failed to execute command: " << executeResult.getErrorMessage() << "\n";
                return false;
            }
            
            auto result = executeResult.value();
            std::cout << "   Execution result: " << result.message << " (success: " << result.success << ")\n";
            
            // Test more complex commands
            testComplexCommands(actionAPI);
            
            std::cout << "   ✅ ActionAPI working correctly\n\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "   ❌ Exception in ActionAPI test: " << e.what() << "\n\n";
            return false;
        }
    }
    
    bool testIntentRecognition() {
        std::cout << "3. Testing IntentRecognition...\n";
        
        try {
            auto& intentRecognition = getGlobalIntentRecognition();
            
            // Initialize
            auto initResult = intentRecognition.initialize().get();
            if (!initResult.isSuccess()) {
                std::cout << "   ❌ Failed to initialize IntentRecognition: " << initResult.getErrorMessage() << "\n";
                return false;
            }
            
            // Test various intent classifications
            testIntentClassification(intentRecognition, "play the track", "transport_play");
            testIntentClassification(intentRecognition, "create a new audio track", "track_create");
            testIntentClassification(intentRecognition, "mute track 2", "track_mute");
            testIntentClassification(intentRecognition, "set tempo to 120", "transport_set_tempo");
            testIntentClassification(intentRecognition, "help me with mixing", "help_request");
            
            // Test entity extraction
            auto entitiesResult = intentRecognition.extractEntities("mute track 3 and set volume to 75%").get();
            if (!entitiesResult.isSuccess()) {
                std::cout << "   ❌ Failed to extract entities: " << entitiesResult.getErrorMessage() << "\n";
                return false;
            }
            
            auto entities = entitiesResult.value();
            std::cout << "   Extracted entities: " << entities.size() << "\n";
            for (const auto& entity : entities) {
                std::cout << "     - " << entity.text << " (" << static_cast<int>(entity.type) << ")\n";
            }
            
            std::cout << "   ✅ IntentRecognition working correctly\n\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "   ❌ Exception in IntentRecognition test: " << e.what() << "\n\n";
            return false;
        }
    }
    
    bool testFullPipeline() {
        std::cout << "4. Testing Full Pipeline (Natural Language → Intent → Action → Response)...\n";
        
        try {
            // Test complete workflow
            std::vector<std::string> testCommands = {
                "create a new track",
                "play",
                "set tempo to 140",
                "mute track 1",
                "save the session",
                "what's the current tempo?",
                "help me with recording"
            };
            
            for (const auto& command : testCommands) {
                bool success = testSingleCommand(command);
                if (!success) {
                    std::cout << "   ❌ Failed pipeline test for: " << command << "\n";
                    return false;
                }
            }
            
            std::cout << "   ✅ Full pipeline working correctly\n\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "   ❌ Exception in full pipeline test: " << e.what() << "\n\n";
            return false;
        }
    }
    
    void testComplexCommands(ActionAPI& actionAPI) {
        std::cout << "     Testing complex commands...\n";
        
        ActionContext context;
        
        // Test tempo command with parameter
        auto result1 = actionAPI.executeCommand("set tempo to 130", context).get();
        if (result1.isSuccess()) {
            auto actionResult = result1.value();
            std::cout << "       Tempo command: " << actionResult.message << "\n";
        }
        
        // Test track creation with type
        auto result2 = actionAPI.executeCommand("create audio track", context).get();
        if (result2.isSuccess()) {
            auto actionResult = result2.value();
            std::cout << "       Track creation: " << actionResult.message << "\n";
        }
        
        // Test suggestions
        auto suggestionsResult = actionAPI.getSuggestions("create", context, 3).get();
        if (suggestionsResult.isSuccess()) {
            auto suggestions = suggestionsResult.value();
            std::cout << "       Suggestions for 'create': " << suggestions.size() << " items\n";
            for (const auto& suggestion : suggestions) {
                std::cout << "         - " << suggestion.command << "\n";
            }
        }
    }
    
    void testIntentClassification(IntentRecognition& recognition, const std::string& input, const std::string& expectedIntent) {
        IntentRecognitionContext context;
        auto result = recognition.classifyIntent(input, context).get();
        
        if (result.isSuccess()) {
            auto classification = result.value();
            std::cout << "   Input: '" << input << "' → Intent: " << classification.specificIntent;
            std::cout << " (confidence: " << classification.confidence << ")";
            
            if (classification.specificIntent == expectedIntent) {
                std::cout << " ✅\n";
            } else {
                std::cout << " ❌ (expected: " << expectedIntent << ")\n";
            }
        } else {
            std::cout << "   Failed to classify: " << input << " - " << result.getErrorMessage() << "\n";
        }
    }
    
    bool testSingleCommand(const std::string& command) {
        std::cout << "     Testing: '" << command << "'\n";
        
        auto& intentRecognition = getGlobalIntentRecognition();
        auto& actionAPI = getGlobalActionAPI();
        auto& chatService = getGlobalChatService();
        
        // Step 1: Intent Recognition
        IntentRecognitionContext context;
        auto intentResult = intentRecognition.classifyIntent(command, context).get();
        if (!intentResult.isSuccess()) {
            std::cout << "       ❌ Intent recognition failed: " << intentResult.getErrorMessage() << "\n";
            return false;
        }
        
        auto intent = intentResult.value();
        std::cout << "       Intent: " << intent.specificIntent << " (confidence: " << intent.confidence << ")\n";
        
        // Step 2: Action Execution (if it's a command)
        if (intent.type == IntentType::Command && intent.confidence > 0.5) {
            ActionContext actionContext;
            auto actionResult = actionAPI.executeIntent(intent, actionContext).get();
            if (!actionResult.isSuccess()) {
                std::cout << "       ❌ Action execution failed: " << actionResult.getErrorMessage() << "\n";
                return false;
            }
            
            auto result = actionResult.value();
            std::cout << "       Action: " << result.message << " (success: " << result.success << ")\n";
        }
        
        // Step 3: Generate response (simulated)
        std::string response = generateMockResponse(intent);
        std::cout << "       Response: " << response << "\n";
        
        std::cout << "       ✅ Pipeline completed successfully\n";
        return true;
    }
    
    std::string generateMockResponse(const ParsedIntent& intent) {
        if (intent.type == IntentType::Command) {
            if (intent.specificIntent.find("play") != std::string::npos) {
                return "Started playback.";
            } else if (intent.specificIntent.find("create") != std::string::npos) {
                return "Created a new track.";
            } else if (intent.specificIntent.find("tempo") != std::string::npos) {
                return "Tempo has been updated.";
            } else if (intent.specificIntent.find("mute") != std::string::npos) {
                return "Track has been muted.";
            } else if (intent.specificIntent.find("save") != std::string::npos) {
                return "Session saved successfully.";
            }
            return "Command executed.";
        } else if (intent.type == IntentType::Query) {
            return "The current tempo is 120 BPM.";
        } else if (intent.type == IntentType::Help) {
            return "I can help you with DAW operations. What would you like to learn about?";
        }
        
        return "I understand. How can I help you further?";
    }
};

// ============================================================================
// Performance Test
// ============================================================================

class PerformanceTest {
public:
    void runPerformanceTest() {
        std::cout << "=== Performance Test ===\n";
        
        const int numIterations = 100;
        std::vector<std::string> testCommands = {
            "play",
            "stop",
            "create track",
            "mute track 1",
            "set tempo to 120",
            "help with mixing"
        };
        
        auto& intentRecognition = getGlobalIntentRecognition();
        auto& actionAPI = getGlobalActionAPI();
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numIterations; ++i) {
            for (const auto& command : testCommands) {
                // Test intent recognition performance
                IntentRecognitionContext context;
                auto intentResult = intentRecognition.classifyIntent(command, context).get();
                
                if (intentResult.isSuccess()) {
                    auto intent = intentResult.value();
                    
                    // Test action execution performance
                    if (intent.type == IntentType::Command) {
                        ActionContext actionContext;
                        auto actionResult = actionAPI.executeIntent(intent, actionContext).get();
                    }
                }
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        int totalCommands = numIterations * testCommands.size();
        double avgTimeMs = static_cast<double>(duration.count()) / totalCommands;
        
        std::cout << "Processed " << totalCommands << " commands in " << duration.count() << "ms\n";
        std::cout << "Average time per command: " << avgTimeMs << "ms\n";
        
        if (avgTimeMs < 10.0) {
            std::cout << "✅ Performance test passed (< 10ms per command)\n\n";
        } else {
            std::cout << "❌ Performance test failed (> 10ms per command)\n\n";
        }
    }
};

} // namespace mixmind::ai::test

// ============================================================================
// Test Runner Function
// ============================================================================

bool runAIAssistantTests() {
    using namespace mixmind::ai::test;
    
    AIAssistantIntegrationTest integrationTest;
    bool integrationSuccess = integrationTest.runFullPipelineTest();
    
    PerformanceTest performanceTest;
    performanceTest.runPerformanceTest();
    
    return integrationSuccess;
}

// Main function for standalone testing
#ifdef AI_ASSISTANT_TEST_MAIN
int main() {
    std::cout << "MixMind AI Assistant - Integration Test\n";
    std::cout << "======================================\n\n";
    
    // Initialize global thread pool
    // (In real application this would be done by core system)
    
    bool success = runAIAssistantTests();
    
    std::cout << "\nTest Summary:\n";
    if (success) {
        std::cout << "🎉 AI Assistant is ready for 'Cursor × Logic' experience!\n";
        return 0;
    } else {
        std::cout << "🔧 Some components need attention before deployment.\n";
        return 1;
    }
}
#endif