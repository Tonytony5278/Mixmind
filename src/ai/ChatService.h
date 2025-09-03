#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../core/async.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <optional>

namespace mixmind::ai {

// ============================================================================
// Chat Message Types and Structures
// ============================================================================

enum class MessageRole {
    User,       // Message from user
    Assistant,  // Response from AI assistant
    System,     // System/context message
    Tool        // Tool execution result
};

enum class MessageType {
    Text,           // Plain text message
    Command,        // DAW command request
    Query,          // Information query
    Context,        // Context/state information
    Error,          // Error message
    Confirmation    // Action confirmation
};

struct ChatMessage {
    std::string id;
    MessageRole role;
    MessageType type;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    
    // Optional metadata
    std::unordered_map<std::string, std::string> metadata;
    
    // For tool calls and responses
    std::optional<std::string> toolCallId;
    std::optional<std::string> toolName;
    std::optional<std::string> toolArgs;
    std::optional<std::string> toolResult;
    
    // Conversation context
    std::string conversationId;
    std::string sessionId;
    
    ChatMessage() : timestamp(std::chrono::system_clock::now()) {}
    
    ChatMessage(MessageRole r, MessageType t, std::string content)
        : role(r), type(t), content(std::move(content))
        , timestamp(std::chrono::system_clock::now()) {}
};

// ============================================================================
// Conversation Context and Session Management
// ============================================================================

struct ConversationContext {
    std::string conversationId;
    std::string sessionId;
    std::string userId;
    
    // Conversation state
    std::vector<ChatMessage> messages;
    size_t maxMessages = 100;  // Rolling window
    
    // Current DAW state context
    std::unordered_map<std::string, std::string> dawContext;
    
    // User preferences and history
    std::unordered_map<std::string, std::string> userPreferences;
    std::vector<std::string> frequentCommands;
    
    // Conversation metadata
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastActivity;
    bool isActive = true;
    
    // Performance tracking
    size_t totalMessages = 0;
    double averageResponseTime = 0.0;
    
    ConversationContext() 
        : createdAt(std::chrono::system_clock::now())
        , lastActivity(std::chrono::system_clock::now()) {}
};

// ============================================================================
// AI Provider Configuration
// ============================================================================

enum class AIProvider {
    OpenAI,     // OpenAI GPT models
    Azure,      // Azure OpenAI
    Anthropic,  // Claude models
    Local,      // Local LLM
    Mock        // Mock provider for testing
};

struct AIProviderConfig {
    AIProvider provider = AIProvider::OpenAI;
    std::string apiKey;
    std::string apiEndpoint;
    std::string modelName = "gpt-4";
    
    // Request parameters
    double temperature = 0.7;
    int maxTokens = 2000;
    double topP = 1.0;
    int frequencyPenalty = 0;
    int presencePenalty = 0;
    
    // Rate limiting
    int maxRequestsPerMinute = 60;
    int maxTokensPerMinute = 100000;
    
    // Timeouts
    std::chrono::milliseconds requestTimeout{30000};
    std::chrono::milliseconds connectionTimeout{10000};
    
    // Retry configuration
    int maxRetries = 3;
    std::chrono::milliseconds retryDelay{1000};
};

// ============================================================================
// Chat Response and Streaming
// ============================================================================

struct ChatResponse {
    std::string id;
    std::string content;
    MessageType type;
    
    // Response metadata
    std::string model;
    int tokensUsed = 0;
    double responseTime = 0.0;
    
    // Tool calls (if any)
    struct ToolCall {
        std::string id;
        std::string name;
        std::string arguments;
    };
    std::vector<ToolCall> toolCalls;
    
    // Confidence and quality metrics
    double confidence = 1.0;
    bool requiresConfirmation = false;
    std::vector<std::string> suggestedActions;
    
    // Error information
    bool hasError = false;
    std::string errorMessage;
    std::string errorCode;
};

// Streaming support
using StreamingCallback = std::function<void(const std::string& chunk, bool isComplete)>;
using ProgressCallback = std::function<void(const std::string& stage, double progress)>;

// ============================================================================
// Chat Service - AI Conversation Management
// ============================================================================

class ChatService {
public:
    using MessageCallback = std::function<void(const ChatMessage& message)>;
    using ConversationCallback = std::function<void(const std::string& conversationId, const std::string& event)>;
    
