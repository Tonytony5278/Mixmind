#include "OpenAIIntegration.h"
#include "../core/async.h"
#include <httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

namespace mixmind::ai {

// ============================================================================
// OpenAI Integration - GPT-4 Audio Intelligence
// ============================================================================

class OpenAIClient::Impl {
public:
    std::string apiKey_;
    std::string baseUrl_ = "https://api.openai.com/v1";
    std::unique_ptr<httplib::Client> client_;
    std::mutex requestMutex_;
    
    // Request configuration
    int timeoutSeconds_ = 30;
    int maxRetries_ = 3;
    
    bool initialize(const std::string& apiKey) {
        if (apiKey.empty()) {
            std::cerr << "❌ OpenAI API key is empty" << std::endl;
            return false;
        }
        
        apiKey_ = apiKey;
        client_ = std::make_unique<httplib::Client>(baseUrl_);
        client_->set_connection_timeout(timeoutSeconds_, 0);
        client_->set_read_timeout(timeoutSeconds_, 0);
        
        // Set default headers
        client_->set_default_headers({
            {"Authorization", "Bearer " + apiKey_},
            {"Content-Type", "application/json"},
            {"User-Agent", "MixMind-AI/1.0"}
        });
        
        std::cout << "🤖 OpenAI Client initialized successfully" << std::endl;
        return true;
    }
    
    core::AsyncResult<AIResponse> sendChatRequest(const ChatRequest& request) {
        return core::executeAsyncGlobal<AIResponse>([this, request]() -> core::Result<AIResponse> {
            try {
                json requestJson = {
                    {"model", request.model},
                    {"messages", json::array()},
                    {"temperature", request.temperature},
                    {"max_tokens", request.maxTokens}
                };
                
                // Add messages
                for (const auto& msg : request.messages) {
                    requestJson["messages"].push_back({
                        {"role", msg.role},
                        {"content", msg.content}
                    });
                }
                
                std::lock_guard<std::mutex> lock(requestMutex_);
                
                auto response = client_->Post("/chat/completions", 
                                            requestJson.dump(), 
                                            "application/json");
                
                if (!response) {
                    return core::Result<AIResponse>::failure("Failed to connect to OpenAI API");
                }
                
                if (response->status != 200) {
                    std::string error = "OpenAI API error: " + std::to_string(response->status);
                    if (!response->body.empty()) {
                        error += " - " + response->body;
                    }
                    return core::Result<AIResponse>::failure(error);
                }
                
                // Parse response
                json responseJson = json::parse(response->body);
                
                AIResponse aiResponse;
                aiResponse.success = true;
                
                if (responseJson.contains("choices") && !responseJson["choices"].empty()) {
                    auto& choice = responseJson["choices"][0];
                    if (choice.contains("message") && choice["message"].contains("content")) {
                        aiResponse.content = choice["message"]["content"].get<std::string>();
                    }
                }
                
                if (responseJson.contains("usage")) {
                    auto& usage = responseJson["usage"];
                    if (usage.contains("prompt_tokens")) {
                        aiResponse.promptTokens = usage["prompt_tokens"].get<int>();
                    }
                    if (usage.contains("completion_tokens")) {
                        aiResponse.completionTokens = usage["completion_tokens"].get<int>();
                    }
                }
                
                return core::Result<AIResponse>::success(aiResponse);
                
            } catch (const std::exception& e) {
                return core::Result<AIResponse>::failure("OpenAI request failed: " + std::string(e.what()));
            }
        });
    }
};

// ============================================================================
// OpenAI Client Public Interface
// ============================================================================

OpenAIClient::OpenAIClient() : pImpl_(std::make_unique<Impl>()) {}
OpenAIClient::~OpenAIClient() = default;

bool OpenAIClient::initialize(const std::string& apiKey) {
    return pImpl_->initialize(apiKey);
}

core::AsyncResult<AIResponse> OpenAIClient::sendChatRequest(const ChatRequest& request) {
    return pImpl_->sendChatRequest(request);
}

core::AsyncResult<AIResponse> OpenAIClient::analyzeAudio(const std::string& description, const AudioAnalysisContext& context) {
    ChatRequest request;
    request.model = "gpt-4";
    request.temperature = 0.3;
    request.maxTokens = 1000;
    
    // Create system prompt for audio analysis
    ChatMessage systemMsg;
    systemMsg.role = "system";
    systemMsg.content = R"(
You are MixMind AI, a professional audio engineer and music production expert. 
You analyze audio content and provide detailed, actionable feedback for music production.

Your expertise includes:
- Audio engineering (mixing, mastering, effects)
- Music theory and arrangement
- Genre-specific production techniques  
- Creative suggestions for improvements
- Technical audio problem diagnosis

Always provide specific, actionable advice that producers can implement immediately.
)";
    
