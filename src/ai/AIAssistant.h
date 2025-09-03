#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../core/async.h"
#include "ChatService.h"
#include "ActionAPI.h"
#include "IntentRecognition.h"
#include "ConversationContext.h"
#include "OpenAIProvider.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

namespace mixmind::ai {

// ============================================================================
// AI Assistant Integration - Complete AI-First DAW Experience
// ============================================================================

enum class AssistantMode {
    Conversational,     // Free-form conversation
    CommandMode,        // Direct command execution
    Tutorial,           // Teaching/learning mode
    Creative,           // Creative collaboration mode
    Troubleshooting,    // Problem-solving mode
    Analysis            // Analysis and insights mode
};

enum class AssistantPersonality {
    Professional,       // Formal, technical
    Friendly,          // Casual, encouraging
    Expert,            // Detailed, comprehensive
    Concise,           // Brief, to-the-point
    Educational,       // Teaching-focused
    Creative           // Inspiring, artistic
};

struct AssistantConfig {
    AssistantMode defaultMode = AssistantMode::Conversational;
    AssistantPersonality personality = AssistantPersonality::Friendly;
    
    // AI Configuration
    AIProviderConfig providerConfig;
    std::string systemPrompt;
    bool useToolCalls = true;
    bool streamResponses = true;
    
    // Behavior settings
    bool proactiveHelp = true;
    bool contextAwareness = true;
    bool learningEnabled = true;
    double confidenceThreshold = 0.7;
    
    // Response preferences
    bool includeExplanations = true;
    bool suggestAlternatives = true;
    bool confirmDestructiveActions = true;
    int maxSuggestionsPerResponse = 3;
    
    // Integration settings
    std::vector<std::string> enabledServices;
    std::unordered_map<std::string, std::string> customSettings;
};

// ============================================================================
// Assistant Response Types
// ============================================================================

enum class ResponseType {
    Answer,             // Direct answer to question
    ActionConfirmation, // Confirming action was taken
    Clarification,      // Asking for clarification
    Suggestion,         // Suggesting actions
    Explanation,        // Explaining concepts
    Error,              // Error occurred
    Warning,            // Warning about potential issues
    Success             // Task completed successfully
};

struct AssistantResponse {
    std::string conversationId;
    std::string responseId;
    ResponseType type;
    
    // Response content
    std::string primaryMessage;
    std::vector<std::string> additionalInfo;
    std::vector<std::string> suggestions;
    std::vector<std::string> alternatives;
    
    // Action information
    std::vector<std::string> actionsPerformed;
    std::vector<std::string> stateChanges;
    std::optional<std::string> undoInformation;
    
    // Follow-up options
    std::vector<std::string> followUpQuestions;
    std::vector<std::string> relatedTopics;
    
    // Metadata
    double confidence;
    std::chrono::milliseconds responseTime;
    std::unordered_map<std::string, std::string> metadata;
    
    // Error information
    bool hasError = false;
    std::string errorMessage;
    std::string errorCode;
};

// ============================================================================
// AI Assistant - Main AI Integration Class
// ============================================================================

class AIAssistant {
public:
    using ResponseCallback = std::function<void(const AssistantResponse&)>;
    using StreamingCallback = std::function<void(const std::string& chunk, bool isComplete)>;
    using ActionConfirmationCallback = std::function<bool(const std::string& action, const std::vector<std::string>& details)>;
    
    AIAssistant();
    ~AIAssistant();
    
    // Non-copyable
    AIAssistant(const AIAssistant&) = delete;
    AIAssistant& operator=(const AIAssistant&) = delete;
    
    // ========================================================================
    // Service Lifecycle
    // ========================================================================
    
    /// Initialize AI Assistant with full DAW integration
    core::AsyncResult<core::VoidResult> initialize(
        const AssistantConfig& config,
        std::shared_ptr<core::ISession> session,
        std::shared_ptr<core::ITrack> track,
        std::shared_ptr<core::IClip> clip,
        std::shared_ptr<core::ITransport> transport,
        std::shared_ptr<core::IPluginHost> pluginHost
    );
    
    /// Shutdown AI Assistant
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if assistant is ready
    bool isReady() const { return isInitialized_.load(); }
    
    /// Update configuration
    core::VoidResult updateConfig(const AssistantConfig& config);
    
    /// Get current configuration
    AssistantConfig getConfig() const;
    
    // ========================================================================
    // Conversation Management
    // ========================================================================
    
    /// Start new conversation session
    core::AsyncResult<core::Result<std::string>> startConversation(
        const std::string& userId = "",
        AssistantMode mode = AssistantMode::Conversational
    );
    
    /// End conversation session
    core::AsyncResult<core::VoidResult> endConversation(const std::string& conversationId);
    
    /// Send message to assistant
    core::AsyncResult<core::Result<AssistantResponse>> sendMessage(
        const std::string& conversationId,
        const std::string& message
    );
    
    /// Send message with streaming response
    core::AsyncResult<core::Result<std::string>> sendMessageStreaming(
        const std::string& conversationId,
        const std::string& message,
        StreamingCallback callback
    );
    
