#pragma once
#include "../core/result.h"
#include "../core/ITrack.h"
#include "MusicKnowledgeBase.h"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace mixmind::ai {

// ============================================================================
// Style Matcher - Advanced artist and genre reference matching system
// Bridges natural language requests with specific artist processing
// ============================================================================

class StyleMatcher {
public:
    StyleMatcher(std::shared_ptr<MusicKnowledgeBase> knowledge);
    ~StyleMatcher() = default;
    
    // Non-copyable
    StyleMatcher(const StyleMatcher&) = delete;
    StyleMatcher& operator=(const StyleMatcher&) = delete;
    
    // ========================================================================
    // Reference Matching and Analysis
    // ========================================================================
    
    /// Match artist references in natural language text
    struct ArtistMatch {
        std::string artist_name;
        std::string original_text;
        float confidence;
        std::vector<std::string> matched_keywords;
        ArtistStyle style;
    };
    
    /// Find artist references in user message
    std::vector<ArtistMatch> findArtistReferences(const std::string& message);
    
    /// Match genre references with confidence scoring
    struct GenreMatch {
        std::string genre;
        float confidence;
        std::vector<std::string> matched_indicators;
        GenreCharacteristics characteristics;
    };
    
    std::vector<GenreMatch> findGenreReferences(const std::string& message);
    
    // ========================================================================
    // Style Analysis and Recommendation
    // ========================================================================
    
    /// Analyze track and suggest similar artists
    core::AsyncResult<core::Result<std::vector<ArtistMatch>>> 
    findSimilarArtists(std::shared_ptr<core::ITrack> track, int maxResults = 5);
    
    /// Analyze track characteristics and suggest processing
    struct ProcessingRecommendation {
        std::string type; // "eq", "compression", "reverb", etc.
        std::string description;
        std::map<std::string, float> parameters;
        std::string reasoning;
        float confidence;
    };
    
    std::vector<ProcessingRecommendation> recommendProcessing(
        std::shared_ptr<core::ITrack> track,
        const std::string& target_artist = ""
    );
    
    // ========================================================================
    // Reference Blending and Morphing
    // ========================================================================
    
    /// Create blended style from multiple artist references
    struct BlendedStyle {
        std::vector<std::pair<std::string, float>> artist_weights;
        ArtistStyle combined_style;
        std::string description;
    };
    
    BlendedStyle createBlendedStyle(
        const std::vector<std::pair<std::string, float>>& artist_weights
    );
    
    /// Calculate style distance between two artists
    float calculateStyleDistance(const std::string& artist1, const std::string& artist2);
    
    /// Find artists in style spectrum between two references
    std::vector<ArtistMatch> findIntermediateStyles(
        const std::string& artist1, 
        const std::string& artist2,
        int steps = 3
    );
    
    // ========================================================================
    // Natural Language Processing
    // ========================================================================
    
    /// Parse complex requests like "make it 60% Billie Eilish, 40% The Pixies"
    struct ComplexStyleRequest {
        std::vector<std::pair<std::string, float>> artist_references;
        std::vector<std::string> characteristic_requests; // "warmer", "punchier"
        std::vector<std::string> genre_influences;
        std::string era_reference; // "90s style", "modern production"
        float intensity = 1.0f;
    };
    
    ComplexStyleRequest parseComplexRequest(const std::string& message);
    
    /// Convert descriptive words to style parameters
    std::map<std::string, float> parseDescriptors(const std::vector<std::string>& descriptors);
    
    // ========================================================================
    // Contextual Matching
    // ========================================================================
    
    /// Match based on current track context
    struct ContextualMatch {
        ArtistMatch base_match;
        std::string context_reason; // "Similar BPM", "Same key", etc.
        float context_boost;
        float final_confidence;
    };
    
    std::vector<ContextualMatch> findContextualMatches(
        std::shared_ptr<core::ITrack> track,
        const std::string& query
    );
    
    /// Update matching based on user feedback
    void updateMatchingPreferences(
        const std::string& query,
        const std::string& accepted_artist,
        bool positive_feedback
    );
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Fuzzy artist name matching with typo tolerance
    std::vector<std::string> findSimilarArtistNames(const std::string& query, float threshold = 0.7f);
    
