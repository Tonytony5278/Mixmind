#pragma once

#include "../core/result.h"
#include "../core/audio_types.h"
#include "../audio/AudioBuffer.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>

namespace mixmind::ai {

// ============================================================================
// Audio Analysis Data Structures
// ============================================================================

struct SpectralAnalysis {
    std::vector<double> frequency_bins;     // Frequency values in Hz
    std::vector<double> magnitude_db;       // Magnitude in dB
    double spectral_centroid = 0.0;         // Brightness measure
    double spectral_rolloff = 0.0;          // High frequency content
    double spectral_flux = 0.0;             // Measure of spectral change
    double spectral_flatness = 0.0;         // Measure of noise vs tone
    
    // Frequency band energies
    double sub_bass_energy = 0.0;           // 20-60 Hz
    double bass_energy = 0.0;               // 60-250 Hz  
    double low_mid_energy = 0.0;            // 250-500 Hz
    double mid_energy = 0.0;                // 500-2000 Hz
    double high_mid_energy = 0.0;           // 2000-4000 Hz
    double presence_energy = 0.0;           // 4000-6000 Hz
    double brilliance_energy = 0.0;         // 6000-20000 Hz
};

struct DynamicAnalysis {
    double peak_db = -70.0;                 // Peak level
    double rms_db = -70.0;                  // RMS level
    double lufs = -70.0;                    // Loudness level
    double crest_factor = 0.0;              // Peak to RMS ratio
    double dynamic_range = 0.0;             // DR measurement
    
    // Envelope characteristics
    double attack_time_ms = 0.0;            // Attack time estimate
    double decay_time_ms = 0.0;             // Decay time estimate
    double sustain_level = 0.0;             // Sustain level ratio
    double release_time_ms = 0.0;           // Release time estimate
    
    // Transient analysis
    bool has_transients = false;            // Contains transient material
    double transient_density = 0.0;         // Transients per second
    std::vector<double> transient_times;    // Transient positions in seconds
};

struct StereoAnalysis {
    double width = 0.0;                     // Stereo width measure (0-1)
    double correlation = 0.0;               // L/R correlation (-1 to 1)
    double balance = 0.0;                   // L/R balance (-1 to 1)
    double phase_coherence = 1.0;           // Phase alignment (0-1)
    
    // Imaging characteristics
    double mono_compatibility = 1.0;        // Mono fold-down quality
    bool has_phase_issues = false;          // Phase problems detected
    double center_image_strength = 0.0;     // Center image content
    double side_content_ratio = 0.0;        // Side/Mid ratio
};

struct AudioCharacteristics {
    enum class AudioType {
        UNKNOWN, DRUMS, BASS, LEAD, PAD, VOCAL, GUITAR, 
        PIANO, STRINGS, BRASS, WOODWINDS, PERCUSSION,
        SYNTH_LEAD, SYNTH_PAD, EFFECTS, AMBIENT
    };
    
    AudioType detected_type = AudioType::UNKNOWN;
    double classification_confidence = 0.0;
    
    // Musical characteristics
    double fundamental_frequency = 0.0;      // Primary pitch
    double pitch_stability = 0.0;           // How stable the pitch is
    std::vector<double> harmonic_content;    // Harmonic series analysis
    double inharmonicity = 0.0;             // Amount of inharmonic content
    
    // Rhythmic characteristics  
    double tempo_bpm = 120.0;                // Detected tempo
    double tempo_stability = 0.0;           // How stable the tempo is
    std::vector<double> beat_positions;      // Beat locations
    double rhythmic_complexity = 0.0;       // Complexity measure
};

struct ComprehensiveAudioAnalysis {
    SpectralAnalysis spectral;
    DynamicAnalysis dynamics;
    StereoAnalysis stereo;
    AudioCharacteristics characteristics;
    
    // Quality metrics
    double overall_quality_score = 0.0;     // 0-1 quality assessment
    std::vector<std::string> quality_issues; // Identified problems
    std::vector<std::string> strengths;     // Positive qualities
    
