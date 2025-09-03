#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../core/async.h"
#include "ChatService.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>

// Forward declaration for HTTP client
namespace httplib {
    class Client;
}

namespace mixmind::ai {

// ============================================================================
// OpenAI API Configuration and Models
// ============================================================================

struct OpenAIModel {
    std::string id;
    std::string name;
    std::string description;
    int maxTokens;
    bool supportsTools;
    bool supportsStreaming;
    double costPer1kTokens;
    
    // Model capabilities
    bool supportsCodeGeneration = false;
    bool supportsReasoning = false;
    bool supportsFunctionCalling = false;
    bool supportsVision = false;
};

struct OpenAIUsage {
    int promptTokens = 0;
    int completionTokens = 0;
    int totalTokens = 0;
    double estimatedCost = 0.0;
};

struct OpenAIRequest {
    std::string model;
    std::vector<ChatMessage> messages;
    
    // Generation parameters
    double temperature = 0.7;
    int maxTokens = 2000;
    double topP = 1.0;
    int frequencyPenalty = 0;
    int presencePenalty = 0;
    std::vector<std::string> stop;
    
    // Tool/Function calling
    std::vector<std::string> tools;
    std::string toolChoice = "auto";  // "auto", "none", or specific tool
    
    // Streaming
    bool stream = false;
    
    // User/session tracking
    std::string user;
    std::string sessionId;
};

struct OpenAIResponse {
    std::string id;
    std::string object;
    std::string model;
    std::chrono::system_clock::time_point created;
    
    struct Choice {
        int index;
        ChatMessage message;
        std::string finishReason;  // "stop", "length", "tool_calls", etc.
    };
    
    std::vector<Choice> choices;
    OpenAIUsage usage;
    
    // Error information
    bool hasError = false;
    std::string errorType;
    std::string errorMessage;
    std::string errorCode;
};

// ============================================================================
// Tool/Function Definitions for DAW Integration
// ============================================================================

struct ToolFunction {
    std::string name;
    std::string description;
    std::string parametersSchema;  // JSON schema
    std::function<core::AsyncResult<core::Result<std::string>>(const std::string& args)> handler;
};

struct ToolCall {
    std::string id;
    std::string type;
    struct Function {
        std::string name;
        std::string arguments;
    } function;
};

// ============================================================================
// Rate Limiting and Request Management
// ============================================================================

struct RateLimitInfo {
    int requestsPerMinute = 3000;
    int tokensPerMinute = 250000;
    int tokensPerDay = 10000000;
    
    // Current usage
    std::atomic<int> currentRPM{0};
    std::atomic<int> currentTPM{0};
    std::atomic<int> currentTPD{0};
    
    // Reset times
    std::chrono::steady_clock::time_point lastMinuteReset;
    std::chrono::steady_clock::time_point lastDayReset;
    
    bool isWithinLimits(int estimatedTokens) const;
    void recordRequest(int tokensUsed);
    void resetCounters();
};

struct RequestQueueItem {
    std::string requestId;
    OpenAIRequest request;
    std::function<void(const core::Result<OpenAIResponse>&)> callback;
    std::chrono::steady_clock::time_point queueTime;
    int priority = 0;  // Higher = more important
    int retryCount = 0;
};

// ============================================================================
// OpenAI Provider - Handles communication with OpenAI API
// ============================================================================

class OpenAIProvider {
public:
    using StreamingCallback = std::function<void(const std::string& chunk, bool isComplete)>;
    using ErrorCallback = std::function<void(const std::string& error, const std::string& context)>;
    using UsageCallback = std::function<void(const OpenAIUsage& usage)>;
    
    OpenAIProvider();
    ~OpenAIProvider();
    
    // Non-copyable
    OpenAIProvider(const OpenAIProvider&) = delete;
    OpenAIProvider& operator=(const OpenAIProvider&) = delete;
    
    // ========================================================================
    // Configuration and Initialization
    // ========================================================================
    
    /// Initialize provider with API configuration
    core::AsyncResult<core::VoidResult> initialize(const AIProviderConfig& config);
    
    /// Shutdown provider
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if provider is ready
    bool isReady() const { return isInitialized_.load(); }
    
    /// Update configuration
    core::VoidResult updateConfig(const AIProviderConfig& config);
    
    /// Get current configuration
    AIProviderConfig getConfig() const;
    
    // ========================================================================
    // Model Management
    // ========================================================================
    
    /// List available models
    core::AsyncResult<core::Result<std::vector<OpenAIModel>>> listModels();
    
    /// Get specific model information
    core::AsyncResult<core::Result<OpenAIModel>> getModel(const std::string& modelId);
    
    /// Get recommended model for use case
    std::string getRecommendedModel(const std::string& useCase) const;
    
