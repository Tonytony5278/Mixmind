#include "ProactiveMonitor.h"
#include "../core/async.h"
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iomanip>

namespace mixmind::ai {

ProactiveAIMonitor::ProactiveAIMonitor() {
    // Initialize with default enabled suggestion types
    enabled_suggestion_types_ = {
        "mix_balance",
        "frequency_issues", 
        "dynamics_problems",
        "stereo_issues",
        "workflow_optimization",
        "creative_suggestions"
    };
    
    // Initialize learning statistics
    learning_stats_.learning_since = std::chrono::steady_clock::now();
}

ProactiveAIMonitor::~ProactiveAIMonitor() {
    if (isMonitoring()) {
        stopMonitoring();
    }
}

// ========================================================================
// Service Lifecycle
// ========================================================================

core::AsyncResult<core::VoidResult> ProactiveAIMonitor::initialize(
    std::shared_ptr<core::ISession> session,
    SuggestionCallback suggestion_callback,
    AlertCallback alert_callback
) {
    return core::async<core::VoidResult>([this, session, suggestion_callback, alert_callback]() -> core::VoidResult {
        try {
            if (!session) {
                return core::VoidResult::failure("Session pointer is null");
            }
            
            session_ = session;
            suggestion_callback_ = suggestion_callback;
            alert_callback_ = alert_callback;
            
            // Initialize mixing intelligence (would use real implementation)
            // For now, create a mock that provides the interface
            mixing_ai_ = std::make_shared<ai::MixingIntelligence>();
            
            std::cout << "🧠 Proactive AI Monitor initialized successfully" << std::endl;
            std::cout << "   Analysis interval: " << analysis_interval_.count() << "ms" << std::endl;
            std::cout << "   Suggestion threshold: " << suggestion_threshold_ << std::endl;
            std::cout << "   Enabled suggestion types: " << enabled_suggestion_types_.size() << std::endl;
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to initialize ProactiveAIMonitor: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> ProactiveAIMonitor::startMonitoring() {
    return core::async<core::VoidResult>([this]() -> core::VoidResult {
        if (is_monitoring_.load()) {
            return core::VoidResult::failure("Already monitoring");
        }
        
        if (!session_ || !suggestion_callback_) {
            return core::VoidResult::failure("Not properly initialized");
        }
        
        should_stop_.store(false);
        is_monitoring_.store(true);
        
        // Start background monitoring thread
        monitoring_thread_ = std::thread(&ProactiveAIMonitor::monitoringLoop, this);
        
        std::cout << "🧠 Started proactive AI monitoring" << std::endl;
        if (alert_callback_) {
            alert_callback_("Proactive AI monitoring started - I'll watch your mix and suggest improvements", 
                          SuggestionPriority::MEDIUM);
        }
        
        return core::VoidResult::success();
    });
}

core::VoidResult ProactiveAIMonitor::stopMonitoring() {
    if (!is_monitoring_.load()) {
        return core::VoidResult::success();
    }
    
    should_stop_.store(true);
    is_monitoring_.store(false);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    std::cout << "🧠 Stopped proactive AI monitoring" << std::endl;
    return core::VoidResult::success();
}

// ========================================================================
// Configuration
// ========================================================================

void ProactiveAIMonitor::setAnalysisInterval(std::chrono::milliseconds interval) {
    analysis_interval_ = interval;
    std::cout << "🧠 Analysis interval set to " << interval.count() << "ms" << std::endl;
}

void ProactiveAIMonitor::setSuggestionThreshold(double threshold) {
    suggestion_threshold_ = std::clamp(threshold, 0.0, 1.0);
    std::cout << "🧠 Suggestion threshold set to " << suggestion_threshold_ << std::endl;
}

void ProactiveAIMonitor::enableSuggestionType(const std::string& type, bool enabled) {
    if (enabled) {
        enabled_suggestion_types_.insert(type);
        std::cout << "🧠 Enabled suggestion type: " << type << std::endl;
    } else {
        enabled_suggestion_types_.erase(type);
        std::cout << "🧠 Disabled suggestion type: " << type << std::endl;
    }
}

void ProactiveAIMonitor::setTracksToMonitor(const std::vector<std::string>& track_names) {
    monitored_tracks_ = track_names;
    
    if (track_names.empty()) {
        std::cout << "🧠 Monitoring all tracks" << std::endl;
    } else {
        std::cout << "🧠 Monitoring " << track_names.size() << " specific tracks" << std::endl;
    }
}

// ========================================================================
// Real-time Analysis
// ========================================================================

core::AsyncResult<core::Result<std::vector<ProactiveAIMonitor::ProactiveSuggestion>>> 
ProactiveAIMonitor::analyzeCurrentMix() {
    return core::async<core::Result<std::vector<ProactiveSuggestion>>>([this]() 
        -> core::Result<std::vector<ProactiveSuggestion>> {
        
        try {
            // Get current metrics
            auto metrics = getCurrentMetrics();
            
            // Generate suggestions based on analysis
            auto suggestions = generateSuggestions(metrics);
            
            // Check for critical issues
            auto critical_issues = checkForCriticalIssues(metrics);
            suggestions.insert(suggestions.end(), critical_issues.begin(), critical_issues.end());
            
            // Sort by priority (critical first)
            std::sort(suggestions.begin(), suggestions.end(), 
                [](const ProactiveSuggestion& a, const ProactiveSuggestion& b) {
                    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
                });
            
            return core::Result<std::vector<ProactiveSuggestion>>::success(suggestions);
            
        } catch (const std::exception& e) {
            return core::Result<std::vector<ProactiveSuggestion>>::failure(
                "Failed to analyze current mix: " + std::string(e.what()));
        }
    });
}

double ProactiveAIMonitor::getCurrentMixQuality() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    return current_mix_quality_;
}

ProactiveAIMonitor::RealTimeMetrics ProactiveAIMonitor::getCurrentMetrics() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    
    // In real implementation, this would analyze actual audio from the session
    // For now, generate realistic mock metrics
    RealTimeMetrics metrics;
    metrics.timestamp = std::chrono::steady_clock::now();
    
    // Simulate realistic audio analysis
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> lufs_dist(-18.0, -8.0);
    static std::uniform_real_distribution<> peak_dist(-6.0, -0.1);
    static std::uniform_real_distribution<> dr_dist(4.0, 15.0);
    static std::uniform_real_distribution<> width_dist(0.3, 1.0);
    
    metrics.overall_lufs = lufs_dist(gen);
    metrics.peak_db = peak_dist(gen);
    metrics.dynamic_range = dr_dist(gen);
    metrics.stereo_width = width_dist(gen);
    
    // Frequency balance (should sum to roughly 1.0)
    metrics.frequency_balance["low"] = std::uniform_real_distribution<>(0.2, 0.4)(gen);
    metrics.frequency_balance["mid"] = std::uniform_real_distribution<>(0.3, 0.5)(gen);
    metrics.frequency_balance["high"] = 1.0 - metrics.frequency_balance["low"] - metrics.frequency_balance["mid"];
    
    // Detect issues based on metrics
    metrics.detected_issues = detectAudioIssues(metrics);
    
    return metrics;
}

// ========================================================================
// Suggestion Management
// ========================================================================

std::vector<ProactiveAIMonitor::ProactiveSuggestion> ProactiveAIMonitor::getActiveSuggestions() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    return active_suggestions_;
}

core::VoidResult ProactiveAIMonitor::markSuggestionSeen(const std::string& suggestion_id) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    
    for (auto& suggestion : active_suggestions_) {
        if (suggestion.id == suggestion_id) {
            suggestion.user_seen = true;
            std::cout << "🧠 User saw suggestion: " << suggestion.title << std::endl;
            return core::VoidResult::success();
        }
    }
    
    return core::VoidResult::failure("Suggestion not found: " + suggestion_id);
}

core::VoidResult ProactiveAIMonitor::acceptSuggestion(const std::string& suggestion_id, const std::string& feedback) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    
    for (auto& suggestion : active_suggestions_) {
        if (suggestion.id == suggestion_id) {
            suggestion.user_accepted = true;
            suggestion.user_feedback = feedback;
            
            // Update learning statistics
            {
                std::lock_guard<std::mutex> learning_lock(learning_mutex_);
                learning_stats_.suggestions_accepted++;
                updateLearningModel(suggestion, true);
            }
            
            std::cout << "✅ User accepted suggestion: " << suggestion.title << std::endl;
            if (!feedback.empty()) {
                std::cout << "   Feedback: " << feedback << std::endl;
            }
            
            return core::VoidResult::success();
        }
    }
    