    // Context information
    std::string track_name;
    double duration_seconds = 0.0;
    uint32_t sample_rate = 44100;
    uint32_t channels = 2;
    std::chrono::steady_clock::time_point analysis_time;
};

// ============================================================================
// Mixing Suggestions and Recommendations
// ============================================================================

enum class SuggestionCategory {
    EQ_CORRECTION,          // Corrective EQ adjustments
    EQ_CREATIVE,            // Creative EQ shaping
    DYNAMICS_COMPRESSION,   // Compression settings
    DYNAMICS_EXPANSION,     // Expansion/gating
    SPATIAL_REVERB,         // Reverb recommendations
    SPATIAL_DELAY,          // Delay settings
    SPATIAL_STEREO,         // Stereo enhancement
    BALANCE_LEVEL,          // Volume adjustments
    BALANCE_PAN,            // Panning suggestions
    CREATIVE_EFFECTS,       // Creative processing
    TECHNICAL_FIX,          // Technical issue fixes
    WORKFLOW_OPTIMIZATION   // Workflow improvements
};

enum class SuggestionPriority {
    LOW = 1,               // Nice to have
    MEDIUM = 2,            // Should consider
    HIGH = 3,              // Recommended
    CRITICAL = 4           // Must address
};

struct MixingSuggestion {
    SuggestionCategory category;
    SuggestionPriority priority;
    
    std::string title;                      // Brief description
    std::string description;                // Detailed explanation
    std::string reasoning;                  // Why this is suggested
    
    // Implementation details
    struct ParameterAdjustment {
        std::string parameter_name;
        double current_value = 0.0;
        double suggested_value = 0.0;
        std::string unit = "";
        double confidence = 0.0;
    };
    
    std::vector<ParameterAdjustment> parameter_adjustments;
    std::string suggested_plugin;           // Recommended plugin
    std::vector<std::string> alternative_plugins;
    
    // Measurement and validation
    double confidence_score = 0.0;          // How confident the AI is
    std::string success_metric;             // How to measure success
    double expected_improvement = 0.0;      // Expected quality gain
    
    // User interaction
    bool user_accepted = false;             // User feedback
    bool user_rejected = false;
    std::string user_feedback;              // Optional user comments
    
    MixingSuggestion() = default;
    MixingSuggestion(SuggestionCategory cat, const std::string& desc) 
        : category(cat), title(desc), description(desc) {}
};

struct PluginRecommendation {
    std::string plugin_name;
    std::string plugin_category;            // "EQ", "Compressor", etc.
    std::string manufacturer;
    
    std::string reason;                     // Why this plugin
    double suitability_score = 0.0;        // 0-1 how suitable
    
    // Suggested settings
    std::map<std::string, double> initial_settings;
    std::vector<std::string> preset_suggestions;
    
    // Usage context
    std::string usage_scenario;             // When to use this
    std::vector<std::string> alternative_options;
    double cpu_impact_estimate = 0.0;       // Relative CPU usage
    
    bool is_available = true;               // Plugin is installed
    std::string installation_note;          // How to get it if not available
};

// ============================================================================
// Intelligent Mixing Engine
// ============================================================================

class MixingIntelligence {
public:
    MixingIntelligence();
    ~MixingIntelligence();
    
    // ========================================================================
    // Audio Analysis
    // ========================================================================
    
    /// Perform comprehensive audio analysis
    core::Result<ComprehensiveAudioAnalysis> analyzeAudio(
        std::shared_ptr<AudioBuffer> buffer,
        const std::string& track_name = ""
    );
    
    /// Analyze multiple tracks for context-aware suggestions
    core::Result<std::vector<ComprehensiveAudioAnalysis>> analyzeMix(
        const std::map<std::string, std::shared_ptr<AudioBuffer>>& tracks
    );
    
    /// Quick real-time analysis for live feedback
    core::Result<ComprehensiveAudioAnalysis> analyzeRealtime(
        std::shared_ptr<AudioBuffer> buffer
    );
    
    /// Compare before/after analysis
    core::Result<std::string> compareAnalysis(
        const ComprehensiveAudioAnalysis& before,
        const ComprehensiveAudioAnalysis& after
    );
    
    // ========================================================================
    // Intelligent Suggestions
    // ========================================================================
    
    /// Generate mixing suggestions based on analysis
    core::Result<std::vector<MixingSuggestion>> generateMixingSuggestions(
        const ComprehensiveAudioAnalysis& analysis,
        const std::string& mix_context = ""
    );
    
    /// Generate suggestions for multi-track mix
    core::Result<std::vector<MixingSuggestion>> generateMixSuggestions(
        const std::vector<ComprehensiveAudioAnalysis>& track_analyses,
        const ComprehensiveAudioAnalysis& master_analysis
    );
    
    /// Generate corrective suggestions (fix problems)
    core::Result<std::vector<MixingSuggestion>> generateCorrectiveSuggestions(
        const ComprehensiveAudioAnalysis& analysis
    );
    
    /// Generate creative suggestions (enhance musicality)
    core::Result<std::vector<MixingSuggestion>> generateCreativeSuggestions(
        const ComprehensiveAudioAnalysis& analysis,
        const std::string& musical_style = ""
    );
    