    // Create user prompt with context
    ChatMessage userMsg;
    userMsg.role = "user";
    
    std::stringstream prompt;
    prompt << "Analyze this audio content: " << description << "\n\n";
    
    if (!context.audioFormat.empty()) {
        prompt << "Audio Format: " << context.audioFormat << "\n";
    }
    if (context.duration > 0) {
        prompt << "Duration: " << context.duration << " seconds\n";
    }
    if (context.sampleRate > 0) {
        prompt << "Sample Rate: " << context.sampleRate << " Hz\n";
    }
    if (!context.genre.empty()) {
        prompt << "Genre: " << context.genre << "\n";
    }
    if (!context.key.empty()) {
        prompt << "Key: " << context.key << "\n";
    }
    if (context.tempo > 0) {
        prompt << "Tempo: " << context.tempo << " BPM\n";
    }
    
    prompt << "\nProvide detailed analysis and suggestions for:\n";
    prompt << "1. Mix balance and EQ recommendations\n";
    prompt << "2. Dynamic range and compression suggestions\n";
    prompt << "3. Spatial positioning and reverb/delay\n";
    prompt << "4. Creative enhancement ideas\n";
    prompt << "5. Genre-specific production notes\n";
    
    userMsg.content = prompt.str();
    
    request.messages = {systemMsg, userMsg};
    
    return sendChatRequest(request);
}

core::AsyncResult<AIResponse> OpenAIClient::generateMusicIdeas(const MusicGenerationRequest& request) {
    ChatRequest chatRequest;
    chatRequest.model = "gpt-4";
    chatRequest.temperature = 0.8; // Higher creativity for music generation
    chatRequest.maxTokens = 1500;
    
    ChatMessage systemMsg;
    systemMsg.role = "system";
    systemMsg.content = R"(
You are MixMind AI, a creative music composition and production assistant.
You generate innovative musical ideas, chord progressions, melodies, and production concepts.

Your capabilities include:
- Creating chord progressions in any key and style
- Generating melody ideas and hooks
- Suggesting arrangement structures
- Proposing creative production techniques
- Adapting ideas to different genres
- Creating detailed production roadmaps

Always provide practical, implementable ideas with specific musical details.
)";
    
    ChatMessage userMsg;
    userMsg.role = "user";
    
    std::stringstream prompt;
    prompt << "Generate creative music ideas with these parameters:\n\n";
    
    if (!request.genre.empty()) {
        prompt << "Genre: " << request.genre << "\n";
    }
    if (!request.mood.empty()) {
        prompt << "Mood: " << request.mood << "\n";
    }
    if (!request.key.empty()) {
        prompt << "Key: " << request.key << "\n";
    }
    if (request.tempo > 0) {
        prompt << "Tempo: " << request.tempo << " BPM\n";
    }
    if (!request.instruments.empty()) {
        prompt << "Instruments: ";
        for (size_t i = 0; i < request.instruments.size(); ++i) {
            prompt << request.instruments[i];
            if (i < request.instruments.size() - 1) prompt << ", ";
        }
        prompt << "\n";
    }
    
    prompt << "\nPlease generate:\n";
    prompt << "1. A compelling chord progression (with specific chords)\n";
    prompt << "2. Melodic ideas and hooks\n";
    prompt << "3. Rhythm and groove suggestions\n";
    prompt << "4. Arrangement structure (intro, verse, chorus, etc.)\n";
    prompt << "5. Production techniques and sound design ideas\n";
    prompt << "6. Creative variations and development concepts\n";
    
    if (!request.additionalPrompt.empty()) {
        prompt << "\nAdditional requirements: " << request.additionalPrompt << "\n";
    }
    
    userMsg.content = prompt.str();
    chatRequest.messages = {systemMsg, userMsg};
    
    return sendChatRequest(chatRequest);
}

