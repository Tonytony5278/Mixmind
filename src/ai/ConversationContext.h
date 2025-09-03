#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../core/async.h"
#include "ChatService.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>
#include <optional>
#include <functional>

namespace mixmind::ai {

// ============================================================================
// Session State and Context Tracking
// ============================================================================

enum class SessionStateType {
    Transport,      // Play state, tempo, time position
    Tracks,         // Track count, names, states
    Selection,      // Currently selected elements
    Focus,          // Current focus (track, clip, etc.)
    Project,        // Project metadata, settings
    UI,             // UI state, zoom, view mode
    Workflow,       // Current workflow step
    Performance     // CPU usage, memory, etc.
};

struct SessionStateSnapshot {
    std::string sessionId;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<SessionStateType, std::unordered_map<std::string, std::string>> state;
    
    // Diff from previous snapshot
    std::vector<std::string> changedKeys;
    std::unordered_map<std::string, std::string> previousValues;
    
    // Context metadata
    std::string triggerReason; // "user_action", "time_update", "state_change"
    std::string userId;
    bool isSignificant = false; // Whether this represents a meaningful change
};

// ============================================================================
// User Intent and Goal Tracking
// ============================================================================

enum class UserGoal {
    Recording,          // Recording audio or MIDI
    Editing,            // Editing existing content
    Mixing,             // Mixing and balancing
    Composing,          // Creating new musical content
    Learning,           // Learning DAW features
    Troubleshooting,    // Solving problems
    Exploring,          // Experimenting with features
    Collaborating,      // Working with others
    Finishing,          // Final polish/export
    Unknown             // Goal not clear yet
};

struct UserIntent {
    std::string intentId;
    UserGoal primaryGoal;
    std::vector<UserGoal> secondaryGoals;
    
    // Intent characteristics
    std::string confidence;         // "high", "medium", "low"
    std::string urgency;           // "immediate", "soon", "eventual"
    std::string complexity;        // "simple", "moderate", "complex"
    
    // Intent context
    std::vector<std::string> relatedEntities; // Tracks, clips, plugins mentioned
    std::string timeFrame;         // "now", "this session", "this project"
    std::string scope;             // "single_track", "entire_mix", "specific_section"
    
    // Intent progression
    std::chrono::system_clock::time_point identifiedAt;
    std::vector<std::string> stepsCompleted;
    std::vector<std::string> stepsRemaining;
    bool isResolved = false;
    
    // Learning and adaptation
    std::unordered_map<std::string, std::string> userPreferences;
    std::vector<std::string> successfulPatterns;
    std::vector<std::string> challengeAreas;
};

// ============================================================================
// Conversation Memory and History
// ============================================================================

struct ConversationMemory {
    std::string conversationId;
    
    // Short-term memory (current session)
    std::vector<ChatMessage> recentMessages;
    std::vector<std::string> recentActions;
    std::unordered_map<std::string, std::string> workingContext;
    
    // Medium-term memory (recent sessions)
    std::vector<std::string> recentTopics;
    std::unordered_map<std::string, int> conceptMentions;
    std::vector<UserIntent> activeIntents;
    
    // Long-term memory (user patterns)
    std::unordered_map<std::string, int> frequentCommands;
    std::unordered_map<std::string, std::string> learnedPreferences;
    std::vector<std::string> commonWorkflows;
    std::unordered_map<std::string, double> skillLevels; // Domain -> proficiency
    
    // Relationship memory
    std::unordered_map<std::string, std::vector<std::string>> entityRelationships;
    std::unordered_map<std::string, std::string> entityAliases; // "my main track" -> "Track 1"
    
    // Error and correction memory
    std::vector<std::string> commonMisunderstandings;
    std::unordered_map<std::string, std::string> correctionHistory;
};

// ============================================================================
// Context Aware Suggestions and Predictions
// ============================================================================

enum class SuggestionType {
    NextAction,         // Suggest next logical action
    Alternative,        // Alternative approach
    Optimization,       // Performance/workflow optimization
    Learning,           // Educational suggestion
    Correction,         // Error prevention/correction
    Workflow,           // Workflow completion
    Shortcut            // Faster way to achieve goal
};

struct ContextualSuggestion {
    SuggestionType type;
    std::string suggestion;
    std::string reasoning;
    double relevance;       // 0.0 - 1.0
    double confidence;      // 0.0 - 1.0
    
