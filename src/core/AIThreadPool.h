#pragma once

#include "result.h"
#include "async.h"
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <variant>
#include <memory>

// Third-party lock-free queue for maximum performance
#include <concurrentqueue.h>

namespace mixmind::core {

// ============================================================================
// AI Thread Pool - CRITICAL AUDIO THREAD SAFETY
// Ensures AI processing NEVER blocks real-time audio
// ============================================================================

enum class TaskPriority : int {
    LOW = 0,      // Background tasks
    NORMAL = 1,   // Regular AI requests
    HIGH = 2,     // Voice commands
    CRITICAL = 3  // Emergency tasks
};

struct AITask {
    std::function<void()> task;
    TaskPriority priority;
    std::chrono::steady_clock::time_point timestamp;
    
    AITask(std::function<void()> t, TaskPriority p = TaskPriority::NORMAL)
        : task(std::move(t)), priority(p), timestamp(std::chrono::steady_clock::now()) {}
        
    // Priority queue ordering
    bool operator<(const AITask& other) const {
        if (priority != other.priority) {
            return priority < other.priority; // Higher priority first
        }
        return timestamp > other.timestamp; // Earlier tasks first
    }
};

class AIThreadPool {
public:
    AIThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~AIThreadPool();
    
    // Non-copyable
    AIThreadPool(const AIThreadPool&) = delete;
    AIThreadPool& operator=(const AIThreadPool&) = delete;
    
    // Enqueue AI task (NEVER blocks audio thread)
    template<typename F>
    void enqueueTask(F&& task, TaskPriority priority = TaskPriority::NORMAL) {
        if (shutdown_.load()) {
            return; // Don't accept new tasks during shutdown
        }
        
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            tasks_.emplace(std::forward<F>(task), priority);
        }
        condition_.notify_one();
        
        // Update metrics
        tasksEnqueued_.fetch_add(1);
    }
    
    // Enqueue with callback for results
    template<typename F, typename Callback>
    void enqueueTaskWithCallback(F&& task, Callback&& callback, TaskPriority priority = TaskPriority::NORMAL) {
        enqueueTask([task = std::forward<F>(task), callback = std::forward<Callback>(callback)]() {
            try {
                auto result = task();
                callback(result);
            } catch (const std::exception& e) {
                // Log error but don't crash
                logError("AI task failed: " + std::string(e.what()));
            }
        }, priority);
    }
    
    // Get performance metrics
    struct Metrics {
        size_t tasksEnqueued = 0;
        size_t tasksCompleted = 0;
        size_t tasksInQueue = 0;
        size_t activeThreads = 0;
        std::chrono::milliseconds avgExecutionTime{0};
    };
    
    Metrics getMetrics() const;
    
    // Emergency shutdown
    void shutdown();
    
    // Check if healthy
    bool isHealthy() const {
        return !shutdown_.load() && getMetrics().tasksInQueue < maxQueueSize_;
    }
    
private:
    void workerThread();
    void logError(const std::string& message);
    
    std::vector<std::thread> workers_;
    std::priority_queue<AITask> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> tasksEnqueued_{0};
    std::atomic<size_t> tasksCompleted_{0};
    
    static constexpr size_t maxQueueSize_ = 1000;
};

// ============================================================================
// Lock-Free Audio <-> AI Communication
// Uses lock-free queues for zero-latency communication
// ============================================================================

enum class AICommandType {
    VOICE_COMMAND,
    CHAT_REQUEST,
    PARAMETER_CHANGE,
    PLUGIN_REQUEST,
    MIXING_SUGGESTION
};

struct AICommand {
    AICommandType type;
    std::string payload;
    std::string sessionId;
    std::chrono::steady_clock::time_point timestamp;
    
    // Response callback (called on AI thread)
    std::function<void(const std::string&)> responseCallback;
    
    AICommand(AICommandType t, std::string p, std::string sid = "")
        : type(t), payload(std::move(p)), sessionId(std::move(sid))
        , timestamp(std::chrono::steady_clock::now()) {}
};

class AudioSafeAIInterface {
public:
    AudioSafeAIInterface(AIThreadPool& threadPool);
    ~AudioSafeAIInterface();
    
    // Called from audio thread (MUST be lock-free and fast)
    bool enqueueAICommand(AICommand command);
    
    // Called from audio thread (MUST be lock-free and fast)
    bool pollAIResponse(std::string& response, std::string& sessionId);
    
    // Called from AI threads
    void sendResponseToAudio(const std::string& response, const std::string& sessionId);
    
    // Performance monitoring
    struct AudioThreadMetrics {
        std::atomic<uint64_t> commandsEnqueued{0};
        std::atomic<uint64_t> responsesReceived{0};
        std::atomic<uint64_t> queueOverflows{0};
        std::atomic<double> avgEnqueueLatency{0.0}; // nanoseconds
    };
    
    const AudioThreadMetrics& getAudioMetrics() const { return audioMetrics_; }
    
private:
    AIThreadPool& threadPool_;
    
    // Lock-free queues for real-time safety
    moodycamel::ConcurrentQueue<AICommand> commandQueue_;
    moodycamel::ConcurrentQueue<std::pair<std::string, std::string>> responseQueue_;
    
    AudioThreadMetrics audioMetrics_;
    
    static constexpr size_t MAX_QUEUE_SIZE = 1024;
};

// ============================================================================
// Real-Time Safe AI Processing Manager
// Coordinates all AI operations without blocking audio
// ============================================================================

class RealTimeAIManager {
public:
    RealTimeAIManager();
    ~RealTimeAIManager();
    
    // Initialize with thread pool
    bool initialize(size_t aiThreads = 4);
    void shutdown();
    
    // Audio-thread safe operations
    void processVoiceCommand(const std::string& command, 
                           std::function<void(const std::string&)> callback = nullptr);
    
    void processChatRequest(const std::string& message,
                          std::function<void(const std::string&)> callback = nullptr);
    
    void requestMixingSuggestion(const std::string& context,
                               std::function<void(const std::string&)> callback = nullptr);
    
    // Called from audio thread every buffer
    void processAudioThreadUpdates();
    
    // Performance monitoring
    struct PerformanceStats {
        double audioThreadLatency = 0.0;    // microseconds
        double aiProcessingTime = 0.0;      // milliseconds
        size_t pendingRequests = 0;
        size_t completedRequests = 0;
        bool isHealthy = true;
    };
    
    PerformanceStats getPerformanceStats() const;
    
    // Health check
    bool isHealthy() const;
    
private:
    void processAICommand(const AICommand& command);
    
    std::unique_ptr<AIThreadPool> threadPool_;
    std::unique_ptr<AudioSafeAIInterface> audioInterface_;
    
    std::atomic<bool> initialized_{false};
    
    // Performance tracking
    mutable std::mutex statsMutex_;
    PerformanceStats stats_;
};

} // namespace mixmind::core