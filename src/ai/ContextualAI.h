#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../api/ActionAPI.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>

namespace mixmind::ai {

using json = nlohmann::json;

// ============================================================================
// Contextual AI - Intelligent context-aware assistance for DAW operations
// ============================================================================

class ContextualAI {
public:
    explicit ContextualAI(std::shared_ptr<api::ActionAPI> actionAPI);
    ~ContextualAI();
    
    // Non-copyable, movable
    ContextualAI(const ContextualAI&) = delete;
    ContextualAI& operator=(const ContextualAI&) = delete;
    ContextualAI(ContextualAI&&) = default;
    ContextualAI& operator=(ContextualAI&&) = default;
    
    // ========================================================================
    // AI Configuration
    // ========================================================================
    
    /// AI engine types
    enum class AIEngine {
        OpenAI,         // OpenAI GPT models
        Anthropic,      // Anthropic Claude models  
        Azure,          // Azure OpenAI Service
        Local,          // Local LLM (Llama, etc.)
        Hybrid          // Combination of engines
    };
    
    /// AI settings
    struct AISettings {
        AIEngine engine = AIEngine::OpenAI;
        std::string apiKey;
        std::string endpoint;
        std::string model = "gpt-4";
        float temperature = 0.7f;
        int32_t maxTokens = 2048;
        bool enableContextMemory = true;
        int32_t maxContextHistory = 50;
        bool enableProactiveAssistance = true;
        bool enableWorkflowSuggestions = true;
        bool enableErrorAnalysis = true;
        std::string personalityPrompt = "You are MixMind AI, a helpful and knowledgeable music production assistant.";
    };
    
    /// Initialize contextual AI
    core::AsyncResult<core::VoidResult> initialize(const AISettings& settings);
    
    /// Shutdown contextual AI
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if AI is active
    bool isActive() const;
    
    /// Update AI settings
    core::VoidResult updateSettings(const AISettings& settings);
    
    /// Get current settings
    AISettings getSettings() const;
    
    // ========================================================================
    // Context Management
    // ========================================================================
    
    /// Current session context
    struct SessionContext {
        std::string sessionName;
        std::string sessionPath;
        core::SampleRate sampleRate = 48000;
        int32_t bitDepth = 24;
        double duration = 0.0;
        int32_t trackCount = 0;
        int32_t clipCount = 0;
        int32_t pluginCount = 0;
        std::string genre;
        std::string mood;
        std::vector<std::string> tags;
        std::chrono::system_clock::time_point lastModified;
    };
    
    /// User behavior context
    struct UserContext {
        std::string userId;
        std::string skillLevel = "intermediate"; // beginner, intermediate, advanced, professional
        std::vector<std::string> preferredGenres;
        std::vector<std::string> commonWorkflows;
        std::unordered_map<std::string, int32_t> actionCounts;
        std::vector<std::string> recentErrors;
        std::chrono::system_clock::time_point sessionStart;
        double sessionDuration = 0.0; // minutes
    };
    
    /// Audio analysis context
    struct AudioContext {
        float currentRMSLevel = 0.0f;
        float peakLevel = 0.0f;
        float lufsLevel = 0.0f;
        std::vector<float> spectrumData;
        double detectedTempo = 0.0;
        std::string detectedKey;
        std::vector<std::string> detectedInstruments;
        bool hasAudioIssues = false;
        std::vector<std::string> audioIssues;
    };
    
    /// Update context information
    void updateSessionContext(const SessionContext& context);
    void updateUserContext(const UserContext& context);
    void updateAudioContext(const AudioContext& context);
    
    /// Get current contexts
    SessionContext getSessionContext() const;
    UserContext getUserContext() const;
    AudioContext getAudioContext() const;
    
    // ========================================================================
    // Intelligent Chat Interface
    // ========================================================================
    
    /// Chat message types
    enum class MessageType {
        User,
        Assistant,
        System,
        Error,
        Suggestion
    };
    
    /// Chat message
    struct ChatMessage {
        MessageType type;
        std::string content;
        json metadata;
        std::chrono::system_clock::time_point timestamp;
        std::string messageId;
        
        ChatMessage(MessageType t, const std::string& c) 
            : type(t), content(c), timestamp(std::chrono::system_clock::now()) {}
    };
    
    /// Send chat message to AI
    core::AsyncResult<core::Result<std::string>> sendChatMessage(const std::string& message);
    
    /// Send chat message with context
    core::AsyncResult<core::Result<std::string>> sendChatMessageWithContext(
        const std::string& message,
        const json& additionalContext = json::object()
    );
    
