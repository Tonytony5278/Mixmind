#include "RealOpenAIService.h"
#include "../core/logging.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace mixmind::services {

// ============================================================================
// Real OpenAI Service Implementation
// ============================================================================

class RealOpenAIService::Impl {
public:
    std::string apiKey;
    std::string organization;
    AIModel defaultModel = AIModel::GPT_4_TURBO;
    
    // HTTP client for OpenAI API
    std::unique_ptr<httplib::Client> httpClient;
    
    // Rate limiting and statistics
    std::atomic<int> requestsToday{0};
    std::atomic<int> queuedRequests{0};
    std::atomic<double> totalCostToday{0.0};
    std::atomic<double> averageResponseTime{0.0};
    std::atomic<bool> isOnline{false};
    
    int maxConcurrentRequests = 5;
    std::chrono::seconds requestTimeout{30};
    
    // Request queue and processing
    struct QueuedRequest {
        AIRequest request;
        std::promise<mixmind::core::Result<AIResponse>> promise;
        std::chrono::steady_clock::time_point queueTime;
    };
    
    std::queue<QueuedRequest> requestQueue;
    std::mutex requestMutex;
    std::condition_variable requestCondition;
    std::atomic<bool> processingActive{false};
    std::vector<std::thread> workerThreads;
    
    // Session management for conversations
    struct ConversationSession {
        std::vector<std::pair<std::string, std::string>> messages; // role, content
        std::chrono::steady_clock::time_point lastActivity;
        std::string context;
    };
    
    std::unordered_map<std::string, ConversationSession> sessions;
    std::mutex sessionMutex;
    
    // Token and cost tracking
    static constexpr double GPT4_TURBO_INPUT_COST = 0.01 / 1000;   // $0.01 per 1K tokens
    static constexpr double GPT4_TURBO_OUTPUT_COST = 0.03 / 1000;  // $0.03 per 1K tokens
    static constexpr double GPT4_INPUT_COST = 0.03 / 1000;         // $0.03 per 1K tokens  
    static constexpr double GPT4_OUTPUT_COST = 0.06 / 1000;        // $0.06 per 1K tokens
    static constexpr double GPT35_TURBO_INPUT_COST = 0.0015 / 1000; // $0.0015 per 1K tokens
    static constexpr double GPT35_TURBO_OUTPUT_COST = 0.002 / 1000; // $0.002 per 1K tokens
    
    Impl() {
        // Initialize HTTP client for OpenAI API
        httpClient = std::make_unique<httplib::Client>("https://api.openai.com");
        httpClient->set_connection_timeout(0, 300000); // 5 minutes
        httpClient->set_read_timeout(30, 0); // 30 seconds
        httpClient->set_write_timeout(30, 0); // 30 seconds
        
        // Test connectivity
        testConnectivity();
    }
    
    ~Impl() {
        shutdown();
    }
    
    void testConnectivity() {
        try {
            auto result = httpClient->Get("/v1/models");
            isOnline = (result && result->status == 200);
            MIXMIND_LOG_INFO("OpenAI API connectivity test: {}", isOnline.load() ? "ONLINE" : "OFFLINE");
        } catch (const std::exception& e) {
            isOnline = false;
            MIXMIND_LOG_ERROR("OpenAI API connectivity test failed: {}", e.what());
        }
    }
    
    bool initialize(const std::string& key, const std::string& org) {
        apiKey = key;
        organization = org;
        
        if (apiKey.empty()) {
            MIXMIND_LOG_ERROR("OpenAI API key is required");
            return false;
        }
        
        // Validate API key
        if (!validateApiKey()) {
            MIXMIND_LOG_ERROR("Invalid OpenAI API key");
            return false;
        }
        
        // Start worker threads
        startWorkerThreads();
        
        MIXMIND_LOG_INFO("RealOpenAIService initialized successfully");
        return true;
    }
    
    bool validateApiKey() {
        httplib::Headers headers = {
            {"Authorization", "Bearer " + apiKey},
            {"Content-Type", "application/json"}
        };
        
        if (!organization.empty()) {
            headers.emplace("OpenAI-Organization", organization);
        }
        
        try {
            auto result = httpClient->Get("/v1/models", headers);
            
            if (result && result->status == 200) {
                isOnline = true;
                return true;
            } else {
                MIXMIND_LOG_ERROR("API key validation failed. Status: {}", 
                                result ? result->status : -1);
                return false;
            }
        } catch (const std::exception& e) {
            MIXMIND_LOG_ERROR("API key validation exception: {}", e.what());
            return false;
        }
    }
    
