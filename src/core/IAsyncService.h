#pragma once

#include "types.h"
#include "result.h"
#include <vector>
#include <memory>
#include <functional>
#include <future>
#include <chrono>
#include <queue>

namespace mixmind::core {

// ============================================================================
// Async Service Interface - Asynchronous operation management
// ============================================================================

class IAsyncService {
public:
    virtual ~IAsyncService() = default;
    
    // ========================================================================
    // Service Initialization and Configuration
    // ========================================================================
    
    /// Initialize async service
    virtual AsyncResult<VoidResult> initialize() = 0;
    
    /// Shutdown async service (wait for completion)
    virtual AsyncResult<VoidResult> shutdown() = 0;
    
    /// Shutdown with timeout (force terminate after timeout)
    virtual AsyncResult<VoidResult> shutdownWithTimeout(std::chrono::milliseconds timeout) = 0;
    
    /// Check if service is initialized
    virtual bool isInitialized() const = 0;
    
    /// Check if service is shutting down
    virtual bool isShuttingDown() const = 0;
    
    // ========================================================================
    // Task Management
    // ========================================================================
    
    using TaskID = StrongID<struct TaskTag>;
    
    enum class TaskPriority {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3,
        Realtime = 4
    };
    
    enum class TaskStatus {
        Pending,        // Queued but not started
        Running,        // Currently executing
        Completed,      // Finished successfully
        Failed,         // Finished with error
        Cancelled,      // Cancelled by user/system
        Timeout         // Exceeded time limit
    };
    
    struct TaskInfo {
        TaskID id;
        std::string description;
        TaskPriority priority = TaskPriority::Normal;
        TaskStatus status = TaskStatus::Pending;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point started;
        std::chrono::system_clock::time_point completed;
        std::chrono::milliseconds timeout{0};  // 0 = no timeout
        float progress = 0.0f;                 // 0.0-1.0
        std::string statusMessage;
        std::string errorMessage;
        size_t retryCount = 0;
        size_t maxRetries = 0;
        std::vector<std::string> tags;         // For categorization
    };
    
    // ========================================================================
    // Task Execution
    // ========================================================================
    
    /// Execute task asynchronously
    template<typename T>
    AsyncResult<Result<T>> executeAsync(
        std::function<Result<T>()> task,
        const std::string& description = "",
        TaskPriority priority = TaskPriority::Normal,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
    );
    
    /// Execute task with progress reporting
    template<typename T>
    AsyncResult<Result<T>> executeAsyncWithProgress(
        std::function<Result<T>(ProgressCallback)> task,
        const std::string& description = "",
        TaskPriority priority = TaskPriority::Normal,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
    );
    
    /// Execute void task asynchronously
    virtual AsyncResult<VoidResult> executeAsyncVoid(
        std::function<VoidResult()> task,
        const std::string& description = "",
        TaskPriority priority = TaskPriority::Normal,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
    ) = 0;
    
    /// Schedule task for later execution
    virtual TaskID scheduleTask(
        std::function<VoidResult()> task,
        std::chrono::system_clock::time_point when,
        const std::string& description = "",
        TaskPriority priority = TaskPriority::Normal
    ) = 0;
    
    /// Schedule recurring task
    virtual TaskID scheduleRecurringTask(
        std::function<VoidResult()> task,
        std::chrono::milliseconds interval,
        const std::string& description = "",
        TaskPriority priority = TaskPriority::Normal,
        int32_t maxExecutions = -1  // -1 = infinite
    ) = 0;
    
    // ========================================================================
    // Task Control
    // ========================================================================
    
    /// Cancel task
    virtual AsyncResult<VoidResult> cancelTask(TaskID taskId) = 0;
    
    /// Cancel all tasks with specific tag
    virtual AsyncResult<Result<int32_t>> cancelTasksWithTag(const std::string& tag) = 0;
    
    /// Cancel all tasks with priority
    virtual AsyncResult<Result<int32_t>> cancelTasksWithPriority(TaskPriority priority) = 0;
    
    /// Cancel all pending tasks
    virtual AsyncResult<Result<int32_t>> cancelAllPendingTasks() = 0;
    