    // Suggestion context
    std::vector<std::string> triggerConditions;
    std::string applicableScope;    // When this applies
    std::vector<std::string> requiredState;  // Required DAW state
    
    // Action information
    std::string actionCommand;      // If actionable
    std::vector<std::string> prerequisites;
    std::string estimatedTime;
    std::string difficulty;         // "easy", "intermediate", "advanced"
    
    // Metadata
    std::chrono::system_clock::time_point generatedAt;
    std::string generatedBy;        // "rule_based", "ml_model", "pattern_matching"
    std::unordered_map<std::string, std::string> metadata;
};

// ============================================================================
// Workflow and Task Context
// ============================================================================

enum class WorkflowPhase {
    Setup,              // Initial project setup
    Recording,          // Recording phase
    Editing,            // Basic editing
    Arrangement,        // Song structure
    Mixing,             // Mixing and balancing  
    Mastering,          // Final processing
    Export,             // Export and delivery
    Review,             // Review and revision
    Collaboration,      // Working with others
    Learning            // Educational/exploratory
};

struct WorkflowContext {
    std::string workflowId;
    WorkflowPhase currentPhase;
    std::vector<WorkflowPhase> phaseHistory;
    
    // Progress tracking
    std::unordered_map<WorkflowPhase, double> phaseProgress; // 0.0 - 1.0
    std::vector<std::string> completedMilestones;
    std::vector<std::string> upcomingMilestones;
    
    // Context variables
    std::string projectType;        // "song", "podcast", "soundtrack", etc.
    std::string genre;              // Musical genre if applicable
    std::string complexity;         // "simple", "moderate", "complex"
    std::vector<std::string> techniques; // Techniques being used
    
    // Collaboration context
    std::vector<std::string> collaborators;
    std::unordered_map<std::string, std::string> roles; // User -> role
    std::vector<std::string> sharedElements;
    
    // Quality and standards
    std::string qualityTarget;      // "demo", "professional", "broadcast"
    std::vector<std::string> requirements;
    std::unordered_map<std::string, std::string> standards;
};

// ============================================================================
// Context Manager - Central context management system
// ============================================================================

class ConversationContextManager {
public:
    using StateChangeCallback = std::function<void(const SessionStateSnapshot&)>;
    using IntentChangeCallback = std::function<void(const UserIntent&)>;
    using SuggestionCallback = std::function<void(const std::vector<ContextualSuggestion>&)>;
    
    ConversationContextManager();
    ~ConversationContextManager();
    
    // Non-copyable
    ConversationContextManager(const ConversationContextManager&) = delete;
    ConversationContextManager& operator=(const ConversationContextManager&) = delete;
    
    // ========================================================================
    // Service Lifecycle
    // ========================================================================
    
    /// Initialize context manager
    core::AsyncResult<core::VoidResult> initialize();
    
    /// Shutdown context manager
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if manager is ready
    bool isReady() const { return isInitialized_.load(); }
    
    // ========================================================================
    // Session State Management
    // ========================================================================
    
    /// Update session state
    core::VoidResult updateSessionState(
        const std::string& conversationId,
        SessionStateType stateType,
        const std::unordered_map<std::string, std::string>& updates
    );
    
    /// Get current session state
    std::optional<SessionStateSnapshot> getCurrentState(const std::string& conversationId) const;
    
    /// Get state history
    std::vector<SessionStateSnapshot> getStateHistory(
        const std::string& conversationId,
        size_t maxSnapshots = 10
    ) const;
    
    /// Compare two state snapshots
    std::vector<std::string> compareStates(
        const SessionStateSnapshot& oldState,
        const SessionStateSnapshot& newState
    ) const;
    
    /// Detect significant state changes
    bool isSignificantChange(
        const SessionStateSnapshot& oldState,
        const SessionStateSnapshot& newState
    ) const;
    
    // ========================================================================
    // User Intent Tracking
    // ========================================================================
    
    /// Identify user intent from conversation
    core::AsyncResult<core::Result<UserIntent>> identifyIntent(
        const std::string& conversationId,
        const std::vector<ChatMessage>& recentMessages
    );
    
    /// Update user intent
    core::VoidResult updateIntent(
        const std::string& conversationId,
        const UserIntent& intent
    );
    