    // ========================================================================
    // Plugin Recommendations
    // ========================================================================
    
    /// Recommend plugins based on analysis
    core::Result<std::vector<PluginRecommendation>> recommendPlugins(
        const ComprehensiveAudioAnalysis& analysis,
        const std::string& goal = ""
    );
    
    /// Recommend EQ plugins and settings
    core::Result<std::vector<PluginRecommendation>> recommendEQ(
        const SpectralAnalysis& spectral_analysis
    );
    
    /// Recommend dynamics processors
    core::Result<std::vector<PluginRecommendation>> recommendDynamicsProcessors(
        const DynamicAnalysis& dynamic_analysis
    );
    
    /// Recommend spatial effects
    core::Result<std::vector<PluginRecommendation>> recommendSpatialEffects(
        const StereoAnalysis& stereo_analysis
    );
    
    // ========================================================================
    // Mix Optimization
    // ========================================================================
    
    /// Suggest optimal level balance between tracks
    core::Result<std::map<std::string, double>> optimizeLevelBalance(
        const std::map<std::string, ComprehensiveAudioAnalysis>& track_analyses
    );
    
    /// Suggest optimal panning arrangement
    core::Result<std::map<std::string, double>> optimizePanning(
        const std::map<std::string, ComprehensiveAudioAnalysis>& track_analyses
    );
    
    /// Suggest frequency separation strategies
    core::Result<std::vector<MixingSuggestion>> suggestFrequencySeparation(
        const std::vector<ComprehensiveAudioAnalysis>& track_analyses
    );
    
    /// Analyze and suggest improvements to stereo field
    core::Result<std::vector<MixingSuggestion>> optimizeStereoField(
        const std::map<std::string, ComprehensiveAudioAnalysis>& track_analyses
    );
    
    // ========================================================================
    // Mastering Intelligence
    // ========================================================================
    
    /// Analyze mix for mastering readiness
    core::Result<std::vector<MixingSuggestion>> analyzeMasteringReadiness(
        const ComprehensiveAudioAnalysis& master_analysis
    );
    
    /// Suggest mastering chain and settings
    core::Result<std::vector<PluginRecommendation>> suggestMasteringChain(
        const ComprehensiveAudioAnalysis& master_analysis,
        const std::string& target_platform = ""
    );
    
    /// Suggest loudness targets based on platform
    core::Result<std::map<std::string, double>> suggestLoudnessTargets(
        const std::string& target_platform
    );
    
    // ========================================================================
    // Learning and Adaptation
    // ========================================================================
    
    /// Learn from user acceptance/rejection of suggestions
    void provideFeedback(
        const MixingSuggestion& suggestion,
        bool accepted,
        const std::string& user_feedback = ""
    );
    
    /// Learn from user's mixing decisions
    void learnFromUserAction(
        const std::string& action_type,
        const std::map<std::string, double>& parameters,
        const ComprehensiveAudioAnalysis& before_analysis,
        const ComprehensiveAudioAnalysis& after_analysis
    );
    
    /// Update knowledge base from successful mixes
    void updateKnowledgeBase(
        const std::vector<ComprehensiveAudioAnalysis>& successful_mix_analyses
    );
    
    /// Get personalized recommendations based on user history
    core::Result<std::vector<MixingSuggestion>> getPersonalizedSuggestions(
        const ComprehensiveAudioAnalysis& analysis,
        const std::string& user_id
    );
    
    // ========================================================================
    // Configuration and Customization
    // ========================================================================
    
    /// Set analysis quality level (affects processing time)
    void setAnalysisQuality(int quality_level); // 1-10 scale
    
    /// Enable/disable specific suggestion categories
    void setSuggestionCategories(const std::vector<SuggestionCategory>& enabled_categories);
    
    /// Set musical style preferences for suggestions
    void setMusicalStyleContext(const std::string& style);
    
    /// Set target mixing standards (broadcast, streaming, etc.)
    void setTargetStandards(const std::string& standard);
    
    /// Configure confidence thresholds for suggestions
    void setConfidenceThresholds(double min_confidence);
    
    // ========================================================================
    // Reporting and Analytics
    // ========================================================================
    
    /// Generate detailed analysis report
    core::Result<std::string> generateAnalysisReport(
        const ComprehensiveAudioAnalysis& analysis,
        const std::string& format = "markdown"
    );
    
    /// Generate mixing suggestions report
    core::Result<std::string> generateSuggestionsReport(
        const std::vector<MixingSuggestion>& suggestions,
        const std::string& format = "markdown"
    );
    