    /// Wait for task completion
    virtual AsyncResult<VoidResult> waitForTask(TaskID taskId) = 0;
    
    /// Wait for multiple tasks
    virtual AsyncResult<VoidResult> waitForTasks(const std::vector<TaskID>& taskIds) = 0;
    
    /// Wait for all tasks to complete
    virtual AsyncResult<VoidResult> waitForAllTasks() = 0;
    
    // ========================================================================
    // Task Information and Status
    // ========================================================================
    
    /// Get task information
    virtual std::optional<TaskInfo> getTaskInfo(TaskID taskId) const = 0;
    
    /// Get all active tasks
    virtual std::vector<TaskInfo> getActiveTasks() const = 0;
    
    /// Get tasks by status
    virtual std::vector<TaskInfo> getTasksByStatus(TaskStatus status) const = 0;
    
    /// Get tasks by priority
    virtual std::vector<TaskInfo> getTasksByPriority(TaskPriority priority) const = 0;
    
    /// Get tasks by tag
    virtual std::vector<TaskInfo> getTasksByTag(const std::string& tag) const = 0;
    
    /// Get task count
    virtual int32_t getTaskCount() const = 0;
    
    /// Get task count by status
    virtual std::unordered_map<TaskStatus, int32_t> getTaskCountByStatus() const = 0;
    
    /// Get running task count
    virtual int32_t getRunningTaskCount() const = 0;
    
    /// Get pending task count
    virtual int32_t getPendingTaskCount() const = 0;
    
    // ========================================================================
    // Batch Operations
    // ========================================================================
    
    struct BatchConfig {
        std::string description;
        TaskPriority priority = TaskPriority::Normal;
        int32_t maxConcurrentTasks = 1;    // Max tasks running simultaneously
        bool stopOnFirstError = false;     // Stop batch if any task fails
        std::chrono::milliseconds timeout{0}; // Timeout for entire batch
        std::vector<std::string> tags;
    };
    
    /// Execute batch of void tasks
    virtual AsyncResult<Result<std::vector<VoidResult>>> executeBatch(
        const std::vector<std::function<VoidResult()>>& tasks,
        const BatchConfig& config = {}
    ) = 0;
    
    /// Execute batch with progress tracking
    virtual AsyncResult<Result<std::vector<VoidResult>>> executeBatchWithProgress(
        const std::vector<std::function<VoidResult(ProgressCallback)>>& tasks,
        const BatchConfig& config,
        ProgressCallback overallProgress = nullptr
    ) = 0;
    
    // ========================================================================
    // Thread Pool Management
    // ========================================================================
    
    /// Set thread pool size
    virtual VoidResult setThreadPoolSize(int32_t threadCount) = 0;
    
    /// Get thread pool size
    virtual int32_t getThreadPoolSize() const = 0;
    
    /// Get optimal thread count for system
    virtual int32_t getOptimalThreadCount() const = 0;
    
    /// Set thread priority
    enum class ThreadPriority {
        Low,
        Normal,
        High,
        Realtime
    };
    
    virtual VoidResult setThreadPriority(ThreadPriority priority) = 0;
    
    /// Get thread priority
    virtual ThreadPriority getThreadPriority() const = 0;
    
    /// Get thread pool statistics
    struct ThreadPoolStats {
        int32_t totalThreads = 0;
        int32_t activeThreads = 0;
        int32_t idleThreads = 0;
        int32_t queuedTasks = 0;
        double averageTaskDuration = 0.0;  // seconds
        int64_t totalTasksExecuted = 0;
        int64_t totalTasksFailed = 0;
        std::chrono::system_clock::time_point statsStartTime;
    };
    
    virtual ThreadPoolStats getThreadPoolStats() const = 0;
    
    /// Reset thread pool statistics
    virtual VoidResult resetThreadPoolStats() = 0;
    
    // ========================================================================
    // Queue Management
    // ========================================================================
    
    /// Get queue size
    virtual int32_t getQueueSize() const = 0;
    
    /// Get queue capacity
    virtual int32_t getQueueCapacity() const = 0;
    
    /// Set queue capacity
    virtual VoidResult setQueueCapacity(int32_t capacity) = 0;
    
