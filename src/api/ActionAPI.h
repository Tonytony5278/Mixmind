#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>

// Forward declarations
namespace mixmind::core {
    class ISession;
    class ITransport;
    class ITrack;
    class IClip;
    class IPluginHost;
    class IAutomation;
    class IRenderService;
    class IMediaLibrary;
    class IAudioProcessor;
    class IAsyncService;
}

namespace mixmind::services {
    class OSSServiceRegistry;
}

namespace mixmind::api {

using json = nlohmann::json;

// ============================================================================
// AI Action API - JSON-validated interface for AI systems
// ============================================================================

/// Action result with metadata
struct ActionResult {
    bool success = false;
    std::string message;
    json data = json::object();
    std::string actionId;
    std::chrono::system_clock::time_point timestamp;
    double executionTimeMs = 0.0;
    std::string errorCode;
    std::vector<std::string> warnings;
    
    ActionResult() : timestamp(std::chrono::system_clock::now()) {}
    
    /// Convert to JSON
    json toJson() const {
        return json{
            {"success", success},
            {"message", message},
            {"data", data},
            {"actionId", actionId},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count()},
            {"executionTimeMs", executionTimeMs},
            {"errorCode", errorCode},
            {"warnings", warnings}
        };
    }
    
    /// Create success result
    static ActionResult createSuccess(const std::string& msg = "", const json& resultData = json::object()) {
        ActionResult result;
        result.success = true;
        result.message = msg;
        result.data = resultData;
        return result;
    }
    
    /// Create error result
    static ActionResult createError(const std::string& msg, const std::string& code = "") {
        ActionResult result;
        result.success = false;
        result.message = msg;
        result.errorCode = code;
        return result;
    }
};

/// Action context for execution
struct ActionContext {
    std::string userId;
    std::string sessionId;
    json metadata = json::object();
    bool dryRun = false;
    core::ProgressCallback progressCallback = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(30000);
    
    ActionContext() = default;
    ActionContext(const std::string& user, const std::string& session) 
        : userId(user), sessionId(session) {}
};

/// Action handler function type
using ActionHandler = std::function<ActionResult(const json& params, const ActionContext& context)>;

/// Action validation function type  
using ActionValidator = std::function<core::Result<core::VoidResult>(const json& params)>;

/// Action metadata for registration
struct ActionDefinition {
    std::string name;
    std::string category;
    std::string description;
    json jsonSchema;                    // JSON schema for parameter validation
    ActionHandler handler;
    ActionValidator validator = nullptr;
    bool requiresSession = true;
    bool supportsUndo = false;
    bool supportsDryRun = false;
    std::vector<std::string> requiredServices;
    std::chrono::milliseconds defaultTimeout = std::chrono::milliseconds(10000);
};

class ActionAPI {
public:
    ActionAPI(
        std::shared_ptr<core::ISession> session,
        std::shared_ptr<core::ITransport> transport,
        std::shared_ptr<core::ITrack> trackManager,
        std::shared_ptr<core::IClip> clipManager,
        std::shared_ptr<core::IPluginHost> pluginHost,
        std::shared_ptr<core::IAutomation> automation,
        std::shared_ptr<core::IRenderService> renderService,
        std::shared_ptr<core::IMediaLibrary> mediaLibrary,
        std::shared_ptr<core::IAudioProcessor> audioProcessor,
        std::shared_ptr<core::IAsyncService> asyncService,
        std::shared_ptr<services::OSSServiceRegistry> ossServices
    );
    
    ~ActionAPI() = default;
    
    // Non-copyable, movable
    ActionAPI(const ActionAPI&) = delete;
    ActionAPI& operator=(const ActionAPI&) = delete;
    ActionAPI(ActionAPI&&) = default;
    ActionAPI& operator=(ActionAPI&&) = default;
    
    // ========================================================================
    // Core API Operations
    // ========================================================================
    
    /// Initialize the Action API
    core::AsyncResult<core::VoidResult> initialize();
    
    /// Shutdown the Action API
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if API is ready
    bool isReady() const;
    
    /// Get API version
    std::string getVersion() const;
    
    // ========================================================================
    // Action Registration and Execution
    // ========================================================================
    
    /// Register an action
    core::VoidResult registerAction(const ActionDefinition& definition);
    
    /// Unregister an action
    core::VoidResult unregisterAction(const std::string& actionName);
    
    /// Get registered actions
    std::vector<std::string> getRegisteredActions() const;
    
    /// Get action definition
    std::optional<ActionDefinition> getActionDefinition(const std::string& actionName) const;
    