    /// Extract era/decade references
    struct EraReference {
        std::string era;
        std::string specific_years;
        std::vector<std::string> characteristics;
        float confidence;
    };
    
    std::vector<EraReference> findEraReferences(const std::string& message);
    
    /// Smart aliasing (nicknames, alternate names)
    void addArtistAlias(const std::string& alias, const std::string& canonical_name);
    std::string resolveArtistAlias(const std::string& name);
    
    // ========================================================================
    // Matching Statistics and Learning
    // ========================================================================
    
    /// Get matching statistics for debugging
    struct MatchingStats {
        int total_queries;
        int successful_matches;
        float average_confidence;
        std::map<std::string, int> most_requested_artists;
        std::vector<std::string> unmatched_queries;
    };
    
    MatchingStats getMatchingStatistics();
    
    /// Reset learning data
    void clearMatchingHistory();

private:
    // ========================================================================
    // Internal Processing Methods
    // ========================================================================
    
    /// Calculate text similarity using various algorithms
    float calculateTextSimilarity(const std::string& text1, const std::string& text2);
    
    /// Extract keywords from natural language
    std::vector<std::string> extractKeywords(const std::string& text);
    
    /// Normalize artist names for matching
    std::string normalizeArtistName(const std::string& name);
    
    /// Calculate confidence based on multiple factors
    float calculateMatchConfidence(
        const std::string& query,
        const std::string& artist,
        const std::vector<std::string>& matched_keywords
    );
    
    /// Score contextual relevance
    float scoreContextualRelevance(
        std::shared_ptr<core::ITrack> track,
        const ArtistStyle& style
    );
    
    // ========================================================================
    // Style Analysis Helpers
    // ========================================================================
    
    /// Analyze track tempo, key, energy
    std::map<std::string, float> analyzeTrackCharacteristics(std::shared_ptr<core::ITrack> track);
    
    /// Compare track characteristics with artist style
    float compareWithArtistStyle(
        const std::map<std::string, float>& track_characteristics,
        const ArtistStyle& artist_style
    );
    
    /// Generate style fingerprint for comparison
    std::vector<float> generateStyleFingerprint(const ArtistStyle& style);
    
    // ========================================================================
    // Natural Language Processing Helpers
    // ========================================================================
    
    /// Extract percentage/weight references
    std::vector<std::pair<std::string, float>> extractWeights(const std::string& text);
    
    /// Parse intensity modifiers ("very", "slightly", "much more")
    float parseIntensityModifiers(const std::string& text);
    
    /// Extract negative constraints ("not like", "without")
    std::vector<std::string> extractNegativeConstraints(const std::string& text);
    
    // ========================================================================
    // State Management
    // ========================================================================
    
    std::shared_ptr<MusicKnowledgeBase> knowledge_base_;
    
    // Artist alias mapping
    std::map<std::string, std::string> artist_aliases_;
    
    // Learning and statistics
    MatchingStats stats_;
    std::map<std::string, float> user_preference_weights_;
    std::vector<std::string> query_history_;
    
    // Precomputed similarity matrices for performance
    std::map<std::pair<std::string, std::string>, float> style_distance_cache_;
    
    /// Initialize built-in aliases and common misspellings
    void initializeArtistAliases();
    
    /// Update statistics
    void updateStats(const std::string& query, bool successful, float confidence);
    
    /// Precompute style distances for common artists
    void precomputeStyleDistances();
};

// ============================================================================
// Style Matching Utilities
// ============================================================================

namespace style_matching_utils {
    /// Levenshtein distance for fuzzy string matching
    int calculateEditDistance(const std::string& s1, const std::string& s2);
    
    /// Extract quoted strings from text
    std::vector<std::string> extractQuotedStrings(const std::string& text);
    
    /// Parse percentage expressions ("80%", "half", "mostly")
    float parsePercentage(const std::string& text);
    
    /// Normalize genre names
    std::string normalizeGenreName(const std::string& genre);
    
    /// Extract time period references
    std::vector<std::string> extractTimePeriods(const std::string& text);
}

} // namespace mixmind::ai