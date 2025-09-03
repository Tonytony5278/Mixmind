#include "ChatService.h"
#include "../core/async.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

namespace mixmind::ai {

// ============================================================================
// ChatService Implementation
// ============================================================================

ChatService::ChatService() {
    // Initialize with default configuration
    config_.provider = AIProvider::Mock; // Start with mock for testing
    config_.modelName = "gpt-4";
    config_.temperature = 0.7;
    config_.maxTokens = 2000;
}

ChatService::~ChatService() {
    if (isInitialized_.load()) {
        shutdown().get(); // Block until shutdown complete
    }
}

// ========================================================================
// Service Lifecycle
// ========================================================================

core::AsyncResult<core::VoidResult> ChatService::initialize(const AIProviderConfig& config) {
    return core::getGlobalThreadPool().executeAsyncVoid([this, config]() -> core::VoidResult {
        std::lock_guard lock(conversationsMutex_);
        
        if (isInitialized_.load()) {
            return core::VoidResult::success();
        }
        
        // Store configuration
        config_ = config;
        
        // Create AI provider
        provider_ = createProvider(config_);
        if (!provider_) {
            return core::VoidResult::error(
                core::ErrorCode::InitializationFailed,
                core::ErrorCategory::general(),
                "Failed to create AI provider"
            );
        }
        
        // Initialize provider
        // For mock provider, this is a no-op
        
        // Initialize rate limiting
        lastRequestTime_ = std::chrono::steady_clock::now();
        requestsThisMinute_ = 0;
        tokensThisMinute_ = 0;
        
        // Reset statistics
        resetStats();
        
        isInitialized_.store(true);
        
        return core::VoidResult::success();
    }, "Initializing ChatService");
}

core::AsyncResult<core::VoidResult> ChatService::shutdown() {
    return core::getGlobalThreadPool().executeAsyncVoid([this]() -> core::VoidResult {
        std::lock_guard lock(conversationsMutex_);
        
        if (!isInitialized_.load()) {
            return core::VoidResult::success();
        }
        
        // Clear all conversations
        conversations_.clear();
        
        // Cleanup provider
        provider_.reset();
        
        isInitialized_.store(false);
        
        return core::VoidResult::success();
    }, "Shutting down ChatService");
}

AIProviderConfig ChatService::getConfig() const {
    return config_;
}

core::VoidResult ChatService::updateConfig(const AIProviderConfig& config) {
    config_ = config;
    
    // Recreate provider if needed
    if (config.provider != config_.provider) {
        provider_ = createProvider(config);
    }
    
    return core::VoidResult::success();
}

// ========================================================================
// Conversation Management
// ========================================================================

core::AsyncResult<core::Result<std::string>> ChatService::startConversation(
    const std::string& userId, 
    const std::string& sessionId) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<std::string>>(
        [this, userId, sessionId]() -> core::Result<std::string> {
            
            std::lock_guard lock(conversationsMutex_);
            
            // Generate conversation ID
            std::string conversationId = generateConversationId();
            
            // Create conversation context
            ConversationContext context;
            context.conversationId = conversationId;
            context.sessionId = sessionId.empty() ? conversationId : sessionId;
            context.userId = userId;
            context.createdAt = std::chrono::system_clock::now();
            context.lastActivity = context.createdAt;
            context.isActive = true;
            
            // Initialize with system message
            ChatMessage systemMessage;
            systemMessage.id = generateMessageId();
            systemMessage.role = MessageRole::System;
            systemMessage.type = MessageType::Context;
            systemMessage.content = buildSystemPrompt();
            systemMessage.conversationId = conversationId;
            systemMessage.sessionId = context.sessionId;
            
            context.messages.push_back(systemMessage);
            
            // Store conversation
            conversations_[conversationId] = context;
            
            return core::Result<std::string>::success(conversationId);
        },
        "Starting new conversation"
    );
}

core::AsyncResult<core::VoidResult> ChatService::endConversation(const std::string& conversationId) {
    return core::getGlobalThreadPool().executeAsyncVoid([this, conversationId]() -> core::VoidResult {
        std::lock_guard lock(conversationsMutex_);
        
        auto it = conversations_.find(conversationId);
        if (it != conversations_.end()) {
            it->second.isActive = false;
            // Don't remove immediately - keep for history
        }
        
        return core::VoidResult::success();
    }, "Ending conversation");
}