    /// Execute action from JSON
    core::AsyncResult<ActionResult> executeAction(
        const std::string& actionName,
        const json& parameters,
        const ActionContext& context = ActionContext()
    );
    
    /// Execute action from JSON string
    core::AsyncResult<ActionResult> executeActionFromString(
        const std::string& actionName,
        const std::string& parametersJson,
        const ActionContext& context = ActionContext()
    );
    
    /// Batch execute multiple actions
    core::AsyncResult<std::vector<ActionResult>> executeActionBatch(
        const std::vector<std::pair<std::string, json>>& actions,
        const ActionContext& context = ActionContext()
    );
    
    // ========================================================================
    // JSON Schema Validation
    // ========================================================================
    
    /// Validate parameters against action schema
    core::Result<core::VoidResult> validateActionParameters(
        const std::string& actionName,
        const json& parameters
    ) const;
    
    /// Validate JSON against schema
    static core::Result<core::VoidResult> validateJson(
        const json& data,
        const json& schema
    );
    
    /// Get validation errors as human-readable strings
    static std::vector<std::string> getValidationErrors(
        const json& data,
        const json& schema
    );
    
    // ========================================================================
    // Action Categories and Discovery
    // ========================================================================
    
    /// Get actions by category
    std::vector<std::string> getActionsByCategory(const std::string& category) const;
    
    /// Get all categories
    std::vector<std::string> getCategories() const;
    
    /// Search actions by name or description
    std::vector<std::string> searchActions(const std::string& query) const;
    
    /// Get action schema for documentation/IDE integration
    json getActionSchema(const std::string& actionName) const;
    
    /// Export all action schemas as OpenAPI specification
    json exportOpenAPISpec() const;
    
    // ========================================================================
    // Session State and Context Management
    // ========================================================================
    
    /// Get current session state as JSON
    json getSessionState() const;
    
    /// Get current transport state
    json getTransportState() const;
    
    /// Get track information
    json getTrackInfo(core::TrackID trackId = core::TrackID{}) const;
    
    /// Get global DAW state for AI context
    json getAIContext() const;
    
    /// Set AI context metadata
    core::VoidResult setAIContextMetadata(const json& metadata);
    
    // ========================================================================
    // Undo/Redo Support
    // ========================================================================
    
    /// Action history entry
    struct ActionHistoryEntry {
        std::string actionId;
        std::string actionName;
        json parameters;
        ActionContext context;
        ActionResult result;
        std::chrono::system_clock::time_point timestamp;
        bool canUndo = false;
        json undoData = json::object();
    };
    
    /// Get action history
    std::vector<ActionHistoryEntry> getActionHistory(int32_t maxEntries = 100) const;
    
    /// Undo last action (if supported)
    core::AsyncResult<ActionResult> undoLastAction();
    
    /// Redo last undone action
    core::AsyncResult<ActionResult> redoLastAction();
    
    /// Clear action history
    void clearActionHistory();
    
    // ========================================================================
    // Dry-Run and Preview Support
    // ========================================================================
    
    /// Execute action in dry-run mode to preview changes
    core::AsyncResult<ActionResult> previewAction(
        const std::string& actionName,
        const json& parameters,
        const ActionContext& context = ActionContext()
    );
    
    /// Generate diff of what an action would change
    core::AsyncResult<json> generateActionDiff(
        const std::string& actionName,
        const json& parameters,
        const ActionContext& context = ActionContext()
    );
    
    // ========================================================================
    // Built-in Action Categories
    // ========================================================================
    
    /// Register all built-in actions
    void registerBuiltInActions();
    
    // Session actions
    void registerSessionActions();
    
    // Transport actions  
    void registerTransportActions();
    
    // Track actions
    void registerTrackActions();
    
    // Clip actions
    void registerClipActions();
    
    // Plugin actions
    void registerPluginActions();
    
    // Automation actions
    void registerAutomationActions();
    
    // Render actions
    void registerRenderActions();
    
    // Media library actions
    void registerMediaLibraryActions();
    
    // Audio processing actions
    void registerAudioProcessingActions();
    
    // OSS service actions
    void registerOSSServiceActions();
    
    // ========================================================================
    // Monitoring and Analytics
    // ========================================================================
    
    struct ActionStatistics {
        int64_t totalExecutions = 0;
        int64_t successfulExecutions = 0;
        int64_t failedExecutions = 0;
        double averageExecutionTimeMs = 0.0;
        double maxExecutionTimeMs = 0.0;
        std::unordered_map<std::string, int64_t> actionCounts;
        std::unordered_map<std::string, int64_t> errorCounts;
        std::chrono::system_clock::time_point lastExecution;
    };
    