    /// Get chat history
    std::vector<ChatMessage> getChatHistory(int32_t maxMessages = 50) const;
    
    /// Clear chat history
    void clearChatHistory();
    
    /// Export chat history
    core::VoidResult exportChatHistory(const std::string& filePath) const;
    
    // ========================================================================
    // Proactive Assistance
    // ========================================================================
    
    /// AI suggestion types
    enum class SuggestionType {
        WorkflowOptimization,
        PluginRecommendation,
        MixingSuggestion,
        CreativeIdea,
        ProblemSolution,
        LearningTip,
        ShortcutSuggestion
    };
    
    /// AI suggestion
    struct AISuggestion {
        SuggestionType type;
        std::string title;
        std::string description;
        std::string actionCommand;
        json actionParameters;
        float confidence = 0.0f;
        std::string reasoning;
        std::vector<std::string> tags;
        std::chrono::system_clock::time_point timestamp;
    };
    
    /// Get contextual suggestions
    core::AsyncResult<core::Result<std::vector<AISuggestion>>> getSuggestions();
    
    /// Get suggestions for specific context
    core::AsyncResult<core::Result<std::vector<AISuggestion>>> getSuggestionsForContext(
        const std::string& contextType,
        const json& contextData
    );
    
    /// Apply suggestion
    core::AsyncResult<api::ActionResult> applySuggestion(const AISuggestion& suggestion);
    
    /// Dismiss suggestion
    void dismissSuggestion(const AISuggestion& suggestion);
    
    /// Rate suggestion (for learning)
    void rateSuggestion(const AISuggestion& suggestion, int32_t rating); // 1-5 stars
    
    // ========================================================================
    // Workflow Analysis and Optimization
    // ========================================================================
    
    /// Workflow analysis results
    struct WorkflowAnalysis {
        std::string analysisId;
        std::string summary;
        std::vector<std::string> inefficiencies;
        std::vector<std::string> recommendations;
        std::vector<std::string> potentialImprovements;
        double efficiencyScore = 0.0; // 0.0-1.0
        std::chrono::system_clock::time_point analysisTime;
    };
    
    /// Analyze current workflow
    core::AsyncResult<core::Result<WorkflowAnalysis>> analyzeWorkflow();
    
    /// Analyze workflow for specific time period
    core::AsyncResult<core::Result<WorkflowAnalysis>> analyzeWorkflowForPeriod(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end
    );
    
    /// Get workflow recommendations
    core::AsyncResult<core::Result<std::vector<std::string>>> getWorkflowRecommendations();
    
    /// Learn from user workflow patterns
    void learnFromWorkflow(const std::vector<api::ActionResult>& actionSequence);
    
    // ========================================================================
    // Error Analysis and Solutions
    // ========================================================================
    
    /// Error analysis
    struct ErrorAnalysis {
        std::string errorCode;
        std::string errorMessage;
        std::string likelyCause;
        std::vector<std::string> possibleSolutions;
        std::vector<std::string> preventionTips;
        float confidence = 0.0f;
        json technicalDetails;
    };
    
    /// Analyze error and provide solutions
    core::AsyncResult<core::Result<ErrorAnalysis>> analyzeError(
        const std::string& errorCode,
        const std::string& errorMessage,
        const json& context = json::object()
    );
    
    /// Get solutions for common errors
    std::vector<ErrorAnalysis> getCommonErrorSolutions() const;
    
    /// Report error resolution success
    void reportErrorResolution(const std::string& errorCode, bool successful);
    
    // ========================================================================
    // Learning and Adaptation
    // ========================================================================
    
    /// User interaction learning
    struct InteractionData {
        std::string action;
        json parameters;
        bool successful;
        std::chrono::system_clock::time_point timestamp;
        json context;
    };
    
    /// Learn from user interaction
    void learnFromInteraction(const InteractionData& interaction);
    
    /// Update user skill assessment
    void updateSkillAssessment(const std::string& skill, float proficiencyLevel);
    
    /// Get personalized learning recommendations
    core::AsyncResult<core::Result<std::vector<std::string>>> getLearningRecommendations();
    
    /// Adapt AI responses based on user expertise
    void adaptResponseStyle(const std::string& userLevel);
    
    // ========================================================================
    // Creative Assistance
    // ========================================================================
    
    /// Creative generation types
    enum class CreativeType {
        MelodyIdeas,
        ChordProgressions,
        RhythmPatterns,
        ArrangementIdeas,
        SoundDesign,
        MixingIdeas,
        GenreExploration
    };
    
