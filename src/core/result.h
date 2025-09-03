#pragma once

#include "types.h"
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <functional>
#include <future>
#include <memory>

namespace mixmind::core {

// ============================================================================
// Error Codes and Categories
// ============================================================================

enum class ErrorCode {
    // Success
    Success = 0,
    
    // General errors
    Unknown = 1000,
    InvalidParameter,
    OutOfMemory,
    ResourceBusy,
    OperationCancelled,
    NotSupported,
    
    // File/IO errors
    FileNotFound = 2000,
    FileAccessDenied,
    FileCorrupted,
    DiskFull,
    NetworkError,
    
    // Audio errors
    AudioDeviceError = 3000,
    AudioFormatNotSupported,
    AudioLatencyTooHigh,
    AudioBufferUnderrun,
    AudioBufferOverrun,
    
    // Plugin errors  
    PluginLoadFailed = 4000,
    PluginNotFound,
    PluginIncompatible,
    PluginCrashed,
    PluginLicenseError,
    
    // Session errors
    SessionNotFound = 5000,
    SessionCorrupted,
    SessionVersionMismatch,
    TrackNotFound,
    ClipNotFound,
    
    // Render errors
    RenderFailed = 6000,
    RenderCancelled,
    RenderOutOfDisk,
    CodecError,
    
    // Collaboration errors
    NetworkTimeout = 7000,
    AuthenticationFailed,
    PermissionDenied,
    SyncConflict
};

struct ErrorCategory {
    static constexpr const char* general() { return "general"; }
    static constexpr const char* fileIO() { return "file_io"; }
    static constexpr const char* audio() { return "audio"; }
    static constexpr const char* plugin() { return "plugin"; }
    static constexpr const char* session() { return "session"; }
    static constexpr const char* render() { return "render"; }
    static constexpr const char* collaboration() { return "collaboration"; }
};

// ============================================================================
// Error Information
// ============================================================================

struct ErrorInfo {
    ErrorCode code = ErrorCode::Success;
    std::string category;
    std::string message;
    std::string context;
    std::vector<DiagnosticMessage> diagnostics;
    
    ErrorInfo() = default;
    
    ErrorInfo(ErrorCode c, std::string cat, std::string msg, std::string ctx = "")
        : code(c), category(std::move(cat)), message(std::move(msg)), context(std::move(ctx)) {}
    
    bool isSuccess() const { return code == ErrorCode::Success; }
    bool isError() const { return code != ErrorCode::Success; }
    
    void addDiagnostic(Severity severity, std::string diagCode, std::string diagMsg) {
        diagnostics.emplace_back(severity, std::move(diagCode), std::move(diagMsg), context);
    }
    
    std::string toString() const {
        std::string result = category + "::" + std::to_string(static_cast<int>(code)) + ": " + message;
        if (!context.empty()) {
            result += " [" + context + "]";
        }
        return result;
    }
};

// ============================================================================
// Result Type - Success/Error with optional value
// ============================================================================

template<typename T = void>
class Result {
public:
    // Default constructor (creates error state for std::future compatibility)
    Result() : data_(ErrorInfo{ErrorCode::Unknown, ErrorCategory::general(), "Uninitialized result"}) {}
    
    // Constructors for success cases
    template<typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    Result(U&& value) : data_(std::forward<U>(value)) {}
    
    template<typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
    explicit Result(bool) : data_(SuccessTag{}) {} // Explicit to avoid conflicts
    
    // Constructor for error case
    Result(ErrorInfo error) : data_(std::move(error)) {}
    
    // Factory methods
    template<typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    static Result success(U&& value) {
        return Result(std::forward<U>(value));
    }
    
    template<typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
    static Result success() {
        return Result(true); // Use the explicit bool constructor
    }
    
    static Result error(ErrorCode code, std::string category, std::string message, std::string context = "") {
        return Result(ErrorInfo{code, std::move(category), std::move(message), std::move(context)});
    }
    
    // Status checking
    bool isSuccess() const {
        return std::visit([](const auto& data) {
            using DataType = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<DataType, ErrorInfo>) {
                return false;
            } else {
                return true;
            }
        }, data_);
    }
    
    bool isError() const { return !isSuccess(); }
    
    explicit operator bool() const { return isSuccess(); }
    
    // Legacy API compatibility
    bool hasValue() const { return isSuccess(); }
    
