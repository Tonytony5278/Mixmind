#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include <functional>

namespace mixmind::ai {

// ============================================================================
// Intelligent Preference Learning System - CRITICAL COMPONENT 2
// Learns user preferences and adapts MixMind AI behavior
// ============================================================================

struct Context {
    std::string currentGenre;
    std::string currentProject;
    std::string currentTask;
    std::vector<std::string> activePlugins;
    float currentTempo;
    std::string timeSignature;
    int trackCount;
    std::string mixingPhase; // "tracking", "mixing", "mastering"
    std::map<std::string, std::string> metadata;
    
    std::string getCurrentGenre() const { return currentGenre; }
    std::string getCurrentTask() const { return currentTask; }
};

struct Value {
    enum Type { FLOAT, INT, STRING, BOOL } type;
    
    union {
        float floatValue;
        int intValue;
        bool boolValue;
    };
    std::string stringValue;
    
    Value(float f) : type(FLOAT), floatValue(f) {}
    Value(int i) : type(INT), intValue(i) {}
    Value(bool b) : type(BOOL), boolValue(b) {}
    Value(const std::string& s) : type(STRING), stringValue(s) {}
    
    operator float() const { return type == FLOAT ? floatValue : 0.0f; }
    operator int() const { return type == INT ? intValue : 0; }
    operator bool() const { return type == BOOL ? boolValue : false; }
    operator std::string() const { return type == STRING ? stringValue : ""; }
};

struct UserPreference {
    std::string category;           // "mixing", "plugin_selection", "workflow"
    std::string parameter;          // "vocal_compressor_ratio", "kick_eq_frequency"
    std::vector<Value> historicalValues;
    Value preferredValue{0.0f};
    float confidence = 0.0f;        // 0.0 to 1.0
    std::map<std::string, Value> contextualPreferences; // Different for genres
    std::chrono::system_clock::time_point lastUpdated;
    int usageCount = 0;
    
    UserPreference() : preferredValue(0.0f), lastUpdated(std::chrono::system_clock::now()) {}
};

struct MixProfile {
    std::string genre;
    float avgLoudness = -23.0f;      // LUFS
    float dynamicRange = 12.0f;      // DR units
    float stereoWidth = 0.7f;        // 0-1
    float bassEnergyRatio = 0.25f;   // 0-1
    float midEnergyRatio = 0.50f;    // 0-1
    float highEnergyRatio = 0.25f;   // 0-1
    
    struct EQCurve {
        std::map<float, float> frequencyResponse; // frequency -> gain in dB
    } eqCurve;
    
    struct CompressionStyle {
        float ratio = 3.0f;
        float attack = 10.0f;        // ms
        float release = 100.0f;      // ms
        float knee = 2.0f;           // dB
    } compressionStyle;
    
    std::vector<std::string> preferredPlugins;
    std::map<std::string, float> pluginDefaults;
};

class PreferenceLearning {
public:
    PreferenceLearning();
    ~PreferenceLearning();
    
    // Non-copyable
    PreferenceLearning(const PreferenceLearning&) = delete;
    PreferenceLearning& operator=(const PreferenceLearning&) = delete;
    
    // Learning from user actions
    void observeUserAction(const std::string& action, const Value& value, const Context& context);
    void observeParameterChange(const std::string& pluginId, const std::string& parameter, float value, const Context& context);
    void observePluginSelection(const std::string& pluginName, const Context& context);
    void observeWorkflowAction(const std::string& action, const Context& context);
    void observeMixDecision(const std::string& decision, const Value& value, const Context& context);
    
    // Predictive assistance
    void offerPredictiveAction(const Context& context);
    std::vector<std::string> generateSuggestions(const Context& context);
    Value predictPreferredValue(const std::string& parameter, const Context& context);
    
    // Genre-specific learning
    std::map<std::string, MixProfile> learnMixingStyles();
    MixProfile getMixProfileForGenre(const std::string& genre) const;
    void updateMixProfile(const std::string& genre, const MixProfile& profile);
    
    // Plugin preference learning
    std::vector<std::string> getPreferredPluginsForTask(const std::string& task, const std::string& genre = "");
    std::map<std::string, float> getPreferredPluginSettings(const std::string& pluginName, const std::string& genre = "");
    void learnPluginUsagePattern(const std::string& pluginName, const std::string& task, const Context& context);
    
    // Workflow optimization
    struct WorkflowPattern {
        std::string name;
        std::vector<std::string> actionSequence;
        float frequency;
        std::string triggerContext;
        float timeSavingPotential; // minutes
    };
    
    std::vector<WorkflowPattern> identifyWorkflowPatterns();
    void suggestWorkflowOptimization(const Context& context);
    
    // Adaptive suggestion system
    struct Suggestion {
        enum Type {
            PARAMETER_ADJUSTMENT,
            PLUGIN_RECOMMENDATION,
            WORKFLOW_OPTIMIZATION,
            MIX_GUIDANCE,
            CREATIVE_INSPIRATION
        } type;
        
        std::string title;
        std::string description;
        std::string action;
        float confidence;
        float potentialImpact; // 1-10 scale
        std::map<std::string, Value> parameters;
        
        Suggestion(Type t, const std::string& title, const std::string& desc, float conf = 0.8f)
            : type(t), title(title), description(desc), confidence(conf), potentialImpact(5.0f) {}
    };
    
    std::vector<Suggestion> generateAdaptiveSuggestions(const Context& context);
    void processSuggestionFeedback(const std::string& suggestionId, bool accepted, float userRating);
    
