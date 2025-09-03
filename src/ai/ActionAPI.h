#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../core/async.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <variant>
#include <optional>

namespace mixmind::ai {

// Forward declarations for core interfaces
namespace core {
    class ISession;
    class ITrack;
    class IClip;
    class ITransport;
    class IPluginHost;
}

// ============================================================================
// Action Parameter Types
// ============================================================================

using ActionValue = std::variant<
    std::string,
    int,
    double,
    bool,
    std::vector<std::string>,
    std::vector<int>,
    std::vector<double>
>;

struct ActionParameter {
    std::string name;
    std::string type;           // "string", "number", "boolean", "array"
    ActionValue value;
    bool required = true;
    std::string description;
    
    // Validation
    std::optional<ActionValue> minValue;
    std::optional<ActionValue> maxValue;
    std::vector<ActionValue> allowedValues;
    
    template<typename T>
    std::optional<T> getValue() const {
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        return std::nullopt;
    }
};

// ============================================================================
// Action Definition and Metadata
// ============================================================================

enum class ActionCategory {
    Session,        // Session management (new, open, save)
    Transport,      // Playback control (play, stop, record)
    Track,          // Track operations (create, delete, mute)
    Clip,           // Clip operations (create, edit, split)
    Plugin,         // Plugin management (insert, remove, automate)
    Mix,            // Mixing operations (volume, pan, effects)
    Edit,           // Editing operations (cut, copy, paste)
    Navigation,     // Navigation (goto, zoom, select)
    Automation,     // Automation (record, edit, clear)
    Export,         // Export/render operations
    Analysis,       // Audio analysis and visualization
    Utility         // Utility operations (undo, redo, preferences)
};

enum class ActionComplexity {
    Simple,         // Single operation
    Compound,       // Multiple related operations
    Workflow        // Complex multi-step workflow
};

struct ActionDefinition {
    std::string id;
    std::string name;
    std::string description;
    ActionCategory category;
    ActionComplexity complexity;
    
    // Parameters
    std::vector<ActionParameter> parameters;
    std::vector<ActionParameter> optionalParameters;
    
    // Natural language patterns
    std::vector<std::string> patterns;          // Regex patterns for matching
    std::vector<std::string> examples;          // Example phrases
    std::vector<std::string> synonyms;          // Alternative names
    
    // Execution context
    std::vector<std::string> requiredServices; // Required services/adapters
    std::vector<std::string> conflictsWith;    // Conflicting actions
    std::vector<std::string> prerequisites;    // Required state/conditions
    
    // Help and documentation
    std::string helpText;
    std::string syntax;
    std::vector<std::string> tags;
    
    // Metadata
    bool isUndoable = true;
    bool requiresConfirmation = false;
    bool isDestructive = false;
    double estimatedTime = 0.0;  // Seconds
};

// ============================================================================
// Action Execution Context and Result
// ============================================================================

struct ActionContext {
    std::string conversationId;
    std::string sessionId;
    std::string userId;
    
    // Current DAW state
    std::unordered_map<std::string, std::string> dawState;
    
    // User preferences
    std::unordered_map<std::string, std::string> userPreferences;
    
    // Execution options
    bool dryRun = false;                // Preview mode
    bool requireConfirmation = false;   // Ask for confirmation
    bool verbose = false;               // Detailed output
    
    // Progress tracking
    std::function<void(const std::string&, double)> progressCallback;
    std::function<void(const std::string&)> statusCallback;
};

struct ActionResult {
    std::string actionId;
    bool success;
    std::string message;
    
    // Execution details
    std::chrono::milliseconds executionTime{0};
    std::vector<std::string> operations;       // List of operations performed
    std::unordered_map<std::string, ActionValue> outputs; // Action outputs
    
    // State changes
    std::vector<std::string> stateChanges;     // What changed
    std::unordered_map<std::string, std::string> oldState;  // State before
    std::unordered_map<std::string, std::string> newState;  // State after
    
