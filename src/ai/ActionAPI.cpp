#include "ActionAPI.h"
#include "../core/async.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <regex>
#include <chrono>
#include <shared_mutex>

namespace mixmind::ai {

// ============================================================================
// ActionAPI Implementation
// ============================================================================

ActionAPI::ActionAPI() {
    // Initialize with basic actions
}

ActionAPI::~ActionAPI() {
    // Cleanup
}

// ========================================================================
// Service Lifecycle
// ========================================================================

core::AsyncResult<core::VoidResult> ActionAPI::initialize(
    std::shared_ptr<core::ISession> session,
    std::shared_ptr<core::ITrack> track,
    std::shared_ptr<core::IClip> clip,
    std::shared_ptr<core::ITransport> transport,
    std::shared_ptr<core::IPluginHost> pluginHost) {
    
    return core::getGlobalThreadPool().executeAsyncVoid([this, session, track, clip, transport, pluginHost]() -> core::VoidResult {
        std::unique_lock lock(actionsMutex_);
        
        // Store service references
        session_ = session;
        track_ = track;
        clip_ = clip;
        transport_ = transport;
        pluginHost_ = pluginHost;
        
        // Register built-in actions
        registerBuiltInActions();
        
        // Initialize NLP processor (mock for now)
        nlpProcessor_ = std::make_unique<NLPProcessor>();
        
        // Reset statistics
        resetActionStats();
        
        isInitialized_.store(true);
        
        return core::VoidResult::success();
    }, "Initializing ActionAPI");
}

core::AsyncResult<core::VoidResult> ActionAPI::shutdown() {
    return core::getGlobalThreadPool().executeAsyncVoid([this]() -> core::VoidResult {
        std::unique_lock lock(actionsMutex_);
        
        if (!isInitialized_.load()) {
            return core::VoidResult::success();
        }
        
        // Clear actions and handlers
        actions_.clear();
        handlers_.clear();
        
        // Clear service references
        session_.reset();
        track_.reset();
        clip_.reset();
        transport_.reset();
        pluginHost_.reset();
        
        // Cleanup NLP processor
        nlpProcessor_.reset();
        
        isInitialized_.store(false);
        
        return core::VoidResult::success();
    }, "Shutting down ActionAPI");
}

bool ActionAPI::isReady() const {
    return isInitialized_.load();
}

// ========================================================================
// Action Registration
// ========================================================================

core::VoidResult ActionAPI::registerAction(
    const ActionDefinition& definition,
    ActionHandler handler) {
    
    std::unique_lock lock(actionsMutex_);
    
    actions_[definition.id] = definition;
    handlers_[definition.id] = handler;
    
    return core::VoidResult::success();
}

core::VoidResult ActionAPI::unregisterAction(const std::string& actionId) {
    std::unique_lock lock(actionsMutex_);
    
    actions_.erase(actionId);
    handlers_.erase(actionId);
    
    return core::VoidResult::success();
}

std::optional<ActionDefinition> ActionAPI::getActionDefinition(const std::string& actionId) const {
    std::shared_lock lock(actionsMutex_);
    
    auto it = actions_.find(actionId);
    if (it != actions_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::vector<ActionDefinition> ActionAPI::getAllActions() const {
    std::shared_lock lock(actionsMutex_);
    
    std::vector<ActionDefinition> result;
    result.reserve(actions_.size());
    
    for (const auto& [id, definition] : actions_) {
        result.push_back(definition);
    }
    
    return result;
}

std::vector<ActionDefinition> ActionAPI::getActionsByCategory(ActionCategory category) const {
    std::shared_lock lock(actionsMutex_);
    
    std::vector<ActionDefinition> result;
    
    for (const auto& [id, definition] : actions_) {
        if (definition.category == category) {
            result.push_back(definition);
        }
    }
    
    return result;
}

std::vector<ActionDefinition> ActionAPI::searchActions(const std::string& query) const {
    std::shared_lock lock(actionsMutex_);
    
    std::vector<ActionDefinition> result;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    for (const auto& [id, definition] : actions_) {
        std::string lowerName = definition.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        std::string lowerDescription = definition.description;
        std::transform(lowerDescription.begin(), lowerDescription.end(), lowerDescription.begin(), ::tolower);
        
        if (lowerName.find(lowerQuery) != std::string::npos ||
            lowerDescription.find(lowerQuery) != std::string::npos) {
            result.push_back(definition);
        }
    }
    
    return result;
}

// ========================================================================
// Natural Language Processing
// ========================================================================

core::AsyncResult<core::Result<ParsedIntent>> ActionAPI::parseIntent(
    const std::string& input,
    const ActionContext& context) const {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ParsedIntent>>(
        [this, input, context]() -> core::Result<ParsedIntent> {
            
            ParsedIntent intent;
            intent.originalText = input;
            intent.confidence = 0.0;
            
            // Simple pattern matching for demo
            std::string lowerInput = input;
            std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
            
            // Transport commands
            if (lowerInput.find("play") != std::string::npos) {
                intent.intent = "transport_play";
                intent.confidence = 0.9;
            }
            else if (lowerInput.find("stop") != std::string::npos) {
                intent.intent = "transport_stop";
                intent.confidence = 0.9;
            }
            else if (lowerInput.find("record") != std::string::npos) {
                intent.intent = "transport_record";
                intent.confidence = 0.9;
            }
            // Track commands
            else if (lowerInput.find("create") != std::string::npos && lowerInput.find("track") != std::string::npos) {
                intent.intent = "track_create";
                intent.confidence = 0.8;
                
                // Extract track type
                if (lowerInput.find("audio") != std::string::npos) {
                    intent.entities["type"] = ActionValue(std::string("audio"));
                } else if (lowerInput.find("midi") != std::string::npos) {
                    intent.entities["type"] = ActionValue(std::string("midi"));
                }
            }
            else if (lowerInput.find("mute") != std::string::npos) {
                intent.intent = "track_mute";
                intent.confidence = 0.8;
                
                // Extract track number/name
                std::regex trackRegex(R"(track\s+(\d+))");
                std::smatch match;
                if (std::regex_search(lowerInput, match, trackRegex)) {
                    int trackNum = std::stoi(match[1]);
                    intent.entities["track_number"] = ActionValue(trackNum);
                }
            }
            // Session commands
            else if (lowerInput.find("save") != std::string::npos) {
                intent.intent = "session_save";
                intent.confidence = 0.8;
            }
            else if (lowerInput.find("tempo") != std::string::npos) {
                intent.intent = "transport_set_tempo";
                intent.confidence = 0.7;
                
                // Extract tempo value
                std::regex tempoRegex(R"((\d+)\s*bpm)");
                std::smatch match;
                if (std::regex_search(lowerInput, match, tempoRegex)) {
                    int tempo = std::stoi(match[1]);
                    intent.entities["tempo"] = ActionValue(tempo);
                }
            }
            else {
                intent.intent = "unknown";
                intent.confidence = 0.1;
                intent.requiresClarification = true;
                intent.clarifications.push_back("I'm not sure what you want to do. Can you be more specific?");
            }
            
            return core::Result<ParsedIntent>::success(intent);
        },
        "Parsing intent"
    );
}

core::AsyncResult<core::Result<std::vector<CommandSuggestion>>> ActionAPI::getSuggestions(
    const std::string& partialInput,
    const ActionContext& context,
    size_t maxSuggestions) const {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<std::vector<CommandSuggestion>>>(
        [this, partialInput, context, maxSuggestions]() -> core::Result<std::vector<CommandSuggestion>> {
            
            std::vector<CommandSuggestion> suggestions;
            std::string lowerInput = partialInput;
            std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
            
            // Get all actions and filter by relevance
            auto allActions = getAllActions();
            
            for (const auto& action : allActions) {
                CommandSuggestion suggestion;
                suggestion.command = action.name;
                suggestion.description = action.description;
                suggestion.category = action.category;
                suggestion.relevance = 0.0;
                
                // Simple relevance scoring
                std::string lowerName = action.name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                
                if (lowerName.find(lowerInput) == 0) {
                    suggestion.relevance = 1.0; // Prefix match
                } else if (lowerName.find(lowerInput) != std::string::npos) {
                    suggestion.relevance = 0.8; // Contains match
                } else {
                    // Check examples and patterns
                    for (const auto& example : action.examples) {
                        std::string lowerExample = example;
                        std::transform(lowerExample.begin(), lowerExample.end(), lowerExample.begin(), ::tolower);
                        
                        if (lowerExample.find(lowerInput) != std::string::npos) {
                            suggestion.relevance = std::max(suggestion.relevance, 0.6);
                        }
                    }
                }
                
                if (suggestion.relevance > 0.0) {
                    suggestion.reasoning = "Matches '" + partialInput + "'";
                    suggestions.push_back(suggestion);
                }
            }
            
            // Sort by relevance
            std::sort(suggestions.begin(), suggestions.end(),
                [](const CommandSuggestion& a, const CommandSuggestion& b) {
                    return a.relevance > b.relevance;
                });
            
            // Limit results
            if (suggestions.size() > maxSuggestions) {
                suggestions.resize(maxSuggestions);
            }
            
            return core::Result<std::vector<CommandSuggestion>>::success(suggestions);
        },
        "Getting command suggestions"
    );
}

// ========================================================================
// Action Execution
// ========================================================================

core::AsyncResult<core::Result<ActionResult>> ActionAPI::executeCommand(
    const std::string& command,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [this, command, context]() -> core::Result<ActionResult> {
            
            // Parse intent from command
            auto intentResult = parseIntent(command, context).get();
            if (!intentResult.isSuccess()) {
                return core::Result<ActionResult>::error(
                    intentResult.error().code,
                    intentResult.error().category,
                    intentResult.error().message
                );
            }
            
            auto intent = intentResult.value();
            
            // Execute the intent
            auto executeResult = executeIntent(intent, context).get();
            return executeResult;
        },
        "Executing command"
    );
}

core::AsyncResult<core::Result<ActionResult>> ActionAPI::executeIntent(
    const ParsedIntent& intent,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [this, intent, context]() -> core::Result<ActionResult> {
            
            auto startTime = std::chrono::high_resolution_clock::now();
            
            ActionResult result;
            result.actionId = intent.intent;
            result.success = false;
            
            // Check if we have a handler for this intent
            std::shared_lock lock(actionsMutex_);
            auto handlerIt = handlers_.find(intent.intent);
            
            if (handlerIt == handlers_.end()) {
                result.message = "Unknown action: " + intent.intent;
                result.errorCode = "UNKNOWN_ACTION";
                result.suggestions.push_back("Try 'help' to see available commands");
                
                auto endTime = std::chrono::high_resolution_clock::now();
                result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                
                updateStats(intent.intent, result);
                return core::Result<ActionResult>::success(result);
            }
            
            // Convert intent entities to action parameters
            std::vector<ActionParameter> parameters;
            for (const auto& [key, value] : intent.entities) {
                ActionParameter param;
                param.name = key;
                param.value = value;
                param.required = true;
                
                // Determine type from value
                if (std::holds_alternative<std::string>(value)) {
                    param.type = "string";
                } else if (std::holds_alternative<int>(value)) {
                    param.type = "number";
                } else if (std::holds_alternative<double>(value)) {
                    param.type = "number";
                } else if (std::holds_alternative<bool>(value)) {
                    param.type = "boolean";
                }
                
                parameters.push_back(param);
            }
            lock.unlock();
            
            // Execute the action
            auto handler = handlerIt->second;
            auto executeResult = handler(parameters, context).get();
            
            if (executeResult.isSuccess()) {
                result = executeResult.value();
                result.success = true;
            } else {
                result.success = false;
                result.message = executeResult.error().message;
                result.errorCode = "EXECUTION_FAILED";
                result.errorDetails = executeResult.error().message;
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            // Update statistics
            updateStats(intent.intent, result);
            
            return core::Result<ActionResult>::success(result);
        },
        "Executing intent: " + intent.intent
    );
}

core::AsyncResult<core::Result<ActionResult>> ActionAPI::executeAction(
    const std::string& actionId,
    const std::vector<ActionParameter>& parameters,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [this, actionId, parameters, context]() -> core::Result<ActionResult> {
            
            std::shared_lock lock(actionsMutex_);
            auto handlerIt = handlers_.find(actionId);
            
            if (handlerIt == handlers_.end()) {
                ActionResult result;
                result.actionId = actionId;
                result.success = false;
                result.message = "Action not found: " + actionId;
                result.errorCode = "ACTION_NOT_FOUND";
                
                updateStats(actionId, result);
                return core::Result<ActionResult>::success(result);
            }
            
            auto handler = handlerIt->second;
            lock.unlock();
            
            auto executeResult = handler(parameters, context).get();
            
            if (executeResult.isSuccess()) {
                auto result = executeResult.value();
                result.actionId = actionId;
                result.success = true;
                
                updateStats(actionId, result);
                return core::Result<ActionResult>::success(result);
            } else {
                ActionResult result;
                result.actionId = actionId;
                result.success = false;
                result.message = executeResult.error().message;
                result.errorCode = "EXECUTION_FAILED";
                result.errorDetails = executeResult.error().message;
                
                updateStats(actionId, result);
                return core::Result<ActionResult>::success(result);
            }
        },
        "Executing action: " + actionId
    );
}

// ========================================================================
// Built-in Actions Registration
// ========================================================================

void ActionAPI::registerBuiltInActions() {
    registerTransportActions();
    registerTrackActions();
    registerSessionActions();
}

void ActionAPI::registerTransportActions() {
    // Play action
    {
        ActionDefinition def;
        def.id = "transport_play";
        def.name = "Play";
        def.description = "Start audio playback";
        def.category = ActionCategory::Transport;
        def.complexity = ActionComplexity::Simple;
        def.patterns = {"play", "start playback", "begin playing"};
        def.examples = {"play", "start playing", "begin playback"};
        def.helpText = "Starts audio playback from the current position";
        def.isUndoable = true;
        
        registerAction(def, actions::TransportActions::play);
    }
    
    // Stop action
    {
        ActionDefinition def;
        def.id = "transport_stop";
        def.name = "Stop";
        def.description = "Stop audio playback";
        def.category = ActionCategory::Transport;
        def.complexity = ActionComplexity::Simple;
        def.patterns = {"stop", "halt", "pause playback"};
        def.examples = {"stop", "halt playback", "stop playing"};
        def.helpText = "Stops audio playback";
        def.isUndoable = true;
        
        registerAction(def, actions::TransportActions::stop);
    }
    
    // Record action
    {
        ActionDefinition def;
        def.id = "transport_record";
        def.name = "Record";
        def.description = "Start recording";
        def.category = ActionCategory::Transport;
        def.complexity = ActionComplexity::Simple;
        def.patterns = {"record", "start recording", "begin recording"};
        def.examples = {"record", "start recording", "begin rec"};
        def.helpText = "Starts recording on armed tracks";
        def.isUndoable = true;
        def.requiresConfirmation = false;
        
        registerAction(def, actions::TransportActions::record);
    }
    
    // Set tempo action
    {
        ActionDefinition def;
        def.id = "transport_set_tempo";
        def.name = "Set Tempo";
        def.description = "Change the project tempo";
        def.category = ActionCategory::Transport;
        def.complexity = ActionComplexity::Simple;
        
        ActionParameter tempoParam;
        tempoParam.name = "tempo";
        tempoParam.type = "number";
        tempoParam.required = true;
        tempoParam.description = "Tempo in BPM";
        tempoParam.minValue = ActionValue(60);
        tempoParam.maxValue = ActionValue(200);
        def.parameters.push_back(tempoParam);
        
        def.patterns = {"set tempo to (\\d+)", "change tempo", "tempo (\\d+)"};
        def.examples = {"set tempo to 120", "change tempo to 140", "tempo 110"};
        def.helpText = "Changes the project tempo to the specified BPM";
        def.isUndoable = true;
        
        registerAction(def, actions::TransportActions::setTempo);
    }
}

void ActionAPI::registerTrackActions() {
    // Create track action
    {
        ActionDefinition def;
        def.id = "track_create";
        def.name = "Create Track";
        def.description = "Create a new audio or MIDI track";
        def.category = ActionCategory::Track;
        def.complexity = ActionComplexity::Simple;
        
        ActionParameter typeParam;
        typeParam.name = "type";
        typeParam.type = "string";
        typeParam.required = false;
        typeParam.description = "Track type (audio or midi)";
        typeParam.allowedValues = {ActionValue(std::string("audio")), ActionValue(std::string("midi"))};
        def.optionalParameters.push_back(typeParam);
        
        def.patterns = {"create track", "new track", "add track"};
        def.examples = {"create track", "create audio track", "new midi track"};
        def.helpText = "Creates a new track in the current session";
        def.isUndoable = true;
        
        registerAction(def, actions::TrackActions::createTrack);
    }
    
    // Mute track action
    {
        ActionDefinition def;
        def.id = "track_mute";
        def.name = "Mute Track";
        def.description = "Mute or unmute a track";
        def.category = ActionCategory::Track;
        def.complexity = ActionComplexity::Simple;
        
        ActionParameter trackParam;
        trackParam.name = "track_number";
        trackParam.type = "number";
        trackParam.required = false;
        trackParam.description = "Track number to mute";
        def.optionalParameters.push_back(trackParam);
        
        def.patterns = {"mute track (\\d+)", "mute", "unmute track (\\d+)"};
        def.examples = {"mute track 1", "mute", "unmute track 2"};
        def.helpText = "Mutes or unmutes the specified track";
        def.isUndoable = true;
        
        registerAction(def, actions::TrackActions::muteTrack);
    }
}

void ActionAPI::registerSessionActions() {
    // Save session action
    {
        ActionDefinition def;
        def.id = "session_save";
        def.name = "Save Session";
        def.description = "Save the current session";
        def.category = ActionCategory::Session;
        def.complexity = ActionComplexity::Simple;
        def.patterns = {"save", "save session", "save project"};
        def.examples = {"save", "save session", "save project"};
        def.helpText = "Saves the current session to disk";
        def.isUndoable = false;
        
        registerAction(def, actions::SessionActions::saveSession);
    }
}

// ========================================================================
// Statistics and Utilities
// ========================================================================

ActionAPI::ActionStats ActionAPI::getActionStats() const {
    std::lock_guard lock(statsMutex_);
    return stats_;
}

void ActionAPI::resetActionStats() {
    std::lock_guard lock(statsMutex_);
    stats_ = ActionStats{};
}

void ActionAPI::updateStats(const std::string& actionId, const ActionResult& result) {
    std::lock_guard lock(statsMutex_);
    
    stats_.totalExecutions++;
    if (result.success) {
        stats_.successfulExecutions++;
    } else {
        stats_.failedExecutions++;
    }
    
    stats_.actionUsageCounts[actionId]++;
    
    double execTime = result.executionTime.count();
    if (stats_.totalExecutions == 1) {
        stats_.averageExecutionTime = execTime;
    } else {
        stats_.averageExecutionTime = 
            (stats_.averageExecutionTime * (stats_.totalExecutions - 1) + execTime) / 
            stats_.totalExecutions;
    }
    
    stats_.actionAverageTime[actionId] = 
        (stats_.actionAverageTime[actionId] * (stats_.actionUsageCounts[actionId] - 1) + execTime) / 
        stats_.actionUsageCounts[actionId];
}

// ============================================================================
// NLP Processor Mock Implementation
// ============================================================================

class ActionAPI::NLPProcessor {
public:
    NLPProcessor() = default;
    ~NLPProcessor() = default;
    