    /// Check if queue is full
    virtual bool isQueueFull() const = 0;
    
    /// Clear completed tasks from queue
    virtual VoidResult clearCompletedTasks() = 0;
    
    /// Clear failed tasks from queue
    virtual VoidResult clearFailedTasks() = 0;
    
    /// Clear all tasks from queue
    virtual AsyncResult<VoidResult> clearAllTasks() = 0;
    
    // ========================================================================
    // Error Handling and Retry Logic
    // ========================================================================
    
    struct RetryPolicy {
        size_t maxRetries = 3;
        std::chrono::milliseconds initialDelay{1000};
        float backoffMultiplier = 2.0f;        // Exponential backoff
        std::chrono::milliseconds maxDelay{30000};
        std::function<bool(const std::string&)> shouldRetry;  // Custom retry condition
    };
    
    /// Set default retry policy
    virtual VoidResult setDefaultRetryPolicy(const RetryPolicy& policy) = 0;
    
    /// Get default retry policy
    virtual RetryPolicy getDefaultRetryPolicy() const = 0;
    
    /// Execute task with retry policy
    virtual AsyncResult<VoidResult> executeWithRetry(
        std::function<VoidResult()> task,
        const RetryPolicy& retryPolicy,
        const std::string& description = "",
        TaskPriority priority = TaskPriority::Normal
    ) = 0;
    
    /// Get failed tasks (for manual retry)
    virtual std::vector<TaskInfo> getFailedTasks() const = 0;
    
    /// Retry specific task
    virtual AsyncResult<VoidResult> retryTask(TaskID taskId) = 0;
    
    /// Retry all failed tasks
    virtual AsyncResult<Result<int32_t>> retryAllFailedTasks() = 0;
    
    // ========================================================================
    // Resource Management
    // ========================================================================
    
    /// Set memory limit for async operations
    virtual VoidResult setMemoryLimit(size_t limitBytes) = 0;
    
    /// Get memory limit
    virtual size_t getMemoryLimit() const = 0;
    
    /// Get current memory usage
    virtual size_t getCurrentMemoryUsage() const = 0;
    
    /// Check if near memory limit
    virtual bool isNearMemoryLimit(float threshold = 0.9f) const = 0;
    
    /// Enable/disable memory monitoring
    virtual VoidResult setMemoryMonitoringEnabled(bool enabled) = 0;
    
    /// Check if memory monitoring is enabled
    virtual bool isMemoryMonitoringEnabled() const = 0;
    
    // ========================================================================
    // Background Processing
    // ========================================================================
    
    /// Submit background task (low priority, non-blocking)
    virtual TaskID submitBackgroundTask(
        std::function<VoidResult()> task,
        const std::string& description = ""
    ) = 0;
    
    /// Enable/disable background processing
    virtual VoidResult setBackgroundProcessingEnabled(bool enabled) = 0;
    
    /// Check if background processing is enabled
    virtual bool isBackgroundProcessingEnabled() const = 0;
    
    /// Set maximum background tasks
    virtual VoidResult setMaxBackgroundTasks(int32_t maxTasks) = 0;
    
    /// Get maximum background tasks
    virtual int32_t getMaxBackgroundTasks() const = 0;
    
    /// Pause background processing
    virtual VoidResult pauseBackgroundProcessing() = 0;
    
    /// Resume background processing
    virtual VoidResult resumeBackgroundProcessing() = 0;
    
    /// Check if background processing is paused
    virtual bool isBackgroundProcessingPaused() const = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class AsyncEvent {
        TaskQueued,
        TaskStarted,
        TaskProgress,
        TaskCompleted,
        TaskFailed,
        TaskCancelled,
        TaskRetrying,
        QueueFull,
        MemoryLimitReached,
        ThreadPoolResized,
        ServiceShutdown
    };
    
    using AsyncEventCallback = std::function<void(AsyncEvent event, const std::optional<TaskID>& taskId, const std::string& details)>;
    