    // Undo information
    std::string undoActionId;
    std::unordered_map<std::string, ActionValue> undoData;
    
    // Error information
    std::string errorCode;
    std::string errorDetails;
    std::vector<std::string> warnings;
    
    // Suggestions
    std::vector<std::string> suggestions;      // Next suggested actions
    std::vector<std::string> alternatives;     // Alternative approaches
};

// ============================================================================
// Natural Language Processing
// ============================================================================

struct ParsedIntent {
    std::string intent;                 // Main intent (e.g., "create_track")
    std::string originalText;           // Original user input
    double confidence;                  // Confidence score 0.0-1.0
    
    // Extracted entities
    std::unordered_map<std::string, ActionValue> entities;
    
    // Context
    std::string context;                // Conversation context
    std::vector<std::string> modifiers; // Qualifiers (e.g., "quickly", "carefully")
    
    // Ambiguity handling
    std::vector<std::string> alternatives;      // Alternative interpretations
    std::vector<std::string> clarifications;    // Questions to ask user
    bool requiresClarification = false;
};

struct CommandSuggestion {
    std::string command;
    std::string description;
    double relevance;                   // Relevance score 0.0-1.0
    ActionCategory category;
    
    // Context matching
    std::vector<std::string> matchedKeywords;
    std::string reasoning;              // Why this was suggested
};

// ============================================================================
// Action API - Natural Language to DAW Operations
// ============================================================================

class ActionAPI {
public:
    using ActionHandler = std::function<core::AsyncResult<core::Result<ActionResult>>(
        const std::vector<ActionParameter>&, const ActionContext&)>;
    
    using IntentCallback = std::function<void(const ParsedIntent&)>;
    using ActionCallback = std::function<void(const ActionResult&)>;
    using ConfirmationCallback = std::function<bool(const std::string& action, const std::vector<std::string>& details)>;
    
    ActionAPI();
    ~ActionAPI();
    
    // Non-copyable
    ActionAPI(const ActionAPI&) = delete;
    ActionAPI& operator=(const ActionAPI&) = delete;
    
    // ========================================================================
    // Service Lifecycle
    // ========================================================================
    
    /// Initialize Action API with core service references
    core::AsyncResult<core::VoidResult> initialize(
        std::shared_ptr<core::ISession> session,
        std::shared_ptr<core::ITrack> track,
        std::shared_ptr<core::IClip> clip,
        std::shared_ptr<core::ITransport> transport,
        std::shared_ptr<core::IPluginHost> pluginHost
    );
    
    /// Shutdown Action API
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if API is ready
    bool isReady() const;
    
    // ========================================================================
    // Action Registration and Discovery
    // ========================================================================
    
    /// Register action handler
    core::VoidResult registerAction(
        const ActionDefinition& definition,
        ActionHandler handler
    );
    
    /// Unregister action
    core::VoidResult unregisterAction(const std::string& actionId);
    
    /// Get action definition
    std::optional<ActionDefinition> getActionDefinition(const std::string& actionId) const;
    
    /// List all available actions
    std::vector<ActionDefinition> getAllActions() const;
    
    /// Get actions by category
    std::vector<ActionDefinition> getActionsByCategory(ActionCategory category) const;
    
    /// Search actions by keywords
    std::vector<ActionDefinition> searchActions(const std::string& query) const;
    
    // ========================================================================
    // Natural Language Processing
    // ========================================================================
    
    /// Parse natural language input to intent
    core::AsyncResult<core::Result<ParsedIntent>> parseIntent(
        const std::string& input,
        const ActionContext& context = {}
    ) const;
    
    /// Get command suggestions for partial input
    core::AsyncResult<core::Result<std::vector<CommandSuggestion>>> getSuggestions(
        const std::string& partialInput,
        const ActionContext& context = {},
        size_t maxSuggestions = 10
    ) const;
    
    /// Validate parsed intent and parameters
    core::Result<std::vector<std::string>> validateIntent(const ParsedIntent& intent) const;
    
