#pragma once

#include "../core/result.h"
#include "../core/audio_types.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <variant>

namespace mixmind {

// Forward declarations
class AudioBuffer;
class MIDIEvent;
class VSTiHost;
class AudioBus;

// AI Command Categories
enum class AICommandCategory {
    PLAYBACK_CONTROL,       // Play, stop, record, navigate
    TRACK_MANAGEMENT,       // Create, delete, rename tracks
    MIXER_CONTROL,          // Volume, pan, solo, mute, routing
    AUTOMATION,             // Record, edit, clear automation
    PLUGIN_CONTROL,         // Load, configure, bypass plugins
    MIDI_EDITING,           // Piano roll operations, note editing
    AUDIO_EDITING,          // Audio region manipulation
    RENDERING,              // Export, bounce operations
    PROJECT_MANAGEMENT,     // Save, load, templates
    ANALYSIS,               // Audio analysis, suggestions
    WORKFLOW,               // Intelligent workflows and shortcuts
    CREATIVE,               // Generate content, arrange suggestions
    MIXING,                 // Intelligent mixing assistance
    MASTERING,              // Mastering chain suggestions
    TROUBLESHOOTING         // Problem diagnosis and solutions
};

// AI Context Types
enum class AIContextType {
    PROJECT_OVERVIEW,       // High-level project information
    CURRENT_SELECTION,      // What's currently selected
    PLAYBACK_STATE,         // Transport and timing info
    MIXER_STATE,            // Current mixer configuration
    PLUGIN_STATE,           // Active plugins and settings
    AUDIO_ANALYSIS,         // Analyzed audio characteristics
    USER_WORKFLOW,          // User's working patterns
    SESSION_HISTORY,        // Recent actions and changes
    CREATIVE_INTENT,        // Inferred musical goals
    TECHNICAL_SPECS         // Audio format, sample rate, etc.
};

// AI Response Types
enum class AIResponseType {
    CONFIRMATION,           // "Done" or acknowledgment
    INFORMATION,            // Providing requested information
    SUGGESTION,             // Offering recommendations
    QUESTION,               // Asking for clarification
    WARNING,                // Potential issues or concerns
    ERROR,                  // Cannot complete request
    WORKFLOW,               // Multi-step process guidance
    ANALYSIS,               // Audio/project analysis results
    CREATIVE_IDEA,          // Creative suggestions
    TUTORIAL                // How-to explanations
};

// AI Confidence Level
enum class AIConfidenceLevel {
    VERY_LOW = 0,          // 0-20% confident
    LOW = 1,               // 20-40% confident  
    MEDIUM = 2,            // 40-60% confident
    HIGH = 3,              // 60-80% confident
    VERY_HIGH = 4          // 80-100% confident
};

// AI Command Intent
struct AICommandIntent {
    AICommandCategory category;
    std::string primary_action;           // Main verb/action
    std::vector<std::string> targets;     // What to act upon
    std::map<std::string, std::string> parameters;  // Action parameters
    AIConfidenceLevel confidence;
    std::string original_text;            // Original user input
    std::vector<std::string> alternatives;  // Alternative interpretations
    
    AICommandIntent() : category(AICommandCategory::PLAYBACK_CONTROL), confidence(AIConfidenceLevel::MEDIUM) {}
};

// AI Context Data
struct AIContextData {
    AIContextType type;
    std::string key;                      // Context identifier
    std::variant<std::string, double, int64_t, bool> value;
    std::chrono::steady_clock::time_point timestamp;
    double relevance_score = 1.0;        // How relevant to current task
    
    AIContextData(AIContextType t, const std::string& k, const std::variant<std::string, double, int64_t, bool>& v)
        : type(t), key(k), value(v), timestamp(std::chrono::steady_clock::now()) {}
};

// AI Response
struct AIResponse {
    AIResponseType type;
    std::string text;                     // Response text for user
    std::vector<std::string> actions;     // Actions to execute
    std::map<std::string, std::string> parameters;  // Action parameters
    AIConfidenceLevel confidence;
    
    // Additional response data
    std::vector<std::string> suggestions;     // Follow-up suggestions
    std::vector<std::string> warnings;       // Any warnings to display
    std::string help_text;                   // Additional help/context
    
    // Workflow data
    struct WorkflowStep {
        std::string description;
        std::string action;
        std::map<std::string, std::string> parameters;
        bool completed = false;
    };
    std::vector<WorkflowStep> workflow_steps;
    
    AIResponse() : type(AIResponseType::CONFIRMATION), confidence(AIConfidenceLevel::HIGH) {}
};

// Audio Analysis Data
struct AudioAnalysisData {
    // Basic characteristics
    double duration_seconds = 0.0;
    uint32_t sample_rate = 44100;
    uint32_t channels = 2;
    
    // Level analysis
    double peak_level_db = -70.0;
    double rms_level_db = -70.0;
    double lufs_level = -70.0;
    double dynamic_range_db = 0.0;
    