    ChatService();
    ~ChatService();
    
    // Non-copyable
    ChatService(const ChatService&) = delete;
    ChatService& operator=(const ChatService&) = delete;
    
    // ========================================================================
    // Service Lifecycle
    // ========================================================================
    
    /// Initialize chat service
    core::AsyncResult<core::VoidResult> initialize(const AIProviderConfig& config);
    
    /// Shutdown service
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if service is ready
    bool isReady() const { return isInitialized_.load(); }
    
    /// Update configuration
    core::VoidResult updateConfig(const AIProviderConfig& config);
    
    /// Get current configuration
    AIProviderConfig getConfig() const;
    
    // ========================================================================
    // Conversation Management
    // ========================================================================
    
    /// Start new conversation
    core::AsyncResult<core::Result<std::string>> startConversation(
        const std::string& userId = "",
        const std::string& sessionId = ""
    );
    
    /// End conversation
    core::AsyncResult<core::VoidResult> endConversation(const std::string& conversationId);
    
    /// Get conversation context
    std::optional<ConversationContext> getConversation(const std::string& conversationId) const;
    
    /// List active conversations for user
    std::vector<std::string> getActiveConversations(const std::string& userId = "") const;
    
    /// Update DAW context for conversation
    core::VoidResult updateDAWContext(
        const std::string& conversationId,
        const std::unordered_map<std::string, std::string>& context
    );
    
    // ========================================================================
    // Message Processing
    // ========================================================================
    
    /// Send message and get response
    core::AsyncResult<core::Result<ChatResponse>> sendMessage(
        const std::string& conversationId,
        const std::string& message,
        MessageType type = MessageType::Text
    );
    
    /// Send message with streaming response
    core::AsyncResult<core::Result<std::string>> sendMessageStreaming(
        const std::string& conversationId,
        const std::string& message,
        StreamingCallback streamingCallback,
        MessageType type = MessageType::Text
    );
    
    /// Add system message to conversation
    core::VoidResult addSystemMessage(
        const std::string& conversationId,
        const std::string& content,
        const std::unordered_map<std::string, std::string>& metadata = {}
    );
    
    /// Add tool result to conversation
    core::VoidResult addToolResult(
        const std::string& conversationId,
        const std::string& toolCallId,
        const std::string& result,
        bool success = true
    );
    
    // ========================================================================
    // Message History and Context
    // ========================================================================
    
    /// Get conversation messages
    std::vector<ChatMessage> getMessages(
        const std::string& conversationId,
        size_t limit = 50,
        size_t offset = 0
    ) const;
    
    /// Search messages in conversation
    core::AsyncResult<core::Result<std::vector<ChatMessage>>> searchMessages(
        const std::string& conversationId,
        const std::string& query,
        MessageType type = MessageType::Text
    ) const;
    
    /// Clear conversation history
    core::VoidResult clearHistory(const std::string& conversationId);
    
    /// Export conversation
    core::AsyncResult<core::Result<std::string>> exportConversation(
        const std::string& conversationId,
        const std::string& format = "json"  // json, markdown, plain
    ) const;
    
    // ========================================================================
    // Intent Recognition and Command Processing
    // ========================================================================
    
    /// Detect intent from user message
    core::AsyncResult<core::Result<std::string>> detectIntent(
        const std::string& message,
        const std::string& conversationId = ""
    ) const;
    
    /// Extract DAW commands from message
    core::AsyncResult<core::Result<std::vector<std::string>>> extractCommands(
        const std::string& message,
        const std::string& conversationId = ""
    ) const;
    
    /// Suggest completions for partial message
    core::AsyncResult<core::Result<std::vector<std::string>>> getSuggestions(
        const std::string& partialMessage,
        const std::string& conversationId = "",
        size_t maxSuggestions = 5
    ) const;
    
    // ========================================================================
    // Context and Memory Management
    // ========================================================================
    
    /// Update user preferences
    core::VoidResult updateUserPreferences(
        const std::string& conversationId,
        const std::unordered_map<std::string, std::string>& preferences
    );
    