    /// Suggest corrections for invalid commands
    core::AsyncResult<core::Result<std::vector<std::string>>> suggestCorrections(
        const std::string& invalidInput,
        const std::string& errorMessage
    ) const;
    
    // ========================================================================
    // Action Execution
    // ========================================================================
    
    /// Execute action from natural language
    core::AsyncResult<core::Result<ActionResult>> executeCommand(
        const std::string& command,
        const ActionContext& context = {}
    );
    
    /// Execute action from parsed intent
    core::AsyncResult<core::Result<ActionResult>> executeIntent(
        const ParsedIntent& intent,
        const ActionContext& context = {}
    );
    
    /// Execute action directly with parameters
    core::AsyncResult<core::Result<ActionResult>> executeAction(
        const std::string& actionId,
        const std::vector<ActionParameter>& parameters,
        const ActionContext& context = {}
    );
    
    /// Batch execute multiple actions
    core::AsyncResult<core::Result<std::vector<ActionResult>>> executeBatch(
        const std::vector<std::string>& commands,
        const ActionContext& context = {}
    );
    
    // ========================================================================
    // Workflow and Macros
    // ========================================================================
    
    /// Record action sequence as macro
    core::VoidResult startMacroRecording(const std::string& macroName);
    
    /// Stop macro recording
    core::Result<std::string> stopMacroRecording();
    
    /// Execute recorded macro
    core::AsyncResult<core::Result<ActionResult>> executeMacro(
        const std::string& macroName,
        const ActionContext& context = {}
    );
    
    /// Get available macros
    std::vector<std::string> getAvailableMacros() const;
    
    /// Delete macro
    core::VoidResult deleteMacro(const std::string& macroName);
    
    // ========================================================================
    // Context and State Management
    // ========================================================================
    
    /// Update DAW state context
    core::VoidResult updateDAWState(const std::unordered_map<std::string, std::string>& state);
    
    /// Get current DAW state
    std::unordered_map<std::string, std::string> getDAWState() const;
    
    /// Set user preferences
    core::VoidResult setUserPreferences(const std::unordered_map<std::string, std::string>& preferences);
    
    /// Learn from user behavior
    core::VoidResult recordUserAction(
        const std::string& action,
        const ActionResult& result,
        const std::unordered_map<std::string, std::string>& metadata = {}
    );
    
    // ========================================================================
    // Help and Documentation
    // ========================================================================
    
    /// Get help for action
    std::string getActionHelp(const std::string& actionId) const;
    
    /// Get category help
    std::string getCategoryHelp(ActionCategory category) const;
    
    /// Generate command examples
    std::vector<std::string> generateExamples(
        const std::string& actionId,
        const ActionContext& context = {}
    ) const;
    
    /// Get contextual help based on current state
    core::AsyncResult<core::Result<std::string>> getContextualHelp(
        const ActionContext& context
    ) const;
    
    // ========================================================================
    // Analytics and Optimization
    // ========================================================================
    
    struct ActionStats {
        int totalExecutions = 0;
        int successfulExecutions = 0;
        int failedExecutions = 0;
        double averageExecutionTime = 0.0;
        
        // Most used actions
        std::unordered_map<std::string, int> actionUsageCounts;
        std::unordered_map<std::string, double> actionAverageTime;
        
        // User patterns
        std::vector<std::string> commonWorkflows;
        std::unordered_map<std::string, int> intentConfidenceDistribution;
        
        // Error patterns
        std::unordered_map<std::string, int> commonErrors;
        std::vector<std::string> frequentMisinterpretations;
    };
    
    ActionStats getActionStats() const;
    
    /// Reset statistics
    void resetActionStats();
    
    /// Generate usage report
    core::AsyncResult<core::Result<std::string>> generateUsageReport(
        const std::string& format = "json"  // json, markdown, csv
    ) const;
    
    // ========================================================================
    // Callbacks and Events
    // ========================================================================
    
    /// Set intent parsing callback
    void setIntentCallback(IntentCallback callback);
    
    /// Set action execution callback
    void setActionCallback(ActionCallback callback);
    
