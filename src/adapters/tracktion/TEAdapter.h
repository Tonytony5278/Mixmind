#pragma once

#include "../../core/types.h"
#include "../../core/result.h"
#include "../../core/async.h"
#include "TEUtils.h"
#include <tracktion_engine/tracktion_engine.h>
#include <memory>
#include <mutex>

namespace mixmind::adapters::tracktion {

// ============================================================================
// Base Tracktion Engine Adapter - Common functionality for all TE adapters
// ============================================================================

class TEAdapter {
public:
    explicit TEAdapter(te::Engine& engine);
    virtual ~TEAdapter() = default;
    
    // Non-copyable, movable
    TEAdapter(const TEAdapter&) = delete;
    TEAdapter& operator=(const TEAdapter&) = delete;
    TEAdapter(TEAdapter&&) = default;
    TEAdapter& operator=(TEAdapter&&) = default;
    
protected:
    /// Get reference to Tracktion Engine
    te::Engine& getEngine() { return engine_; }
    const te::Engine& getEngine() const { return engine_; }
    
    /// Get async service for background operations
    std::shared_ptr<core::IAsyncService> getAsyncService() const;
    
    /// Convert TE error to our Result type
    template<typename T>
    core::Result<T> convertTEResult(const juce::Result& teResult, T&& value) const;
    
    /// Convert TE error to VoidResult
    core::VoidResult convertTEResult(const juce::Result& teResult) const;
    
    /// Execute operation with proper error handling
    template<typename T, typename Func>
    core::AsyncResult<core::Result<T>> executeAsync(
        Func&& operation,
        const std::string& description = ""
    ) const;
    
    /// Execute void operation with proper error handling
    template<typename Func>
    core::AsyncResult<core::VoidResult> executeAsyncVoid(
        Func&& operation,
        const std::string& description = ""
    ) const;
    
    /// Thread-safe property access
    template<typename T>
    T getProperty(std::function<T()> getter) const;
    
    /// Thread-safe property setting
    template<typename T>
    core::AsyncResult<core::VoidResult> setProperty(
        std::function<void(T)> setter,
        T value,
        const std::string& description = ""
    ) const;
    
    /// Register for TE engine callbacks
    void registerEngineCallback(std::function<void()> callback);
    
    /// Unregister engine callback
    void unregisterEngineCallback();
    
    /// Check if operation should be cancelled
    bool shouldCancel() const;
    
    /// Set cancellation flag
    void setCancellationFlag(bool cancel);
    
private:
    te::Engine& engine_;
    mutable std::recursive_mutex mutex_;
    std::atomic<bool> cancellationFlag_{false};
    std::function<void()> engineCallback_;
    std::shared_ptr<core::IAsyncService> asyncService_;
    
    void initializeAsyncService();
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T>
inline core::Result<T> TEAdapter::convertTEResult(const juce::Result& teResult, T&& value) const {
    if (teResult.wasOk()) {
        return core::Result<T>::success(std::forward<T>(value));
    } else {
        return core::Result<T>::error(
            core::ErrorCode::TracktionEngineError,
            teResult.getErrorMessage().toStdString()
        );
    }
}

template<typename T, typename Func>
inline core::AsyncResult<core::Result<T>> TEAdapter::executeAsync(
    Func&& operation,
    const std::string& description
) const {
    if (!asyncService_) {
        // Fallback to synchronous execution
        auto promise = std::make_shared<std::promise<core::Result<T>>>();
        auto future = promise->get_future();
        
        try {
            auto result = operation();
            promise->set_value(std::move(result));
        } catch (const std::exception& e) {
            promise->set_value(core::Result<T>::error(
                core::ErrorCode::UnknownError,
                e.what()
            ));
        }
        
        return core::AsyncResult<core::Result<T>>(std::move(future));
    }
    
    return asyncService_->executeAsync<T>(
        [this, operation = std::forward<Func>(operation)]() mutable -> core::Result<T> {
            if (shouldCancel()) {
                return core::Result<T>::error(
                    core::ErrorCode::OperationCancelled,
                    "Operation was cancelled"
                );
            }
            
            try {
                return operation();
            } catch (const std::exception& e) {
                return core::Result<T>::error(
                    core::ErrorCode::TracktionEngineError,
                    e.what()
                );
            }
        },
        description.empty() ? "TE Operation" : description,
        core::IAsyncService::TaskPriority::Normal
    );
}

template<typename Func>
inline core::AsyncResult<core::VoidResult> TEAdapter::executeAsyncVoid(
    Func&& operation,
    const std::string& description
) const {
    if (!asyncService_) {
        // Fallback to synchronous execution
        auto promise = std::make_shared<std::promise<core::VoidResult>>();
        auto future = promise->get_future();
        
        try {
            auto result = operation();
            promise->set_value(std::move(result));
        } catch (const std::exception& e) {
            promise->set_value(core::VoidResult::error(
                core::ErrorCode::UnknownError,
                e.what()
            ));
        }
        
        return core::AsyncResult<core::VoidResult>(std::move(future));
    }
    
    return asyncService_->executeAsyncVoid(
        [this, operation = std::forward<Func>(operation)]() mutable -> core::VoidResult {
            if (shouldCancel()) {
                return core::VoidResult::error(
                    core::ErrorCode::OperationCancelled,
                    "Operation was cancelled"
                );
            }
            
            try {
                return operation();
            } catch (const std::exception& e) {
                return core::VoidResult::error(
                    core::ErrorCode::TracktionEngineError,
                    e.what()
                );
            }
        },
        description.empty() ? "TE Operation" : description,
        core::IAsyncService::TaskPriority::Normal
    );
}

template<typename T>
inline T TEAdapter::getProperty(std::function<T()> getter) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return getter();
}

template<typename T>
inline core::AsyncResult<core::VoidResult> TEAdapter::setProperty(
    std::function<void(T)> setter,
    T value,
    const std::string& description
) const {
    return executeAsyncVoid([setter, value]() {
        setter(value);
        return core::VoidResult::success();
    }, description);
}

} // namespace mixmind::adapters::tracktion