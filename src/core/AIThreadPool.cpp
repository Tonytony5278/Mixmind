#include "AIThreadPool.h"
#include <iostream>
#include <chrono>
#include <algorithm>

namespace mixmind::core {

// ============================================================================
// AIThreadPool Implementation
// ============================================================================

AIThreadPool::AIThreadPool(size_t numThreads) {
    // Ensure we have at least 2 threads for AI processing
    numThreads = std::max(numThreads, static_cast<size_t>(2));
    
    // Create worker threads
    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&AIThreadPool::workerThread, this);
    }
    
    std::cout << "AIThreadPool initialized with " << numThreads << " threads" << std::endl;
}

AIThreadPool::~AIThreadPool() {
    shutdown();
}

void AIThreadPool::shutdown() {
    // Signal shutdown
    shutdown_.store(true);
    
    // Wake up all threads
    condition_.notify_all();
    
    // Join all threads
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers_.clear();
}

void AIThreadPool::workerThread() {
    while (!shutdown_.load()) {
        AITask task([]{}, TaskPriority::LOW);
        
        // Wait for task or shutdown
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this] { 
                return shutdown_.load() || !tasks_.empty(); 
            });
            
            if (shutdown_.load() && tasks_.empty()) {
                break;
            }
            
            if (!tasks_.empty()) {
                task = std::move(const_cast<AITask&>(tasks_.top()));
                tasks_.pop();
            }
        }
        
        // Execute task
        if (task.task) {
            auto start = std::chrono::high_resolution_clock::now();
            
            try {
                task.task();
                tasksCompleted_.fetch_add(1);
            } catch (const std::exception& e) {
                logError("AI task execution failed: " + std::string(e.what()));
            } catch (...) {
                logError("AI task execution failed with unknown exception");
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            // Log slow tasks
            if (duration.count() > 100000) { // > 100ms
                logError("Slow AI task detected: " + std::to_string(duration.count()) + " microseconds");
            }
        }
    }
}

AIThreadPool::Metrics AIThreadPool::getMetrics() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    
    Metrics metrics;
    metrics.tasksEnqueued = tasksEnqueued_.load();
    metrics.tasksCompleted = tasksCompleted_.load();
    metrics.tasksInQueue = tasks_.size();
    metrics.activeThreads = workers_.size();
    
    return metrics;
}

void AIThreadPool::logError(const std::string& message) {
    std::cerr << "[AIThreadPool ERROR] " << message << std::endl;
    // TODO: Integrate with proper logging system
}

// ============================================================================
// AudioSafeAIInterface Implementation  
// ============================================================================

AudioSafeAIInterface::AudioSafeAIInterface(AIThreadPool& threadPool) 
    : threadPool_(threadPool)
    , commandQueue_(MAX_QUEUE_SIZE)
    , responseQueue_(MAX_QUEUE_SIZE) {
}

AudioSafeAIInterface::~AudioSafeAIInterface() = default;

bool AudioSafeAIInterface::enqueueAICommand(AICommand command) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Attempt to enqueue (non-blocking)
    bool success = commandQueue_.try_enqueue(std::move(command));
    
    auto end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    if (success) {
        audioMetrics_.commandsEnqueued.fetch_add(1);
        
        // Update rolling average latency
        double currentAvg = audioMetrics_.avgEnqueueLatency.load();
        audioMetrics_.avgEnqueueLatency.store((currentAvg * 0.95) + (latency * 0.05));
    } else {
        audioMetrics_.queueOverflows.fetch_add(1);
    }
    
    return success;
}

bool AudioSafeAIInterface::pollAIResponse(std::string& response, std::string& sessionId) {
    std::pair<std::string, std::string> responsePair;
    
    if (responseQueue_.try_dequeue(responsePair)) {
        response = std::move(responsePair.first);
        sessionId = std::move(responsePair.second);
        audioMetrics_.responsesReceived.fetch_add(1);
        return true;
    }
    
    return false;
}

void AudioSafeAIInterface::sendResponseToAudio(const std::string& response, const std::string& sessionId) {
    // Non-blocking enqueue from AI thread
    responseQueue_.try_enqueue(std::make_pair(response, sessionId));
}

// ============================================================================
// RealTimeAIManager Implementation
// ============================================================================

RealTimeAIManager::RealTimeAIManager() = default;

RealTimeAIManager::~RealTimeAIManager() {
    shutdown();
}

bool RealTimeAIManager::initialize(size_t aiThreads) {
    if (initialized_.load()) {
        return true; // Already initialized
    }
    
    try {
        // Create AI thread pool
        threadPool_ = std::make_unique<AIThreadPool>(aiThreads);
        
        // Create audio-safe interface
        audioInterface_ = std::make_unique<AudioSafeAIInterface>(*threadPool_);
        
        initialized_.store(true);
        
        std::cout << "RealTimeAIManager initialized with " << aiThreads << " AI threads" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize RealTimeAIManager: " << e.what() << std::endl;
        return false;
    }
}

void RealTimeAIManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    initialized_.store(false);
    
    // Shutdown components in reverse order
    audioInterface_.reset();
    threadPool_.reset();
}

void RealTimeAIManager::processVoiceCommand(const std::string& command, 
                                          std::function<void(const std::string&)> callback) {
    if (!initialized_.load()) {
        if (callback) callback("AI system not initialized");
        return;
    }
    
    AICommand aiCommand(AICommandType::VOICE_COMMAND, command);
    
    if (callback) {
        aiCommand.responseCallback = callback;
    }
    
    // This is lock-free and safe to call from audio thread
    bool success = audioInterface_->enqueueAICommand(std::move(aiCommand));
    
    if (!success && callback) {
        callback("AI system overloaded - command dropped");
    }
}

void RealTimeAIManager::processChatRequest(const std::string& message,
                                         std::function<void(const std::string&)> callback) {
    if (!initialized_.load()) {
        if (callback) callback("AI system not initialized");
        return;
    }
    
    AICommand aiCommand(AICommandType::CHAT_REQUEST, message);
    
    if (callback) {
        aiCommand.responseCallback = callback;
    }
    
    bool success = audioInterface_->enqueueAICommand(std::move(aiCommand));
    
    if (!success && callback) {
        callback("AI system overloaded - request dropped");
    }
}

void RealTimeAIManager::requestMixingSuggestion(const std::string& context,
                                              std::function<void(const std::string&)> callback) {
    if (!initialized_.load()) {
        if (callback) callback("AI system not initialized");
        return;
    }
    
    AICommand aiCommand(AICommandType::MIXING_SUGGESTION, context);
    
    if (callback) {
        aiCommand.responseCallback = callback;
    }
    
    bool success = audioInterface_->enqueueAICommand(std::move(aiCommand));
    
    if (!success && callback) {
        callback("AI system overloaded - suggestion request dropped");
    }
}

void RealTimeAIManager::processAudioThreadUpdates() {
    if (!initialized_.load()) {
        return;
    }
    
    // Poll for AI responses (lock-free, fast)
    std::string response, sessionId;
    while (audioInterface_->pollAIResponse(response, sessionId)) {
        // Process response on audio thread
        // This should be very fast - just trigger UI updates or parameter changes
        
        // TODO: Route response to appropriate handler based on sessionId
        // For now, just update stats
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.completedRequests++;
        }
    }
    
    // Dequeue and process AI commands on thread pool
    AICommand command(AICommandType::VOICE_COMMAND, "");
    // Note: We would typically process commands here, but since this is called
    // from audio thread, we just poll responses. Command processing happens
    // in the thread pool automatically.
}

void RealTimeAIManager::processAICommand(const AICommand& command) {
    // This runs on AI thread pool, not audio thread
    
    try {
        std::string response;
        
        switch (command.type) {
            case AICommandType::VOICE_COMMAND:
                response = "Processing voice command: " + command.payload;
                // TODO: Integrate with actual voice processing
                break;
                
            case AICommandType::CHAT_REQUEST:
                response = "Processing chat: " + command.payload;
                // TODO: Integrate with OpenAI API
                break;
                
            case AICommandType::MIXING_SUGGESTION:
                response = "Mixing suggestion for: " + command.payload;
                // TODO: Integrate with mixing AI
                break;
                
            default:
                response = "Unknown command type";
        }
        
        // Send response back to audio thread
        if (command.responseCallback) {
            command.responseCallback(response);
        }
        
        audioInterface_->sendResponseToAudio(response, command.sessionId);
        
    } catch (const std::exception& e) {
        std::string errorResponse = "AI processing error: " + std::string(e.what());
        
        if (command.responseCallback) {
            command.responseCallback(errorResponse);
        }
        
        audioInterface_->sendResponseToAudio(errorResponse, command.sessionId);
    }
}

RealTimeAIManager::PerformanceStats RealTimeAIManager::getPerformanceStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    PerformanceStats stats = stats_;
    
    if (threadPool_) {
        auto metrics = threadPool_->getMetrics();
        stats.pendingRequests = metrics.tasksInQueue;
        stats.isHealthy = threadPool_->isHealthy();
    }
    
    return stats;
}

bool RealTimeAIManager::isHealthy() const {
    if (!initialized_.load()) {
        return false;
    }
    
    auto stats = getPerformanceStats();
    
    // Health criteria
    bool lowLatency = stats.audioThreadLatency < 1000.0; // < 1ms
    bool reasonable_queue = stats.pendingRequests < 100;
    bool threads_healthy = threadPool_ ? threadPool_->isHealthy() : false;
    
    return lowLatency && reasonable_queue && threads_healthy;
}

} // namespace mixmind::core