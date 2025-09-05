#pragma once
#include "../core/result.h"
#include "../core/async.h"
#include "../audio/AudioBuffer.h"
#include "../core/ISession.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <map>
#include <set>
#include <unordered_map>

namespace mixmind::ai {

// Forward declaration for MixingIntelligence (would be implemented separately)
class MixingIntelligence {
public:
    // Mock interface for the AI mixing intelligence engine
    virtual ~MixingIntelligence() = default;
    virtual bool initialize() { return true; }
    virtual double analyzeMixQuality(const std::map<std::string, double>& metrics) { return 0.8; }
};

// ============================================================================
// Proactive AI Monitor - Continuously analyzes and suggests improvements
// ============================================================================

class ProactiveAIMonitor {
public:
    enum class SuggestionPriority {
        LOW = 1,        // Nice to have suggestion
        MEDIUM = 2,     // Should consider this
        HIGH = 3,       // Recommended action
        CRITICAL = 4    // Issue that needs immediate attention
    };
    
    struct ProactiveSuggestion {
        std::string id;                             // Unique suggestion ID
        std::string title;                          // "Mix too bright" 
        std::string description;                    // Detailed explanation
        std::string reasoning;                      // Why AI suggests this
        SuggestionPriority priority;
        std::chrono::steady_clock::time_point timestamp;
        
        // Action information
        std::string suggested_action;               // "Reduce high frequencies on master EQ"
        std::vector<std::string> affected_tracks;   // Which tracks are involved
        std::map<std::string, double> parameters;   // Suggested parameter changes
        
        // User interaction
        bool user_seen = false;                     // Has user seen this?
        bool user_accepted = false;                 // Did user accept suggestion?
        bool user_dismissed = false;                // Did user dismiss it?
        std::string user_feedback;                  // Optional user response
        
        // AI confidence and validation
        double confidence_score = 0.0;              // 0.0 - 1.0
        std::string validation_metric;              // How to measure if suggestion helped
        
        ProactiveSuggestion() = default;
        ProactiveSuggestion(const std::string& t, const std::string& desc, SuggestionPriority p)
            : title(t), description(desc), priority(p), timestamp(std::chrono::steady_clock::now()) {}
    };
    
    using SuggestionCallback = std::function<void(const std::vector<ProactiveSuggestion>&)>;
    using AlertCallback = std::function<void(const std::string& alert, SuggestionPriority priority)>;
    
    ProactiveAIMonitor();
    ~ProactiveAIMonitor();
    
    // ========================================================================
    // Service Lifecycle
    // ========================================================================
    
    /// Initialize proactive monitoring
    core::AsyncResult<core::VoidResult> initialize(
        std::shared_ptr<core::ISession> session,
        SuggestionCallback suggestion_callback,
        AlertCallback alert_callback = nullptr
    );
    
    /// Start monitoring user's session
    core::AsyncResult<core::VoidResult> startMonitoring();
    
    /// Stop monitoring
    core::VoidResult stopMonitoring();
    
    /// Check if currently monitoring
    bool isMonitoring() const { return is_monitoring_.load(); }
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /// Set how often to analyze the mix (default: every 10 seconds)
    void setAnalysisInterval(std::chrono::milliseconds interval);
    
    /// Set minimum confidence before making suggestions (default: 0.7)
    void setSuggestionThreshold(double threshold);
    
    /// Enable/disable specific types of suggestions
    void enableSuggestionType(const std::string& type, bool enabled);
    
    /// Set which tracks to monitor (empty = monitor all)
    void setTracksToMonitor(const std::vector<std::string>& track_names);
    
    // ========================================================================
    // Real-time Analysis
    // ========================================================================
    
    /// Force immediate analysis of current mix state
    core::AsyncResult<core::Result<std::vector<ProactiveSuggestion>>> analyzeCurrentMix();
    
    /// Get current mix quality score (0.0 - 1.0)
    double getCurrentMixQuality() const;
    
    /// Get real-time audio characteristics
    struct RealTimeMetrics {
        double overall_lufs = -70.0;
        double peak_db = -70.0;
        double dynamic_range = 0.0;
        double stereo_width = 0.0;
        std::map<std::string, double> frequency_balance;  // Low, mid, high energy
        std::vector<std::string> detected_issues;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    RealTimeMetrics getCurrentMetrics() const;
    
    // ========================================================================
    // Suggestion Management
    // ========================================================================
    
    /// Get all active suggestions
    std::vector<ProactiveSuggestion> getActiveSuggestions() const;
    