    /// Get mixing intelligence statistics
    struct MixingStats {
        uint32_t analyses_performed = 0;
        uint32_t suggestions_generated = 0;
        uint32_t suggestions_accepted = 0;
        double average_confidence = 0.0;
        double suggestion_acceptance_rate = 0.0;
        std::map<SuggestionCategory, uint32_t> category_usage;
    };
    
    MixingStats getStatistics() const;
    
private:
    // ========================================================================
    // Internal Analysis Methods
    // ========================================================================
    
    SpectralAnalysis performSpectralAnalysis(std::shared_ptr<AudioBuffer> buffer);
    DynamicAnalysis performDynamicAnalysis(std::shared_ptr<AudioBuffer> buffer);
    StereoAnalysis performStereoAnalysis(std::shared_ptr<AudioBuffer> buffer);
    AudioCharacteristics classifyAudioContent(std::shared_ptr<AudioBuffer> buffer);
    
    // Specific analysis algorithms
    std::vector<double> computeFFT(std::shared_ptr<AudioBuffer> buffer);
    double computeSpectralCentroid(const std::vector<double>& spectrum);
    double computeSpectralRolloff(const std::vector<double>& spectrum, double rolloff_percent = 0.85);
    std::vector<double> detectTransients(std::shared_ptr<AudioBuffer> buffer);
    AudioCharacteristics::AudioType classifyAudioType(const ComprehensiveAudioAnalysis& analysis);
    
    // ========================================================================
    // Suggestion Generation
    // ========================================================================
    
    std::vector<MixingSuggestion> generateEQSuggestions(const SpectralAnalysis& spectral);
    std::vector<MixingSuggestion> generateDynamicsSuggestions(const DynamicAnalysis& dynamics);
    std::vector<MixingSuggestion> generateStereoSuggestions(const StereoAnalysis& stereo);
    std::vector<MixingSuggestion> generateBalanceSuggestions(
        const std::vector<ComprehensiveAudioAnalysis>& track_analyses);
    
    // Knowledge base and pattern matching
    bool matchesKnownPattern(const ComprehensiveAudioAnalysis& analysis, std::string& pattern_name);
    std::vector<MixingSuggestion> getSuggestionsForPattern(const std::string& pattern_name);
    
    // ========================================================================
    // Internal State
    // ========================================================================
    
    int analysis_quality_level_ = 7;
    std::vector<SuggestionCategory> enabled_categories_;
    std::string musical_style_context_;
    std::string target_standards_;
    double min_confidence_threshold_ = 0.5;
    
    // Statistics and learning
    mutable std::mutex stats_mutex_;
    MixingStats statistics_;
    
    // User learning data
    std::map<std::string, std::vector<MixingSuggestion>> user_feedback_history_;
    std::map<std::string, double> user_preference_weights_;
    
    // Knowledge base
    struct MixingKnowledge {
        std::map<std::string, std::vector<MixingSuggestion>> pattern_suggestions;
        std::map<AudioCharacteristics::AudioType, std::vector<PluginRecommendation>> type_plugins;
        std::map<std::string, std::map<std::string, double>> successful_settings;
    };
    
    MixingKnowledge knowledge_base_;
    
    void initializeKnowledgeBase();
    void updateStatistics();
};

// ============================================================================
// Audio Quality Analyzer
// ============================================================================

class AudioQualityAnalyzer {
public:
    struct QualityMetrics {
        double overall_score = 0.0;         // 0-100 overall quality
        
        // Technical quality
        double dynamic_range_score = 0.0;   // Dynamic range quality
        double frequency_balance_score = 0.0; // Frequency balance
        double stereo_quality_score = 0.0;  // Stereo field quality
        double noise_floor_score = 0.0;     // Noise and distortion
        
        // Musical quality
        double tonal_balance_score = 0.0;   // Musical balance
        double punch_presence_score = 0.0;  // Impact and presence
        double clarity_definition_score = 0.0; // Clarity and definition
        double spaciousness_score = 0.0;    // Spatial qualities
        
        // Issues detected
        std::vector<std::string> quality_issues;
        std::vector<std::string> recommendations;
    };
    
    static QualityMetrics assessAudioQuality(const ComprehensiveAudioAnalysis& analysis);
    static std::string generateQualityReport(const QualityMetrics& metrics);
    
private:
    static double assessDynamicRange(const DynamicAnalysis& dynamics);
    static double assessFrequencyBalance(const SpectralAnalysis& spectral);
    static double assessStereoQuality(const StereoAnalysis& stereo);
    static double assessNoiseFloor(const ComprehensiveAudioAnalysis& analysis);
};

} // namespace mixmind::ai