    /// Switch conversation mode
    core::VoidResult setConversationMode(
        const std::string& conversationId,
        AssistantMode mode
    );
    
    // ========================================================================
    // DAW Command Processing
    // ========================================================================
    
    /// Process DAW command through natural language
    core::AsyncResult<core::Result<AssistantResponse>> processCommand(
        const std::string& conversationId,
        const std::string& command
    );
    
    /// Execute batch commands
    core::AsyncResult<core::Result<std::vector<AssistantResponse>>> processBatchCommands(
        const std::string& conversationId,
        const std::vector<std::string>& commands
    );
    
    /// Get command suggestions based on context
    core::AsyncResult<core::Result<std::vector<std::string>>> getCommandSuggestions(
        const std::string& conversationId,
        const std::string& partialCommand = ""
    );
    
    /// Validate command before execution
    core::AsyncResult<core::Result<std::vector<std::string>>> validateCommand(
        const std::string& conversationId,
        const std::string& command
    );
    
    // ========================================================================
    // Creative Collaboration Features
    // ========================================================================
    
    /// Analyze current project and provide insights
    core::AsyncResult<core::Result<AssistantResponse>> analyzeProject(
        const std::string& conversationId
    );
    
    /// Generate creative suggestions
    core::AsyncResult<core::Result<std::vector<std::string>>> generateCreativeSuggestions(
        const std::string& conversationId,
        const std::string& context = ""
    );
    
    /// Provide mixing feedback and suggestions
    core::AsyncResult<core::Result<AssistantResponse>> provideMixingFeedback(
        const std::string& conversationId,
        const std::vector<std::string>& focusAreas = {}
    );
    
    /// Suggest workflow optimizations
    core::AsyncResult<core::Result<std::vector<std::string>>> suggestWorkflowOptimizations(
        const std::string& conversationId
    );
    
    /// Generate arrangement ideas
    core::AsyncResult<core::Result<std::vector<std::string>>> generateArrangementIdeas(
        const std::string& conversationId,
        const std::string& musicStyle = ""
    );
    
    // ========================================================================
    // Learning and Tutorial Features
    // ========================================================================
    
    /// Enter tutorial mode for specific topic
    core::AsyncResult<core::Result<AssistantResponse>> startTutorial(
        const std::string& conversationId,
        const std::string& topic
    );
    
    /// Get contextual help based on current situation
    core::AsyncResult<core::Result<AssistantResponse>> getContextualHelp(
        const std::string& conversationId
    );
    
    /// Explain DAW concepts
    core::AsyncResult<core::Result<AssistantResponse>> explainConcept(
        const std::string& conversationId,
        const std::string& concept
    );
    
    /// Provide guided workflow assistance
    core::AsyncResult<core::Result<AssistantResponse>> provideGuidedWorkflow(
        const std::string& conversationId,
        const std::string& workflowType
    );
    
    /// Assess user skill level and provide personalized guidance
    core::AsyncResult<core::Result<AssistantResponse>> assessAndGuide(
        const std::string& conversationId,
        const std::string& domain = ""
    );
    
    // ========================================================================
    // Troubleshooting and Problem Solving
    // ========================================================================
    
    /// Enter troubleshooting mode
    core::AsyncResult<core::Result<AssistantResponse>> startTroubleshooting(
        const std::string& conversationId,
        const std::string& problemDescription
    );
    
    /// Diagnose common DAW issues
    core::AsyncResult<core::Result<AssistantResponse>> diagnoseIssue(
        const std::string& conversationId,
        const std::unordered_map<std::string, std::string>& symptoms = {}
    );
    
    /// Provide step-by-step problem resolution
    core::AsyncResult<core::Result<AssistantResponse>> provideTroubleshootingSteps(
        const std::string& conversationId,
        const std::string& issueType
    );
    
    /// Analyze system performance and suggest improvements
    core::AsyncResult<core::Result<AssistantResponse>> analyzePerformance(
        const std::string& conversationId
    );
    
    // ========================================================================
    // Proactive Assistance
    // ========================================================================
    
    /// Generate proactive suggestions based on current context
    core::AsyncResult<core::Result<std::vector<std::string>>> generateProactiveSuggestions(
        const std::string& conversationId
    );
    
    /// Detect potential issues and warn user
    core::AsyncResult<core::Result<std::vector<std::string>>> detectPotentialIssues(
        const std::string& conversationId
    );
    
    /// Monitor workflow and suggest optimizations
    core::AsyncResult<core::Result<std::vector<std::string>>> monitorWorkflow(
        const std::string& conversationId
    );
    
    /// Suggest best practices based on current activity
    core::AsyncResult<core::Result<std::vector<std::string>>> suggestBestPractices(
        const std::string& conversationId
    );
    
    // ========================================================================
    // Integration and Context Management
    // ========================================================================
    
    /// Update DAW state context
    core::VoidResult updateDAWContext(
        const std::string& conversationId,
        const std::unordered_map<std::string, std::string>& context
    );
    
    /// Sync with external services
    core::AsyncResult<core::VoidResult> syncWithServices(const std::string& conversationId);
    
