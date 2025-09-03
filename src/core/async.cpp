#include "async.h"
#include <queue>
#include <condition_variable>
#include <atomic>

namespace mixmind::core {

// ============================================================================
// ThreadPool Implementation
// ============================================================================

class ThreadPool::ThreadPoolImpl {
public:
    explicit ThreadPoolImpl(size_t numThreads) : shutdown_(false) {
        workers_.reserve(numThreads);
        
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                workerLoop();
            });
        }
    }
    
    ~ThreadPoolImpl() {
        shutdown();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    template<typename Func>
    void enqueue(Func&& task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            if (shutdown_) {
                throw std::runtime_error("ThreadPool is shut down");
            }
            
            tasks_.emplace(std::forward<Func>(task));
        }
        
        condition_.notify_one();
    }
    
    size_t getThreadCount() const {
        return workers_.size();
    }
    
    size_t getPendingTaskCount() const {
        std::unique_lock<std::mutex> lock(queueMutex_);
        return tasks_.size();
    }
    
    void waitForAll() {
        std::unique_lock<std::mutex> lock(queueMutex_);
        finishCondition_.wait(lock, [this] {
            return tasks_.empty() && activeTasks_ == 0;
        });
    }
    
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            shutdown_ = true;
        }
        
        condition_.notify_all();
    }
    
private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                condition_.wait(lock, [this] {
                    return shutdown_ || !tasks_.empty();
                });
                
                if (shutdown_ && tasks_.empty()) {
                    return;
                }
                
                task = std::move(tasks_.front());
                tasks_.pop();
                activeTasks_++;
            }
            
            // Execute the task
            try {
                task();
            } catch (...) {
                // Swallow exceptions to prevent worker thread termination
            }
            
            // Mark task as completed
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                activeTasks_--;
                if (tasks_.empty() && activeTasks_ == 0) {
                    finishCondition_.notify_all();
                }
            }
        }
    }
    
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::condition_variable finishCondition_;
    
    std::atomic<bool> shutdown_;
    std::atomic<size_t> activeTasks_{0};
};

ThreadPool::ThreadPool(size_t numThreads) 
    : impl_(std::make_unique<ThreadPoolImpl>(numThreads)) 
{
}

ThreadPool::~ThreadPool() = default;

template<typename T, typename Func>
AsyncResult<T> ThreadPool::executeAsync(Func&& operation, const std::string& description) {
    auto promise = std::make_shared<std::promise<Result<T>>>();
    auto future = promise->get_future();
    
    impl_->enqueue([promise, operation = std::forward<Func>(operation), description]() {
        try {
            auto result = operation();
            promise->set_value(std::move(result));
        } catch (const std::exception& e) {
            promise->set_value(Result<T>::failure(
                std::string("ThreadPool operation failed") + 
                (description.empty() ? "" : " (" + description + ")") +
                ": " + e.what()
            ));
        } catch (...) {
            promise->set_value(Result<T>::failure(
                std::string("ThreadPool operation failed with unknown error") +
                (description.empty() ? "" : " (" + description + ")")
            ));
        }
    });
    
    return AsyncResult<T>(std::move(future));
}

template<typename Func>
AsyncResult<void> ThreadPool::executeAsyncVoid(Func&& operation, const std::string& description) {
    auto promise = std::make_shared<std::promise<VoidResult>>();
    auto future = promise->get_future();
    
    impl_->enqueue([promise, operation = std::forward<Func>(operation), description]() {
        try {
            auto result = operation();
            promise->set_value(std::move(result));
        } catch (const std::exception& e) {
            promise->set_value(VoidResult::failure(
                std::string("ThreadPool operation failed") + 
                (description.empty() ? "" : " (" + description + ")") +
                ": " + e.what()
            ));
        } catch (...) {
            promise->set_value(VoidResult::failure(
                std::string("ThreadPool operation failed with unknown error") +
                (description.empty() ? "" : " (" + description + ")")
            ));
        }
    });
    
    return AsyncResult<void>(std::move(future));
}

size_t ThreadPool::getThreadCount() const {
    return impl_->getThreadCount();
}

size_t ThreadPool::getPendingTaskCount() const {
    return impl_->getPendingTaskCount();
}

void ThreadPool::waitForAll() {
    impl_->waitForAll();
}

void ThreadPool::shutdown() {
    impl_->shutdown();
}

// ============================================================================
// Global ThreadPool Instance
// ============================================================================

ThreadPool& getGlobalThreadPool() {
    static ThreadPool instance(std::thread::hardware_concurrency());
    return instance;
}

// Note: CancellationToken is defined in result.h, no implementation needed here

// ============================================================================
// Explicit Template Instantiations
// ============================================================================

// Common instantiations to reduce compile times
// Note: These will be provided when the templates are actually used

} // namespace mixmind::core