    return core::VoidResult::failure("Suggestion not found: " + suggestion_id);
}

core::VoidResult ProactiveAIMonitor::dismissSuggestion(const std::string& suggestion_id, const std::string& reason) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    
    for (auto it = active_suggestions_.begin(); it != active_suggestions_.end(); ++it) {
        if (it->id == suggestion_id) {
            it->user_dismissed = true;
            it->user_feedback = reason;
            
            // Update learning statistics
            {
                std::lock_guard<std::mutex> learning_lock(learning_mutex_);
                learning_stats_.suggestions_dismissed++;
                updateLearningModel(*it, false);
            }
            
            std::cout << "❌ User dismissed suggestion: " << it->title << std::endl;
            if (!reason.empty()) {
                std::cout << "   Reason: " << reason << std::endl;
            }
            
            // Remove dismissed suggestion
            active_suggestions_.erase(it);
            return core::VoidResult::success();
        }
    }
    
    return core::VoidResult::failure("Suggestion not found: " + suggestion_id);
}

core::VoidResult ProactiveAIMonitor::clearSuggestions() {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    active_suggestions_.clear();
    std::cout << "🧠 Cleared all active suggestions" << std::endl;
    return core::VoidResult::success();
}

// ========================================================================
// Learning and Adaptation
// ========================================================================

