#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../core/async.h"
#include "ActionAPI.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <regex>
#include <optional>
#include <functional>

namespace mixmind::ai {

// ============================================================================
// Audio Production Domain Knowledge
// ============================================================================

enum class AudioProductionDomain {
    Recording,      // Recording audio, setting levels
    Editing,        // Cut, copy, paste, trim operations
    Mixing,         // Volume, pan, EQ, effects
    Composition,    // MIDI, notes, instruments
    Mastering,      // Final processing, loudness
    Arrangement,    // Song structure, sections
    Automation,     // Parameter automation
    Analysis,       // Spectral, level analysis
    Workflow,       // Session management, organization
    Technical       // Hardware, routing, settings
};

struct DomainConcept {
    std::string concept;                    // e.g., "compressor", "reverb", "track"
    AudioProductionDomain domain;
    std::vector<std::string> synonyms;      // Alternative terms
    std::vector<std::string> relatedConcepts;
    std::string definition;
    std::vector<std::string> typicalActions; // Common actions with this concept
};

// ============================================================================
// Intent Classification and Confidence
// ============================================================================

enum class IntentType {
    Command,        // Direct command (e.g., "mute track 1")
    Query,          // Information request (e.g., "what's the current tempo?")
    Help,           // Help request (e.g., "how do I add reverb?")
    Navigation,     // Navigation (e.g., "go to bar 32")
    Preference,     // Setting preferences (e.g., "set default fade to 1 second")
    Conversation,   // General conversation (e.g., "that sounds good")
    Clarification,  // Asking for clarification
    Feedback        // Providing feedback on results
};

struct IntentFeatures {
    // Linguistic features
    std::vector<std::string> keywords;
    std::vector<std::string> actionWords;   // Verbs indicating actions
    std::vector<std::string> objectWords;   // Nouns (tracks, clips, effects)
    std::vector<std::string> modifiers;     // Adjectives, adverbs
    std::vector<std::string> quantifiers;   // Numbers, amounts
    
    // Grammatical features
    bool hasQuestion = false;
    bool hasImperative = false;
    bool hasNegation = false;
    bool hasConditional = false;
    
    // Domain-specific features
    std::vector<std::string> audioTerms;
    std::vector<std::string> technicalTerms;
    std::vector<std::string> musicalTerms;
    
    // Contextual features
    std::vector<std::string> timeReferences;    // "now", "later", "at bar 16"
    std::vector<std::string> scopeReferences;   // "all tracks", "selected clips"
    std::vector<std::string> valueReferences;   // "louder", "50%", "-6dB"
};

struct IntentClassification {
    IntentType type;
    std::string specificIntent;            // Specific action intent
    double confidence;                     // 0.0 - 1.0
    AudioProductionDomain domain;
    
    IntentFeatures features;
    
    // Alternative interpretations
    std::vector<std::pair<std::string, double>> alternatives;
    
    // Confidence breakdown
    double linguisticConfidence = 0.0;
    double domainConfidence = 0.0;
    double contextualConfidence = 0.0;
    
    // Disambiguation info
    bool needsClarification = false;
    std::vector<std::string> clarificationQuestions;
    std::vector<std::string> assumptions;
};

// ============================================================================
// Entity Recognition and Extraction
// ============================================================================

enum class EntityType {
    // Core DAW entities
    Track,          // Track names/numbers
    Clip,           // Clip names/positions
    Plugin,         // Plugin names/types
    Parameter,      // Plugin/track parameters
    
    // Musical entities
    Note,           // Musical notes (C4, A#)
    Chord,          // Chord names (Cmaj7, F#m)
    Tempo,          // BPM values
    TimeSignature,  // 4/4, 3/4, etc.
    Key,            // Musical keys (C major, Bb minor)
    
    // Technical entities
    Frequency,      // 440Hz, 1kHz
    Level,          // dB values, percentages
    Time,           // Time positions, durations
    Sample,         // Sample names/paths
    