    /// Learn from user behavior
    core::VoidResult recordUserAction(
        const std::string& conversationId,
        const std::string& action,
        const std::unordered_map<std::string, std::string>& metadata = {}
    );
    
    /// Get personalized system prompt
    std::string getPersonalizedSystemPrompt(const std::string& conversationId) const;
    
    /// Summarize conversation for long-term memory
    core::AsyncResult<core::Result<std::string>> summarizeConversation(
        const std::string& conversationId
    ) const;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Generate DAW tutorial content
    core::AsyncResult<core::Result<std::string>> generateTutorial(
        const std::string& topic,
        const std::string& userLevel = "beginner"  // beginner, intermediate, advanced
    ) const;
    
    /// Analyze audio production workflow
    core::AsyncResult<core::Result<std::string>> analyzeWorkflow(
        const std::vector<std::string>& userActions,
        const std::string& conversationId = ""
    ) const;
    
    /// Suggest optimizations
    core::AsyncResult<core::Result<std::vector<std::string>>> suggestOptimizations(
        const std::string& conversationId
    ) const;
    
    /// Generate project documentation
    core::AsyncResult<core::Result<std::string>> generateProjectDocs(
        const std::string& conversationId,
        const std::string& projectContext
    ) const;
    
    // ========================================================================
    // Callbacks and Events
    // ========================================================================
    
    /// Set message callback
    void setMessageCallback(MessageCallback callback);
    
    /// Set conversation callback  
    void setConversationCallback(ConversationCallback callback);
    
    /// Set progress callback
    void setProgressCallback(ProgressCallback callback);
    
    // ========================================================================
    // Statistics and Monitoring
    // ========================================================================
    
    struct ChatStats {
        int activeConversations = 0;
        int totalMessages = 0;
        int totalTokensUsed = 0;
        double averageResponseTime = 0.0;
        int successfulRequests = 0;
        int failedRequests = 0;
        
        // Provider-specific stats
        std::unordered_map<std::string, int> tokensPerModel;
        std::unordered_map<std::string, double> responseTimesPerModel;
        
        // Usage patterns
        std::unordered_map<std::string, int> intentCounts;
        std::unordered_map<std::string, int> commandCounts;
    };
    
    ChatStats getStats() const;
    
    /// Reset statistics
    void resetStats();
    
    /// Get conversation analytics
    core::AsyncResult<core::Result<std::string>> getConversationAnalytics(
        const std::string& conversationId
    ) const;

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    class AIProvider;
    class OpenAIProvider;
    class MockProvider;
    
    /// Create AI provider instance
    std::unique_ptr<AIProvider> createProvider(const AIProviderConfig& config);
    
    /// Generate unique IDs
    std::string generateMessageId() const;
    std::string generateConversationId() const;
    
    /// Build conversation context for AI
    std::string buildContextPrompt(const std::string& conversationId) const;
    
    /// Process AI response for tool calls
    std::vector<ChatResponse::ToolCall> extractToolCalls(const std::string& response) const;
    
    /// Update conversation statistics
    void updateStats(const ChatResponse& response, bool success);
    
    /// Cleanup old conversations
    void cleanupOldConversations();
    
    /// Validate message content
    core::VoidResult validateMessage(const std::string& content) const;
    
    /// Rate limiting check
    bool checkRateLimit() const;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
    AIProviderConfig config_;
    std::unique_ptr<AIProvider> provider_;
    
    // Conversation management
    mutable std::mutex conversationsMutex_;
    std::unordered_map<std::string, ConversationContext> conversations_;
    
    // Callbacks
    MessageCallback messageCallback_;
    ConversationCallback conversationCallback_;
    ProgressCallback progressCallback_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    ChatStats stats_;
    
    // Rate limiting
    mutable std::mutex rateLimitMutex_;
    std::chrono::steady_clock::time_point lastRequestTime_;
    int requestsThisMinute_ = 0;
    int tokensThisMinute_ = 0;
};

// ============================================================================
// Global Chat Service Instance
// ============================================================================

/// Get the global chat service instance
ChatService& getGlobalChatService();

} // namespace mixmind::ai