    /// Get active user intents
    std::vector<UserIntent> getActiveIntents(const std::string& conversationId) const;
    
    /// Mark intent as resolved
    core::VoidResult resolveIntent(
        const std::string& conversationId,
        const std::string& intentId,
        const std::string& resolution
    );
    
    /// Predict next user actions based on intent
    core::AsyncResult<core::Result<std::vector<std::string>>> predictNextActions(
        const std::string& conversationId
    ) const;
    
    // ========================================================================
    // Memory Management
    // ========================================================================
    
    /// Update conversation memory
    core::VoidResult updateMemory(
        const std::string& conversationId,
        const ChatMessage& message,
        const std::unordered_map<std::string, std::string>& context = {}
    );
    
    /// Get conversation memory
    std::optional<ConversationMemory> getMemory(const std::string& conversationId) const;
    
    /// Search memory for relevant information
    core::AsyncResult<core::Result<std::vector<std::string>>> searchMemory(
        const std::string& conversationId,
        const std::string& query
    ) const;
    
    /// Forget old or irrelevant information
    core::VoidResult forgetOldMemory(
        const std::string& conversationId,
        const std::chrono::hours& maxAge = std::chrono::hours(24)
    );
    
    /// Consolidate memory (compress old information)
    core::AsyncResult<core::VoidResult> consolidateMemory(const std::string& conversationId);
    
    // ========================================================================
    // Contextual Suggestions
    // ========================================================================
    
    /// Generate contextual suggestions
    core::AsyncResult<core::Result<std::vector<ContextualSuggestion>>> generateSuggestions(
        const std::string& conversationId,
        size_t maxSuggestions = 5
    );
    
    /// Get suggestions by type
    core::AsyncResult<core::Result<std::vector<ContextualSuggestion>>> getSuggestionsByType(
        const std::string& conversationId,
        SuggestionType type
    );
    
    /// Rate suggestion usefulness (learning feedback)
    core::VoidResult rateSuggestion(
        const std::string& conversationId,
        const std::string& suggestionId,
        double rating,  // 0.0 - 1.0
        const std::string& feedback = ""
    );
    
    /// Generate proactive help based on context
    core::AsyncResult<core::Result<std::vector<std::string>>> generateProactiveHelp(
        const std::string& conversationId
    );
    
    // ========================================================================
    // Workflow Context Management
    // ========================================================================
    
    /// Detect current workflow phase
    core::AsyncResult<core::Result<WorkflowPhase>> detectWorkflowPhase(
        const std::string& conversationId
    );
    
    /// Update workflow context
    core::VoidResult updateWorkflowContext(
        const std::string& conversationId,
        const WorkflowContext& context
    );
    
    /// Get workflow context
    std::optional<WorkflowContext> getWorkflowContext(const std::string& conversationId) const;
    
    /// Suggest workflow optimizations
    core::AsyncResult<core::Result<std::vector<std::string>>> suggestWorkflowOptimizations(
        const std::string& conversationId
    );
    
    // ========================================================================
    // Learning and Adaptation
    // ========================================================================
    
    /// Learn from user behavior
    core::VoidResult learnFromInteraction(
        const std::string& conversationId,
        const std::string& userAction,
        const std::string& context,
        bool wasSuccessful
    );
    
    /// Update user skill assessment
    core::VoidResult updateSkillAssessment(
        const std::string& conversationId,
        const std::string& domain,
        double proficiencyLevel
    );
    
    /// Get personalized recommendations
    core::AsyncResult<core::Result<std::vector<std::string>>> getPersonalizedRecommendations(
        const std::string& conversationId
    );
    
    /// Adapt conversation style to user
    core::AsyncResult<core::Result<std::string>> adaptConversationStyle(
        const std::string& conversationId,
        const std::string& defaultResponse
    );
    
    // ========================================================================
    // Context Persistence and Recovery
    // ========================================================================
    
    /// Save context to persistent storage
    core::AsyncResult<core::VoidResult> saveContext(const std::string& conversationId);
    
    /// Load context from persistent storage
    core::AsyncResult<core::VoidResult> loadContext(const std::string& conversationId);
    
    /// Export context data
    core::AsyncResult<core::Result<std::string>> exportContext(
        const std::string& conversationId,
        const std::string& format = "json"
    ) const;
    