    // Quantitative entities
    Number,         // Generic numbers
    Percentage,     // Percentage values
    Range,          // Value ranges (1-10, 0% to 100%)
    
    // Qualitative entities
    Quality,        // "bright", "warm", "punchy"
    Intensity,      // "loud", "soft", "subtle"
    Direction,      // "up", "down", "left", "right"
    
    // Context entities
    Selection,      // "selected", "all", "current"
    Location,       // "here", "at the beginning", "bar 32"
    Condition       // "if", "when", "unless"
};

struct Entity {
    EntityType type;
    std::string text;                   // Original text
    std::string value;                  // Normalized value
    std::string unit;                   // Unit if applicable
    
    // Position in original text
    size_t startPos;
    size_t endPos;
    
    // Confidence and alternatives
    double confidence;
    std::vector<std::string> alternatives;
    
    // Additional metadata
    std::unordered_map<std::string, std::string> metadata;
};

// ============================================================================
// Context-Aware Intent Recognition
// ============================================================================

struct ConversationContext {
    std::string conversationId;
    std::vector<std::string> recentIntents;    // Last few intents
    std::unordered_map<std::string, std::string> sessionState; // Current DAW state
    
    // User interaction patterns
    std::vector<std::string> frequentActions;
    std::unordered_map<std::string, int> conceptUsage;
    
    // Current focus
    std::string currentTrack;
    std::string currentClip;
    std::string currentTimePosition;
    std::vector<std::string> selectedElements;
    
    // Workflow context
    std::string currentWorkflowStep;
    std::vector<std::string> workflowHistory;
};

struct IntentRecognitionContext {
    ConversationContext conversation;
    AudioProductionDomain primaryDomain;
    std::string userExpertiseLevel;         // "beginner", "intermediate", "advanced"
    
    // Disambiguation history
    std::vector<std::string> recentClarifications;
    std::unordered_map<std::string, std::string> assumptionHistory;
    
    // Error patterns
    std::vector<std::string> frequentMisunderstandings;
    std::unordered_map<std::string, std::string> correctionHistory;
};

// ============================================================================
// Intent Recognition Engine
// ============================================================================

class IntentRecognition {
public:
    using EntityExtractor = std::function<std::vector<Entity>(const std::string&)>;
    using IntentClassifier = std::function<IntentClassification(const std::string&, const IntentFeatures&)>;
    using ContextEnricher = std::function<void(IntentClassification&, const IntentRecognitionContext&)>;
    
    IntentRecognition();
    ~IntentRecognition();
    
    // Non-copyable
    IntentRecognition(const IntentRecognition&) = delete;
    IntentRecognition& operator=(const IntentRecognition&) = delete;
    
    // ========================================================================
    // Service Lifecycle
    // ========================================================================
    
    /// Initialize intent recognition engine
    core::AsyncResult<core::VoidResult> initialize();
    
    /// Shutdown engine
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if engine is ready
    bool isReady() const;
    
    /// Load domain knowledge
    core::AsyncResult<core::VoidResult> loadDomainKnowledge(const std::string& knowledgeFile = "");
    
    /// Update user model based on interactions
    core::VoidResult updateUserModel(
        const std::string& userId,
        const std::string& intent,
        const IntentClassification& classification,
        bool wasCorrect
    );
    
    // ========================================================================
    // Intent Classification
    // ========================================================================
    
    /// Classify intent from natural language input
    core::AsyncResult<core::Result<IntentClassification>> classifyIntent(
        const std::string& input,
        const IntentRecognitionContext& context = {}
    );
    
    /// Classify with custom confidence threshold
    core::AsyncResult<core::Result<IntentClassification>> classifyIntentWithThreshold(
        const std::string& input,
        double confidenceThreshold,
        const IntentRecognitionContext& context = {}
    );
    
