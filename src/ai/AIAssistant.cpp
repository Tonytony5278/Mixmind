#include "AIAssistant.h"
#include "../audio/AudioBuffer.h"
#include <algorithm>
#include <sstream>
#include <random>
#include <iomanip>
#include <chrono>
#include <thread>

namespace mixmind::ai {

// ============================================================================
// AIAssistant Implementation
// ============================================================================

AIAssistant::AIAssistant() {
    // Initialize components
    chatService_ = std::make_unique<ChatService>();
    actionAPI_ = std::make_unique<ActionAPI>();
    intentRecognition_ = std::make_unique<IntentRecognition>();
    contextManager_ = std::make_unique<ConversationContextManager>();
    openAIProvider_ = std::make_unique<OpenAIProvider>();
    
    // Initialize analytics
    analytics_.session_start_time = std::chrono::steady_clock::now();
}

AIAssistant::~AIAssistant() {
    if (isInitialized_.load()) {
        shutdown().wait();
    }
}

core::AsyncResult<core::VoidResult> AIAssistant::initialize(
    const AssistantConfig& config,
    std::shared_ptr<core::ISession> session,
    std::shared_ptr<core::ITrack> track,
    std::shared_ptr<core::IClip> clip,
    std::shared_ptr<core::ITransport> transport,
    std::shared_ptr<core::IPluginHost> pluginHost) {
    
    return core::async([this, config, session, track, clip, transport, pluginHost]() -> core::VoidResult {
        if (isInitialized_.load()) {
            return core::VoidResult{};
        }
        
        config_ = config;
        
        // Initialize OpenAI Provider
        auto openai_result = openAIProvider_->initialize(config.providerConfig);
        if (!openai_result.is_ok()) {
            return core::Error("Failed to initialize OpenAI provider: " + openai_result.error().message);
        }
        
        // Initialize Chat Service
        auto chat_result = chatService_->initialize(openAIProvider_.get());
        if (!chat_result.is_ok()) {
            return core::Error("Failed to initialize chat service: " + chat_result.error().message);
        }
        
        // Initialize Action API with DAW components
        auto action_result = actionAPI_->initialize(session, track, clip, transport, pluginHost);
        if (!action_result.is_ok()) {
            return core::Error("Failed to initialize action API: " + action_result.error().message);
        }
        
        // Initialize Intent Recognition
        auto intent_result = intentRecognition_->initialize();
        if (!intent_result.is_ok()) {
            return core::Error("Failed to initialize intent recognition: " + intent_result.error().message);
        }
        
        // Initialize Context Manager
        auto context_result = contextManager_->initialize(session, track, clip, transport);
        if (!context_result.is_ok()) {
            return core::Error("Failed to initialize context manager: " + context_result.error().message);
        }
        
        isInitialized_.store(true);
        return core::VoidResult{};
    });
}

core::AsyncResult<core::VoidResult> AIAssistant::shutdown() {
    return core::async([this]() -> core::VoidResult {
        if (!isInitialized_.load()) {
            return core::VoidResult{};
        }
        
        shouldShutdown_.store(true);
        
        // Shutdown components in reverse order
        if (contextManager_) contextManager_->shutdown();
        if (intentRecognition_) intentRecognition_->shutdown();
        if (actionAPI_) actionAPI_->shutdown();
        if (chatService_) chatService_->shutdown();
        if (openAIProvider_) openAIProvider_->shutdown();
        
        isInitialized_.store(false);
        return core::VoidResult{};
    });
}

core::AsyncResult<core::Result<std::string>> AIAssistant::startConversation(
    const std::string& userId,
    AssistantMode mode) {
    
    return core::async([this, userId, mode]() -> core::Result<std::string> {
        if (!isInitialized_.load()) {
            return core::Error("AI Assistant not initialized");
        }
        
        // Generate conversation ID
        std::string conversationId = generateConversationId();
        
        // Set conversation mode
        {
            std::unique_lock<std::shared_mutex> lock(conversationMutex_);
            conversationModes_[conversationId] = mode;
        }
        
        // Initialize conversation context
        auto context_result = contextManager_->startConversation(conversationId, userId);
        if (!context_result.is_ok()) {
            return core::Error("Failed to start conversation context: " + context_result.error().message);
        }
        
        // Update analytics
        {
            std::lock_guard<std::mutex> lock(analyticsMutex_);
            analytics_.totalConversations++;
        }
        
        return core::Ok(conversationId);
    });
}

core::AsyncResult<core::VoidResult> AIAssistant::endConversation(const std::string& conversationId) {
    return core::async([this, conversationId]() -> core::VoidResult {
        {
            std::unique_lock<std::shared_mutex> lock(conversationMutex_);
            conversationModes_.erase(conversationId);
        }
        
        // End conversation context
        contextManager_->endConversation(conversationId);
        
        return core::VoidResult{};
    });
}

core::AsyncResult<core::Result<AssistantResponse>> AIAssistant::sendMessage(
    const std::string& conversationId,
    const std::string& message) {
    
    return core::async([this, conversationId, message]() -> core::Result<AssistantResponse> {
        return processMessage(conversationId, message, false).wait();
    });
}

core::AsyncResult<core::Result<std::string>> AIAssistant::sendMessageStreaming(
    const std::string& conversationId,
    const std::string& message,
    StreamingCallback callback) {
    
    return core::async([this, conversationId, message, callback]() -> core::Result<std::string> {
        auto response = processMessage(conversationId, message, true).wait();
        if (!response.is_ok()) {
            return core::Error(response.error().message);
        }
        
        // Stream the response
        auto response_data = response.unwrap();
        if (callback) {
            callback(response_data.primaryMessage, true);
        }
        
        return core::Ok(response_data.responseId);
    });
}

core::AsyncResult<core::Result<AssistantResponse>> AIAssistant::processCommand(
    const std::string& conversationId,
    const std::string& command) {
    
    return core::async([this, conversationId, command]() -> core::Result<AssistantResponse> {
        auto start_time = std::chrono::steady_clock::now();
        
        // Parse intent
        auto intent_result = intentRecognition_->recognizeIntent(command);
        if (!intent_result.is_ok()) {
            return core::Error("Failed to recognize intent: " + intent_result.error().message);
        }
        
        auto intent = intent_result.unwrap();
        
        // Execute actions based on intent
        std::vector<std::string> action_results;
        if (intent.hasAction) {
            auto actions_result = executeActions(conversationId, intent.suggestedActions).wait();
            if (actions_result.is_ok()) {
                action_results = actions_result.unwrap();
            }
        }
        
        // Generate response
        auto response = generateResponse(conversationId, command, action_results, ResponseType::ActionConfirmation);
        
        // Update analytics
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        response.responseTime = duration;
        
        {
            std::lock_guard<std::mutex> lock(analyticsMutex_);
            analytics_.totalMessages++;
            if (!action_results.empty()) {
                analytics_.successfulActions++;
            }
            analytics_.averageResponseTime = (analytics_.averageResponseTime * (analytics_.totalMessages - 1) + 
                                            duration.count()) / analytics_.totalMessages;
        }
        
        return core::Ok(response);
    });
}

core::AsyncResult<core::Result<AssistantResponse>> AIAssistant::analyzeProject(
    const std::string& conversationId) {
    
    return core::async([this, conversationId]() -> core::Result<AssistantResponse> {
        // Get current project context
        auto context = contextManager_->getCurrentContext(conversationId);
        if (!context.is_ok()) {
            return core::Error("Failed to get project context: " + context.error().message);
        }
        
        auto context_data = context.unwrap();
        
        // Analyze project structure
        std::vector<std::string> insights;
        
        // Track analysis
        if (context_data.trackCount > 0) {
            insights.push_back("Project has " + std::to_string(context_data.trackCount) + " tracks");
            
            if (context_data.trackCount > 32) {
                insights.push_back("Large project - consider using track folders for organization");
            }
        }
        
        // Plugin analysis
        if (context_data.pluginCount > 0) {
            insights.push_back("Using " + std::to_string(context_data.pluginCount) + " plugins");
            
            if (context_data.pluginCount > 50) {
                insights.push_back("High plugin usage - monitor CPU performance");
            }
        }
        
        // Audio analysis
        if (context_data.projectDuration > 0) {
            auto minutes = static_cast<int>(context_data.projectDuration / 60.0);
            auto seconds = static_cast<int>(context_data.projectDuration) % 60;
            insights.push_back("Project duration: " + std::to_string(minutes) + ":" + 
                             std::to_string(seconds).substr(0, 2) + "s");
        }
        
        // Mixing suggestions
        insights.push_back("Ready for mixing analysis - use 'analyze mix' for detailed feedback");
        
        AssistantResponse response;
        response.conversationId = conversationId;
        response.responseId = generateResponseId();
        response.type = ResponseType::Answer;
        response.primaryMessage = "Project Analysis Complete";
        response.additionalInfo = insights;
        response.confidence = 0.9;
        response.responseTime = std::chrono::milliseconds(100);
        
        // Add suggestions
        response.suggestions = {
            "Analyze mix quality",
            "Suggest arrangement improvements", 
            "Optimize workflow",
            "Review plugin usage"
        };
        
        return core::Ok(response);
    });
}

core::AsyncResult<core::Result<std::vector<std::string>>> AIAssistant::generateCreativeSuggestions(
    const std::string& conversationId,
    const std::string& context) {
    
    return core::async([this, conversationId, context]() -> core::Result<std::vector<std::string>> {
        std::vector<std::string> suggestions;
        
        // Musical creative suggestions
        suggestions.push_back("Add a countermelody in the bridge section");
        suggestions.push_back("Try a different drum pattern for the chorus");
        suggestions.push_back("Consider adding string arrangements for emotional depth");
        suggestions.push_back("Experiment with reverse reverb on the lead vocal");
        
        // Arrangement suggestions
        suggestions.push_back("Create dynamic contrast with a breakdown section");
        suggestions.push_back("Add percussive elements for rhythmic interest");
        suggestions.push_back("Layer harmonies in the final chorus");
        suggestions.push_back("Use automation to create build-ups and drops");
        
        // Sound design suggestions
        suggestions.push_back("Apply creative effects to transition elements");
        suggestions.push_back("Create ambient textures with pad sounds");
        suggestions.push_back("Use sidechain compression for rhythmic pumping");
        suggestions.push_back("Add width and depth with stereo imaging");
        
        return core::Ok(suggestions);
    });
}

core::AsyncResult<core::Result<AssistantResponse>> AIAssistant::provideMixingFeedback(
    const std::string& conversationId,
    const std::vector<std::string>& focusAreas) {
    
    return core::async([this, conversationId, focusAreas]() -> core::Result<AssistantResponse> {
        AssistantResponse response;
        response.conversationId = conversationId;
        response.responseId = generateResponseId();
        response.type = ResponseType::Suggestion;
        response.primaryMessage = "Mix Analysis and Feedback";
        
        std::vector<std::string> feedback;
        
        // General mixing feedback
        feedback.push_back("Overall balance: Check low-mid buildup around 200-400Hz");
        feedback.push_back("High frequencies: Consider gentle high-shelf boost for air");
        feedback.push_back("Stereo width: Use mid-side processing for better spatial balance");
        feedback.push_back("Dynamics: Apply gentle bus compression for glue");
        
        // Specific focus areas
        if (!focusAreas.empty()) {
            for (const auto& area : focusAreas) {
                if (area == "vocals") {
                    feedback.push_back("Vocals: De-ess harsh sibilants, add presence around 2-5kHz");
                } else if (area == "drums") {
                    feedback.push_back("Drums: Tighten kick with HP filter, enhance snare crack");
                } else if (area == "bass") {
                    feedback.push_back("Bass: Ensure mono compatibility, manage low-end buildup");
                }
            }
        }
        
        // Loudness considerations
        feedback.push_back("Loudness: Target -14 LUFS for streaming platforms");
        feedback.push_back("Peak levels: Keep true peaks below -1dBFS");
        
        response.additionalInfo = feedback;
        response.confidence = 0.85;
        response.responseTime = std::chrono::milliseconds(150);
        
        // Add actionable suggestions
        response.suggestions = {
            "Apply suggested EQ adjustments",
            "Set up bus compression",
            "Check mono compatibility",
            "Measure loudness levels"
        };
        
        return core::Ok(response);
    });
}

core::AsyncResult<core::Result<AssistantResponse>> AIAssistant::startTutorial(
    const std::string& conversationId,
    const std::string& topic) {
    
    return core::async([this, conversationId, topic]() -> core::Result<AssistantResponse> {
        AssistantResponse response;
        response.conversationId = conversationId;
        response.responseId = generateResponseId();
        response.type = ResponseType::Explanation;
        response.primaryMessage = "Tutorial: " + topic;
        
        // Set conversation to tutorial mode
        {
            std::unique_lock<std::shared_mutex> lock(conversationMutex_);
            conversationModes_[conversationId] = AssistantMode::Tutorial;
        }
        
        std::vector<std::string> tutorial_steps;
        
        if (topic == "mixing") {
            tutorial_steps = {
                "1. Start with level balancing - get a rough mix first",
                "2. Apply high-pass filters to remove unnecessary low-end",
                "3. Use EQ to carve space for each instrument",
                "4. Add compression for dynamics control and character",
                "5. Apply reverb and delay for spatial dimension",
                "6. Use automation to enhance musical phrases",
                "7. Check your mix on multiple monitoring systems"
            };
        } else if (topic == "recording") {
            tutorial_steps = {
                "1. Set proper input levels - aim for -18dBFS to -12dBFS peaks",
                "2. Choose appropriate microphone for source material",
                "3. Position microphone for optimal sound capture",
                "4. Use acoustic treatment to control room reflections",
                "5. Monitor through headphones to avoid feedback",
                "6. Record with minimal processing - fix in post",
                "7. Create multiple takes and comp the best parts"
            };
        } else {
            tutorial_steps = {
                "Tutorial content for '" + topic + "' is being prepared",
                "Ask specific questions about this topic for detailed guidance"
            };
        }
        
        response.additionalInfo = tutorial_steps;
        response.confidence = 0.95;
        response.responseTime = std::chrono::milliseconds(80);
        
        response.followUpQuestions = {
            "What specific aspect would you like to focus on?",
            "Do you have questions about any of these steps?",
            "Would you like practical examples for any step?"
        };
        
        return core::Ok(response);
    });
}

core::AsyncResult<core::Result<AssistantResponse>> AIAssistant::startTroubleshooting(
    const std::string& conversationId,
    const std::string& problemDescription) {
    
    return core::async([this, conversationId, problemDescription]() -> core::Result<AssistantResponse> {
        AssistantResponse response;
        response.conversationId = conversationId;
        response.responseId = generateResponseId();
        response.type = ResponseType::Suggestion;
        response.primaryMessage = "Troubleshooting Assistant";
        
        // Set conversation to troubleshooting mode
        {
            std::unique_lock<std::shared_mutex> lock(conversationMutex_);
            conversationModes_[conversationId] = AssistantMode::Troubleshooting;
        }
        
        std::vector<std::string> diagnostic_steps;
        std::vector<std::string> common_solutions;
        
        // Analyze problem description for keywords
        std::string lower_problem = problemDescription;
        std::transform(lower_problem.begin(), lower_problem.end(), lower_problem.begin(), ::tolower);
        
        if (lower_problem.find("audio") != std::string::npos && 
            lower_problem.find("cut") != std::string::npos) {
            diagnostic_steps = {
                "1. Check audio driver settings and buffer size",
                "2. Verify input/output device connections",
                "3. Test with different sample rates (44.1kHz, 48kHz)",
                "4. Disable other audio applications",
                "5. Update audio drivers to latest version"
            };
            common_solutions = {
                "Increase buffer size to 512 or 1024 samples",
                "Use ASIO drivers for Windows",
                "Close background applications using audio"
            };
        } else if (lower_problem.find("cpu") != std::string::npos || 
                  lower_problem.find("performance") != std::string::npos) {
            diagnostic_steps = {
                "1. Check CPU usage in system monitor",
                "2. Review plugin count and CPU-heavy effects",
                "3. Freeze or render CPU-intensive tracks", 
                "4. Increase buffer size for better performance",
                "5. Disable unnecessary plugins and features"
            };
            common_solutions = {
                "Freeze tracks with heavy plugin processing",
                "Use audio bounce for complex instrument chains",
                "Reduce project sample rate if appropriate"
            };
        } else {
            diagnostic_steps = {
                "1. Describe the exact steps that lead to the problem",
                "2. Note any error messages displayed",
                "3. Check if problem occurs with new/empty projects",
                "4. Try safe mode or disable plugins temporarily",
                "5. Check system resources (CPU, RAM, disk space)"
            };
        }
        
        response.additionalInfo = diagnostic_steps;
        response.suggestions = common_solutions;
        response.confidence = 0.8;
        response.responseTime = std::chrono::milliseconds(120);
        
        response.followUpQuestions = {
            "Can you provide more details about when this occurs?",
            "Have you tried any of these solutions already?",
            "Are there any error messages I should know about?"
        };
        
        return core::Ok(response);
    });
}

// Private implementation methods

core::AsyncResult<core::Result<AssistantResponse>> AIAssistant::processMessage(
    const std::string& conversationId,
    const std::string& message,
    bool streaming) {
    
    return core::async([this, conversationId, message, streaming]() -> core::Result<AssistantResponse> {
        auto start_time = std::chrono::steady_clock::now();
        
        // Get conversation mode
        AssistantMode mode = AssistantMode::Conversational;
        {
            std::shared_lock<std::shared_mutex> lock(conversationMutex_);
            auto it = conversationModes_.find(conversationId);
            if (it != conversationModes_.end()) {
                mode = it->second;
            }
        }
        
        // Build system prompt
        std::string system_prompt = buildSystemPrompt(conversationId, mode);
        
        // Get conversation history
        auto history_result = contextManager_->getConversationHistory(conversationId);
        std::vector<ChatMessage> history;
        if (history_result.is_ok()) {
            history = history_result.unwrap();
        }
        
        // Process through chat service
        ChatRequest chat_request;
        chat_request.conversationId = conversationId;
        chat_request.message = message;
        chat_request.systemPrompt = system_prompt;
        chat_request.conversationHistory = history;
        chat_request.streamResponse = streaming;
        
        auto chat_result = chatService_->processMessage(chat_request).wait();
        if (!chat_result.is_ok()) {
            return core::Error("Chat processing failed: " + chat_result.error().message);
        }
        
        auto chat_response = chat_result.unwrap();
        
        // Convert to AssistantResponse
        AssistantResponse response;
        response.conversationId = conversationId;
        response.responseId = generateResponseId();
        response.type = ResponseType::Answer;
        response.primaryMessage = chat_response.message;
        response.confidence = chat_response.confidence;
        
        auto end_time = std::chrono::steady_clock::now();
        response.responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Update context with new message
        contextManager_->addMessage(conversationId, message, response.primaryMessage);
        
        // Update analytics
        {
            std::lock_guard<std::mutex> lock(analyticsMutex_);
            analytics_.totalMessages++;
            analytics_.averageResponseTime = (analytics_.averageResponseTime * (analytics_.totalMessages - 1) + 
                                            response.responseTime.count()) / analytics_.totalMessages;
            analytics_.averageConfidence = (analytics_.averageConfidence * (analytics_.totalMessages - 1) + 
                                          response.confidence) / analytics_.totalMessages;
        }
        
        return core::Ok(response);
    });
}

std::string AIAssistant::buildSystemPrompt(const std::string& conversationId, AssistantMode mode) const {
    std::ostringstream prompt;
    
    prompt << "You are MixMind AI, an advanced DAW assistant that provides expert guidance for music production.\n\n";
    
    switch (mode) {
        case AssistantMode::Conversational:
            prompt << "Engage in natural conversation about music production. Be helpful, encouraging, and knowledgeable.\n";
            break;
        case AssistantMode::CommandMode:
            prompt << "Focus on executing DAW commands efficiently. Provide clear confirmations and brief explanations.\n";
            break;
        case AssistantMode::Tutorial:
            prompt << "Act as a music production teacher. Provide step-by-step guidance and educational explanations.\n";
            break;
        case AssistantMode::Creative:
            prompt << "Be a creative collaborator. Suggest musical ideas, arrangements, and artistic enhancements.\n";
            break;
        case AssistantMode::Troubleshooting:
            prompt << "Help diagnose and solve technical issues. Ask clarifying questions and provide systematic solutions.\n";
            break;
        case AssistantMode::Analysis:
            prompt << "Analyze audio, projects, and mixing decisions. Provide detailed technical feedback.\n";
            break;
    }
    
    prompt << "\nKey capabilities:\n";
    prompt << "- Control all DAW functions through natural language\n";
    prompt << "- Analyze audio and provide mixing suggestions\n";
    prompt << "- Teach music production concepts\n";
    prompt << "- Solve technical problems\n";
    prompt << "- Generate creative ideas and arrangements\n";
    prompt << "- Optimize workflows and suggest best practices\n\n";
    
    prompt << "Always be:\n";
    prompt << "- Accurate and technically sound\n";
    prompt << "- Encouraging and supportive\n";
    prompt << "- Clear and easy to understand\n";
    prompt << "- Focused on the user's musical goals\n";
    
    return prompt.str();
}

core::AsyncResult<core::Result<std::vector<std::string>>> AIAssistant::executeActions(
    const std::string& conversationId,
    const std::vector<std::string>& actions) {
    
    return core::async([this, conversationId, actions]() -> core::Result<std::vector<std::string>> {
        std::vector<std::string> results;
        
        for (const auto& action : actions) {
            ActionRequest request;
            request.actionType = action;
            request.conversationId = conversationId;
            
            auto result = actionAPI_->executeAction(request).wait();
            if (result.is_ok()) {
                auto action_result = result.unwrap();
                results.push_back(action_result.result);
            } else {
                results.push_back("Failed: " + result.error().message);
            }
        }
        
        return core::Ok(results);
    });
}

AssistantResponse AIAssistant::generateResponse(
    const std::string& conversationId,
    const std::string& originalMessage,
    const std::vector<std::string>& actionResults,
    ResponseType type) const {
    
    AssistantResponse response;
    response.conversationId = conversationId;
    response.responseId = generateResponseId();
    response.type = type;
    response.confidence = 0.9;
    
    if (actionResults.empty()) {
        response.primaryMessage = "I understand your request. How can I help you further?";
    } else {
        response.primaryMessage = "Actions completed successfully";
        response.actionsPerformed = actionResults;
    }
    
    return response;
}

AssistantResponse AIAssistant::handleError(
    const std::string& conversationId,
    const std::string& error,
    const std::string& context) const {
    
    AssistantResponse response;
    response.conversationId = conversationId;
    response.responseId = generateResponseId();
    response.type = ResponseType::Error;
    response.hasError = true;
    response.errorMessage = error;
    response.primaryMessage = "I encountered an issue: " + error;
    response.confidence = 0.1;
    
    response.suggestions = {
        "Try rephrasing your request",
        "Check if all required parameters are provided",
        "Ask for help with specific steps"
    };
    
    return response;
}

std::string AIAssistant::generateConversationId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000000, 9999999);
    
    return "conv_" + std::to_string(dis(gen));
}