    // Simple pattern-based processing for demo
    std::vector<std::string> extractKeywords(const std::string& input) {
        std::vector<std::string> keywords;
        std::istringstream iss(input);
        std::string word;
        
        while (iss >> word) {
            // Simple cleanup
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            
            // Remove common words
            if (word != "the" && word != "a" && word != "an" && word != "to" && 
                word != "and" && word != "or" && word != "but" && word.length() > 2) {
                keywords.push_back(word);
            }
        }
        
        return keywords;
    }
};

// ============================================================================
// Built-in Action Implementations
// ============================================================================

namespace actions {

// Transport Actions
core::AsyncResult<core::Result<ActionResult>> TransportActions::play(
    const std::vector<ActionParameter>& params,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [params, context]() -> core::Result<ActionResult> {
            ActionResult result;
            result.actionId = "transport_play";
            result.success = true;
            result.message = "Playback started";
            result.operations = {"transport_play"};
            result.stateChanges = {"transport_state:playing"};
            
            // In real implementation, would call transport->play()
            
            return core::Result<ActionResult>::success(result);
        },
        "Transport play action"
    );
}

core::AsyncResult<core::Result<ActionResult>> TransportActions::stop(
    const std::vector<ActionParameter>& params,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [params, context]() -> core::Result<ActionResult> {
            ActionResult result;
            result.actionId = "transport_stop";
            result.success = true;
            result.message = "Playback stopped";
            result.operations = {"transport_stop"};
            result.stateChanges = {"transport_state:stopped"};
            
            return core::Result<ActionResult>::success(result);
        },
        "Transport stop action"
    );
}