    /// Validate model availability
    core::AsyncResult<core::Result<bool>> validateModel(const std::string& modelId);
    
    // ========================================================================
    // Chat Completions
    // ========================================================================
    
    /// Send chat completion request
    core::AsyncResult<core::Result<OpenAIResponse>> chatCompletion(
        const OpenAIRequest& request
    );
    
    /// Send streaming chat completion request
    core::AsyncResult<core::Result<std::string>> chatCompletionStreaming(
        const OpenAIRequest& request,
        StreamingCallback callback
    );
    
    /// Send chat completion with automatic retry
    core::AsyncResult<core::Result<OpenAIResponse>> chatCompletionWithRetry(
        const OpenAIRequest& request,
        int maxRetries = 3
    );
    
    // ========================================================================
    // Tool/Function Calling
    // ========================================================================
    
    /// Register DAW tool/function
    core::VoidResult registerTool(const ToolFunction& tool);
    
    /// Unregister tool
    core::VoidResult unregisterTool(const std::string& toolName);
    
    /// Get registered tools
    std::vector<std::string> getRegisteredTools() const;
    
    /// Execute tool call
    core::AsyncResult<core::Result<std::string>> executeTool(
        const std::string& toolName,
        const std::string& arguments
    );
    
    /// Send chat completion with tool support
    core::AsyncResult<core::Result<OpenAIResponse>> chatCompletionWithTools(
        const OpenAIRequest& request,
        const std::vector<std::string>& enabledTools = {}
    );
    
    // ========================================================================
    // DAW-Specific Tools Registration
    // ========================================================================
    
    /// Register all built-in DAW tools
    core::VoidResult registerDAWTools();
    
    /// Transport control tools
    core::VoidResult registerTransportTools();
    
    /// Track management tools
    core::VoidResult registerTrackTools();
    
    /// Clip manipulation tools
    core::VoidResult registerClipTools();
    
    /// Plugin management tools
    core::VoidResult registerPluginTools();
    
    /// Session management tools
    core::VoidResult registerSessionTools();
    
    /// Analysis and information tools
    core::VoidResult registerAnalysisTools();
    
    // ========================================================================
    // Context and Memory Management
    // ========================================================================
    
    /// Build context-aware system prompt
    std::string buildSystemPrompt(
        const std::string& conversationId,
        const std::unordered_map<std::string, std::string>& dawContext = {}
    ) const;
    
    /// Optimize message history for token limits
    std::vector<ChatMessage> optimizeMessageHistory(
        const std::vector<ChatMessage>& messages,
        int maxTokens
    ) const;
    
    /// Estimate token count for messages
    int estimateTokenCount(const std::vector<ChatMessage>& messages) const;
    
    /// Summarize conversation for context compression
    core::AsyncResult<core::Result<std::string>> summarizeConversation(
        const std::vector<ChatMessage>& messages
    );
    
    // ========================================================================
    // Rate Limiting and Queue Management
    // ========================================================================
    
    /// Check current rate limit status
    RateLimitInfo getRateLimitInfo() const;
    
    /// Queue request for processing
    core::AsyncResult<core::Result<std::string>> queueRequest(
        const OpenAIRequest& request,
        int priority = 0
    );
    
    /// Get queue status
    struct QueueStatus {
        int queueSize = 0;
        int processingRequests = 0;
        double averageWaitTime = 0.0;
        double averageProcessingTime = 0.0;
    };
    
    QueueStatus getQueueStatus() const;
    
    /// Clear request queue
    core::VoidResult clearQueue();
    
    // ========================================================================
    // Error Handling and Retry Logic
    // ========================================================================
    
    /// Set custom error handler
    void setErrorCallback(ErrorCallback callback);
    
    /// Check if error is retryable
    bool isRetryableError(const std::string& errorCode) const;
    
    /// Get retry delay for error type
    std::chrono::milliseconds getRetryDelay(
        const std::string& errorCode,
        int retryCount
    ) const;
    
    /// Handle API errors gracefully
    core::Result<OpenAIResponse> handleAPIError(
        const std::string& response,
        int httpCode
    ) const;
    
    // ========================================================================
    // Usage Tracking and Analytics
    // ========================================================================
    
    struct ProviderStats {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        int retryAttempts = 0;
        
        OpenAIUsage totalUsage;
        double totalCost = 0.0;
        
        double averageResponseTime = 0.0;
        double averageTokensPerRequest = 0.0;
        
        std::unordered_map<std::string, int> modelUsage;
        std::unordered_map<std::string, int> errorCounts;
        std::unordered_map<std::string, int> toolUsage;
    };
    
    ProviderStats getProviderStats() const;
    
    /// Set usage tracking callback
    void setUsageCallback(UsageCallback callback);
    
