#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../api/ActionAPI.h"
#include "../services/OSSServiceRegistry.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>

namespace mixmind::ai {

using json = nlohmann::json;

// ============================================================================
// AI Mixing Assistant - Intelligent mixing analysis and suggestions
// ============================================================================

class MixingAssistant {
public:
    MixingAssistant(
        std::shared_ptr<api::ActionAPI> actionAPI,
        std::shared_ptr<services::OSSServiceRegistry> ossServices
    );
    ~MixingAssistant() = default;
    
    // Non-copyable, movable
    MixingAssistant(const MixingAssistant&) = delete;
    MixingAssistant& operator=(const MixingAssistant&) = delete;
    MixingAssistant(MixingAssistant&&) = default;
    MixingAssistant& operator=(MixingAssistant&&) = default;
    
    // ========================================================================
    // Mix Analysis
    // ========================================================================
    
    /// Mix analysis results
    struct MixAnalysis {
        std::string analysisId;
        std::chrono::system_clock::time_point timestamp;
        
        // Frequency analysis
        struct FrequencyAnalysis {
            std::vector<float> spectrum;                // Full spectrum data
            float lowEnd = 0.0f;                       // 20-250 Hz energy
            float lowMids = 0.0f;                      // 250-500 Hz energy
            float mids = 0.0f;                         // 500-2000 Hz energy
            float highMids = 0.0f;                     // 2-8 kHz energy
            float highEnd = 0.0f;                      // 8-20 kHz energy
            std::vector<float> prominentFrequencies;   // Frequency peaks
            bool hasFrequencyImbalance = false;
            std::vector<std::string> frequencyIssues;
        } frequencyAnalysis;
        
        // Loudness analysis
        struct LoudnessAnalysis {
            float integratedLUFS = 0.0f;
            float shortTermLUFS = 0.0f;
            float momentaryLUFS = 0.0f;
            float truePeak = 0.0f;
            float loudnessRange = 0.0f;
            bool meetsStandards = false;
            std::string targetStandard = "streaming"; // streaming, broadcast, mastering
            std::vector<std::string> loudnessIssues;
        } loudnessAnalysis;
        
        // Stereo analysis
        struct StereoAnalysis {
            float stereoWidth = 0.0f;                  // 0-1 (mono to wide)
            float phaseCoherence = 0.0f;               // 0-1 (poor to perfect)
            float leftRightBalance = 0.0f;             // -1 to 1 (left to right)
            std::vector<float> stereoImage;            // Stereo field visualization
            bool hasPhaseIssues = false;
            bool hasImbalance = false;
            std::vector<std::string> stereoIssues;
        } stereoAnalysis;
        
        // Dynamic analysis
        struct DynamicAnalysis {
            float dynamicRange = 0.0f;                 // dB
            float compressionRatio = 0.0f;
            float averageRMS = 0.0f;
            float peakToCrest = 0.0f;
            std::vector<float> dynamicsOverTime;
            bool isOverCompressed = false;
            bool needsCompression = false;
            std::vector<std::string> dynamicIssues;
        } dynamicAnalysis;
        
        // Overall assessment
        struct OverallAssessment {
            float overallScore = 0.0f;                 // 0-100 quality score
            std::string genre;
            std::string style;
            std::vector<std::string> strengths;
            std::vector<std::string> weaknesses;
            std::vector<std::string> recommendations;
            bool isCommerciallyReady = false;
        } overallAssessment;
    };
    
    /// Analyze current mix
    core::AsyncResult<core::Result<MixAnalysis>> analyzeMix(
        core::ProgressCallback progress = nullptr
    );
    
    /// Analyze specific time range
    core::AsyncResult<core::Result<MixAnalysis>> analyzeMixRange(
        core::TimePosition startTime,
        core::TimePosition endTime,
        core::ProgressCallback progress = nullptr
    );
    
    /// Analyze specific tracks
    core::AsyncResult<core::Result<MixAnalysis>> analyzeTracksMix(
        const std::vector<core::TrackID>& trackIds,
        core::ProgressCallback progress = nullptr
    );
    