    /// Creative suggestion
    struct CreativeSuggestion {
        CreativeType type;
        std::string title;
        std::string description;
        json musicalData; // MIDI, chord symbols, etc.
        std::vector<std::string> implementationSteps;
        std::string inspirationSource;
        std::vector<std::string> relatedConcepts;
    };
    
    /// Generate creative ideas
    core::AsyncResult<core::Result<std::vector<CreativeSuggestion>>> generateCreativeIdeas(
        CreativeType type,
        const json& constraints = json::object()
    );
    
    /// Get style-based suggestions
    core::AsyncResult<core::Result<std::vector<CreativeSuggestion>>> getStyleSuggestions(
        const std::string& targetStyle,
        const std::string& currentContext
    );
    
    /// Analyze creative potential of current project
    core::AsyncResult<core::Result<std::string>> analyzeCreativePotential();
    
    // ========================================================================
    // Event Callbacks
    // ========================================================================
    
    /// AI events
    enum class AIEvent {
        MessageReceived,
        ResponseGenerated,
        SuggestionCreated,
        ContextUpdated,
        ErrorAnalyzed,
        LearningUpdate
    };
    
    /// AI event callback type
    using AIEventCallback = std::function<void(AIEvent event, const json& data)>;
    
    /// Set AI event callback
    void setAIEventCallback(AIEventCallback callback);
    
    /// Clear AI event callback
    void clearAIEventCallback();
    
    // ========================================================================
    // Statistics and Monitoring
    // ========================================================================
    
    struct AIStatistics {
        int64_t totalMessages = 0;
        int64_t suggestionsGenerated = 0;
        int64_t suggestionsApplied = 0;
        int64_t errorsAnalyzed = 0;
        double averageResponseTime = 0.0; // ms
        double averageSatisfactionRating = 0.0; // 1-5
        std::unordered_map<std::string, int64_t> queryTypes;
        std::chrono::system_clock::time_point sessionStart;
    };
    
    /// Get AI statistics
    AIStatistics getStatistics() const;
    
    /// Reset statistics
    void resetStatistics();

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize AI engine
    core::VoidResult initializeAIEngine();
    
    /// Cleanup AI resources
    void cleanupAIEngine();
    
    /// Build context prompt
    std::string buildContextPrompt(const std::string& userMessage, const json& additionalContext);
    
    /// Process AI response
    std::string processAIResponse(const std::string& response);
    
    /// Generate suggestions based on current context
    std::vector<AISuggestion> generateContextualSuggestions();
    
    /// Analyze user behavior patterns
    void analyzeBehaviorPatterns();
    
    /// Update context memory
    void updateContextMemory(const ChatMessage& message);
    
    /// Emit AI event
    void emitAIEvent(AIEvent event, const json& data);

private:
    // Action API reference
    std::shared_ptr<api::ActionAPI> actionAPI_;
    
    // AI settings and state
    AISettings settings_;
    std::atomic<bool> isActive_{false};
    mutable std::mutex settingsMutex_;
    
    // Context information
    SessionContext sessionContext_;
    UserContext userContext_;
    AudioContext audioContext_;
    mutable std::shared_mutex contextMutex_;
    
    // Chat history
    std::vector<ChatMessage> chatHistory_;
    mutable std::shared_mutex chatHistoryMutex_;
    
    // Suggestions and analysis
    std::vector<AISuggestion> activeSuggestions_;
    std::vector<ErrorAnalysis> errorAnalyses_;
    mutable std::shared_mutex suggestionsMutex_;
    
    // Learning data
    std::vector<InteractionData> interactions_;
    std::unordered_map<std::string, float> skillAssessments_;
    mutable std::shared_mutex learningMutex_;
    
    // Background processing
    std::thread analysisThread_;
    std::atomic<bool> shouldStopAnalysis_{false};
    std::queue<std::function<void()>> analysisQueue_;
    std::mutex analysisQueueMutex_;
    std::condition_variable analysisQueueCondition_;
    
    // Statistics
    mutable AIStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Event callback
    AIEventCallback aiEventCallback_;
    std::mutex callbackMutex_;
    
    // Constants
    static constexpr int32_t MAX_CHAT_HISTORY = 100;
    static constexpr int32_t MAX_SUGGESTIONS = 10;
    static constexpr int32_t MAX_CONTEXT_MEMORY = 50;
};

} // namespace mixmind::ai