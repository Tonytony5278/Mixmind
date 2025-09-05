#pragma once
#include "../core/result.h"
#include "../core/async.h"
#include "../core/ITrack.h"
#include "MusicKnowledgeBase.h"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace mixmind::ai {

// ============================================================================
// Intelligent Audio Processor - Applies AI-driven processing based on 
// musical styles, artist references, and production techniques
// ============================================================================

class IntelligentProcessor {
public:
    IntelligentProcessor(std::shared_ptr<MusicKnowledgeBase> knowledge);
    ~IntelligentProcessor() = default;
    
    // Non-copyable
    IntelligentProcessor(const IntelligentProcessor&) = delete;
    IntelligentProcessor& operator=(const IntelligentProcessor&) = delete;
    
    // ========================================================================
    // Artist-Style Processing
    // ========================================================================
    
    /// Apply complete artist style to track
    core::AsyncResult<core::VoidResult> applyArtistStyle(
        std::shared_ptr<core::ITrack> track,
        const std::string& artist,
        float intensity = 1.0f
    );
    
    /// Apply artist-specific vocal processing
    core::AsyncResult<core::VoidResult> applyVocalChain(
        std::shared_ptr<core::ITrack> track,
        const std::string& artist,
        float intensity = 1.0f
    );
    
    /// Apply artist-specific drum processing  
    core::AsyncResult<core::VoidResult> applyDrumProcessing(
        std::shared_ptr<core::ITrack> track,
        const std::string& artist,
        float intensity = 1.0f
    );
    
    /// Apply instrument processing in artist's style
    core::AsyncResult<core::VoidResult> applyInstrumentProcessing(
        std::shared_ptr<core::ITrack> track,
        const std::string& artist,
        const std::string& instrument,
        float intensity = 1.0f
    );
    
    // ========================================================================
    // Genre-Based Processing
    // ========================================================================
    
    /// Apply genre-typical processing
    core::AsyncResult<core::VoidResult> applyGenreProcessing(
        std::shared_ptr<core::ITrack> track,
        const std::string& genre,
        float intensity = 1.0f
    );
    
    /// Apply era-specific production characteristics
    core::AsyncResult<core::VoidResult> applyEraCharacteristics(
        std::shared_ptr<core::ITrack> track,
        const std::string& era,
        float intensity = 1.0f
    );
    
    // ========================================================================
    // Master Bus Processing
    // ========================================================================
    
    /// Master in the style of reference artist/song
    core::AsyncResult<core::VoidResult> masterInStyleOf(
        std::shared_ptr<core::ITrack> masterTrack,
        const std::string& reference,
        float intensity = 1.0f
    );
    
    /// Apply genre-appropriate mastering
    core::AsyncResult<core::VoidResult> masterForGenre(
        std::shared_ptr<core::ITrack> masterTrack,
        const std::string& genre,
        float intensity = 1.0f
    );
    
    /// Apply era-appropriate mastering characteristics
    core::AsyncResult<core::VoidResult> masterForEra(
        std::shared_ptr<core::ITrack> masterTrack,
        const std::string& era,
        float intensity = 1.0f
    );
    
    // ========================================================================
    // Descriptive Processing
    // ========================================================================
    
    /// Apply processing based on descriptive words (bright, warm, punchy, etc.)
    core::AsyncResult<core::VoidResult> applyDescriptiveProcessing(
        std::shared_ptr<core::ITrack> track,
        const std::vector<std::string>& descriptors,
        float intensity = 1.0f
    );
    
    /// Make track sound "brighter", "warmer", "punchier", etc.
    core::AsyncResult<core::VoidResult> applyCharacteristic(
        std::shared_ptr<core::ITrack> track,
        const std::string& characteristic,
        float intensity = 1.0f
    );
    
    // ========================================================================
    // Natural Language Processing
    // ========================================================================
    
    /// Process natural language requests
    core::AsyncResult<core::VoidResult> processNaturalLanguageRequest(
        std::shared_ptr<core::ITrack> track,
        const std::string& request
    );
    
    /// Generate processing recommendations based on analysis
    core::AsyncResult<core::Result<std::vector<std::string>>> recommendProcessing(
        std::shared_ptr<core::ITrack> track,
        const std::string& targetStyle = ""
    );
    
    /// Explain what processing was applied
    std::string explainProcessing(const std::string& artistOrStyle) const;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Blend multiple artist styles
    core::AsyncResult<core::VoidResult> blendArtistStyles(
        std::shared_ptr<core::ITrack> track,
        const std::vector<std::pair<std::string, float>>& artistWeights
    );
    
    /// Morph between two styles over time
    core::AsyncResult<core::VoidResult> morphBetweenStyles(
        std::shared_ptr<core::ITrack> track,
        const std::string& fromArtist,
        const std::string& toArtist,
        float position // 0.0 to 1.0
    );
    
    /// Analyze track and suggest similar artists
    core::AsyncResult<core::Result<std::vector<std::string>>> suggestSimilarArtists(
        std::shared_ptr<core::ITrack> track,
        int maxSuggestions = 5
    );
    