void ProactiveAIMonitor::learnFromUserAction(
    const std::string& action_type,
    const std::map<std::string, std::string>& context,
    bool was_suggestion_triggered
) {
    std::lock_guard<std::mutex> lock(learning_mutex_);
    
    // Update workflow patterns
    user_workflow_patterns_.push_back(action_type);
    
    // Keep only recent patterns (last 100 actions)
    if (user_workflow_patterns_.size() > 100) {
        user_workflow_patterns_.erase(user_workflow_patterns_.begin());
    }
    
    std::cout << "🧠 Learned from user action: " << action_type 
              << (was_suggestion_triggered ? " (suggestion triggered)" : "") << std::endl;
}

ProactiveAIMonitor::LearningStats ProactiveAIMonitor::getLearningStats() const {
    std::lock_guard<std::mutex> lock(learning_mutex_);
    
    auto stats = learning_stats_;
    if (stats.suggestions_made > 0) {
        stats.acceptance_rate = static_cast<double>(stats.suggestions_accepted) / stats.suggestions_made;
    }
    
    return stats;
}

// ========================================================================
// Advanced Features
// ========================================================================

core::AsyncResult<core::Result<std::vector<std::string>>> ProactiveAIMonitor::detectWorkflowPatterns() {
    return core::async<core::Result<std::vector<std::string>>>([this]() -> core::Result<std::vector<std::string>> {
        std::lock_guard<std::mutex> lock(learning_mutex_);
        
        std::vector<std::string> patterns;
        
        if (user_workflow_patterns_.size() < 10) {
            return core::Result<std::vector<std::string>>::success(patterns);
        }
        
        // Analyze patterns in user workflow
        std::map<std::string, int> action_frequency;
        for (const auto& action : user_workflow_patterns_) {
            action_frequency[action]++;
        }
        
        // Find most common patterns
        for (const auto& pair : action_frequency) {
            if (pair.second >= 3) {  // Action performed at least 3 times
                patterns.push_back("Frequently uses: " + pair.first);
            }
        }
        
        return core::Result<std::vector<std::string>>::success(patterns);
    });
}

core::AsyncResult<core::Result<std::vector<std::string>>> ProactiveAIMonitor::suggestWorkflowOptimizations() {
    return core::async<core::Result<std::vector<std::string>>>([this]() -> core::Result<std::vector<std::string>> {
        std::vector<std::string> optimizations = {
            "Consider using keyboard shortcuts for frequently used actions",
            "Group similar tracks for easier mixing workflow",
            "Use track templates to speed up future projects",
            "Set up mix buses for better organization"
        };
        
        return core::Result<std::vector<std::string>>::success(optimizations);
    });
}

core::AsyncResult<core::Result<std::vector<std::string>>> ProactiveAIMonitor::predictNextActions() {
    return core::async<core::Result<std::vector<std::string>>>([this]() -> core::Result<std::vector<std::string>> {
        std::vector<std::string> predictions = {
            "You might want to adjust the vocal levels",
            "Consider adding some reverb to create space",
            "The mix might benefit from some EQ on the master bus",
            "Try automation on the lead instrument for dynamics"
        };
        
        return core::Result<std::vector<std::string>>::success(predictions);
    });
}