std::optional<ConversationContext> ChatService::getConversation(const std::string& conversationId) const {
    std::lock_guard lock(conversationsMutex_);
    
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::vector<std::string> ChatService::getActiveConversations(const std::string& userId) const {
    std::lock_guard lock(conversationsMutex_);
    
    std::vector<std::string> activeIds;
    
    for (const auto& [id, context] : conversations_) {
        if (context.isActive && (userId.empty() || context.userId == userId)) {
            activeIds.push_back(id);
        }
    }
    
    return activeIds;
}

core::VoidResult ChatService::updateDAWContext(
    const std::string& conversationId,
    const std::unordered_map<std::string, std::string>& context) {
    
    std::lock_guard lock(conversationsMutex_);
    
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end()) {
        // Merge new context with existing
        for (const auto& [key, value] : context) {
            it->second.dawContext[key] = value;
        }
        it->second.lastActivity = std::chrono::system_clock::now();
    }
    
    return core::VoidResult::success();
}

// ========================================================================
// Message Processing
// ========================================================================

core::AsyncResult<core::Result<ChatResponse>> ChatService::sendMessage(
    const std::string& conversationId,
    const std::string& message,
    MessageType type) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ChatResponse>>(
        [this, conversationId, message, type]() -> core::Result<ChatResponse> {
            
            // Check rate limits
            if (!checkRateLimit()) {
                return core::Result<ChatResponse>::error(
                    core::ErrorCode::RateLimited,
                    core::ErrorCategory::network(),
                    "Rate limit exceeded"
                );
            }
            
            // Get conversation
            auto conversation = getConversation(conversationId);
            if (!conversation) {
                return core::Result<ChatResponse>::error(
                    core::ErrorCode::NotFound,
                    core::ErrorCategory::session(),
                    "Conversation not found"
                );
            }
            
            // Create user message
            ChatMessage userMessage;
            userMessage.id = generateMessageId();
            userMessage.role = MessageRole::User;
            userMessage.type = type;
            userMessage.content = message;
            userMessage.conversationId = conversationId;
            userMessage.sessionId = conversation->sessionId;
            
            // Add to conversation
            {
                std::lock_guard lock(conversationsMutex_);
                auto& context = conversations_[conversationId];
                context.messages.push_back(userMessage);
                context.lastActivity = std::chrono::system_clock::now();
                context.totalMessages++;
                
                // Maintain rolling window
                if (context.messages.size() > context.maxMessages) {
                    context.messages.erase(context.messages.begin());
                }
            }
            
            // Generate response using provider
            ChatResponse response;
            if (provider_) {
                response = generateMockResponse(conversationId, message, type);
            } else {
                response.hasError = true;
                response.errorMessage = "No AI provider available";
                response.errorCode = "NO_PROVIDER";
            }
            
            // Create assistant message
            if (!response.hasError) {
                ChatMessage assistantMessage;
                assistantMessage.id = generateMessageId();
                assistantMessage.role = MessageRole::Assistant;
                assistantMessage.type = response.type;
                assistantMessage.content = response.content;
                assistantMessage.conversationId = conversationId;
                assistantMessage.sessionId = conversation->sessionId;
                
                // Add to conversation
                {
                    std::lock_guard lock(conversationsMutex_);
                    auto& context = conversations_[conversationId];
                    context.messages.push_back(assistantMessage);
                    context.totalMessages++;
                }
            }
            
            // Update statistics
            updateStats(response, !response.hasError);
            
            return core::Result<ChatResponse>::success(response);
        },
        "Processing chat message"
    );
}

