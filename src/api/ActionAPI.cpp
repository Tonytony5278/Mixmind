#include "ActionAPI.h"
#include "ActionSchemas.h"
#include "../core/ISession.h"
#include "../core/ITransport.h"
#include "../core/ITrack.h"
#include "../core/IClip.h"
#include "../core/IPluginHost.h"
#include "../core/IAutomation.h"
#include "../core/IRenderService.h"
#include "../core/IMediaLibrary.h"
#include "../core/IAudioProcessor.h"
#include "../core/IAsyncService.h"
#include "../services/OSSServiceRegistry.h"
#include <nlohmann/json-schema.hpp>
#include <algorithm>
#include <regex>
#include <juce_core/juce_core.h>

namespace mixmind::api {

ActionAPI::ActionAPI(
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
) : session_(session),
    transport_(transport),
    trackManager_(trackManager),
    clipManager_(clipManager),
    pluginHost_(pluginHost),
    automation_(automation),
    renderService_(renderService),
    mediaLibrary_(mediaLibrary),
    audioProcessor_(audioProcessor),
    asyncService_(asyncService),
    ossServices_(ossServices) {
}

// ============================================================================
// Core API Operations
// ============================================================================

core::AsyncResult<core::VoidResult> ActionAPI::initialize() {
    return asyncService_->executeAsync<core::VoidResult>([this]() -> core::Result<core::VoidResult> {
        try {
            // Register all built-in actions
            registerBuiltInActions();
            
            // Mark as ready
            isReady_.store(true);
            
            return core::VoidResult::success();
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to initialize Action API: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> ActionAPI::shutdown() {
    return asyncService_->executeAsync<core::VoidResult>([this]() -> core::Result<core::VoidResult> {
        try {
            // Clear all registered actions
            std::unique_lock lock(actionsMutex_);
            registeredActions_.clear();
            
            // Clear history and logs
            {
                std::lock_guard histLock(historyMutex_);
                actionHistory_.clear();
                actionLog_.clear();
            }
            
            // Mark as not ready
            isReady_.store(false);
            
            return core::VoidResult::success();
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to shutdown Action API: " + std::string(e.what()));
        }
    });
}

bool ActionAPI::isReady() const {
    return isReady_.load();
}

std::string ActionAPI::getVersion() const {
    return "1.0.0";
}

// ============================================================================
// Action Registration and Execution
// ============================================================================

core::VoidResult ActionAPI::registerAction(const ActionDefinition& definition) {
    if (definition.name.empty()) {
        return core::VoidResult::failure("Action name cannot be empty");
    }
    
    if (!definition.handler) {
        return core::VoidResult::failure("Action handler cannot be null");
    }
    
    std::unique_lock lock(actionsMutex_);
    registeredActions_[definition.name] = definition;
    
    return core::VoidResult::success();
}

core::VoidResult ActionAPI::unregisterAction(const std::string& actionName) {
    std::unique_lock lock(actionsMutex_);
    auto it = registeredActions_.find(actionName);
    if (it == registeredActions_.end()) {
        return core::VoidResult::failure("Action not found: " + actionName);
    }
    
    registeredActions_.erase(it);
    return core::VoidResult::success();
}

std::vector<std::string> ActionAPI::getRegisteredActions() const {
    std::shared_lock lock(actionsMutex_);
    std::vector<std::string> actions;
    actions.reserve(registeredActions_.size());
    
    for (const auto& [name, definition] : registeredActions_) {
        actions.push_back(name);
    }
    
    return actions;
}

std::optional<ActionDefinition> ActionAPI::getActionDefinition(const std::string& actionName) const {
    std::shared_lock lock(actionsMutex_);
    auto it = registeredActions_.find(actionName);
    if (it == registeredActions_.end()) {
        return std::nullopt;
    }
    return it->second;
}

core::AsyncResult<ActionResult> ActionAPI::executeAction(
    const std::string& actionName,
    const json& parameters,
    const ActionContext& context
) {
    return asyncService_->executeAsync<ActionResult>([this, actionName, parameters, context]() -> core::Result<ActionResult> {
        if (!isReady()) {
            return ActionResult::createError("Action API not ready", "API_NOT_READY");
        }
        
        // Find action definition
        std::shared_lock lock(actionsMutex_);
        auto it = registeredActions_.find(actionName);
        if (it == registeredActions_.end()) {
            return ActionResult::createError("Action not found: " + actionName, "ACTION_NOT_FOUND");
        }
        
        const ActionDefinition& definition = it->second;
        lock.unlock();
        
        // Execute action
        auto result = executeActionInternal(definition, parameters, context);
        
        // Update statistics and log
        updateActionStatistics(actionName, result);
        logActionExecution(actionName, parameters, context, result);
        
        // Call execution callback
        if (executionCallback_) {
            executionCallback_(actionName, result);
        }
        
        return result;
    });
}

core::AsyncResult<ActionResult> ActionAPI::executeActionFromString(
    const std::string& actionName,
    const std::string& parametersJson,
    const ActionContext& context
) {
    try {
        json parameters = json::parse(parametersJson);
        return executeAction(actionName, parameters, context);
    } catch (const json::parse_error& e) {
        return asyncService_->makeResolvedFuture<ActionResult>(
            ActionResult::createError("Invalid JSON: " + std::string(e.what()), "INVALID_JSON")
        );
    }
}

core::AsyncResult<std::vector<ActionResult>> ActionAPI::executeActionBatch(
    const std::vector<std::pair<std::string, json>>& actions,
    const ActionContext& context
) {
    return asyncService_->executeAsync<std::vector<ActionResult>>([this, actions, context]() -> core::Result<std::vector<ActionResult>> {
        std::vector<ActionResult> results;
        results.reserve(actions.size());
        
        for (const auto& [actionName, parameters] : actions) {
            auto future = executeAction(actionName, parameters, context);
            auto result = future.get(); // Wait for completion
            results.push_back(result);
            
            // Stop on first error unless continuing on error
            if (!result.success && context.metadata.value("continueOnError", false) == false) {
                break;
            }
        }
        
        return results;
    });
}

// ============================================================================
// JSON Schema Validation
// ============================================================================

core::Result<core::VoidResult> ActionAPI::validateActionParameters(
    const std::string& actionName,
    const json& parameters
) const {
    std::shared_lock lock(actionsMutex_);
    auto it = registeredActions_.find(actionName);
    if (it == registeredActions_.end()) {
        return core::VoidResult::failure("Action not found: " + actionName);
    }
    
    const ActionDefinition& definition = it->second;
    lock.unlock();
    
    // Use custom validator if provided
    if (definition.validator) {
        return definition.validator(parameters);
    }
    
    // Otherwise use JSON schema validation
    return validateJson(parameters, definition.jsonSchema);
}

core::Result<core::VoidResult> ActionAPI::validateJson(
    const json& data,
    const json& schema
) {
    try {
        using nlohmann::json_schema::json_validator;
        
        json_validator validator;
        validator.set_root_schema(schema);
        
        auto errors = validator.validate(data);
        if (!errors.empty()) {
            std::string errorMsg = "Validation errors: ";
            for (const auto& error : errors) {
                errorMsg += error.message + "; ";
            }
            return core::VoidResult::failure(errorMsg);
        }
        
        return core::VoidResult::success();
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Schema validation failed: " + std::string(e.what()));
    }
}

std::vector<std::string> ActionAPI::getValidationErrors(
    const json& data,
    const json& schema
) {
    try {
        using nlohmann::json_schema::json_validator;
        
        json_validator validator;
        validator.set_root_schema(schema);
        
        auto errors = validator.validate(data);
        
        std::vector<std::string> errorMessages;
        for (const auto& error : errors) {
            errorMessages.push_back(error.message);
        }
        
        return errorMessages;
    } catch (const std::exception& e) {
        return {"Schema validation failed: " + std::string(e.what())};
    }
}

// ============================================================================
// Action Categories and Discovery
// ============================================================================

std::vector<std::string> ActionAPI::getActionsByCategory(const std::string& category) const {
    std::shared_lock lock(actionsMutex_);
    std::vector<std::string> actions;
    
    for (const auto& [name, definition] : registeredActions_) {
        if (definition.category == category) {
            actions.push_back(name);
        }
    }
    
    return actions;
}

std::vector<std::string> ActionAPI::getCategories() const {
    std::shared_lock lock(actionsMutex_);
    std::set<std::string> categories;
    
    for (const auto& [name, definition] : registeredActions_) {
        if (!definition.category.empty()) {
            categories.insert(definition.category);
        }
    }
    
    return {categories.begin(), categories.end()};
}

std::vector<std::string> ActionAPI::searchActions(const std::string& query) const {
    std::shared_lock lock(actionsMutex_);
    std::vector<std::string> matches;
    
    std::regex queryRegex(query, std::regex_constants::icase);
    
    for (const auto& [name, definition] : registeredActions_) {
        if (std::regex_search(name, queryRegex) || 
            std::regex_search(definition.description, queryRegex)) {
            matches.push_back(name);
        }
    }
    
    return matches;
}

json ActionAPI::getActionSchema(const std::string& actionName) const {
    std::shared_lock lock(actionsMutex_);
    auto it = registeredActions_.find(actionName);
    if (it == registeredActions_.end()) {
        return json::object();
    }
    
    const ActionDefinition& definition = it->second;
    
    return json{
        {"name", definition.name},
        {"category", definition.category},
        {"description", definition.description},
        {"schema", definition.jsonSchema},
        {"requiresSession", definition.requiresSession},
        {"supportsUndo", definition.supportsUndo},
        {"supportsDryRun", definition.supportsDryRun},
        {"requiredServices", definition.requiredServices},
        {"defaultTimeout", definition.defaultTimeout.count()}
    };
}

json ActionAPI::exportOpenAPISpec() const {
    json spec = json{
        {"openapi", "3.0.0"},
        {"info", {
            {"title", "MixMind AI Action API"},
            {"version", getVersion()},
            {"description", "JSON-validated API for AI systems to control DAW functionality"}
        }},
        {"paths", json::object()},
        {"components", {
            {"schemas", json::object()}
        }}
    };
    
    std::shared_lock lock(actionsMutex_);
    
    for (const auto& [name, definition] : registeredActions_) {
        std::string path = "/actions/" + name;
        
        spec["paths"][path] = json{
            {"post", {
                {"summary", definition.description},
                {"tags", {definition.category}},
                {"requestBody", {
                    {"required", true},
                    {"content", {
                        {"application/json", {
                            {"schema", definition.jsonSchema}
                        }}
                    }}
                }},
                {"responses", {
                    {"200", {
                        {"description", "Action executed successfully"},
                        {"content", {
                            {"application/json", {
                                {"schema", {
                                    {"$ref", "#/components/schemas/ActionResult"}
                                }}
                            }}
                        }}
                    }},
                    {"400", {
                        {"description", "Invalid parameters"}
                    }},
                    {"500", {
                        {"description", "Execution error"}
                    }}
                }}
            }}
        };
        
        // Add schema to components
        spec["components"]["schemas"][name + "Request"] = definition.jsonSchema;
    }
    
    // Add ActionResult schema
    spec["components"]["schemas"]["ActionResult"] = json{
        {"type", "object"},
        {"properties", {
            {"success", {"type", "boolean"}},
            {"message", {"type", "string"}},
            {"data", {"type", "object"}},
            {"actionId", {"type", "string"}},
            {"timestamp", {"type", "integer"}},
            {"executionTimeMs", {"type", "number"}},
            {"errorCode", {"type", "string"}},
            {"warnings", {"type", "array", "items", {"type", "string"}}}
        }},
        {"required", {"success", "message", "data"}}
    };
    
    return spec;
}

// ============================================================================
// Session State and Context Management
// ============================================================================

json ActionAPI::getSessionState() const {
    if (!session_) return json::object();
    
    try {
        // Get session info asynchronously and wait
        auto future = session_->getSessionInfo();
        auto sessionInfo = future.get();
        
        if (!sessionInfo.hasValue()) {
            return json::object();
        }
        
        const auto& info = sessionInfo.getValue();
        
        return json{
            {"name", info.name},
            {"filePath", info.filePath},
            {"isModified", info.isModified},
            {"duration", info.duration.count()},
            {"sampleRate", info.sampleRate},
            {"bitDepth", info.bitDepth},
            {"created", std::chrono::duration_cast<std::chrono::milliseconds>(info.created.time_since_epoch()).count()},
            {"lastModified", std::chrono::duration_cast<std::chrono::milliseconds>(info.lastModified.time_since_epoch()).count()}
        };
    } catch (const std::exception& e) {
        return json{{"error", e.what()}};
    }
}

json ActionAPI::getTransportState() const {
    if (!transport_) return json::object();
    
    try {
        return json{
            {"isPlaying", transport_->isPlaying()},
            {"isRecording", transport_->isRecording()},
            {"isPaused", transport_->isPaused()},
            {"position", transport_->getCurrentPosition().count()},
            {"length", transport_->getLength().count()},
            {"tempo", transport_->getTempo()},
            {"isLooping", transport_->isLooping()}
        };
    } catch (const std::exception& e) {
        return json{{"error", e.what()}};
    }
}

json ActionAPI::getTrackInfo(core::TrackID trackId) const {
    if (!trackManager_) return json::object();
    
    try {
        if (trackId == core::TrackID{}) {
            // Get all tracks
            auto future = trackManager_->getAllTracks();
            auto tracks = future.get();
            
            if (!tracks.hasValue()) {
                return json{{"error", "Failed to get tracks"}};
            }
            
            json tracksJson = json::array();
            for (const auto& track : tracks.getValue()) {
                tracksJson.push_back(json{
                    {"id", track.id.value()},
                    {"name", track.name},
                    {"volume", track.volume},
                    {"pan", track.pan},
                    {"isMuted", track.isMuted},
                    {"isSolo", track.isSolo},
                    {"isRecordEnabled", track.isRecordEnabled},
                    {"color", track.color}
                });
            }
            
            return json{{"tracks", tracksJson}};
        } else {
            // Get specific track
            auto future = trackManager_->getTrack(trackId);
            auto trackResult = future.get();
            
            if (!trackResult.hasValue()) {
                return json{{"error", "Track not found"}};
            }
            
            const auto& track = trackResult.getValue();
            return json{
                {"id", track.id.value()},
                {"name", track.name},
                {"volume", track.volume},
                {"pan", track.pan},
                {"isMuted", track.isMuted},
                {"isSolo", track.isSolo},
                {"isRecordEnabled", track.isRecordEnabled},
                {"color", track.color}
            };
        }
    } catch (const std::exception& e) {
        return json{{"error", e.what()}};
    }
}

json ActionAPI::getAIContext() const {
    json context = json::object();
    
    // Add session state
    context["session"] = getSessionState();
    
    // Add transport state
    context["transport"] = getTransportState();
    
    // Add track info
    context["tracks"] = getTrackInfo();
    
    // Add AI context metadata
    {
        std::lock_guard lock(contextMutex_);
        context["metadata"] = aiContextMetadata_;
    }
    
    // Add available actions
    context["availableActions"] = getRegisteredActions();
    
    // Add categories
    context["categories"] = getCategories();
    
    return context;
}

core::VoidResult ActionAPI::setAIContextMetadata(const json& metadata) {
    std::lock_guard lock(contextMutex_);
    aiContextMetadata_ = metadata;
    return core::VoidResult::success();
}

// ============================================================================
// Internal Implementation
// ============================================================================

ActionResult ActionAPI::executeActionInternal(
    const ActionDefinition& definition,
    const json& parameters,
    const ActionContext& context
) {
    auto startTime = std::chrono::steady_clock::now();
    
    ActionResult result;
    result.actionId = generateActionId();
    
    try {
        // Validate context
        auto contextValidation = validateActionContext(definition, context);
        if (!contextValidation.hasValue()) {
            return ActionResult::createError(contextValidation.getErrorMessage(), "INVALID_CONTEXT");
        }
        
        // Validate parameters
        auto paramValidation = validateActionParameters(definition.name, parameters);
        if (!paramValidation.hasValue()) {
            return ActionResult::createError(paramValidation.getErrorMessage(), "INVALID_PARAMETERS");
        }
        
        // Execute action
        result = definition.handler(parameters, context);
        result.actionId = generateActionId();
        
    } catch (const std::exception& e) {
        result = ActionResult::createError("Action execution failed: " + std::string(e.what()), "EXECUTION_ERROR");
    }
    
    // Calculate execution time
    auto endTime = std::chrono::steady_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    return result;
}

void ActionAPI::updateActionStatistics(
    const std::string& actionName,
    const ActionResult& result
) {
    std::lock_guard lock(statisticsMutex_);
    
    statistics_.totalExecutions++;
    if (result.success) {
        statistics_.successfulExecutions++;
    } else {
        statistics_.failedExecutions++;
        statistics_.errorCounts[result.errorCode]++;
    }
    
    statistics_.actionCounts[actionName]++;
    statistics_.lastExecution = result.timestamp;
    
    // Update timing statistics
    double totalTime = statistics_.averageExecutionTimeMs * (statistics_.totalExecutions - 1);
    statistics_.averageExecutionTimeMs = (totalTime + result.executionTimeMs) / statistics_.totalExecutions;
    
    if (result.executionTimeMs > statistics_.maxExecutionTimeMs) {
        statistics_.maxExecutionTimeMs = result.executionTimeMs;
    }
}

void ActionAPI::logActionExecution(
    const std::string& actionName,
    const json& parameters,
    const ActionContext& context,
    const ActionResult& result
) {
    if (!actionLoggingEnabled_.load()) return;
    
    std::lock_guard lock(historyMutex_);
    
    ActionHistoryEntry entry;
    entry.actionId = result.actionId;
    entry.actionName = actionName;
    entry.parameters = parameters;
    entry.context = context;
    entry.result = result;
    entry.timestamp = result.timestamp;
    
    actionLog_.push_back(entry);
    
    // Maintain log size limit
    if (actionLog_.size() > MAX_LOG_SIZE) {
        actionLog_.erase(actionLog_.begin());
    }
    
    // Add to history if action supports undo
    auto actionDef = getActionDefinition(actionName);
    if (actionDef && actionDef->supportsUndo) {
        entry.canUndo = true;
        actionHistory_.push_back(entry);
        
        if (actionHistory_.size() > MAX_HISTORY_SIZE) {
            actionHistory_.erase(actionHistory_.begin());
        }
    }
}

std::string ActionAPI::generateActionId() const {
    return juce::Uuid().toString().toStdString();
}

core::Result<core::VoidResult> ActionAPI::validateActionContext(
    const ActionDefinition& definition,
    const ActionContext& context
) const {
    // Check session requirement
    if (definition.requiresSession && !session_) {
        return core::VoidResult::failure("Action requires active session");
    }
    
    // Check required services
    for (const auto& serviceName : definition.requiredServices) {
        if (!ossServices_->isServiceAvailable(serviceName)) {
            return core::VoidResult::failure("Required service not available: " + serviceName);
        }
    }
    
    return core::VoidResult::success();
}

// ============================================================================
// Built-in Action Registration
// ============================================================================

void ActionAPI::registerBuiltInActions() {
    registerSessionActions();
    registerTransportActions();
    registerTrackActions();
    registerClipActions();
    registerPluginActions();
    registerAutomationActions();
    registerRenderActions();
    registerMediaLibraryActions();
    registerAudioProcessingActions();
    registerOSSServiceActions();
}

void ActionAPI::registerSessionActions() {
    // Implementation of session actions would go here
    // This is a placeholder - full implementation would include actions like:
    // - session.new
    // - session.open
    // - session.save
    // - session.close
    // etc.
}

void ActionAPI::registerTransportActions() {
    // Implementation of transport actions would go here
    // This is a placeholder - full implementation would include actions like:
    // - transport.play
    // - transport.stop
    // - transport.record
    // - transport.setPosition
    // etc.
}

void ActionAPI::registerTrackActions() {
    // Implementation of track actions would go here
    // This is a placeholder - full implementation would include actions like:
    // - track.create
    // - track.delete
    // - track.setVolume
    // - track.setPan
    // etc.
}

void ActionAPI::registerClipActions() {
    // Implementation of clip actions would go here
}

void ActionAPI::registerPluginActions() {
    // Implementation of plugin actions would go here
}

void ActionAPI::registerAutomationActions() {
    // Implementation of automation actions would go here
}

void ActionAPI::registerRenderActions() {
    // Implementation of render actions would go here
}

void ActionAPI::registerMediaLibraryActions() {
    // Implementation of media library actions would go here
}

void ActionAPI::registerAudioProcessingActions() {
    // Implementation of audio processing actions would go here
}

void ActionAPI::registerOSSServiceActions() {
    // Implementation of OSS service actions would go here
}

} // namespace mixmind::api