    /// Get multiple intent classifications ranked by confidence
    core::AsyncResult<core::Result<std::vector<IntentClassification>>> getRankedIntents(
        const std::string& input,
        size_t maxResults = 5,
        const IntentRecognitionContext& context = {}
    );
    
    // ========================================================================
    // Entity Recognition
    // ========================================================================
    
    /// Extract entities from text
    core::AsyncResult<core::Result<std::vector<Entity>>> extractEntities(
        const std::string& input,
        const std::vector<EntityType>& targetTypes = {}
    );
    
    /// Extract entities with domain context
    core::AsyncResult<core::Result<std::vector<Entity>>> extractEntitiesInDomain(
        const std::string& input,
        AudioProductionDomain domain
    );
    
    /// Resolve entity references (e.g., "it", "that track", "the last clip")
    core::Result<std::vector<Entity>> resolveEntityReferences(
        const std::vector<Entity>& entities,
        const IntentRecognitionContext& context
    );
    
    // ========================================================================
    // Context Management
    // ========================================================================
    
    /// Update conversation context
    core::VoidResult updateConversationContext(
        const std::string& conversationId,
        const std::string& intent,
        const std::unordered_map<std::string, std::string>& stateChanges
    );
    
    /// Get conversation context
    std::optional<ConversationContext> getConversationContext(const std::string& conversationId) const;
    
    /// Clear conversation context
    core::VoidResult clearConversationContext(const std::string& conversationId);
    
    /// Merge session state into context
    core::VoidResult updateSessionState(
        const std::string& conversationId,
        const std::unordered_map<std::string, std::string>& sessionState
    );
    
    // ========================================================================
    // Disambiguation and Clarification
    // ========================================================================
    
    /// Generate clarification questions for ambiguous input
    core::AsyncResult<core::Result<std::vector<std::string>>> generateClarificationQuestions(
        const IntentClassification& ambiguousIntent
    );
    
    /// Process clarification response
    core::AsyncResult<core::Result<IntentClassification>> processClarification(
        const std::string& originalInput,
        const std::string& clarificationResponse,
        const IntentClassification& originalClassification
    );
    
    /// Suggest corrections for unrecognized input
    core::AsyncResult<core::Result<std::vector<std::string>>> suggestCorrections(
        const std::string& unrecognizedInput,
        const IntentRecognitionContext& context = {}
    );
    
    // ========================================================================
    // Learning and Adaptation
    // ========================================================================
    
    /// Learn from user feedback
    core::VoidResult learnFromFeedback(
        const std::string& input,
        const IntentClassification& predictedIntent,
        const std::string& actualIntent,
        const std::string& feedback
    );
    
    /// Add custom intent pattern
    core::VoidResult addCustomPattern(
        const std::string& intent,
        const std::string& pattern,
        AudioProductionDomain domain = AudioProductionDomain::Workflow
    );
    
    /// Add domain concept
    core::VoidResult addDomainConcept(const DomainConcept& concept);
    
    /// Update concept relationships
    core::VoidResult updateConceptRelationships(
        const std::string& concept,
        const std::vector<std::string>& relatedConcepts
    );
    
    // ========================================================================
    // Analysis and Insights
    // ========================================================================
    
    struct RecognitionStats {
        int totalClassifications = 0;
        int successfulClassifications = 0;
        int ambiguousClassifications = 0;
        int failedClassifications = 0;
        
        double averageConfidence = 0.0;
        std::unordered_map<std::string, int> intentDistribution;
        std::unordered_map<std::string, double> domainAccuracy;
        
        // User patterns
        std::unordered_map<std::string, int> userIntentPatterns;
        std::unordered_map<std::string, std::vector<std::string>> frequentMisclassifications;
    };
    
    RecognitionStats getRecognitionStats() const;
    
    /// Generate recognition quality report
    core::AsyncResult<core::Result<std::string>> generateQualityReport() const;
    