    // User profile management
    struct UserProfile {
        std::string userId;
        std::string skillLevel;     // "beginner", "intermediate", "advanced", "professional"
        std::vector<std::string> primaryGenres;
        std::map<std::string, UserPreference> preferences;
        std::map<std::string, MixProfile> genreProfiles;
        std::vector<WorkflowPattern> workflows;
        
        struct LearningMetadata {
            std::chrono::system_clock::time_point firstSession;
            std::chrono::system_clock::time_point lastSession;
            int totalSessions = 0;
            int totalProjects = 0;
            float learningRate = 1.0f; // How quickly to adapt to new patterns
        } metadata;
    };
    
    UserProfile getUserProfile() const;
    void updateUserProfile(const UserProfile& profile);
    void exportUserProfile(const std::string& filePath) const;
    mixmind::core::AsyncResult<void> importUserProfile(const std::string& filePath);
    
    // Advanced learning features
    void enableContextualLearning(bool enabled);
    void setLearningRate(float rate); // 0.1 (slow) to 2.0 (fast)
    void setConfidenceThreshold(float threshold); // Minimum confidence for suggestions
    
    // Pattern recognition
    struct Pattern {
        std::string name;
        std::string description;
        std::vector<std::string> conditions;
        std::vector<std::string> actions;
        float strength;
        int occurrenceCount;
    };
    
    std::vector<Pattern> detectPatterns();
    void addPattern(const Pattern& pattern);
    void removePattern(const std::string& patternName);
    
    // Real-time adaptation
    void startRealTimeAdaptation();
    void stopRealTimeAdaptation();
    bool isRealTimeAdaptationEnabled() const;
    
    // Callback system for integration
    using SuggestionCallback = std::function<void(const std::vector<Suggestion>&)>;
    using PreferenceUpdateCallback = std::function<void(const std::string& parameter, const Value& newValue)>;
    using PatternDetectionCallback = std::function<void(const Pattern& pattern)>;
    
    void setSuggestionCallback(SuggestionCallback callback);
    void setPreferenceUpdateCallback(PreferenceUpdateCallback callback);
    void setPatternDetectionCallback(PatternDetectionCallback callback);
    
    // Analytics and insights
    struct LearningAnalytics {
        int totalActionsObserved = 0;
        int totalPreferencesLearned = 0;
        int totalSuggestionsGenerated = 0;
        int totalSuggestionsAccepted = 0;
        float averageSuggestionConfidence = 0.0f;
        std::map<std::string, int> mostUsedPlugins;
        std::map<std::string, int> mostCommonActions;
        std::chrono::hours totalLearningTime{0};
    };
    
    LearningAnalytics getAnalytics() const;
    void resetAnalytics();
    
    // Collaborative learning (optional cloud features)
    void enableCollaborativeLearning(bool enabled);
    void shareAnonymizedPatterns(bool enabled);
    mixmind::core::AsyncResult<std::vector<Pattern>> downloadCommunityPatterns(const std::string& genre);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Context Analyzer - Understands current user situation
// ============================================================================

class ContextAnalyzer {
public:
    ContextAnalyzer();
    ~ContextAnalyzer();
    
    // Context detection
    Context analyzeCurrentContext() const;
    std::string inferCurrentTask(const Context& context) const;
    std::string inferUserIntent(const std::string& recentActions) const;
    
    // Genre detection
    std::string detectGenreFromAudio(const std::vector<float>& audioSamples) const;
    std::string detectGenreFromMIDI(const std::vector<uint8_t>& midiData) const;
    std::string detectGenreFromPlugins(const std::vector<std::string>& plugins) const;
    
    // Mixing phase detection
    std::string detectMixingPhase(const Context& context) const;
    bool isUserRecording(const Context& context) const;
    bool isUserMixing(const Context& context) const;
    bool isUserMastering(const Context& context) const;
    
    // Advanced context understanding
    float estimateProjectComplexity(const Context& context) const;
    std::string inferUserSkillLevel(const Context& context) const;
    float estimateTimeToCompletion(const Context& context) const;
    
    // Context history
    void addContextToHistory(const Context& context);
    std::vector<Context> getContextHistory(std::chrono::minutes duration) const;
    Context predictNextContext(const Context& current) const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Adaptive AI Assistant - Uses learning to provide better assistance
// ============================================================================

class AdaptiveAIAssistant {
public:
    AdaptiveAIAssistant();
    ~AdaptiveAIAssistant();
    
    // Set learning system
    void setPreferenceLearning(std::shared_ptr<PreferenceLearning> learning);
    void setContextAnalyzer(std::shared_ptr<ContextAnalyzer> analyzer);
    
    // Adaptive assistance
    std::string generateAdaptiveResponse(const std::string& userQuery, const Context& context);
    std::vector<std::string> generateContextualSuggestions(const Context& context);
    std::string provideMixingGuidance(const std::string& issue, const Context& context);
    
    // Personalized recommendations
    std::vector<std::string> recommendPluginsForUser(const std::string& task, const Context& context);
    std::map<std::string, float> recommendParameterSettings(const std::string& pluginName, const Context& context);
    std::vector<std::string> recommendNextSteps(const Context& context);
    
    // Learning integration
    void processUserFeedback(const std::string& response, const std::string& feedback);
    void adaptToUserStyle(const Context& context);
    
    // Proactive assistance
    void enableProactiveMode(bool enabled);
    void setProactivityLevel(float level); // 0.0 to 1.0
    std::vector<std::string> generateProactiveInsights(const Context& context);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace mixmind::ai