core::AsyncResult<core::Result<ActionResult>> TransportActions::record(
    const std::vector<ActionParameter>& params,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [params, context]() -> core::Result<ActionResult> {
            ActionResult result;
            result.actionId = "transport_record";
            result.success = true;
            result.message = "Recording started";
            result.operations = {"transport_record"};
            result.stateChanges = {"transport_state:recording"};
            
            return core::Result<ActionResult>::success(result);
        },
        "Transport record action"
    );
}

core::AsyncResult<core::Result<ActionResult>> TransportActions::setTempo(
    const std::vector<ActionParameter>& params,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [params, context]() -> core::Result<ActionResult> {
            ActionResult result;
            result.actionId = "transport_set_tempo";
            
            // Find tempo parameter
            int tempo = 120; // default
            for (const auto& param : params) {
                if (param.name == "tempo") {
                    auto tempoValue = param.getValue<int>();
                    if (tempoValue) {
                        tempo = *tempoValue;
                        break;
                    }
                }
            }
            
            result.success = true;
            result.message = "Tempo set to " + std::to_string(tempo) + " BPM";
            result.operations = {"transport_set_tempo"};
            result.stateChanges = {"tempo:" + std::to_string(tempo)};
            result.outputs["new_tempo"] = ActionValue(tempo);
            
            return core::Result<ActionResult>::success(result);
        },
        "Set tempo action"
    );
}