core::AsyncResult<AIResponse> OpenAIClient::provideMixingAdvice(const MixingRequest& request) {
    ChatRequest chatRequest;
    chatRequest.model = "gpt-4";
    chatRequest.temperature = 0.2; // Lower temperature for technical advice
    chatRequest.maxTokens = 1200;
    
    ChatMessage systemMsg;
    systemMsg.role = "system";
    systemMsg.content = R"(
You are MixMind AI, a world-class mixing and mastering engineer with decades of experience.
You provide precise, technical advice for achieving professional-sounding mixes.

Your expertise covers:
- EQ and frequency management
- Compression and dynamics processing
- Spatial imaging and stereo field
- Effects processing (reverb, delay, modulation)
- Mix bus processing and glue
- Genre-specific mixing techniques
- Problem-solving for common mix issues

Provide specific settings, frequencies, and techniques that engineers can apply immediately.
)";
    
    ChatMessage userMsg;
    userMsg.role = "user";
    
    std::stringstream prompt;
    prompt << "I need mixing advice for: " << request.problemDescription << "\n\n";
    
    if (!request.trackType.empty()) {
        prompt << "Track Type: " << request.trackType << "\n";
    }
    if (!request.genre.empty()) {
        prompt << "Genre: " << request.genre << "\n";
    }
    if (!request.currentIssues.empty()) {
        prompt << "Current Issues: " << request.currentIssues << "\n";
    }
    if (!request.desiredSound.empty()) {
        prompt << "Desired Sound: " << request.desiredSound << "\n";
    }
    
    prompt << "\nPlease provide:\n";
    prompt << "1. Specific EQ recommendations (frequencies and amounts)\n";
    prompt << "2. Compression settings and technique\n";
    prompt << "3. Effects processing suggestions\n";
    prompt << "4. Panning and stereo imaging advice\n";
    prompt << "5. Mix bus processing recommendations\n";
    prompt << "6. Step-by-step action plan\n";
    
    userMsg.content = prompt.str();
    chatRequest.messages = {systemMsg, userMsg};
    
    return sendChatRequest(chatRequest);
}

// ============================================================================
// AI-Powered Audio Analysis Engine
// ============================================================================

class AudioIntelligenceEngine::Impl {
public:
    std::unique_ptr<OpenAIClient> openaiClient_;
    std::atomic<bool> isAnalyzing_{false};
    
    // Analysis cache to avoid redundant API calls
    std::mutex cacheMutex_;
    std::unordered_map<std::string, AIResponse> analysisCache_;
    
    bool initialize(const std::string& openaiApiKey) {
        openaiClient_ = std::make_unique<OpenAIClient>();
        return openaiClient_->initialize(openaiApiKey);
    }
    
    std::string generateCacheKey(const std::string& input, const std::string& type) {
        // Simple hash for caching (in production, use proper hashing)
        return type + "_" + std::to_string(std::hash<std::string>{}(input));
    }
};

AudioIntelligenceEngine::AudioIntelligenceEngine() : pImpl_(std::make_unique<Impl>()) {}
AudioIntelligenceEngine::~AudioIntelligenceEngine() = default;

bool AudioIntelligenceEngine::initialize(const std::string& openaiApiKey) {
    return pImpl_->initialize(openaiApiKey);
}

core::AsyncResult<AIResponse> AudioIntelligenceEngine::analyzeAudioContent(const std::string& description, const AudioAnalysisContext& context) {
    if (pImpl_->isAnalyzing_.load()) {
        return core::executeAsyncGlobal<AIResponse>([]() -> core::Result<AIResponse> {
            return core::Result<AIResponse>::failure("Analysis already in progress");
        });
    }
    
    pImpl_->isAnalyzing_.store(true);
    
    return core::executeAsyncGlobal<AIResponse>([this, description, context]() -> core::Result<AIResponse> {
        auto result = pImpl_->openaiClient_->analyzeAudio(description, context);
        pImpl_->isAnalyzing_.store(false);
        return result.get(); // Block and get result
    });
}

core::AsyncResult<AIResponse> AudioIntelligenceEngine::generateCreativeIdeas(const MusicGenerationRequest& request) {
    return pImpl_->openaiClient_->generateMusicIdeas(request);
}

core::AsyncResult<AIResponse> AudioIntelligenceEngine::provideMixingGuidance(const MixingRequest& request) {
    return pImpl_->openaiClient_->provideMixingAdvice(request);
}

bool AudioIntelligenceEngine::isAnalyzing() const {
    return pImpl_->isAnalyzing_.load();
}

// ============================================================================
// Global AI Engine Access
// ============================================================================

static std::unique_ptr<AudioIntelligenceEngine> g_aiEngine;
static std::mutex g_aiEngineMutex;

AudioIntelligenceEngine& getGlobalAIEngine() {
    std::lock_guard<std::mutex> lock(g_aiEngineMutex);
    if (!g_aiEngine) {
        g_aiEngine = std::make_unique<AudioIntelligenceEngine>();
    }
    return *g_aiEngine;
}

void shutdownGlobalAIEngine() {
    std::lock_guard<std::mutex> lock(g_aiEngineMutex);
    g_aiEngine.reset();
}

} // namespace mixmind::ai