    /// Set confirmation callback
    void setConfirmationCallback(ConfirmationCallback callback);

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Register built-in actions
    void registerBuiltInActions();
    
    /// Register session actions
    void registerSessionActions();
    
    /// Register transport actions
    void registerTransportActions();
    
    /// Register track actions
    void registerTrackActions();
    
    /// Register clip actions
    void registerClipActions();
    
    /// Register plugin actions
    void registerPluginActions();
    
    /// Register mix actions
    void registerMixActions();
    
    /// Match natural language to action
    std::vector<std::pair<std::string, double>> matchActions(const std::string& input) const;
    
    /// Extract parameters from natural language
    std::vector<ActionParameter> extractParameters(
        const std::string& input,
        const ActionDefinition& actionDef
    ) const;
    
    /// Validate action parameters
    core::Result<std::vector<std::string>> validateParameters(
        const std::vector<ActionParameter>& parameters,
        const ActionDefinition& actionDef
    ) const;
    
    /// Build action context
    ActionContext buildContext(
        const std::string& conversationId = "",
        const std::unordered_map<std::string, std::string>& additionalContext = {}
    ) const;
    
    /// Update action statistics
    void updateStats(const std::string& actionId, const ActionResult& result);
    
    // Service references
    std::shared_ptr<core::ISession> session_;
    std::shared_ptr<core::ITrack> track_;
    std::shared_ptr<core::IClip> clip_;
    std::shared_ptr<core::ITransport> transport_;
    std::shared_ptr<core::IPluginHost> pluginHost_;
    
    // Action registry
    std::unordered_map<std::string, ActionDefinition> actions_;
    std::unordered_map<std::string, ActionHandler> handlers_;
    mutable std::shared_mutex actionsMutex_;
    
    // Natural language processing
    class NLPProcessor;
    std::unique_ptr<NLPProcessor> nlpProcessor_;
    
    // State management
    std::unordered_map<std::string, std::string> dawState_;
    std::unordered_map<std::string, std::string> userPreferences_;
    mutable std::mutex stateMutex_;
    
    // Macro recording
    bool isRecordingMacro_ = false;
    std::string currentMacroName_;
    std::vector<std::string> macroCommands_;
    std::unordered_map<std::string, std::vector<std::string>> savedMacros_;
    mutable std::mutex macroMutex_;
    
    // Callbacks
    IntentCallback intentCallback_;
    ActionCallback actionCallback_;
    ConfirmationCallback confirmationCallback_;
    
    // Statistics
    ActionStats stats_;
    mutable std::mutex statsMutex_;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
};

// ============================================================================
// Built-in Action Implementations
// ============================================================================

namespace actions {

/// Session management actions
class SessionActions {
public:
    static core::AsyncResult<core::Result<ActionResult>> createNewSession(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
    
    static core::AsyncResult<core::Result<ActionResult>> saveSession(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
    
    static core::AsyncResult<core::Result<ActionResult>> loadSession(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
};

/// Transport control actions
class TransportActions {
public:
    static core::AsyncResult<core::Result<ActionResult>> play(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
    
    static core::AsyncResult<core::Result<ActionResult>> stop(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
    
    static core::AsyncResult<core::Result<ActionResult>> record(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
    
    static core::AsyncResult<core::Result<ActionResult>> setTempo(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
};

/// Track management actions
class TrackActions {
public:
    static core::AsyncResult<core::Result<ActionResult>> createTrack(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
    
    static core::AsyncResult<core::Result<ActionResult>> deleteTrack(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
    
    static core::AsyncResult<core::Result<ActionResult>> muteTrack(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
    
    static core::AsyncResult<core::Result<ActionResult>> setTrackVolume(
        const std::vector<ActionParameter>& params,
        const ActionContext& context
    );
};

} // namespace actions

// ============================================================================
// Global Action API Instance
// ============================================================================

/// Get the global Action API instance
ActionAPI& getGlobalActionAPI();

} // namespace mixmind::ai