    std::string getErrorMessage() const { 
        return isError() ? error().toString() : ""; 
    }
    
    // Value access (only for non-void types)
    template<typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    const U& value() const {
        if (isError()) {
            throw std::runtime_error("Attempting to access value of error Result: " + error().toString());
        }
        return std::get<T>(data_);
    }
    
    template<typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    U& value() {
        if (isError()) {
            throw std::runtime_error("Attempting to access value of error Result: " + error().toString());
        }
        return std::get<T>(data_);
    }
    
    template<typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
    const U& valueOr(const U& defaultValue) const {
        return isSuccess() ? std::get<T>(data_) : defaultValue;
    }
    
    // Error access
    const ErrorInfo& error() const {
        if (isSuccess()) {
            static ErrorInfo noError;
            return noError;
        }
        return std::get<ErrorInfo>(data_);
    }
    
    // Monadic operations
    template<typename F>
    auto then(F&& func) -> Result<std::invoke_result_t<F, T>> {
        if (isError()) {
            return Result<std::invoke_result_t<F, T>>(error());
        }
        
        if constexpr (std::is_void_v<T>) {
            return func();
        } else {
            return func(value());
        }
    }
    
    template<typename F>
    Result<T> onError(F&& func) {
        if (isError()) {
            func(error());
        }
        return *this;
    }

private:
    struct SuccessTag {};
    
    using DataType = std::conditional_t<
        std::is_void_v<T>,
        std::variant<SuccessTag, ErrorInfo>,
        std::variant<T, ErrorInfo>
    >;
    
    DataType data_;
};

// Convenience type aliases
using VoidResult = Result<void>;

// ============================================================================
// Diff Type - For transactional operations
// ============================================================================

enum class DiffAction {
    Create,
    Update, 
    Delete,
    Move
};

template<typename T>
struct DiffEntry {
    DiffAction action;
    std::string path;  // Hierarchical path (e.g., "session/track[0]/clip[1]")
    std::optional<T> oldValue;
    std::optional<T> newValue;
    
    DiffEntry(DiffAction act, std::string p) 
        : action(act), path(std::move(p)) {}
    
    DiffEntry(DiffAction act, std::string p, T old, T new_val)
        : action(act), path(std::move(p)), oldValue(std::move(old)), newValue(std::move(new_val)) {}
};

template<typename T>
using Diff = std::vector<DiffEntry<T>>;

// For mixed-type diffs, we use a variant
using MixedValue = std::variant<
    std::string,
    int32_t, 
    int64_t,
    float,
    double,
    bool
>;

using MixedDiff = Diff<MixedValue>;

// ============================================================================
// Transaction Support
// ============================================================================

class Transaction {
public:
    Transaction() = default;
    virtual ~Transaction() = default;
    
    // Non-copyable, movable
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = default;
    Transaction& operator=(Transaction&&) = default;
    
    virtual VoidResult commit() = 0;
    virtual VoidResult rollback() = 0;
    virtual bool canCommit() const = 0;
    virtual bool canRollback() const = 0;
    
    const MixedDiff& getDiff() const { return diff_; }
    
protected:
    MixedDiff diff_;
};

using TransactionPtr = std::unique_ptr<Transaction>;

// ============================================================================
// Async Service Support
// ============================================================================

// Progress reporting for long-running operations
struct ProgressInfo {
    float percentage = 0.0f;  // 0.0 to 1.0
    std::string currentTask;
    std::string details;
    bool cancellable = true;
    std::chrono::steady_clock::time_point startTime;
    std::optional<std::chrono::steady_clock::time_point> estimatedEndTime;
    
    ProgressInfo() : startTime(std::chrono::steady_clock::now()) {}
    
    void updateProgress(float pct, std::string task = "", std::string det = "") {
        percentage = std::clamp(pct, 0.0f, 1.0f);
        if (!task.empty()) currentTask = std::move(task);
        if (!det.empty()) details = std::move(det);
        
        // Update ETA based on current progress
        if (percentage > 0.0f && percentage < 1.0f) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = now - startTime;
            auto totalEstimated = std::chrono::duration_cast<std::chrono::steady_clock::duration>(elapsed / percentage);
            estimatedEndTime = startTime + totalEstimated;
        }
    }
    
    bool isComplete() const { return percentage >= 1.0f; }
    
    std::chrono::milliseconds getElapsedTime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
    }
    
    std::optional<std::chrono::milliseconds> getEstimatedRemainingTime() const {
        if (!estimatedEndTime) return std::nullopt;
        
        auto now = std::chrono::steady_clock::now();
        if (now >= *estimatedEndTime) return std::chrono::milliseconds(0);
        
        return std::chrono::duration_cast<std::chrono::milliseconds>(*estimatedEndTime - now);
    }
};