core::AsyncResult<core::Result<std::string>> ProactiveAIMonitor::generateContextualHelp() {
    return core::async<core::Result<std::string>>([this]() -> core::Result<std::string> {
        std::ostringstream help;
        help << "🧠 AI Monitor Status:\n";
        help << "• Mix Quality: " << std::fixed << std::setprecision(1) << (getCurrentMixQuality() * 100) << "%\n";
        help << "• Active Suggestions: " << getActiveSuggestions().size() << "\n";
        help << "• Monitoring Interval: " << analysis_interval_.count() << "ms\n";
        help << "\nTip: I'm continuously analyzing your mix and will suggest improvements when I detect issues or opportunities.";
        
        return core::Result<std::string>::success(help.str());
    });
}

// ========================================================================
// Internal Implementation
// ========================================================================

void ProactiveAIMonitor::monitoringLoop() {
    std::cout << "🧠 Proactive AI monitoring loop started" << std::endl;
    
    auto last_analysis = std::chrono::steady_clock::now();
    
    while (!should_stop_.load()) {
        try {
            auto now = std::chrono::steady_clock::now();
            
            // Check if it's time for analysis
            if (now - last_analysis >= analysis_interval_) {
                analyzeSessionState();
                last_analysis = now;
            }
            
            // Sleep for a short time to prevent excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            std::cerr << "🧠 Error in monitoring loop: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "🧠 Proactive AI monitoring loop stopped" << std::endl;
}

void ProactiveAIMonitor::analyzeSessionState() {
    // Get current audio metrics
    auto metrics = getCurrentMetrics();
    
    // Update current state
    {
        std::unique_lock<std::shared_mutex> lock(state_mutex_);
        
        bool significant_change = isSignificantChange(current_metrics_, metrics);
        current_metrics_ = metrics;
        current_mix_quality_ = calculateMixQuality(metrics);
        
        // Only generate new suggestions if there's been a significant change
        // or if we don't have any active suggestions
        if (significant_change || active_suggestions_.empty()) {
            auto new_suggestions = generateSuggestions(metrics);
            
            // Add new suggestions that aren't duplicates
            for (const auto& suggestion : new_suggestions) {
                bool is_duplicate = false;
                for (const auto& existing : active_suggestions_) {
                    if (existing.title == suggestion.title) {
                        is_duplicate = true;
                        break;
                    }
                }
                
                if (!is_duplicate && shouldMakeSuggestion(suggestion)) {
                    active_suggestions_.push_back(suggestion);
                    
                    // Update statistics
                    {
                        std::lock_guard<std::mutex> learning_lock(learning_mutex_);
                        learning_stats_.suggestions_made++;
                    }
                }
            }
        }
    }
    
    // Notify callbacks if we have suggestions
    auto current_suggestions = getActiveSuggestions();
    if (!current_suggestions.empty() && suggestion_callback_) {
        notifyCallbacks(current_suggestions);
    }
}

std::vector<ProactiveAIMonitor::ProactiveSuggestion> 
ProactiveAIMonitor::generateSuggestions(const RealTimeMetrics& metrics) {
    std::vector<ProactiveSuggestion> suggestions;
    
    // Generate mix quality suggestions
    if (enabled_suggestion_types_.count("mix_balance")) {
        auto mix_suggestions = generateMixQualitySuggestions(metrics);
        suggestions.insert(suggestions.end(), mix_suggestions.begin(), mix_suggestions.end());
    }
    
    // Generate workflow suggestions
    if (enabled_suggestion_types_.count("workflow_optimization")) {
        auto workflow_suggestions = generateWorkflowSuggestions();
        suggestions.insert(suggestions.end(), workflow_suggestions.begin(), workflow_suggestions.end());
    }
    
    return suggestions;
}

std::vector<ProactiveAIMonitor::ProactiveSuggestion> 
ProactiveAIMonitor::checkForCriticalIssues(const RealTimeMetrics& metrics) {
    std::vector<ProactiveSuggestion> critical_issues;
    
    // Check for clipping
    if (metrics.peak_db > -0.5) {
        ProactiveSuggestion suggestion("Audio Clipping Detected", 
            "Peak levels are too high and may cause distortion", 
            SuggestionPriority::CRITICAL);
        suggestion.suggested_action = "Reduce master volume or individual track levels";
        suggestion.confidence_score = 0.95;
        suggestion.id = generateSuggestionId();
        critical_issues.push_back(suggestion);
    }
    
    // Check for phase issues
    if (metrics.stereo_width < 0.1) {
        ProactiveSuggestion suggestion("Potential Phase Issues", 
            "Very narrow stereo width may indicate phase cancellation", 
            SuggestionPriority::HIGH);
        suggestion.suggested_action = "Check track polarity and stereo imaging";
        suggestion.confidence_score = 0.8;
        suggestion.id = generateSuggestionId();
        critical_issues.push_back(suggestion);
    }
    
    return critical_issues;
}

std::vector<ProactiveAIMonitor::ProactiveSuggestion> 
ProactiveAIMonitor::generateMixQualitySuggestions(const RealTimeMetrics& metrics) {
    std::vector<ProactiveSuggestion> suggestions;
    
    // Check LUFS levels
    if (metrics.overall_lufs < -20.0) {
        ProactiveSuggestion suggestion("Mix Level Too Low", 
            "Overall loudness is quite low for modern standards", 
            SuggestionPriority::MEDIUM);
        suggestion.suggested_action = "Consider gentle compression or limiting to increase perceived loudness";
        suggestion.confidence_score = 0.7;
        suggestion.id = generateSuggestionId();
        suggestions.push_back(suggestion);
    }
    
    // Check dynamic range
    if (metrics.dynamic_range < 6.0) {
        ProactiveSuggestion suggestion("Limited Dynamic Range", 
            "Mix sounds quite compressed - consider preserving more dynamics", 
            SuggestionPriority::MEDIUM);
        suggestion.suggested_action = "Reduce compression or increase dynamic contrast";
        suggestion.confidence_score = 0.75;
        suggestion.id = generateSuggestionId();
        suggestions.push_back(suggestion);
    }
    
    // Check frequency balance
    if (metrics.frequency_balance.count("high") && metrics.frequency_balance.at("high") > 0.5) {
        ProactiveSuggestion suggestion("Mix Sounds Bright", 
            "High frequencies are prominent - might sound harsh on some systems", 
            SuggestionPriority::LOW);
        suggestion.suggested_action = "Consider gentle high-frequency EQ reduction";
        suggestion.confidence_score = 0.6;
        suggestion.id = generateSuggestionId();
        suggestions.push_back(suggestion);
    }
    
    return suggestions;
}

std::vector<ProactiveAIMonitor::ProactiveSuggestion> 
ProactiveAIMonitor::generateWorkflowSuggestions() {
    std::vector<ProactiveSuggestion> suggestions;
    
    // Suggest workflow improvements based on learned patterns
    if (user_workflow_patterns_.size() > 20) {
        ProactiveSuggestion suggestion("Workflow Optimization Available", 
            "I've noticed patterns in your workflow that could be optimized", 
            SuggestionPriority::LOW);
        suggestion.suggested_action = "Would you like me to suggest keyboard shortcuts or track templates?";
        suggestion.confidence_score = 0.8;
        suggestion.id = generateSuggestionId();
        suggestions.push_back(suggestion);
    }
    
    return suggestions;
}

// ========================================================================
// Helper Methods
// ========================================================================

std::string ProactiveAIMonitor::generateSuggestionId() {
    static uint32_t counter = 0;
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::ostringstream id;
    id << "suggestion_" << timestamp << "_" << (++counter);
    return id.str();
}

void ProactiveAIMonitor::notifyCallbacks(const std::vector<ProactiveSuggestion>& suggestions) {
    if (suggestion_callback_) {
        try {
            suggestion_callback_(suggestions);
        } catch (const std::exception& e) {
            std::cerr << "🧠 Error in suggestion callback: " << e.what() << std::endl;
        }
    }
}

void ProactiveAIMonitor::sendAlert(const std::string& message, SuggestionPriority priority) {
    if (alert_callback_) {
        try {
            alert_callback_(message, priority);
        } catch (const std::exception& e) {
            std::cerr << "🧠 Error in alert callback: " << e.what() << std::endl;
        }
    }
}

bool ProactiveAIMonitor::isSignificantChange(const RealTimeMetrics& previous, const RealTimeMetrics& current) {
    // Check for significant changes in key metrics
    const double lufs_threshold = 1.0;    // 1 dB change in LUFS
    const double peak_threshold = 2.0;    // 2 dB change in peak
    const double dr_threshold = 2.0;      // 2 dB change in dynamic range
    const double width_threshold = 0.1;   // 10% change in stereo width
    
    return (std::abs(current.overall_lufs - previous.overall_lufs) > lufs_threshold) ||
           (std::abs(current.peak_db - previous.peak_db) > peak_threshold) ||
           (std::abs(current.dynamic_range - previous.dynamic_range) > dr_threshold) ||
           (std::abs(current.stereo_width - previous.stereo_width) > width_threshold);
}

double ProactiveAIMonitor::calculateMixQuality(const RealTimeMetrics& metrics) {
    double quality = 1.0;
    
    // Penalize for clipping
    if (metrics.peak_db > -1.0) {
        quality *= 0.5;  // Major penalty
    } else if (metrics.peak_db > -3.0) {
        quality *= 0.8;  // Minor penalty
    }
    
    // Penalize for extreme LUFS levels
    if (metrics.overall_lufs < -25.0 || metrics.overall_lufs > -6.0) {
        quality *= 0.7;
    }
    
    // Penalize for very low dynamic range
    if (metrics.dynamic_range < 4.0) {
        quality *= 0.6;
    }
    
    // Penalize for phase issues (very narrow stereo)
    if (metrics.stereo_width < 0.2) {
        quality *= 0.7;
    }
    
    return std::clamp(quality, 0.0, 1.0);
}

std::vector<std::string> ProactiveAIMonitor::detectAudioIssues(const RealTimeMetrics& metrics) {
    std::vector<std::string> issues;
    
    if (metrics.peak_db > -0.5) {
        issues.push_back("clipping_detected");
    }
    
    if (metrics.overall_lufs < -25.0) {
        issues.push_back("level_too_low");
    }
    
    if (metrics.dynamic_range < 4.0) {
        issues.push_back("over_compressed");
    }
    
    if (metrics.stereo_width < 0.2) {
        issues.push_back("phase_issues");
    }
    
    if (metrics.frequency_balance.count("high") && metrics.frequency_balance.at("high") > 0.6) {
        issues.push_back("too_bright");
    }
    
    return issues;
}

void ProactiveAIMonitor::updateLearningModel(const ProactiveSuggestion& suggestion, bool accepted) {
    // Update suggestion type weights based on user feedback
    std::string suggestion_type = "general";
    
    // Determine suggestion type from title/description
    if (suggestion.title.find("Level") != std::string::npos) {
        suggestion_type = "level_suggestions";
    } else if (suggestion.title.find("EQ") != std::string::npos || suggestion.title.find("Bright") != std::string::npos) {
        suggestion_type = "eq_suggestions";
    } else if (suggestion.title.find("Dynamic") != std::string::npos || suggestion.title.find("Compression") != std::string::npos) {
        suggestion_type = "dynamics_suggestions";
    } else if (suggestion.title.find("Workflow") != std::string::npos) {
        suggestion_type = "workflow_suggestions";
    }
    
    // Adjust weights based on acceptance
    if (accepted) {
        suggestion_type_weights_[suggestion_type] += 0.1;
        learning_stats_.most_accepted_types[suggestion_type]++;
    } else {
        suggestion_type_weights_[suggestion_type] -= 0.05;
        learning_stats_.most_dismissed_types[suggestion_type]++;
    }
    
    // Clamp weights to reasonable ranges
    suggestion_type_weights_[suggestion_type] = std::clamp(suggestion_type_weights_[suggestion_type], 0.1, 2.0);
}

bool ProactiveAIMonitor::shouldMakeSuggestion(const ProactiveSuggestion& suggestion) {
    // Check confidence threshold
    if (suggestion.confidence_score < suggestion_threshold_) {
        return false;
    }
    
    // Check learning weights (if we have learned preferences)
    std::string suggestion_type = "general";  // Would determine from suggestion content
    if (suggestion_type_weights_.count(suggestion_type)) {
        double weight = suggestion_type_weights_[suggestion_type];
        if (weight < 0.5) {  // User typically dismisses this type
            return false;
        }
    }
    
    return true;
}

} // namespace mixmind::ai