    /// Get action execution statistics
    ActionStatistics getActionStatistics() const;
    
    /// Reset statistics
    void resetActionStatistics();
    
    /// Set action execution callback for monitoring
    using ActionExecutionCallback = std::function<void(const std::string& actionName, const ActionResult& result)>;
    void setActionExecutionCallback(ActionExecutionCallback callback);
    
    // ========================================================================
    // Error Handling and Logging
    // ========================================================================
    
    /// Action error callback type
    using ActionErrorCallback = std::function<void(const std::string& actionName, const std::string& error)>;
    
    /// Set error callback
    void setActionErrorCallback(ActionErrorCallback callback);
    
    /// Enable/disable action logging
    void setActionLoggingEnabled(bool enabled);
    
    /// Get action execution log
    std::vector<ActionHistoryEntry> getActionLog() const;
    
    /// Clear action log
    void clearActionLog();
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Execute action with full context and error handling
    ActionResult executeActionInternal(
        const ActionDefinition& definition,
        const json& parameters,
        const ActionContext& context
    );
    
    /// Update statistics after action execution
    void updateActionStatistics(
        const std::string& actionName,
        const ActionResult& result
    );
    
    /// Log action execution
    void logActionExecution(
        const std::string& actionName,
        const json& parameters,
        const ActionContext& context,
        const ActionResult& result
    );
    
    /// Generate unique action ID
    std::string generateActionId() const;
    
    /// Validate action context
    core::Result<core::VoidResult> validateActionContext(
        const ActionDefinition& definition,
        const ActionContext& context
    ) const;

private:
    // Core interface references
    std::shared_ptr<core::ISession> session_;
    std::shared_ptr<core::ITransport> transport_;
    std::shared_ptr<core::ITrack> trackManager_;
    std::shared_ptr<core::IClip> clipManager_;
    std::shared_ptr<core::IPluginHost> pluginHost_;
    std::shared_ptr<core::IAutomation> automation_;
    std::shared_ptr<core::IRenderService> renderService_;
    std::shared_ptr<core::IMediaLibrary> mediaLibrary_;
    std::shared_ptr<core::IAudioProcessor> audioProcessor_;
    std::shared_ptr<core::IAsyncService> asyncService_;
    std::shared_ptr<services::OSSServiceRegistry> ossServices_;
    
    // Action registration
    std::unordered_map<std::string, ActionDefinition> registeredActions_;
    mutable std::shared_mutex actionsMutex_;
    
    // State
    std::atomic<bool> isReady_{false};
    json aiContextMetadata_ = json::object();
    mutable std::mutex contextMutex_;
    
    // History and logging
    std::vector<ActionHistoryEntry> actionHistory_;
    std::vector<ActionHistoryEntry> actionLog_;
    mutable std::mutex historyMutex_;
    std::atomic<bool> actionLoggingEnabled_{true};
    
    // Statistics
    mutable ActionStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Callbacks
    ActionExecutionCallback executionCallback_;
    ActionErrorCallback errorCallback_;
    
    // Constants
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
    static constexpr size_t MAX_LOG_SIZE = 10000;
};

// ============================================================================
// JSON Schema Helpers
// ============================================================================

namespace schemas {
    /// Common parameter schemas
    extern const json TRACK_ID_SCHEMA;
    extern const json CLIP_ID_SCHEMA;
    extern const json PLUGIN_ID_SCHEMA;
    extern const json TIME_POSITION_SCHEMA;
    extern const json VOLUME_SCHEMA;
    extern const json PAN_SCHEMA;
    extern const json FILE_PATH_SCHEMA;
    
    /// Action category schemas
    extern const json SESSION_ACTION_SCHEMAS;
    extern const json TRANSPORT_ACTION_SCHEMAS;
    extern const json TRACK_ACTION_SCHEMAS;
    extern const json CLIP_ACTION_SCHEMAS;
    extern const json PLUGIN_ACTION_SCHEMAS;
    extern const json AUTOMATION_ACTION_SCHEMAS;
    extern const json RENDER_ACTION_SCHEMAS;
    extern const json MEDIA_LIBRARY_ACTION_SCHEMAS;
    extern const json AUDIO_PROCESSING_ACTION_SCHEMAS;
    extern const json OSS_SERVICE_ACTION_SCHEMAS;
}

} // namespace mixmind::api