using ProgressCallback = std::function<void(const ProgressInfo&)>;

// Cancellation support
class CancellationToken {
public:
    CancellationToken() = default;
    
    void cancel() {
        cancelled_.store(true);
        if (onCancelled_) {
            onCancelled_();
        }
    }
    
    bool isCancelled() const {
        return cancelled_.load();
    }
    
    void throwIfCancelled() const {
        if (isCancelled()) {
            throw std::runtime_error("Operation was cancelled");
        }
    }
    
    void setOnCancelled(std::function<void()> callback) {
        onCancelled_ = std::move(callback);
    }

private:
    std::atomic<bool> cancelled_{false};
    std::function<void()> onCancelled_;
};

// ============================================================================
// Future Extensions
// ============================================================================

// Enhanced future that includes progress and cancellation
template<typename T>
class AsyncResult {
public:
    AsyncResult(std::future<Result<T>> future, 
                std::shared_ptr<CancellationToken> cancellation = nullptr,
                std::shared_ptr<ProgressInfo> progress = nullptr)
        : future_(std::move(future)), cancellation_(cancellation), progress_(progress) {}
    
    // Future-like interface
    bool valid() const { return future_.valid(); }
    
    std::future_status wait_for(const std::chrono::milliseconds& timeout) {
        return future_.wait_for(timeout);
    }
    
    void wait() const { future_.wait(); }
    
    Result<T> get() { return future_.get(); }
    
    // Progress tracking
    std::optional<ProgressInfo> getProgress() const {
        return progress_ ? std::optional<ProgressInfo>(*progress_) : std::nullopt;
    }
    
    // Cancellation
    void cancel() {
        if (cancellation_) {
            cancellation_->cancel();
        }
    }
    
    bool isCancelled() const {
        return cancellation_ && cancellation_->isCancelled();
    }
    
    // Completion checking
    bool isReady() const {
        return future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
    }
    
    // Callback for completion
    template<typename F>
    void onComplete(F&& callback) {
        // This would typically be implemented with a thread pool
        // For now, we'll just store the callback
        onComplete_ = std::forward<F>(callback);
    }

private:
    std::future<Result<T>> future_;
    std::shared_ptr<CancellationToken> cancellation_;
    std::shared_ptr<ProgressInfo> progress_;
    std::function<void(const Result<T>&)> onComplete_;
};

// ============================================================================
// Utility Functions for Result Handling
// ============================================================================

namespace result_utils {

// Combine multiple Results into one
template<typename... Ts>
Result<std::tuple<Ts...>> combine(Result<Ts>... results) {
    // Check if any result is an error
    auto checkError = [](const auto& result) {
        if (result.isError()) {
            return std::optional<ErrorInfo>(result.error());
        }
        return std::optional<ErrorInfo>();
    };
    
    std::vector<std::optional<ErrorInfo>> errors;
    (errors.push_back(checkError(results)), ...);
    
    for (const auto& error : errors) {
        if (error) {
            return Result<std::tuple<Ts...>>(*error);
        }
    }
    
    // All results are successful, combine values
    return Result<std::tuple<Ts...>>::success(std::make_tuple(results.value()...));
}

// Convert exceptions to Results
template<typename F>
auto tryCall(F&& func) -> Result<std::invoke_result_t<F>> {
    using ReturnType = std::invoke_result_t<F>;
    
    try {
        if constexpr (std::is_void_v<ReturnType>) {
            func();
            return Result<void>::success();
        } else {
            return Result<ReturnType>::success(func());
        }
    } catch (const std::exception& e) {
        return Result<ReturnType>::error(
            ErrorCode::Unknown, 
            ErrorCategory::general(),
            e.what()
        );
    } catch (...) {
        return Result<ReturnType>::error(
            ErrorCode::Unknown,
            ErrorCategory::general(), 
            "Unknown exception occurred"
        );
    }
}

} // namespace result_utils

} // namespace mixmind::core