    /// Mark suggestion as seen by user
    core::VoidResult markSuggestionSeen(const std::string& suggestion_id);
    
    /// User accepts a suggestion
    core::VoidResult acceptSuggestion(const std::string& suggestion_id, const std::string& feedback = "");
    
    /// User dismisses a suggestion
    core::VoidResult dismissSuggestion(const std::string& suggestion_id, const std::string& reason = "");
    
    /// Clear all suggestions
    core::VoidResult clearSuggestions();
    
    // ========================================================================
    // Learning and Adaptation
    // ========================================================================
    
    /// Update AI based on user actions
    void learnFromUserAction(
        const std::string& action_type,
        const std::map<std::string, std::string>& context,
        bool was_suggestion_triggered
    );
    
    /// Get AI learning statistics
    struct LearningStats {
        uint32_t suggestions_made = 0;
        uint32_t suggestions_accepted = 0;
        uint32_t suggestions_dismissed = 0;
        double acceptance_rate = 0.0;
        std::map<std::string, uint32_t> most_accepted_types;
        std::map<std::string, uint32_t> most_dismissed_types;
        std::chrono::steady_clock::time_point learning_since;
    };
    
    LearningStats getLearningStats() const;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Detect user workflow patterns
    core::AsyncResult<core::Result<std::vector<std::string>>> detectWorkflowPatterns();
    
    /// Suggest workflow optimizations
    core::AsyncResult<core::Result<std::vector<std::string>>> suggestWorkflowOptimizations();
    
    /// Predict what user might want to do next
    core::AsyncResult<core::Result<std::vector<std::string>>> predictNextActions();
    
    /// Generate contextual help based on current situation
    core::AsyncResult<core::Result<std::string>> generateContextualHelp();

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Main monitoring loop running in background thread
    void monitoringLoop();
    
    /// Analyze current session state
    void analyzeSessionState();
    
    /// Generate suggestions based on analysis
    std::vector<ProactiveSuggestion> generateSuggestions(const RealTimeMetrics& metrics);
    
    /// Check for critical issues that need immediate attention
    std::vector<ProactiveSuggestion> checkForCriticalIssues(const RealTimeMetrics& metrics);
    
    /// Generate mix quality suggestions
    std::vector<ProactiveSuggestion> generateMixQualitySuggestions(const RealTimeMetrics& metrics);
    
    /// Generate workflow suggestions
    std::vector<ProactiveSuggestion> generateWorkflowSuggestions();
    
    /// Update learning model based on user feedback
    void updateLearningModel(const ProactiveSuggestion& suggestion, bool accepted);
    
    /// Check if suggestion should be made based on learning
    bool shouldMakeSuggestion(const ProactiveSuggestion& suggestion);
    
    // State management
    std::shared_ptr<core::ISession> session_;
    std::shared_ptr<ai::MixingIntelligence> mixing_ai_;
    
    // Threading and control
    std::atomic<bool> is_monitoring_{false};
    std::atomic<bool> should_stop_{false};
    std::thread monitoring_thread_;
    
    // Configuration
    std::chrono::milliseconds analysis_interval_{10000};  // 10 seconds default
    double suggestion_threshold_ = 0.7;
    std::set<std::string> enabled_suggestion_types_;
    std::vector<std::string> monitored_tracks_;
    
    // Callbacks
    SuggestionCallback suggestion_callback_;
    AlertCallback alert_callback_;
    
    // Current state
    mutable std::shared_mutex state_mutex_;
    RealTimeMetrics current_metrics_;
    std::vector<ProactiveSuggestion> active_suggestions_;
    double current_mix_quality_ = 0.0;
    
    // Learning and adaptation
    mutable std::mutex learning_mutex_;
    LearningStats learning_stats_;
    std::map<std::string, double> suggestion_type_weights_;  // Learned preferences
    std::vector<std::string> user_workflow_patterns_;
    
    // Internal suggestion management
    std::string generateSuggestionId();
    void notifyCallbacks(const std::vector<ProactiveSuggestion>& suggestions);
    void sendAlert(const std::string& message, SuggestionPriority priority);
    
    // Analysis helpers
    bool isSignificantChange(const RealTimeMetrics& previous, const RealTimeMetrics& current);
    double calculateMixQuality(const RealTimeMetrics& metrics);
    std::vector<std::string> detectAudioIssues(const RealTimeMetrics& metrics);
    
    static constexpr double CRITICAL_THRESHOLD = 0.9;
    static constexpr double HIGH_THRESHOLD = 0.8;
    static constexpr double MEDIUM_THRESHOLD = 0.6;
};

} // namespace mixmind::ai