core::AsyncResult<core::Result<std::string>> ChatService::sendMessageStreaming(
    const std::string& conversationId,
    const std::string& message,
    StreamingCallback streamingCallback,
    MessageType type) {
    
    // For now, simulate streaming by sending response in chunks
    return core::getGlobalThreadPool().executeAsync<core::Result<std::string>>(
        [this, conversationId, message, streamingCallback, type]() -> core::Result<std::string> {
            
            auto responseResult = sendMessage(conversationId, message, type).get();
            if (!responseResult.isSuccess()) {
                return core::Result<std::string>::error(
                    responseResult.error().code,
                    responseResult.error().category,
                    responseResult.error().message
                );
            }
            
            auto response = responseResult.value();
            
            // Simulate streaming by sending in chunks
            std::string content = response.content;
            size_t chunkSize = 10; // Small chunks for demo
            
            for (size_t i = 0; i < content.length(); i += chunkSize) {
                std::string chunk = content.substr(i, chunkSize);
                bool isComplete = (i + chunkSize >= content.length());
                
                streamingCallback(chunk, isComplete);
                
                // Small delay to simulate real streaming
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            return core::Result<std::string>::success(response.id);
        },
        "Processing streaming message"
    );
}

core::VoidResult ChatService::addSystemMessage(
    const std::string& conversationId,
    const std::string& content,
    const std::unordered_map<std::string, std::string>& metadata) {
    
    std::lock_guard lock(conversationsMutex_);
    
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end()) {
        ChatMessage systemMessage;
        systemMessage.id = generateMessageId();
        systemMessage.role = MessageRole::System;
        systemMessage.type = MessageType::Context;
        systemMessage.content = content;
        systemMessage.conversationId = conversationId;
        systemMessage.sessionId = it->second.sessionId;
        systemMessage.metadata = metadata;
        
        it->second.messages.push_back(systemMessage);
        it->second.lastActivity = std::chrono::system_clock::now();
    }
    
    return core::VoidResult::success();
}

core::VoidResult ChatService::addToolResult(
    const std::string& conversationId,
    const std::string& toolCallId,
    const std::string& result,
    bool success) {
    
    std::lock_guard lock(conversationsMutex_);
    
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end()) {
        ChatMessage toolMessage;
        toolMessage.id = generateMessageId();
        toolMessage.role = MessageRole::Tool;
        toolMessage.type = success ? MessageType::Confirmation : MessageType::Error;
        toolMessage.content = result;
        toolMessage.conversationId = conversationId;
        toolMessage.sessionId = it->second.sessionId;
        toolMessage.toolCallId = toolCallId;
        toolMessage.toolResult = result;
        
        it->second.messages.push_back(toolMessage);
        it->second.lastActivity = std::chrono::system_clock::now();
    }
    
    return core::VoidResult::success();
}

// ========================================================================
// Message History and Context
// ========================================================================

std::vector<ChatMessage> ChatService::getMessages(
    const std::string& conversationId,
    size_t limit,
    size_t offset) const {
    
    std::lock_guard lock(conversationsMutex_);
    
    auto it = conversations_.find(conversationId);
    if (it == conversations_.end()) {
        return {};
    }
    
    const auto& messages = it->second.messages;
    
    if (offset >= messages.size()) {
        return {};
    }
    
    size_t start = offset;
    size_t end = std::min(start + limit, messages.size());
    
    return std::vector<ChatMessage>(messages.begin() + start, messages.begin() + end);
}

core::AsyncResult<core::Result<std::vector<ChatMessage>>> ChatService::searchMessages(
    const std::string& conversationId,
    const std::string& query,
    MessageType type) const {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<std::vector<ChatMessage>>>(
        [this, conversationId, query, type]() -> core::Result<std::vector<ChatMessage>> {
            
            auto conversation = getConversation(conversationId);
            if (!conversation) {
                return core::Result<std::vector<ChatMessage>>::error(
                    core::ErrorCode::NotFound,
                    core::ErrorCategory::session(),
                    "Conversation not found"
                );
            }
            
            std::vector<ChatMessage> results;
            
            for (const auto& message : conversation->messages) {
                // Simple text search - could be enhanced with better matching
                bool matches = false;
                
                if (type == MessageType::Text || message.type == type) {
                    if (message.content.find(query) != std::string::npos) {
                        matches = true;
                    }
                }
                
                if (matches) {
                    results.push_back(message);
                }
            }
            
            return core::Result<std::vector<ChatMessage>>::success(results);
        },
        "Searching messages"
    );
}

core::VoidResult ChatService::clearHistory(const std::string& conversationId) {
    std::lock_guard lock(conversationsMutex_);
    
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end()) {
        // Keep system messages, clear others
        auto& messages = it->second.messages;
        messages.erase(
            std::remove_if(messages.begin(), messages.end(),
                [](const ChatMessage& msg) {
                    return msg.role != MessageRole::System;
                }),
            messages.end()
        );
        
        it->second.totalMessages = messages.size();
        it->second.lastActivity = std::chrono::system_clock::now();
    }
    
    return core::VoidResult::success();
}

// ========================================================================
// Statistics and Monitoring
// ========================================================================

ChatService::ChatStats ChatService::getStats() const {
    std::lock_guard lock(statsMutex_);
    return stats_;
}