    /// Compare two mixes
    core::AsyncResult<core::Result<json>> compareMixes(
        const MixAnalysis& mix1,
        const MixAnalysis& mix2
    );
    
    // ========================================================================
    // Intelligent Mixing Suggestions
    // ========================================================================
    
    /// Mixing suggestion types
    enum class SuggestionCategory {
        EQAdjustment,
        DynamicsProcessing,
        StereoPlacement,
        VolumeBalance,
        EffectsProcessing,
        Automation,
        Arrangement,
        MasteringPrep
    };
    
    /// Mixing suggestion
    struct MixingSuggestion {
        SuggestionCategory category;
        std::string title;
        std::string description;
        std::string reasoning;
        std::vector<std::string> actionCommands;
        json parameters;
        float priority = 0.0f;                     // 0-1 (low to high)
        float confidence = 0.0f;                   // 0-1 (uncertain to certain)
        std::string targetElement;                 // track, bus, master, etc.
        std::vector<std::string> beforeAfterComparison;
        bool requiresUserInput = false;
    };
    
    /// Generate mixing suggestions
    core::AsyncResult<core::Result<std::vector<MixingSuggestion>>> generateMixingSuggestions(
        const MixAnalysis& analysis
    );
    
    /// Get targeted suggestions for specific issues
    core::AsyncResult<core::Result<std::vector<MixingSuggestion>>> getSuggestionsForIssue(
        const std::string& issueType,
        const json& context = json::object()
    );
    
    /// Apply mixing suggestion
    core::AsyncResult<api::ActionResult> applySuggestion(const MixingSuggestion& suggestion);
    
    /// Batch apply suggestions
    core::AsyncResult<std::vector<api::ActionResult>> applySuggestions(
        const std::vector<MixingSuggestion>& suggestions,
        core::ProgressCallback progress = nullptr
    );
    
    // ========================================================================
    // Automated Mixing Tools
    // ========================================================================
    
    /// Auto-mixing settings
    struct AutoMixSettings {
        bool enableAutoGain = true;
        bool enableAutoPanning = true;
        bool enableAutoEQ = true;
        bool enableAutoCompression = false;        // More aggressive
        bool enableAutoReverb = true;
        bool enableAutoDelay = true;
        std::string targetStyle = "balanced";      // balanced, punchy, smooth, wide
        std::string genre = "pop";
        float aggressiveness = 0.5f;               // 0-1 (subtle to aggressive)
        bool preserveUserSettings = true;
    };
    
    /// Automatic gain staging
    core::AsyncResult<core::VoidResult> autoGainStage(
        const std::vector<core::TrackID>& trackIds = {},
        const AutoMixSettings& settings = AutoMixSettings{}
    );
    
    /// Automatic EQ balancing
    core::AsyncResult<core::VoidResult> autoEQBalance(
        const std::vector<core::TrackID>& trackIds = {},
        const AutoMixSettings& settings = AutoMixSettings{}
    );
    
    /// Automatic stereo placement
    core::AsyncResult<core::VoidResult> autoStereoPlacement(
        const std::vector<core::TrackID>& trackIds = {},
        const AutoMixSettings& settings = AutoMixSettings{}
    );
    
    /// Automatic dynamics processing
    core::AsyncResult<core::VoidResult> autoDynamicsProcessing(
        const std::vector<core::TrackID>& trackIds = {},
        const AutoMixSettings& settings = AutoMixSettings{}
    );
    
    /// Complete auto-mix
    core::AsyncResult<core::VoidResult> performAutoMix(
        const AutoMixSettings& settings = AutoMixSettings{},
        core::ProgressCallback progress = nullptr
    );
    
    // ========================================================================
    // Reference Matching
    // ========================================================================
    
    /// Reference track analysis
    struct ReferenceAnalysis {
        std::string referenceId;
        std::string filePath;
        MixAnalysis analysis;
        std::string genre;
        std::string style;
        std::vector<std::string> characteristics;
        float matchRelevance = 0.0f;
    };
    