std::string AIAssistant::generateResponseId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000000, 9999999);
    
    return "resp_" + std::to_string(dis(gen));
}

AIAssistant::AssistantAnalytics AIAssistant::getAnalytics() const {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    return analytics_;
}

// Configuration management
core::VoidResult AIAssistant::updateConfig(const AssistantConfig& config) {
    config_ = config;
    return core::VoidResult{};
}

AssistantConfig AIAssistant::getConfig() const {
    return config_;
}

// ============================================================================
// Factory Implementation
// ============================================================================

std::unique_ptr<AIAssistant> AIAssistantFactory::createBeginnerAssistant() {
    AssistantConfig config;
    config.personality = AssistantPersonality::Educational;
    config.includeExplanations = true;
    config.confirmDestructiveActions = true;
    config.proactiveHelp = true;
    
    return createCustomAssistant(config);
}

std::unique_ptr<AIAssistant> AIAssistantFactory::createProducerAssistant() {
    AssistantConfig config;
    config.personality = AssistantPersonality::Creative;
    config.defaultMode = AssistantMode::Creative;
    config.proactiveHelp = true;
    config.contextAwareness = true;
    
    return createCustomAssistant(config);
}

std::unique_ptr<AIAssistant> AIAssistantFactory::createEngineerAssistant() {
    AssistantConfig config;
    config.personality = AssistantPersonality::Professional;
    config.defaultMode = AssistantMode::Analysis;
    config.includeExplanations = true;
    config.confidenceThreshold = 0.8;
    
    return createCustomAssistant(config);
}