    /// Get comprehensive status report
    core::AsyncResult<core::Result<std::string>> getStatusReport(
        const std::string& conversationId
    );
    
    /// Export conversation and context data
    core::AsyncResult<core::Result<std::string>> exportConversationData(
        const std::string& conversationId,
        const std::string& format = "json"
    );
    
    // ========================================================================
    // Analytics and Learning
    // ========================================================================
    
    struct AssistantAnalytics {
        // Usage statistics
        int totalConversations = 0;
        int totalMessages = 0;
        int successfulActions = 0;
        int failedActions = 0;
        
        // Response quality
        double averageConfidence = 0.0;
        double averageResponseTime = 0.0;
        int clarificationRequests = 0;
        
        // User satisfaction
        double userSatisfactionScore = 0.0;
        std::unordered_map<std::string, int> featureUsage;
        std::vector<std::string> mostUsefulFeatures;
        
        // Learning metrics
        double intentRecognitionAccuracy = 0.0;
        double commandSuccessRate = 0.0;
        std::unordered_map<std::string, double> topicExpertise;
        
        // Performance metrics
        double systemLoad = 0.0;
        int concurrentConversations = 0;
        std::unordered_map<std::string, double> serviceHealth;
    };
    
    AssistantAnalytics getAnalytics() const;
    
    /// Generate usage report
    core::AsyncResult<core::Result<std::string>> generateUsageReport(
        const std::string& format = "markdown"
    ) const;
    
    /// Get user interaction insights
    core::AsyncResult<core::Result<std::string>> getUserInsights(
        const std::string& userId
    ) const;
    
    // ========================================================================
    // Callbacks and Events
    // ========================================================================
    
    /// Set response callback
    void setResponseCallback(ResponseCallback callback);
    
    /// Set streaming callback
    void setStreamingCallback(StreamingCallback callback);
    
    /// Set action confirmation callback
    void setActionConfirmationCallback(ActionConfirmationCallback callback);

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Process user message through complete AI pipeline
    core::AsyncResult<core::Result<AssistantResponse>> processMessage(
        const std::string& conversationId,
        const std::string& message,
        bool streaming = false
    );
    
    /// Build context-aware system prompt
    std::string buildSystemPrompt(
        const std::string& conversationId,
        AssistantMode mode
    ) const;
    
    /// Execute detected actions
    core::AsyncResult<core::Result<std::vector<std::string>>> executeActions(
        const std::string& conversationId,
        const std::vector<std::string>& actions
    );
    
    /// Generate response based on results
    AssistantResponse generateResponse(
        const std::string& conversationId,
        const std::string& originalMessage,
        const std::vector<std::string>& actionResults,
        ResponseType type
    ) const;
    
    /// Handle errors gracefully
    AssistantResponse handleError(
        const std::string& conversationId,
        const std::string& error,
        const std::string& context
    ) const;
    
    /// Update learning models
    void updateLearning(
        const std::string& conversationId,
        const std::string& input,
        const AssistantResponse& response,
        bool wasSuccessful
    );
    
    // Core AI components
    std::unique_ptr<ChatService> chatService_;
    std::unique_ptr<ActionAPI> actionAPI_;
    std::unique_ptr<IntentRecognition> intentRecognition_;
    std::unique_ptr<ConversationContextManager> contextManager_;
    std::unique_ptr<OpenAIProvider> openAIProvider_;
    
    // Configuration
    AssistantConfig config_;
    std::atomic<bool> isInitialized_{false};
    
    // Conversation management
    std::unordered_map<std::string, AssistantMode> conversationModes_;
    mutable std::shared_mutex conversationMutex_;
    
    // Callbacks
    ResponseCallback responseCallback_;
    StreamingCallback streamingCallback_;
    ActionConfirmationCallback actionConfirmationCallback_;
    
    // Analytics and monitoring
    AssistantAnalytics analytics_;
    mutable std::mutex analyticsMutex_;
    
    // Background processing
    std::atomic<bool> shouldShutdown_{false};
};

// ============================================================================
// AI Assistant Factory and Configuration
// ============================================================================

class AIAssistantFactory {
public:
    /// Create pre-configured AI Assistant for different use cases
    static std::unique_ptr<AIAssistant> createBeginnerAssistant();
    static std::unique_ptr<AIAssistant> createProducerAssistant();
    static std::unique_ptr<AIAssistant> createEngineerAssistant();
    static std::unique_ptr<AIAssistant> createCreativeAssistant();
    static std::unique_ptr<AIAssistant> createEducationalAssistant();
    
    /// Create custom configured assistant
    static std::unique_ptr<AIAssistant> createCustomAssistant(const AssistantConfig& config);
};

// ============================================================================
// Global AI Assistant Instance
// ============================================================================

/// Get the global AI Assistant instance
AIAssistant& getGlobalAIAssistant();

/// Initialize the global AI Assistant with default configuration
core::AsyncResult<core::VoidResult> initializeGlobalAIAssistant(
    const AssistantConfig& config = {}
);

} // namespace mixmind::ai