// Track Actions
core::AsyncResult<core::Result<ActionResult>> TrackActions::createTrack(
    const std::vector<ActionParameter>& params,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [params, context]() -> core::Result<ActionResult> {
            ActionResult result;
            result.actionId = "track_create";
            
            // Determine track type
            std::string trackType = "audio"; // default
            for (const auto& param : params) {
                if (param.name == "type") {
                    auto typeValue = param.getValue<std::string>();
                    if (typeValue) {
                        trackType = *typeValue;
                        break;
                    }
                }
            }
            
            result.success = true;
            result.message = "Created new " + trackType + " track";
            result.operations = {"track_create"};
            result.stateChanges = {"track_count:+1", "track_type:" + trackType};
            result.outputs["track_type"] = ActionValue(trackType);
            result.outputs["track_id"] = ActionValue(std::string("track_" + std::to_string(rand() % 1000)));
            
            return core::Result<ActionResult>::success(result);
        },
        "Create track action"
    );
}

core::AsyncResult<core::Result<ActionResult>> TrackActions::muteTrack(
    const std::vector<ActionParameter>& params,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [params, context]() -> core::Result<ActionResult> {
            ActionResult result;
            result.actionId = "track_mute";
            
            // Find track number
            int trackNumber = 1; // default
            for (const auto& param : params) {
                if (param.name == "track_number") {
                    auto trackValue = param.getValue<int>();
                    if (trackValue) {
                        trackNumber = *trackValue;
                        break;
                    }
                }
            }
            
            result.success = true;
            result.message = "Muted track " + std::to_string(trackNumber);
            result.operations = {"track_mute"};
            result.stateChanges = {"track_" + std::to_string(trackNumber) + "_muted:true"};
            result.outputs["track_number"] = ActionValue(trackNumber);
            
            return core::Result<ActionResult>::success(result);
        },
        "Mute track action"
    );
}

// Session Actions
core::AsyncResult<core::Result<ActionResult>> SessionActions::saveSession(
    const std::vector<ActionParameter>& params,
    const ActionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<ActionResult>>(
        [params, context]() -> core::Result<ActionResult> {
            ActionResult result;
            result.actionId = "session_save";
            result.success = true;
            result.message = "Session saved successfully";
            result.operations = {"session_save"};
            result.stateChanges = {"session_saved:true", "last_save:now"};
            
            return core::Result<ActionResult>::success(result);
        },
        "Save session action"
    );
}

} // namespace actions

// ============================================================================
// Global Action API Instance
// ============================================================================

ActionAPI& getGlobalActionAPI() {
    static ActionAPI instance;
    return instance;
}

} // namespace mixmind::ai