    /// Subscribe to async events
    virtual void addEventListener(AsyncEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(AsyncEventCallback callback) = 0;
    
    // ========================================================================
    // Performance Monitoring
    // ========================================================================
    
    struct PerformanceMetrics {
        double averageQueueWaitTime = 0.0;     // seconds
        double averageExecutionTime = 0.0;     // seconds
        double throughputTasksPerSecond = 0.0;
        float cpuUsagePercent = 0.0f;
        float memoryUsagePercent = 0.0f;
        int32_t peakQueueSize = 0;
        int32_t peakActiveThreads = 0;
        std::chrono::system_clock::time_point metricsStartTime;
    };
    
    /// Get performance metrics
    virtual PerformanceMetrics getPerformanceMetrics() const = 0;
    
    /// Reset performance metrics
    virtual VoidResult resetPerformanceMetrics() = 0;
    
    /// Enable/disable performance monitoring
    virtual VoidResult setPerformanceMonitoringEnabled(bool enabled) = 0;
    
    /// Check if performance monitoring is enabled
    virtual bool isPerformanceMonitoringEnabled() const = 0;
    
    // ========================================================================
    // Task Dependencies
    // ========================================================================
    
    /// Add task dependency (task B depends on task A)
    virtual VoidResult addTaskDependency(TaskID dependentTask, TaskID prerequisiteTask) = 0;
    
    /// Remove task dependency
    virtual VoidResult removeTaskDependency(TaskID dependentTask, TaskID prerequisiteTask) = 0;
    
    /// Get task dependencies
    virtual std::vector<TaskID> getTaskDependencies(TaskID taskId) const = 0;
    
    /// Get dependent tasks
    virtual std::vector<TaskID> getDependentTasks(TaskID taskId) const = 0;
    
    /// Check if task has dependencies
    virtual bool hasTaskDependencies(TaskID taskId) const = 0;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Set task affinity (pin task to specific thread)
    virtual VoidResult setTaskAffinity(TaskID taskId, int32_t threadIndex) = 0;
    
    /// Get task affinity
    virtual std::optional<int32_t> getTaskAffinity(TaskID taskId) const = 0;
    
    /// Enable/disable task profiling
    virtual VoidResult setTaskProfilingEnabled(bool enabled) = 0;
    
    /// Check if task profiling is enabled
    virtual bool isTaskProfilingEnabled() const = 0;
    
    /// Get task profile data
    virtual std::string getTaskProfileData(TaskID taskId) const = 0;
    
    /// Export task execution history
    virtual VoidResult exportTaskHistory(const std::string& filePath) const = 0;
    
    /// Import task templates
    virtual AsyncResult<VoidResult> importTaskTemplates(const std::string& filePath) = 0;
    
    /// Create task checkpoint (save current state)
    virtual VoidResult createCheckpoint(const std::string& checkpointName) = 0;
    
    /// Restore from checkpoint
    virtual AsyncResult<VoidResult> restoreFromCheckpoint(const std::string& checkpointName) = 0;
    
    /// Get available checkpoints
    virtual std::vector<std::string> getAvailableCheckpoints() const = 0;
    
    /// Delete checkpoint
    virtual VoidResult deleteCheckpoint(const std::string& checkpointName) = 0;
};

// Template implementations need to be in header for proper instantiation
template<typename T>
inline AsyncResult<Result<T>> IAsyncService::executeAsync(
    std::function<Result<T>()> task,
    const std::string& description,
    TaskPriority priority,
    std::chrono::milliseconds timeout) 
{
    // This is a pure interface, implementations will provide the actual logic
    // This is just a placeholder to satisfy the template declaration
    auto promise = std::make_shared<std::promise<Result<T>>>();
    auto future = promise->get_future();
    
    // The actual implementation would submit this task to the thread pool
    // and handle the execution asynchronously
    
    return AsyncResult<Result<T>>(std::move(future));
}

template<typename T>
inline AsyncResult<Result<T>> IAsyncService::executeAsyncWithProgress(
    std::function<Result<T>(ProgressCallback)> task,
    const std::string& description,
    TaskPriority priority,
    std::chrono::milliseconds timeout) 
{
    // This is a pure interface, implementations will provide the actual logic
    auto promise = std::make_shared<std::promise<Result<T>>>();
    auto future = promise->get_future();
    
    return AsyncResult<Result<T>>(std::move(future));
}

} // namespace mixmind::core