    /// Import context data
    core::AsyncResult<core::VoidResult> importContext(
        const std::string& conversationId,
        const std::string& contextData,
        const std::string& format = "json"
    );
    
    /// Clear context data
    core::VoidResult clearContext(const std::string& conversationId);
    
    // ========================================================================
    // Analytics and Insights
    // ========================================================================
    
    struct ContextAnalytics {
        // Session metrics
        int totalSessions = 0;
        double averageSessionLength = 0.0;
        std::unordered_map<WorkflowPhase, double> phaseDistribution;
        
        // User behavior patterns
        std::unordered_map<std::string, int> commonIntents;
        std::unordered_map<std::string, double> skillProgressions;
        std::vector<std::string> learningPatterns;
        
        // Context effectiveness
        double suggestionAcceptanceRate = 0.0;
        double intentPredictionAccuracy = 0.0;
        std::unordered_map<std::string, double> featureUsage;
    };
    
    ContextAnalytics getAnalytics(const std::string& conversationId = "") const;
    
    /// Generate context insights report
    core::AsyncResult<core::Result<std::string>> generateInsightsReport(
        const std::string& conversationId
    ) const;
    
    // ========================================================================
    // Callbacks and Events
    // ========================================================================
    
    /// Set state change callback
    void setStateChangeCallback(StateChangeCallback callback);
    
    /// Set intent change callback
    void setIntentChangeCallback(IntentChangeCallback callback);
    
    /// Set suggestion callback
    void setSuggestionCallback(SuggestionCallback callback);

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Process state changes and generate events
    void processStateChange(
        const std::string& conversationId,
        const SessionStateSnapshot& newState
    );
    
    /// Generate suggestions based on current context
    std::vector<ContextualSuggestion> generateRuleBasedSuggestions(
        const std::string& conversationId
    ) const;
    
    /// Update user model with new information
    void updateUserModel(
        const std::string& conversationId,
        const std::string& information,
        const std::string& context
    );
    
    /// Analyze conversation patterns
    void analyzeConversationPatterns(const std::string& conversationId);
    
    /// Clean up old context data
    void cleanupOldContexts();
    
    /// Generate unique IDs
    std::string generateContextId() const;
    std::string generateIntentId() const;
    std::string generateSuggestionId() const;
    
    // Context storage
    std::unordered_map<std::string, SessionStateSnapshot> currentStates_;
    std::unordered_map<std::string, std::vector<SessionStateSnapshot>> stateHistory_;
    std::unordered_map<std::string, ConversationMemory> conversationMemories_;
    std::unordered_map<std::string, WorkflowContext> workflowContexts_;
    
    mutable std::shared_mutex contextMutex_;
    
    // Intent tracking
    std::unordered_map<std::string, std::vector<UserIntent>> activeIntents_;
    mutable std::mutex intentsMutex_;
    
    // Analytics
    ContextAnalytics analytics_;
    mutable std::mutex analyticsMutex_;
    
    // Callbacks
    StateChangeCallback stateChangeCallback_;
    IntentChangeCallback intentChangeCallback_;
    SuggestionCallback suggestionCallback_;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
    
    // Background processing
    std::atomic<bool> shouldShutdown_{false};
    std::thread backgroundProcessor_;
    std::condition_variable processingCondition_;
    std::mutex processingMutex_;
};

// ============================================================================
// Context Utilities
// ============================================================================

namespace context_utils {

/// Extract key information from DAW state
std::unordered_map<std::string, std::string> extractKeyInformation(
    const SessionStateSnapshot& state
);

/// Calculate state similarity score
double calculateStateSimilarity(
    const SessionStateSnapshot& state1,
    const SessionStateSnapshot& state2
);

/// Determine workflow phase from state
WorkflowPhase inferWorkflowPhase(const SessionStateSnapshot& state);

/// Generate context summary
std::string generateContextSummary(
    const ConversationMemory& memory,
    const SessionStateSnapshot& state
);

/// Merge context information
ConversationMemory mergeMemory(
    const ConversationMemory& memory1,
    const ConversationMemory& memory2
);

} // namespace context_utils

// ============================================================================
// Global Context Manager Instance
// ============================================================================

/// Get the global conversation context manager instance
ConversationContextManager& getGlobalContextManager();

} // namespace mixmind::ai