    // Frequency analysis
    double fundamental_frequency = 0.0;      // Detected pitch
    std::vector<double> spectral_peaks;      // Dominant frequencies
    double spectral_centroid = 0.0;         // Brightness measure
    double spectral_rolloff = 0.0;          // High-frequency content
    
    // Temporal analysis
    double tempo_bpm = 120.0;                // Detected tempo
    std::vector<double> beat_positions;      // Beat markers
    bool has_steady_tempo = false;
    
    // Musical analysis
    enum MusicalKey { 
        C_MAJOR, C_SHARP_MAJOR, D_MAJOR, D_SHARP_MAJOR, E_MAJOR, F_MAJOR, 
        F_SHARP_MAJOR, G_MAJOR, G_SHARP_MAJOR, A_MAJOR, A_SHARP_MAJOR, B_MAJOR,
        C_MINOR, C_SHARP_MINOR, D_MINOR, D_SHARP_MINOR, E_MINOR, F_MINOR,
        F_SHARP_MINOR, G_MINOR, G_SHARP_MINOR, A_MINOR, A_SHARP_MINOR, B_MINOR,
        UNKNOWN
    };
    MusicalKey detected_key = UNKNOWN;
    std::vector<int> chord_progression;      // Detected chords
    
    // Quality analysis
    bool has_clipping = false;
    bool has_phase_issues = false;
    double noise_floor_db = -60.0;
    double thd_plus_n_percent = 0.0;        // Total harmonic distortion + noise
    
    // Content classification
    enum AudioType {
        UNKNOWN_AUDIO, MUSIC, SPEECH, DRUMS, BASS, LEAD, PAD, 
        PERCUSSION, VOCAL, INSTRUMENTAL, AMBIENT, EFFECT
    };
    AudioType content_type = UNKNOWN_AUDIO;
    double classification_confidence = 0.0;
    
    void reset() {
        duration_seconds = 0.0;
        peak_level_db = rms_level_db = lufs_level = -70.0;
        dynamic_range_db = 0.0;
        fundamental_frequency = spectral_centroid = spectral_rolloff = 0.0;
        spectral_peaks.clear();
        tempo_bpm = 120.0;
        beat_positions.clear();
        has_steady_tempo = false;
        detected_key = UNKNOWN;
        chord_progression.clear();
        has_clipping = has_phase_issues = false;
        noise_floor_db = -60.0;
        thd_plus_n_percent = 0.0;
        content_type = UNKNOWN_AUDIO;
        classification_confidence = 0.0;
    }
};

// Intelligent Mixing Suggestion
struct MixingSuggestion {
    enum SuggestionType {
        EQ_ADJUSTMENT,          // EQ recommendations
        COMPRESSION_SETTING,    // Dynamics processing
        REVERB_SEND,           // Spatial processing
        VOLUME_BALANCE,        // Level adjustments
        PANNING_POSITION,      // Stereo placement
        ROUTING_CHANGE,        // Signal routing
        PLUGIN_RECOMMENDATION, // Plugin suggestions
        AUTOMATION_CURVE,      // Automation suggestions
        LOUDNESS_TARGET,       // Mastering targets
        CREATIVE_EFFECT        // Creative processing ideas
    };
    
    SuggestionType type;
    std::string description;               // Human-readable description
    std::string target_track;              // Which track/bus to modify
    std::map<std::string, double> parameters;  // Suggested parameter values
    double confidence_score = 0.0;        // How confident AI is (0-1)
    std::string reasoning;                 // Why this suggestion is made
    
    // Implementation details
    std::string plugin_name;               // Specific plugin if applicable
    std::string automation_curve_type;     // Type of automation curve
    double priority = 0.5;                // Suggestion priority (0-1)
    
    MixingSuggestion(SuggestionType t = EQ_ADJUSTMENT) : type(t) {}
};

// Workflow Template
struct WorkflowTemplate {
    std::string name;
    std::string description;
    std::string category;                  // "Mixing", "Recording", "Creative", etc.
    std::vector<std::string> steps;        // Step descriptions
    std::vector<std::string> commands;     // AI commands to execute
    std::map<std::string, std::string> default_parameters;
    
    // Usage tracking
    uint32_t usage_count = 0;
    double user_rating = 0.0;              // 0-5 star rating
    std::chrono::steady_clock::time_point last_used;
    
    WorkflowTemplate(const std::string& n = "", const std::string& desc = "") 
        : name(n), description(desc) {}
};

// Plugin Suggestion
struct PluginSuggestion {
    std::string plugin_name;
    std::string plugin_category;           // "EQ", "Compressor", "Reverb", etc.
    std::string suggested_for_track;       // Track name or type
    std::map<std::string, double> preset_parameters;  // Suggested initial settings
    
    double relevance_score = 0.0;          // How relevant to current context
    std::string reasoning;                 // Why this plugin is suggested
    std::vector<std::string> alternatives; // Alternative plugin options
    