    void startWorkerThreads() {
        processingActive = true;
        
        for (int i = 0; i < maxConcurrentRequests; ++i) {
            workerThreads.emplace_back([this]() { workerThreadFunction(); });
        }
        
        MIXMIND_LOG_INFO("Started {} OpenAI worker threads", maxConcurrentRequests);
    }
    
    void shutdown() {
        processingActive = false;
        requestCondition.notify_all();
        
        for (auto& thread : workerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        workerThreads.clear();
        MIXMIND_LOG_INFO("OpenAI service shutdown complete");
    }
    
    void workerThreadFunction() {
        while (processingActive) {
            std::unique_lock<std::mutex> lock(requestMutex);
            
            requestCondition.wait(lock, [this]() {
                return !requestQueue.empty() || !processingActive;
            });
            
            if (!processingActive) break;
            
            if (requestQueue.empty()) continue;
            
            auto queuedRequest = std::move(requestQueue.front());
            requestQueue.pop();
            queuedRequests--;
            
            lock.unlock();
            
            // Process the request
            processRequest(queuedRequest);
        }
    }
    
    void processRequest(QueuedRequest& queuedRequest) {
        auto startTime = std::chrono::steady_clock::now();
        
        try {
            auto response = makeApiRequest(queuedRequest.request);
            
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            response.responseTime = duration;
            updateStatistics(duration, response.tokensUsed);
            
            queuedRequest.promise.set_value(
                mixmind::core::Result<AIResponse>::success(std::move(response))
            );
            
        } catch (const std::exception& e) {
            queuedRequest.promise.set_value(
                mixmind::core::Result<AIResponse>::error(
                    mixmind::core::ErrorCode::NetworkError,
                    "openai_api",
                    "OpenAI API request failed: " + std::string(e.what())
                )
            );
        }
    }
    
    AIResponse makeApiRequest(const AIRequest& request) {
        AIResponse response;
        response.model = getModelString(request.model);
        
        // Build request JSON
        json requestJson = {
            {"model", response.model},
            {"max_tokens", request.maxTokens},
            {"temperature", request.temperature},
            {"top_p", request.topP}
        };
        
        // Build messages
        json messages = json::array();
        
        if (!request.systemPrompt.empty()) {
            messages.push_back({
                {"role", "system"},
                {"content", request.systemPrompt}
            });
        }
        
        // Add context data if provided
        if (!request.contextData.empty()) {
            std::string contextString = "Context information:\n";
            for (const auto& context : request.contextData) {
                contextString += "- " + context + "\n";
            }
            contextString += "\n" + request.prompt;
            
            messages.push_back({
                {"role", "user"},
                {"content", contextString}
            });
        } else {
            messages.push_back({
                {"role", "user"},
                {"content", request.prompt}
            });
        }
        
        requestJson["messages"] = messages;
        
        // Prepare headers
        httplib::Headers headers = {
            {"Authorization", "Bearer " + apiKey},
            {"Content-Type", "application/json"}
        };
        
        if (!organization.empty()) {
            headers.emplace("OpenAI-Organization", organization);
        }
        
        // Make request
        auto result = httpClient->Post("/v1/chat/completions", headers, requestJson.dump(), "application/json");
        
        if (!result) {
            throw std::runtime_error("Failed to connect to OpenAI API");
        }
        
        if (result->status != 200) {
            throw std::runtime_error("OpenAI API error: " + std::to_string(result->status) + " " + result->body);
        }
        
        // Parse response
        json responseJson = json::parse(result->body);
        
        if (!responseJson.contains("choices") || responseJson["choices"].empty()) {
            throw std::runtime_error("Invalid OpenAI API response format");
        }
        
        auto& choice = responseJson["choices"][0];
        
        response.content = choice["message"]["content"];
        response.isSuccess = true;
        
        // Extract usage information
        if (responseJson.contains("usage")) {
            response.tokensUsed = responseJson["usage"]["total_tokens"];
        }
        
        // Parse structured data and tags from response if JSON format
        parseStructuredResponse(response);
        
        requestsToday++;
        
        return response;
    }
    
    void parseStructuredResponse(AIResponse& response) {
        // Try to parse JSON from the response content
        try {
            // Look for JSON blocks in the response
            size_t jsonStart = response.content.find("```json");
            if (jsonStart != std::string::npos) {
                jsonStart += 7; // Skip "```json"
                size_t jsonEnd = response.content.find("```", jsonStart);
                
                if (jsonEnd != std::string::npos) {
                    std::string jsonStr = response.content.substr(jsonStart, jsonEnd - jsonStart);
                    json parsed = json::parse(jsonStr);
                    
                    // Extract structured data
                    if (parsed.contains("tags") && parsed["tags"].is_array()) {
                        for (const auto& tag : parsed["tags"]) {
                            response.tags.push_back(tag);
                        }
                    }
                    
                    if (parsed.contains("confidence")) {
                        response.confidenceScore = parsed["confidence"];
                    }
                    
                    if (parsed.contains("metadata") && parsed["metadata"].is_object()) {
                        for (auto& [key, value] : parsed["metadata"].items()) {
                            response.structuredData[key] = value;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            // If JSON parsing fails, it's not critical - continue with text response
            MIXMIND_LOG_DEBUG("Could not parse structured data from AI response: {}", e.what());
        }
    }
    
    std::string getModelString(AIModel model) {
        switch (model) {
            case AIModel::GPT_4_TURBO: return "gpt-4-1106-preview";
            case AIModel::GPT_4: return "gpt-4";
            case AIModel::GPT_3_5_TURBO: return "gpt-3.5-turbo";
            case AIModel::GPT_4_VISION: return "gpt-4-vision-preview";
            default: return "gpt-4-1106-preview";
        }
    }
    
    double calculateTokenCost(AIModel model, int inputTokens, int outputTokens) {
        double cost = 0.0;
        
        switch (model) {
            case AIModel::GPT_4_TURBO:
                cost = inputTokens * GPT4_TURBO_INPUT_COST + outputTokens * GPT4_TURBO_OUTPUT_COST;
                break;
            case AIModel::GPT_4:
                cost = inputTokens * GPT4_INPUT_COST + outputTokens * GPT4_OUTPUT_COST;
                break;
            case AIModel::GPT_3_5_TURBO:
                cost = inputTokens * GPT35_TURBO_INPUT_COST + outputTokens * GPT35_TURBO_OUTPUT_COST;
                break;
            case AIModel::GPT_4_VISION:
                cost = inputTokens * GPT4_INPUT_COST + outputTokens * GPT4_OUTPUT_COST;
                break;
        }
        
        return cost;
    }
    
    void updateStatistics(std::chrono::milliseconds responseTime, int tokensUsed) {
        // Update average response time
        double currentAvg = averageResponseTime.load();
        double newAvg = (currentAvg * (requestsToday.load() - 1) + responseTime.count()) / requestsToday.load();
        averageResponseTime.store(newAvg);
        
        // Update cost (simplified - assumes equal input/output tokens)
        double cost = calculateTokenCost(defaultModel, tokensUsed / 2, tokensUsed / 2);
        totalCostToday.fetch_add(cost);
    }
    
    void addMessageToSession(const std::string& sessionId, const std::string& role, const std::string& content) {
        std::lock_guard<std::mutex> lock(sessionMutex);
        
        auto& session = sessions[sessionId];
        session.messages.emplace_back(role, content);
        session.lastActivity = std::chrono::steady_clock::now();
        
        // Limit conversation history to prevent excessive token usage
        if (session.messages.size() > 20) {
            session.messages.erase(session.messages.begin(), session.messages.begin() + 10);
        }
    }
    
    std::vector<std::pair<std::string, std::string>> getSessionMessages(const std::string& sessionId) {
        std::lock_guard<std::mutex> lock(sessionMutex);
        
        auto it = sessions.find(sessionId);
        if (it != sessions.end()) {
            return it->second.messages;
        }
        
        return {};
    }
};

RealOpenAIService::RealOpenAIService()
    : pImpl_(std::make_unique<Impl>()) {
}

RealOpenAIService::~RealOpenAIService() = default;

bool RealOpenAIService::initialize(const std::string& apiKey, const std::string& organization) {
    return pImpl_->initialize(apiKey, organization);
}

void RealOpenAIService::setDefaultModel(AIModel model) {
    pImpl_->defaultModel = model;
}

void RealOpenAIService::setMaxConcurrentRequests(int maxRequests) {
    pImpl_->maxConcurrentRequests = maxRequests;
}

void RealOpenAIService::setRequestTimeout(std::chrono::seconds timeout) {
    pImpl_->requestTimeout = timeout;
}

mixmind::core::AsyncResult<AIResponse> RealOpenAIService::sendRequest(const AIRequest& request) {
    auto promise = std::make_shared<std::promise<mixmind::core::Result<AIResponse>>>();
    auto future = promise->get_future();
    
    // Queue the request
    {
        std::lock_guard<std::mutex> lock(pImpl_->requestMutex);
        
        RealOpenAIService::Impl::QueuedRequest queuedRequest;
        queuedRequest.request = request;
        queuedRequest.promise = std::move(*promise);
        queuedRequest.queueTime = std::chrono::steady_clock::now();
        
        pImpl_->requestQueue.push(std::move(queuedRequest));
        pImpl_->queuedRequests++;
    }
    
    pImpl_->requestCondition.notify_one();
    
    return mixmind::core::AsyncResult<AIResponse>(std::move(future));
}

mixmind::core::AsyncResult<PluginAnalysisResult> RealOpenAIService::analyzePlugin(const PluginAnalysisRequest& request) {
    auto promise = std::make_shared<std::promise<mixmind::core::Result<PluginAnalysisResult>>>();
    auto future = promise->get_future();
    
    // Build specialized prompt for plugin analysis
    std::stringstream promptStream;
    promptStream << "Analyze the following audio plugin for professional music production:\n\n";
    promptStream << "Plugin: " << request.pluginName << "\n";
    promptStream << "Manufacturer: " << request.manufacturer << "\n";
    promptStream << "Category: " << request.category << "\n";
    promptStream << "Version: " << request.version << "\n";
    
    if (request.cpuUsage > 0) {
        promptStream << "CPU Usage: " << request.cpuUsage << "%\n";
    }
    
    if (request.latencySamples > 0) {
        promptStream << "Latency: " << request.latencySamples << " samples\n";
    }
    
    promptStream << "Type: " << (request.isInstrument ? "Virtual Instrument" : "Audio Effect") << "\n\n";
    
    if (!request.parameters.empty()) {
        promptStream << "Available Parameters:\n";
        for (const auto& param : request.parameters) {
            promptStream << "- " << param << "\n";
        }
        promptStream << "\n";
    }
    
    if (!request.additionalContext.empty()) {
        promptStream << "Additional Context: " << request.additionalContext << "\n\n";
    }
    
    promptStream << "Provide a comprehensive analysis including:\n";
    promptStream << "1. Technical capabilities and sound quality\n";
    promptStream << "2. Best use cases and target audience\n";
    promptStream << "3. Workflow integration tips\n";
    promptStream << "4. Quality rating (0-1 scale)\n";
    promptStream << "5. Recommended plugin combinations\n";
    promptStream << "6. Genre-specific applications\n\n";
    promptStream << "Format your response as JSON with the following structure:\n";
    promptStream << "```json\n";
    promptStream << "{\n";
    promptStream << "  \"analysis\": \"detailed analysis text\",\n";
    promptStream << "  \"recommendations\": \"usage recommendations\",\n";
    promptStream << "  \"tags\": [\"tag1\", \"tag2\", \"tag3\"],\n";
    promptStream << "  \"qualityScore\": 0.85,\n";
    promptStream << "  \"workflow\": \"workflow integration advice\",\n";
    promptStream << "  \"compatiblePlugins\": [\"plugin1\", \"plugin2\"],\n";
    promptStream << "  \"bestUseCase\": \"primary use case description\",\n";
    promptStream << "  \"targetAudience\": \"target user description\"\n";
    promptStream << "}\n```";
    
    AIRequest aiRequest;
    aiRequest.model = AIModel::GPT_4_TURBO;
    aiRequest.taskType = AITaskType::PLUGIN_ANALYSIS;
    aiRequest.prompt = promptStream.str();
    aiRequest.maxTokens = 1500;
    aiRequest.temperature = 0.3f; // Lower temperature for more consistent analysis
    aiRequest.systemPrompt = "You are an expert audio engineer and music producer with deep knowledge of audio plugins, "
                           "digital signal processing, and professional music production workflows. Provide detailed, "
                           "accurate, and practical analysis based on industry standards and real-world usage.";
    
    // Send AI request and process response
    auto aiResponseFuture = sendRequest(aiRequest);
    
    // Process in background thread
    std::thread([promise, aiResponseFuture = std::move(aiResponseFuture)]() mutable {
        try {
            auto aiResult = aiResponseFuture.get();
            
            if (!aiResult.isSuccess()) {
                promise->set_value(mixmind::core::Result<PluginAnalysisResult>::error(
                    mixmind::core::ErrorCode::Unknown,
                    "ai_analysis",
                    aiResult.getErrorMessage()
                ));
                return;
            }
            
            auto& aiResponse = aiResult.value();
            PluginAnalysisResult result;
            
            // Parse JSON response
            try {
                size_t jsonStart = aiResponse.content.find("```json");
                if (jsonStart != std::string::npos) {
                    jsonStart += 7;
                    size_t jsonEnd = aiResponse.content.find("```", jsonStart);
                    
                    if (jsonEnd != std::string::npos) {
                        std::string jsonStr = aiResponse.content.substr(jsonStart, jsonEnd - jsonStart);
                        json parsed = json::parse(jsonStr);
                        
                        result.analysis = parsed.value("analysis", "");
                        result.recommendations = parsed.value("recommendations", "");
                        result.qualityScore = parsed.value("qualityScore", 0.5f);
                        result.workflow = parsed.value("workflow", "");
                        result.bestUseCase = parsed.value("bestUseCase", "");
                        result.targetAudience = parsed.value("targetAudience", "");
                        
                        if (parsed.contains("tags") && parsed["tags"].is_array()) {
                            for (const auto& tag : parsed["tags"]) {
                                result.tags.push_back(tag);
                            }
                        }
                        
                        if (parsed.contains("compatiblePlugins") && parsed["compatiblePlugins"].is_array()) {
                            for (const auto& plugin : parsed["compatiblePlugins"]) {
                                result.compatiblePlugins.push_back(plugin);
                            }
                        }
                    }
                }
                
                // If no JSON found, use the raw content
                if (result.analysis.empty()) {
                    result.analysis = aiResponse.content;
                    result.qualityScore = 0.7f; // Default score
                }
                
            } catch (const std::exception& e) {
                // Fallback to raw text if JSON parsing fails
                result.analysis = aiResponse.content;
                result.qualityScore = 0.7f;
                MIXMIND_LOG_WARNING("Failed to parse plugin analysis JSON: {}", e.what());
            }
            
            promise->set_value(mixmind::core::Result<PluginAnalysisResult>::success(std::move(result)));
            
        } catch (const std::exception& e) {
            promise->set_value(mixmind::core::Result<PluginAnalysisResult>::error(
                mixmind::core::ErrorCode::Unknown,
                "plugin_analysis",
                "Plugin analysis failed: " + std::string(e.what())
            ));
        }
    }).detach();
    
    return mixmind::core::AsyncResult<PluginAnalysisResult>(std::move(future));
}

mixmind::core::AsyncResult<StyleMatchingResult> RealOpenAIService::matchStyle(const StyleMatchingRequest& request) {
    auto promise = std::make_shared<std::promise<mixmind::core::Result<StyleMatchingResult>>>();
    auto future = promise->get_future();
    
    // Build specialized prompt for style matching
    std::stringstream promptStream;
    promptStream << "Create a detailed style matching guide for recreating the sound of " << request.targetArtist;
    
    if (!request.targetSong.empty()) {
        promptStream << ", specifically the song \"" << request.targetSong << "\"";
    }
    
    promptStream << ".\n\nUser's Context:\n";
    promptStream << "- Current audio genre: " << request.userGenre << "\n";
    
    if (!request.userAudioPath.empty()) {
        promptStream << "- User has audio file at: " << request.userAudioPath << "\n";
    }
    
    if (!request.specificRequest.empty()) {
        promptStream << "- Specific request: " << request.specificRequest << "\n";
    }
    
    if (!request.availablePlugins.empty()) {
        promptStream << "\nAvailable Plugins:\n";
        for (const auto& plugin : request.availablePlugins) {
            promptStream << "- " << plugin << "\n";
        }
    }
    
    promptStream << "\nProvide a comprehensive style matching guide including:\n";
    promptStream << "1. Analysis of the target artist's signature sound characteristics\n";
    promptStream << "2. Specific plugin chain recommendations using available plugins\n";
    promptStream << "3. Detailed parameter settings for each plugin\n";
    promptStream << "4. Step-by-step processing instructions\n";
    promptStream << "5. Tonal characteristics to achieve\n";
    promptStream << "6. Recording techniques and equipment recommendations\n";
    promptStream << "7. Confidence level of the match (0-1 scale)\n\n";
    
    promptStream << "Format as JSON:\n```json\n{\n";
    promptStream << "  \"analysis\": \"detailed sound analysis\",\n";
    promptStream << "  \"pluginChain\": [\"plugin1\", \"plugin2\", \"plugin3\"],\n";
    promptStream << "  \"pluginSettings\": {\"plugin1\": {\"param1\": 0.7, \"param2\": 0.3}},\n";
    promptStream << "  \"processingSteps\": \"step by step guide\",\n";
    promptStream << "  \"tonalCharacteristics\": \"tonal description\",\n";
    promptStream << "  \"recordingTechniques\": \"recording advice\",\n";
    promptStream << "  \"equipmentRecommendations\": \"gear suggestions\",\n";
    promptStream << "  \"matchConfidence\": 0.85\n";
    promptStream << "}\n```";
    
    AIRequest aiRequest;
    aiRequest.model = AIModel::GPT_4_TURBO;
    aiRequest.taskType = AITaskType::STYLE_MATCHING;
    aiRequest.prompt = promptStream.str();
    aiRequest.maxTokens = 2000;
    aiRequest.temperature = 0.4f;
    aiRequest.systemPrompt = "You are a world-renowned audio engineer and producer with encyclopedic knowledge of "
                           "recording techniques, artist sounds, and audio processing. You have worked with major "
                           "artists and understand the technical details behind iconic sounds. Provide precise, "
                           "actionable advice based on professional industry knowledge.";
    
    // Process similar to plugin analysis
    auto aiResponseFuture = sendRequest(aiRequest);
    
    std::thread([promise, aiResponseFuture = std::move(aiResponseFuture)]() mutable {
        try {
            auto aiResult = aiResponseFuture.get();
            
            if (!aiResult.isSuccess()) {
                promise->set_value(mixmind::core::Result<StyleMatchingResult>::error(
                    mixmind::core::ErrorCode::Unknown,
                    "style_matching",
                    aiResult.getErrorMessage()
                ));
                return;
            }
            
            auto& aiResponse = aiResult.value();
            StyleMatchingResult result;
            
            // Parse JSON response
            try {
                size_t jsonStart = aiResponse.content.find("```json");
                if (jsonStart != std::string::npos) {
                    jsonStart += 7;
                    size_t jsonEnd = aiResponse.content.find("```", jsonStart);
                    
                    if (jsonEnd != std::string::npos) {
                        std::string jsonStr = aiResponse.content.substr(jsonStart, jsonEnd - jsonStart);
                        json parsed = json::parse(jsonStr);
                        
                        result.analysis = parsed.value("analysis", "");
                        result.processingSteps = parsed.value("processingSteps", "");
                        result.tonalCharacteristics = parsed.value("tonalCharacteristics", "");
                        result.recordingTechniques = parsed.value("recordingTechniques", "");
                        result.equipmentRecommendations = parsed.value("equipmentRecommendations", "");
                        result.matchConfidence = parsed.value("matchConfidence", 0.5f);
                        
                        if (parsed.contains("pluginChain") && parsed["pluginChain"].is_array()) {
                            for (const auto& plugin : parsed["pluginChain"]) {
                                result.pluginChain.push_back(plugin);
                            }
                        }
                        
                        if (parsed.contains("pluginSettings") && parsed["pluginSettings"].is_object()) {
                            for (auto& [pluginName, settings] : parsed["pluginSettings"].items()) {
                                if (settings.is_object()) {
                                    for (auto& [paramName, value] : settings.items()) {
                                        if (value.is_number()) {
                                            result.pluginSettings[pluginName + "::" + paramName] = value;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                if (result.analysis.empty()) {
                    result.analysis = aiResponse.content;
                    result.matchConfidence = 0.7f;
                }
                
            } catch (const std::exception& e) {
                result.analysis = aiResponse.content;
                result.matchConfidence = 0.7f;
                MIXMIND_LOG_WARNING("Failed to parse style matching JSON: {}", e.what());
            }
            
            promise->set_value(mixmind::core::Result<StyleMatchingResult>::success(std::move(result)));
            
        } catch (const std::exception& e) {
            promise->set_value(mixmind::core::Result<StyleMatchingResult>::error(
                mixmind::core::ErrorCode::Unknown,
                "style_matching",
                "Style matching failed: " + std::string(e.what())
            ));
        }
    }).detach();
    
    return mixmind::core::AsyncResult<StyleMatchingResult>(std::move(future));
}

mixmind::core::AsyncResult<std::string> RealOpenAIService::chatWithAssistant(
    const std::string& message,
    const std::string& sessionId) {
    
    auto promise = std::make_shared<std::promise<mixmind::core::Result<std::string>>>();
    auto future = promise->get_future();
    
    // Build conversation context
    AIRequest request;
    request.model = pImpl_->defaultModel;
    request.taskType = AITaskType::CREATIVE_ASSISTANCE;
    request.maxTokens = 800;
    request.temperature = 0.7f;
    request.systemPrompt = "You are MixMind AI, an expert music production assistant. You help with mixing, "
                         "mastering, sound design, plugin selection, creative decisions, and technical audio "
                         "questions. Provide helpful, practical advice while being conversational and encouraging.";
    
    // Add conversation history if session exists
    if (!sessionId.empty()) {
        auto sessionMessages = pImpl_->getSessionMessages(sessionId);
        
        std::stringstream contextStream;
        if (!sessionMessages.empty()) {
            contextStream << "Previous conversation:\n";
            for (const auto& [role, content] : sessionMessages) {
                contextStream << role << ": " << content << "\n";
            }
            contextStream << "\n";
        }
        contextStream << "User: " << message;
        
        request.prompt = contextStream.str();
        
        // Add to session
        pImpl_->addMessageToSession(sessionId, "user", message);
    } else {
        request.prompt = message;
    }
    
    auto aiResponseFuture = sendRequest(request);
    
    std::thread([promise, aiResponseFuture = std::move(aiResponseFuture), sessionId, this]() mutable {
        try {
            auto aiResult = aiResponseFuture.get();
            
            if (!aiResult.isSuccess()) {
                promise->set_value(mixmind::core::Result<std::string>::error(
                    mixmind::core::ErrorCode::Unknown,
                    "chat_assistant",
                    aiResult.getErrorMessage()
                ));
                return;
            }
            
            auto& aiResponse = aiResult.value();
            
            // Add assistant response to session
            if (!sessionId.empty()) {
                pImpl_->addMessageToSession(sessionId, "assistant", aiResponse.content);
            }
            
            promise->set_value(mixmind::core::Result<std::string>::success(aiResponse.content));
            
        } catch (const std::exception& e) {
            promise->set_value(mixmind::core::Result<std::string>::error(
                mixmind::core::ErrorCode::Unknown,
                "chat_assistant",
                "Chat failed: " + std::string(e.what())
            ));
        }
    }).detach();
    
    return mixmind::core::AsyncResult<std::string>(std::move(future));
}

void RealOpenAIService::addContextToSession(const std::string& sessionId, const std::string& context) {
    std::lock_guard<std::mutex> lock(pImpl_->sessionMutex);
    pImpl_->sessions[sessionId].context += context + "\n";
}

void RealOpenAIService::clearSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(pImpl_->sessionMutex);
    pImpl_->sessions.erase(sessionId);
}

bool RealOpenAIService::isInitialized() const {
    return !pImpl_->apiKey.empty() && pImpl_->isOnline.load();
}

bool RealOpenAIService::isOnline() const {
    return pImpl_->isOnline.load();
}

int RealOpenAIService::getQueuedRequestsCount() const {
    return pImpl_->queuedRequests.load();
}

double RealOpenAIService::getAverageResponseTime() const {
    return pImpl_->averageResponseTime.load();
}

int RealOpenAIService::getTotalRequestsToday() const {
    return pImpl_->requestsToday.load();
}

double RealOpenAIService::getTotalCostToday() const {
    return pImpl_->totalCostToday.load();
}

bool RealOpenAIService::canMakeRequest() const {
    return isInitialized() && pImpl_->queuedRequests.load() < pImpl_->maxConcurrentRequests * 2;
}

double RealOpenAIService::estimateTokenCost(const AIRequest& request) const {
    // Rough estimation: 4 characters per token
    int estimatedTokens = static_cast<int>(request.prompt.length() / 4) + request.maxTokens;
    return pImpl_->calculateTokenCost(request.model, estimatedTokens / 2, estimatedTokens / 2);
}

int RealOpenAIService::getRemainingQuota() const {
    // This would need to be implemented with actual quota tracking from OpenAI
    return 1000000; // Placeholder - would query actual quota
}

} // namespace mixmind::services