    /// Add reference track
    core::AsyncResult<core::Result<std::string>> addReferenceTrack(
        const std::string& filePath,
        const std::string& genre = "",
        const std::string& style = ""
    );
    
    /// Remove reference track
    core::VoidResult removeReferenceTrack(const std::string& referenceId);
    
    /// Get all reference tracks
    std::vector<ReferenceAnalysis> getReferenceLibrary() const;
    
    /// Match current mix to reference
    core::AsyncResult<core::Result<std::vector<MixingSuggestion>>> matchToReference(
        const std::string& referenceId,
        float matchStrength = 0.7f
    );
    
    /// Find similar reference tracks
    core::AsyncResult<core::Result<std::vector<ReferenceAnalysis>>> findSimilarReferences(
        const MixAnalysis& currentMix,
        int32_t maxResults = 5
    );
    
    /// Auto-select best reference
    core::AsyncResult<core::Result<ReferenceAnalysis>> autoSelectReference(
        const MixAnalysis& currentMix
    );
    
    // ========================================================================
    // Genre-Specific Mixing
    // ========================================================================
    
    /// Genre mixing templates
    enum class GenreTemplate {
        Pop,
        Rock,
        Electronic,
        HipHop,
        Jazz,
        Classical,
        Country,
        Metal,
        Folk,
        Reggae,
        Blues,
        Ambient,
        Custom
    };
    
    /// Genre-specific settings
    struct GenreSettings {
        GenreTemplate genre;
        std::string subGenre;
        
        // Frequency balance preferences
        float bassEmphasis = 0.0f;                 // -1 to 1
        float midrangeFocus = 0.0f;
        float highendsAir = 0.0f;
        
        // Dynamic characteristics
        float dynamicRange = 0.0f;                 // Target DR value
        float punchiness = 0.0f;                   // Transient emphasis
        float smoothness = 0.0f;                   // Compression style
        
        // Stereo image preferences
        float stereoWidth = 0.0f;                  // 0-1 (mono to wide)
        std::vector<std::pair<std::string, float>> instrumentPanning; // instrument -> pan value
        
        // Effects preferences
        float reverbAmount = 0.0f;
        std::string reverbType = "hall";           // room, hall, plate, spring
        float delayAmount = 0.0f;
        bool useParallelCompression = false;
        
        json customSettings;
    };
    
    /// Apply genre-specific mixing template
    core::AsyncResult<core::VoidResult> applyGenreTemplate(
        GenreTemplate genre,
        const std::string& subGenre = "",
        float strength = 1.0f
    );
    
    /// Get genre recommendations based on analysis
    core::AsyncResult<core::Result<std::vector<GenreTemplate>>> recommendGenres(
        const MixAnalysis& analysis
    );
    
    /// Create custom genre template
    core::VoidResult createCustomGenreTemplate(
        const std::string& name,
        const GenreSettings& settings
    );
    
    /// Get available genre templates
    std::vector<std::pair<GenreTemplate, std::string>> getAvailableGenreTemplates() const;
    
    // ========================================================================
    // Mix Validation and Quality Control
    // ========================================================================
    
    /// Quality check results
    struct QualityCheck {
        bool passed = false;
        float overallScore = 0.0f;                 // 0-100
        
        struct Check {
            std::string name;
            bool passed = false;
            float score = 0.0f;
            std::string description;
            std::vector<std::string> issues;
            std::vector<std::string> suggestions;
        };
        
        std::vector<Check> checks;
        std::vector<std::string> criticalIssues;
        std::vector<std::string> warnings;
        std::string overallAssessment;
    };
    
    /// Perform comprehensive quality check
    core::AsyncResult<core::Result<QualityCheck>> performQualityCheck(
        const std::string& targetStandard = "streaming" // streaming, broadcast, vinyl, cd
    );
    
    /// Check against specific standards
    core::AsyncResult<core::Result<QualityCheck>> checkAgainstStandards(
        const std::vector<std::string>& standards
    );
    