    /// Get user interaction insights
    core::AsyncResult<core::Result<std::string>> getUserInsights(const std::string& userId) const;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Detect workflow patterns
    core::AsyncResult<core::Result<std::vector<std::string>>> detectWorkflowPatterns(
        const std::vector<std::string>& intentSequence
    );
    
    /// Predict next likely intents
    core::AsyncResult<core::Result<std::vector<std::string>>> predictNextIntents(
        const IntentRecognitionContext& context,
        size_t maxPredictions = 5
    );
    
    /// Generate proactive suggestions
    core::AsyncResult<core::Result<std::vector<std::string>>> generateProactiveSuggestions(
        const IntentRecognitionContext& context
    );
    
    /// Analyze conversation flow
    core::AsyncResult<core::Result<std::string>> analyzeConversationFlow(
        const std::string& conversationId
    );

private:
    // ========================================================================
    // Internal Components
    // ========================================================================
    
    class FeatureExtractor;
    class PatternMatcher;
    class EntityRecognizer;
    class ContextProcessor;
    class LearningEngine;
    
    /// Initialize built-in patterns and concepts
    void initializeBuiltInKnowledge();
    
    /// Load audio production domain knowledge
    void loadAudioProductionDomain();
    
    /// Initialize entity recognizers
    void initializeEntityRecognizers();
    
    /// Extract linguistic features
    IntentFeatures extractFeatures(const std::string& input) const;
    
    /// Apply context to classification
    void enrichWithContext(
        IntentClassification& classification,
        const IntentRecognitionContext& context
    ) const;
    
    /// Calculate classification confidence
    double calculateConfidence(
        const IntentClassification& classification,
        const IntentFeatures& features
    ) const;
    
    /// Update learning models
    void updateLearningModels(
        const std::string& input,
        const IntentClassification& classification,
        bool wasCorrect
    );
    
    // Core components
    std::unique_ptr<FeatureExtractor> featureExtractor_;
    std::unique_ptr<PatternMatcher> patternMatcher_;
    std::unique_ptr<EntityRecognizer> entityRecognizer_;
    std::unique_ptr<ContextProcessor> contextProcessor_;
    std::unique_ptr<LearningEngine> learningEngine_;
    
    // Knowledge bases
    std::unordered_map<std::string, DomainConcept> domainConcepts_;
    std::unordered_map<std::string, std::vector<std::string>> intentPatterns_;
    std::unordered_map<EntityType, std::vector<std::regex>> entityPatterns_;
    
    // Context management
    std::unordered_map<std::string, ConversationContext> conversationContexts_;
    mutable std::shared_mutex contextMutex_;
    
    // User models and learning
    std::unordered_map<std::string, std::unordered_map<std::string, double>> userModels_;
    mutable std::mutex learningMutex_;
    
    // Statistics
    RecognitionStats stats_;
    mutable std::mutex statsMutex_;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> knowledgeLoaded_{false};
};

// ============================================================================
// Built-in Intent Patterns for Audio Production
// ============================================================================

namespace patterns {

/// Transport control patterns
extern const std::vector<std::string> TRANSPORT_PATTERNS;

/// Track management patterns  
extern const std::vector<std::string> TRACK_PATTERNS;

/// Mixing and effects patterns
extern const std::vector<std::string> MIXING_PATTERNS;

/// Recording patterns
extern const std::vector<std::string> RECORDING_PATTERNS;

/// Editing patterns
extern const std::vector<std::string> EDITING_PATTERNS;

/// Navigation patterns
extern const std::vector<std::string> NAVIGATION_PATTERNS;

/// Query patterns
extern const std::vector<std::string> QUERY_PATTERNS;

/// Help patterns
extern const std::vector<std::string> HELP_PATTERNS;

} // namespace patterns

// ============================================================================
// Global Intent Recognition Instance  
// ============================================================================

/// Get the global intent recognition instance
IntentRecognition& getGlobalIntentRecognition();

} // namespace mixmind::ai