void ChatService::resetStats() {
    std::lock_guard lock(statsMutex_);
    stats_ = ChatStats{};
}

// ========================================================================
// Internal Implementation
// ========================================================================

std::unique_ptr<ChatService::AIProvider> ChatService::createProvider(const AIProviderConfig& config) {
    // For now, create a mock provider
    return std::make_unique<MockProvider>();
}

std::string ChatService::generateMessageId() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    std::ostringstream oss;
    oss << "msg_" << std::hex << dis(gen);
    return oss.str();
}

std::string ChatService::generateConversationId() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    std::ostringstream oss;
    oss << "conv_" << std::hex << dis(gen);
    return oss.str();
}

std::string ChatService::buildSystemPrompt() const {
    return "You are MixMind AI, an intelligent assistant for audio production and music creation. "
           "You help users with DAW operations, mixing advice, and creative guidance. "
           "Respond conversationally and offer helpful suggestions.";
}

bool ChatService::checkRateLimit() const {
    std::lock_guard lock(rateLimitMutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - lastRequestTime_);
    
    if (elapsed.count() >= 1) {
        // Reset counters every minute
        requestsThisMinute_ = 0;
        tokensThisMinute_ = 0;
        lastRequestTime_ = now;
    }
    
    return requestsThisMinute_ < config_.maxRequestsPerMinute &&
           tokensThisMinute_ < config_.maxTokensPerMinute;
}

void ChatService::updateStats(const ChatResponse& response, bool success) {
    std::lock_guard lock(statsMutex_);
    
    stats_.totalMessages++;
    if (success) {
        stats_.successfulRequests++;
        stats_.totalTokensUsed += response.tokensUsed;
        
        // Update average response time
        if (stats_.successfulRequests == 1) {
            stats_.averageResponseTime = response.responseTime;
        } else {
            stats_.averageResponseTime = 
                (stats_.averageResponseTime * (stats_.successfulRequests - 1) + response.responseTime) / 
                stats_.successfulRequests;
        }
    } else {
        stats_.failedRequests++;
    }
}

ChatResponse ChatService::generateMockResponse(
    const std::string& conversationId,
    const std::string& message,
    MessageType type) {
    
    ChatResponse response;
    response.id = generateMessageId();
    response.type = MessageType::Text;
    response.model = "mock-gpt-4";
    response.tokensUsed = static_cast<int>(message.length() / 4); // Rough estimate
    response.responseTime = 0.5; // 500ms
    response.confidence = 0.9;
    
    // Generate different responses based on message content
    std::string lowerMessage = message;
    std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(), ::tolower);
    
    if (lowerMessage.find("create") != std::string::npos && lowerMessage.find("track") != std::string::npos) {
        response.content = "I'll help you create a new track. Would you like an audio track or MIDI track?";
        response.type = MessageType::Command;
        response.suggestedActions = {"create_audio_track", "create_midi_track"};
    }
    else if (lowerMessage.find("tempo") != std::string::npos) {
        response.content = "The current tempo is 120 BPM. Would you like to change it?";
        response.type = MessageType::Query;
    }
    else if (lowerMessage.find("help") != std::string::npos) {
        response.content = "I'm here to help! I can assist with:\n"
                          "• Creating and managing tracks\n"
                          "• Transport controls (play, record, stop)\n" 
                          "• Adding effects and plugins\n"
                          "• Mixing and arrangement tips\n\n"
                          "What would you like to work on?";
        response.type = MessageType::Explanation;
    }
    else if (lowerMessage.find("play") != std::string::npos) {
        response.content = "Starting playback now.";
        response.type = MessageType::ActionConfirmation;
        response.suggestedActions = {"transport_play"};
    }
    else {
        response.content = "I understand you're working on your music project. How can I help you today?";
        response.type = MessageType::Text;
    }
    
    return response;
}

// ============================================================================
// Mock AI Provider Implementation
// ============================================================================

class ChatService::MockProvider : public ChatService::AIProvider {
public:
    MockProvider() = default;
    ~MockProvider() override = default;
    
    // Minimal mock implementation
    std::string processMessage(const std::string& message) override {
        return "Mock response to: " + message;
    }
};

// ============================================================================
// Global Chat Service Instance
// ============================================================================

ChatService& getGlobalChatService() {
    static ChatService instance;
    return instance;
}

} // namespace mixmind::ai