    // ========================================================================
    // Processing History and Undo
    // ========================================================================
    
    struct ProcessingAction {
        std::string actionType;
        std::string targetArtist;
        std::string targetCharacteristic;  
        std::map<std::string, float> appliedSettings;
        std::string description;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    /// Get processing history for track
    std::vector<ProcessingAction> getProcessingHistory(std::shared_ptr<core::ITrack> track) const;
    
    /// Undo last processing action
    core::AsyncResult<core::VoidResult> undoLastProcessing(std::shared_ptr<core::ITrack> track);
    
    /// Clear processing history
    void clearProcessingHistory(std::shared_ptr<core::ITrack> track);

private:
    // ========================================================================
    // Internal Processing Methods
    // ========================================================================
    
    /// Apply EQ based on artist characteristics
    void applyArtistEQ(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity);
    
    /// Apply compression based on artist characteristics
    void applyArtistCompression(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity);
    
    /// Apply reverb/spatial effects
    void applyArtistSpatialEffects(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity);
    
    /// Apply distortion/saturation effects
    void applyArtistSaturation(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity);
    
    /// Apply stereo processing
    void applyArtistStereoProcessing(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity);
    
    // ========================================================================
    // Specific Effect Application
    // ========================================================================
    
    /// Apply compression with specific settings
    void applyCompression(
        std::shared_ptr<core::ITrack> track, 
        float ratio, 
        float threshold, 
        float attack = 10.0f, 
        float release = 100.0f
    );
    
    /// Apply EQ with frequency bands
    void applyEQ(
        std::shared_ptr<core::ITrack> track,
        const std::vector<std::tuple<float, float, float>>& bands // freq, gain, Q
    );
    
    /// Apply reverb with specific characteristics
    void applyReverb(
        std::shared_ptr<core::ITrack> track,
        float roomSize,
        float wetLevel,
        float decayTime = 1.0f
    );
    
    /// Apply distortion/saturation
    void applyDistortion(
        std::shared_ptr<core::ITrack> track,
        float drive,
        const std::string& type = "tube"
    );
    
    /// Apply stereo width processing
    void applyStereoWidth(
        std::shared_ptr<core::ITrack> track,
        float width // 0.0 (mono) to 2.0 (wide)
    );
    
    /// Apply filtering effects
    void applyFiltering(
        std::shared_ptr<core::ITrack> track,
        const std::string& filterType,
        float frequency,
        float resonance = 1.0f
    );
    
    // ========================================================================
    // Processing Analysis and Mapping
    // ========================================================================
    
    /// Convert descriptive words to processing parameters
    std::map<std::string, float> descriptorToParameters(const std::string& descriptor) const;
    
    /// Analyze current track characteristics  
    std::map<std::string, float> analyzeTrackCharacteristics(std::shared_ptr<core::ITrack> track) const;
    
    /// Calculate processing intensity based on current state
    float calculateOptimalIntensity(
        std::shared_ptr<core::ITrack> track,
        const ArtistStyle& targetStyle
    ) const;
    
    /// Generate processing explanation
    std::string generateProcessingExplanation(
        const ArtistStyle& style,
        const std::vector<std::string>& appliedEffects
    ) const;
    
    // ========================================================================
    // State Management
    // ========================================================================
    
    std::shared_ptr<MusicKnowledgeBase> knowledgeBase_;
    
    // Processing history per track
    std::map<core::TrackID, std::vector<ProcessingAction>> processingHistory_;
    
    // Characteristic mappings
    std::map<std::string, std::vector<std::tuple<float, float, float>>> characteristicEQCurves_;
    std::map<std::string, std::pair<float, float>> characteristicCompression_;
    std::map<std::string, std::tuple<float, float, float>> characteristicReverb_;
    
    /// Initialize characteristic mappings
    void initializeCharacteristicMappings();
    
    /// Log processing action for history
    void logProcessingAction(
        std::shared_ptr<core::ITrack> track,
        const std::string& actionType,
        const std::string& target,
        const std::map<std::string, float>& settings,
        const std::string& description
    );
    
    /// Get safe processing intensity (prevent over-processing)
    float getSafeIntensity(float requestedIntensity, int previousProcessingCount) const;
};

// ============================================================================
// Processing Utilities
// ============================================================================

namespace processing_utils {
    /// Convert artist style to EQ curve
    std::vector<std::tuple<float, float, float>> styleToEQCurve(const ArtistStyle& style);
    
    /// Convert artist style to compression settings
    std::tuple<float, float, float, float> styleToCompression(const ArtistStyle& style);
    
    /// Convert artist style to reverb settings
    std::tuple<float, float, float> styleToReverb(const ArtistStyle& style);
    
    /// Blend two processing settings
    template<typename T>
    T blendSettings(const T& setting1, const T& setting2, float blend);
    
    /// Scale processing intensity safely
    std::map<std::string, float> scaleIntensity(
        const std::map<std::string, float>& baseSettings,
        float intensity
    );
}

} // namespace mixmind::ai