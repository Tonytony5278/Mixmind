#include "TEAdapter.h"
#include "../../core/IAsyncService.h"
#include <juce_core/juce_core.h>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TEAdapter Implementation
// ============================================================================

TEAdapter::TEAdapter(te::Engine& engine) 
    : engine_(engine) 
{
    initializeAsyncService();
}

std::shared_ptr<core::IAsyncService> TEAdapter::getAsyncService() const {
    return asyncService_;
}

core::VoidResult TEAdapter::convertTEResult(const juce::Result& teResult) const {
    if (teResult.wasOk()) {
        return core::VoidResult::success();
    } else {
        return core::VoidResult::error(
            TETypeConverter::convertErrorCode(teResult),
            teResult.getErrorMessage().toStdString()
        );
    }
}

void TEAdapter::registerEngineCallback(std::function<void()> callback) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    engineCallback_ = std::move(callback);
    
    // Register with TE engine for relevant events
    // Note: Specific implementation depends on what events we need to monitor
    // This is a placeholder for the callback registration mechanism
}

void TEAdapter::unregisterEngineCallback() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    engineCallback_ = nullptr;
    
    // Unregister from TE engine events
}

bool TEAdapter::shouldCancel() const {
    return cancellationFlag_.load();
}

void TEAdapter::setCancellationFlag(bool cancel) {
    cancellationFlag_.store(cancel);
}

void TEAdapter::initializeAsyncService() {
    // In a full implementation, this would get the async service
    // from a service locator or dependency injection container
    // For now, this is a placeholder
    asyncService_ = nullptr;  // Will be set by the actual implementation
}

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T, typename Func>
core::AsyncResult<core::Result<T>> TEAdapter::executeAsync(
    Func&& operation,
    const std::string& description
) const {
    // Use the global async execution functions from core/async.h
    return core::executeAsync<T>(std::forward<Func>(operation), description);
}

template<typename Func>
core::AsyncResult<core::VoidResult> TEAdapter::executeAsyncVoid(
    Func&& operation,
    const std::string& description
) const {
    // Use the global async execution functions from core/async.h
    return core::executeAsyncVoid(std::forward<Func>(operation), description);
}

// Explicit template instantiations for common use cases
template core::AsyncResult<core::Result<core::TrackID>> TEAdapter::executeAsync<core::TrackID>(
    std::function<core::Result<core::TrackID>()>&&,
    const std::string&
) const;

template core::AsyncResult<core::VoidResult> TEAdapter::executeAsyncVoid(
    std::function<core::VoidResult()>&&,
    const std::string&
) const;

} // namespace mixmind::adapters::tracktion