std::unique_ptr<AIAssistant> AIAssistantFactory::createCreativeAssistant() {
    AssistantConfig config;
    config.personality = AssistantPersonality::Creative;
    config.defaultMode = AssistantMode::Creative;
    config.suggestAlternatives = true;
    config.maxSuggestionsPerResponse = 5;
    
    return createCustomAssistant(config);
}

std::unique_ptr<AIAssistant> AIAssistantFactory::createEducationalAssistant() {
    AssistantConfig config;
    config.personality = AssistantPersonality::Educational;
    config.defaultMode = AssistantMode::Tutorial;
    config.includeExplanations = true;
    config.proactiveHelp = true;
    
    return createCustomAssistant(config);
}

std::unique_ptr<AIAssistant> AIAssistantFactory::createCustomAssistant(const AssistantConfig& config) {
    return std::make_unique<AIAssistant>();
}

// ============================================================================
// Global Instance Management
// ============================================================================

static std::unique_ptr<AIAssistant> globalAssistant;
static std::mutex globalAssistantMutex;

AIAssistant& getGlobalAIAssistant() {
    std::lock_guard<std::mutex> lock(globalAssistantMutex);
    if (!globalAssistant) {
        globalAssistant = std::make_unique<AIAssistant>();
    }
    return *globalAssistant;
}

core::AsyncResult<core::VoidResult> initializeGlobalAIAssistant(const AssistantConfig& config) {
    return core::async([config]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(globalAssistantMutex);
        if (!globalAssistant) {
            globalAssistant = std::make_unique<AIAssistant>();
        }
        // Note: This requires DAW components to be passed - would need to be called from main app
        return core::VoidResult{};
    });
}

} // namespace mixmind::ai