    /// Validate mix for commercial release
    core::AsyncResult<core::Result<QualityCheck>> validateForRelease(
        const std::string& distributionPlatform = "streaming"
    );
    
    /// Generate mix report
    core::AsyncResult<core::VoidResult> generateMixReport(
        const MixAnalysis& analysis,
        const QualityCheck& qualityCheck,
        const std::string& outputPath
    );
    
    // ========================================================================
    // Learning and Adaptation
    // ========================================================================
    
    /// User feedback on suggestions
    void rateSuggestion(const MixingSuggestion& suggestion, int32_t rating); // 1-5
    
    /// Learn from user mixing decisions
    void learnFromMixingDecision(
        const std::string& context,
        const std::string& userAction,
        const json& parameters
    );
    
    /// Update mixing preferences
    void updateMixingPreferences(const json& preferences);
    
    /// Get personalized mixing style
    json getPersonalizedMixingStyle() const;
    
    // ========================================================================
    // Event Callbacks
    // ========================================================================
    
    /// Mixing assistant events
    enum class MixingEvent {
        AnalysisCompleted,
        SuggestionGenerated,
        AutoMixCompleted,
        QualityCheckCompleted,
        ReferenceMatched,
        IssueDetected
    };
    
    /// Mixing event callback type
    using MixingEventCallback = std::function<void(MixingEvent event, const json& data)>;
    
    /// Set mixing event callback
    void setMixingEventCallback(MixingEventCallback callback);
    
    /// Clear mixing event callback
    void clearMixingEventCallback();

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Analyze frequency content
    MixAnalysis::FrequencyAnalysis analyzeFrequencyContent(
        const core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate
    );
    
    /// Analyze loudness characteristics
    MixAnalysis::LoudnessAnalysis analyzeLoudnessCharacteristics(
        const core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate
    );
    
    /// Analyze stereo characteristics
    MixAnalysis::StereoAnalysis analyzeStereoCharacteristics(
        const core::FloatAudioBuffer& buffer
    );
    
    /// Analyze dynamic characteristics
    MixAnalysis::DynamicAnalysis analyzeDynamicCharacteristics(
        const core::FloatAudioBuffer& buffer
    );
    
    /// Generate frequency-based suggestions
    std::vector<MixingSuggestion> generateFrequencySuggestions(
        const MixAnalysis::FrequencyAnalysis& analysis
    );
    
    /// Generate dynamics-based suggestions
    std::vector<MixingSuggestion> generateDynamicsSuggestions(
        const MixAnalysis::DynamicAnalysis& analysis
    );
    
    /// Generate stereo-based suggestions
    std::vector<MixingSuggestion> generateStereoSuggestions(
        const MixAnalysis::StereoAnalysis& analysis
    );
    
    /// Calculate mix similarity
    float calculateMixSimilarity(
        const MixAnalysis& mix1,
        const MixAnalysis& mix2
    );
    
    /// Emit mixing event
    void emitMixingEvent(MixingEvent event, const json& data);

private:
    // Service references
    std::shared_ptr<api::ActionAPI> actionAPI_;
    std::shared_ptr<services::OSSServiceRegistry> ossServices_;
    
    // Reference library
    std::vector<ReferenceAnalysis> referenceLibrary_;
    mutable std::shared_mutex referenceLibraryMutex_;
    
    // Genre templates
    std::unordered_map<std::string, GenreSettings> customGenreTemplates_;
    mutable std::shared_mutex genreTemplatesMutex_;
    
    // Learning data
    std::vector<json> mixingDecisions_;
    json personalizedStyle_;
    mutable std::shared_mutex learningMutex_;
    
    // Event callback
    MixingEventCallback mixingEventCallback_;
    std::mutex callbackMutex_;
    
    // Constants
    static constexpr int32_t MAX_REFERENCE_TRACKS = 100;
    static constexpr float DEFAULT_ANALYSIS_THRESHOLD = -40.0f; // dB
};

} // namespace mixmind::ai