    /// Export usage data
    core::AsyncResult<core::Result<std::string>> exportUsageData(
        const std::string& format = "json"
    ) const;
    
    /// Reset statistics
    void resetStats();
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Generate embeddings for text
    core::AsyncResult<core::Result<std::vector<double>>> generateEmbeddings(
        const std::string& text,
        const std::string& model = "text-embedding-3-small"
    );
    
    /// Moderate content for safety
    core::AsyncResult<core::Result<bool>> moderateContent(const std::string& content);
    
    /// Fine-tune model with custom data (if supported)
    core::AsyncResult<core::Result<std::string>> createFineTune(
        const std::string& trainingFile,
        const std::string& baseModel
    );
    
    /// Validate API key and permissions
    core::AsyncResult<core::Result<bool>> validateAPIKey();

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Make HTTP request to OpenAI API
    core::AsyncResult<core::Result<std::string>> makeRequest(
        const std::string& endpoint,
        const std::string& requestBody,
        const std::string& method = "POST"
    );
    
    /// Build request headers
    std::unordered_map<std::string, std::string> buildHeaders() const;
    
    /// Serialize request to JSON
    std::string serializeRequest(const OpenAIRequest& request) const;
    
    /// Parse response from JSON
    core::Result<OpenAIResponse> parseResponse(const std::string& responseJson) const;
    
    /// Process streaming response
    void processStreamingResponse(
        const std::string& response,
        StreamingCallback callback
    ) const;
    
    /// Update rate limiting counters
    void updateRateLimits(const OpenAIResponse& response);
    
    /// Process request queue
    void processRequestQueue();
    
    /// Generate request ID
    std::string generateRequestId() const;
    
    /// Build tool schemas for API
    std::string buildToolSchemas(const std::vector<std::string>& toolNames) const;
    
    // Configuration
    AIProviderConfig config_;
    std::atomic<bool> isInitialized_{false};
    
    // HTTP client
    std::unique_ptr<httplib::Client> httpClient_;
    mutable std::mutex httpMutex_;
    
    // Model information cache
    std::unordered_map<std::string, OpenAIModel> modelCache_;
    mutable std::mutex modelCacheMutex_;
    
    // Tool/function registry
    std::unordered_map<std::string, ToolFunction> registeredTools_;
    mutable std::shared_mutex toolsMutex_;
    
    // Rate limiting
    RateLimitInfo rateLimitInfo_;
    mutable std::mutex rateLimitMutex_;
    
    // Request queue
    std::queue<RequestQueueItem> requestQueue_;
    std::atomic<bool> queueProcessorRunning_{false};
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    // Statistics and monitoring
    ProviderStats stats_;
    mutable std::mutex statsMutex_;
    
    // Callbacks
    ErrorCallback errorCallback_;
    UsageCallback usageCallback_;
    
    // Background processing
    std::atomic<bool> shouldShutdown_{false};
    std::thread queueProcessor_;
};

// ============================================================================
// Built-in DAW Tool Functions
// ============================================================================

namespace daw_tools {

/// Transport control functions
core::AsyncResult<core::Result<std::string>> play(const std::string& args);
core::AsyncResult<core::Result<std::string>> stop(const std::string& args);
core::AsyncResult<core::Result<std::string>> record(const std::string& args);
core::AsyncResult<core::Result<std::string>> setTempo(const std::string& args);

/// Track management functions
core::AsyncResult<core::Result<std::string>> createTrack(const std::string& args);
core::AsyncResult<core::Result<std::string>> deleteTrack(const std::string& args);
core::AsyncResult<core::Result<std::string>> muteTrack(const std::string& args);
core::AsyncResult<core::Result<std::string>> setTrackVolume(const std::string& args);

/// Session information functions
core::AsyncResult<core::Result<std::string>> getSessionInfo(const std::string& args);
core::AsyncResult<core::Result<std::string>> getTrackInfo(const std::string& args);
core::AsyncResult<core::Result<std::string>> getTransportInfo(const std::string& args);

/// Plugin management functions
core::AsyncResult<core::Result<std::string>> insertPlugin(const std::string& args);
core::AsyncResult<core::Result<std::string>> removePlugin(const std::string& args);
core::AsyncResult<core::Result<std::string>> setPluginParameter(const std::string& args);

} // namespace daw_tools

// ============================================================================
// Utility Functions
// ============================================================================

/// Convert ChatMessage to OpenAI format
std::string chatMessageToOpenAI(const ChatMessage& message);

/// Convert OpenAI response to ChatResponse
ChatResponse openAIToChatResponse(const OpenAIResponse& response);

/// Estimate cost for request
double estimateRequestCost(const OpenAIRequest& request, const std::string& model);

/// Validate OpenAI API key format
bool isValidAPIKey(const std::string& apiKey);

} // namespace mixmind::ai