    // Usage context
    enum UsageContext {
        CORRECTIVE,     // Fix problems
        CREATIVE,       // Enhance creativity
        WORKFLOW,       // Speed up workflow
        TECHNICAL,      // Meet technical requirements
        STYLISTIC       // Achieve specific style
    };
    UsageContext usage_context = CORRECTIVE;
};

// AI Learning Data
struct AILearningData {
    // User preferences learned over time
    std::map<std::string, double> preferred_eq_curves;      // EQ preferences by frequency
    std::map<std::string, double> preferred_dynamics;       // Compression preferences
    std::map<std::string, double> preferred_effects;        // Effect usage patterns
    std::map<std::string, std::string> preferred_workflows; // Common workflow patterns
    
    // Usage statistics
    std::map<std::string, uint32_t> command_usage_count;    // How often commands used
    std::map<std::string, double> command_success_rate;     // Command success rates
    std::map<std::string, double> plugin_usage_frequency;   // Plugin usage patterns
    
    // Musical preferences
    std::map<AudioAnalysisData::MusicalKey, uint32_t> key_preferences;
    std::map<std::string, uint32_t> genre_preferences;
    double preferred_tempo_range_min = 80.0;
    double preferred_tempo_range_max = 140.0;
    
    // Technical preferences
    double preferred_lufs_target = -14.0;
    bool prefers_stereo_width = true;
    double preferred_dynamic_range = 10.0;
    
    void reset() {
        preferred_eq_curves.clear();
        preferred_dynamics.clear();
        preferred_effects.clear();
        preferred_workflows.clear();
        command_usage_count.clear();
        command_success_rate.clear();
        plugin_usage_frequency.clear();
        key_preferences.clear();
        genre_preferences.clear();
        preferred_tempo_range_min = 80.0;
        preferred_tempo_range_max = 140.0;
        preferred_lufs_target = -14.0;
        prefers_stereo_width = true;
        preferred_dynamic_range = 10.0;
    }
};

// AI Session State
struct AISessionState {
    // Current context
    std::vector<AIContextData> active_context;
    std::string current_focus;             // What user is currently working on
    std::string current_workflow;          // Active workflow if any
    uint32_t workflow_step = 0;            // Current step in workflow
    
    // Recent actions
    struct ActionHistory {
        std::string command;
        std::chrono::steady_clock::time_point timestamp;
        bool successful;
        std::string result;
    };
    std::vector<ActionHistory> recent_actions;
    
    // Active suggestions
    std::vector<MixingSuggestion> pending_suggestions;
    std::vector<PluginSuggestion> plugin_suggestions;
    std::vector<WorkflowTemplate> suggested_workflows;
    
    // Learning state
    AILearningData learning_data;
    
    // Session metrics
    uint32_t commands_processed = 0;
    uint32_t successful_commands = 0;
    std::chrono::steady_clock::time_point session_start;
    
    AISessionState() : session_start(std::chrono::steady_clock::now()) {}
    
    void reset() {
        active_context.clear();
        current_focus.clear();
        current_workflow.clear();
        workflow_step = 0;
        recent_actions.clear();
        pending_suggestions.clear();
        plugin_suggestions.clear();
        suggested_workflows.clear();
        commands_processed = 0;
        successful_commands = 0;
        session_start = std::chrono::steady_clock::now();
    }
    
    double get_success_rate() const {
        return commands_processed > 0 ? 
            static_cast<double>(successful_commands) / commands_processed : 1.0;
    }
};

// AI Capability Flags
struct AICapabilities {
    // Natural Language Processing
    bool can_parse_natural_language = true;
    bool can_understand_musical_terms = true;
    bool can_infer_intent = true;
    bool can_handle_ambiguity = true;
    
    // Audio Analysis
    bool can_analyze_audio_content = true;
    bool can_detect_tempo_key = true;
    bool can_classify_instruments = true;
    bool can_detect_problems = true;
    
    // Intelligent Assistance
    bool can_suggest_mixing_moves = true;
    bool can_optimize_workflows = true;
    bool can_recommend_plugins = true;
    bool can_generate_automation = true;
    
    // Learning and Adaptation
    bool can_learn_preferences = true;
    bool can_adapt_suggestions = true;
    bool can_remember_context = true;
    bool can_predict_needs = true;
    
    // Creative Features
    bool can_generate_ideas = true;
    bool can_suggest_arrangements = true;
    bool can_create_variations = true;
    bool can_inspire_creativity = true;
    
    // Technical Integration
    bool can_control_daw_functions = true;
    bool can_access_plugin_parameters = true;
    bool can_manage_projects = true;
    bool can_handle_real_time = true;
};

// Callback function types for AI events
using AICommandCallback = std::function<void(const AICommandIntent&)>;
using AIResponseCallback = std::function<void(const AIResponse&)>;
using AIAnalysisCallback = std::function<void(const AudioAnalysisData&)>;
using AISuggestionCallback = std::function<void(const std::vector<MixingSuggestion>&)>;
using AILearningCallback = std::function<void(const AILearningData&)>;

} // namespace mixmind