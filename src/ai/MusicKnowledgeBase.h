#pragma once
#include "../core/result.h"
#include "../core/async.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>
#include <algorithm>
#include <memory>

namespace mixmind::ai {

// ============================================================================
// Artist and Genre Style Definitions
// ============================================================================

struct VocalStyle {
    std::string description;
    std::vector<std::string> effects;           // reverb, compression, etc.
    std::map<std::string, float> parameters;    // specific settings
    std::string micTechnique;                   // close-mic, distant, etc.
    std::string character;                      // intimate, powerful, raw, etc.
};

struct DrumStyle {
    std::string description;
    std::vector<std::string> characteristics;   // punchy, minimal, heavy, etc.
    std::string typicalKit;                     // vintage, modern, electronic
    std::map<std::string, float> processing;    // compression, EQ settings
};

struct InstrumentStyle {
    std::string instrument;                     // guitar, bass, piano, etc.
    std::string tone;                          // clean, distorted, warm, etc.
    std::vector<std::string> effects;         // chorus, delay, distortion
    std::map<std::string, float> settings;    // specific effect parameters
};

struct ProcessingChain {
    std::string description;
    std::vector<std::string> plugins;         // plugin types needed
    std::map<std::string, float> settings;   // parameter values
    std::string purpose;                      // mixing, mastering, creative
};

struct ArtistStyle {
    std::string artist;
    std::string genre;
    std::string era;                          // 60s, 80s, 2000s, modern, etc.
    std::string overallCharacter;            // raw, polished, experimental, etc.
    
    // Production characteristics
    VocalStyle vocals;
    DrumStyle drums;
    std::vector<InstrumentStyle> instruments;
    
    ProcessingChain mixingStyle;
    ProcessingChain masteringStyle;
    
    // Style descriptors and keywords
    std::vector<std::string> keywords;       // garage, fuzzy, intimate, bright
    std::vector<std::string> influences;     // other artists who influenced this style
    std::vector<std::string> influencedBy;   // artists influenced by this style
    
    // Technical characteristics
    std::map<std::string, std::string> technicalNotes;
    float typicalLoudness = -14.0f;          // LUFS
    float typicalDynamicRange = 8.0f;        // dB
};

struct GenreCharacteristics {
    std::string genre;
    std::string description;
    std::vector<std::string> keyArtists;
    std::map<std::string, std::string> commonElements;
    ProcessingChain typicalProcessing;
    std::vector<std::string> subgenres;
};

// ============================================================================
// Production Technique Database
// ============================================================================

struct ProductionTechnique {
    std::string name;
    std::string description;
    std::string category;                     // vocal, drum, mix, master
    std::vector<std::string> steps;
    std::map<std::string, float> parameters;
    std::vector<std::string> associatedArtists;
};

// ============================================================================
// Music Knowledge Database - Core AI Music Intelligence
// ============================================================================

class MusicKnowledgeBase {
public:
    MusicKnowledgeBase();
    ~MusicKnowledgeBase() = default;
    
    // ========================================================================
    // Service Lifecycle
    // ========================================================================
    
    /// Initialize with comprehensive music database
    core::AsyncResult<core::VoidResult> initialize();
    
    /// Load additional artist database from file
    core::AsyncResult<core::VoidResult> loadArtistDatabase(const std::string& filePath);
    
    /// Check if knowledge base is ready
    bool isReady() const { return isInitialized_; }
    
    // ========================================================================
    // Artist and Style Lookup
    // ========================================================================
    
    /// Get complete artist style information
    std::optional<ArtistStyle> getArtistStyle(const std::string& artist);
    
    /// Search for artists by genre
    std::vector<ArtistStyle> getArtistsByGenre(const std::string& genre);
    
    /// Search for artists by keywords/characteristics
    std::vector<ArtistStyle> searchByKeywords(const std::vector<std::string>& keywords);
    
    /// Find similar artists based on style characteristics
    std::vector<std::string> findSimilarArtists(const std::string& artist, int maxResults = 5);
    
    /// Get all available artists in database
    std::vector<std::string> getAllArtists() const;
    
    /// Check if artist exists in database
    bool hasArtist(const std::string& artist) const;
    
    // ========================================================================
    // Production Chain Recommendations
    // ========================================================================
    
    /// Get vocal processing chain for specific artist
    std::vector<std::string> getVocalChain(const std::string& artist);
    
    /// Get drum processing recommendations
    std::vector<std::string> getDrumProcessing(const std::string& artist);
    
    /// Get instrument processing for specific artist
    std::vector<std::string> getInstrumentProcessing(const std::string& artist, const std::string& instrument);
    
    /// Get mixing approach for artist/genre
    ProcessingChain getMixingStyle(const std::string& artist);
    
    /// Get mastering settings for genre/era
    std::map<std::string, float> getMasteringSettings(const std::string& genre);
    
    // ========================================================================
    // Genre and Style Analysis
    // ========================================================================
    
    /// Get genre characteristics
    std::optional<GenreCharacteristics> getGenreInfo(const std::string& genre);
    
    /// Get all available genres
    std::vector<std::string> getAllGenres() const;
    
    /// Classify unknown style based on characteristics
    std::vector<std::string> classifyStyle(const std::map<std::string, std::string>& characteristics);
    
    /// Get production era characteristics (60s, 70s, 80s, etc.)
    std::map<std::string, std::string> getEraCharacteristics(const std::string& era);
    
    // ========================================================================
    // Production Techniques
    // ========================================================================
    
    /// Get specific production technique
    std::optional<ProductionTechnique> getTechnique(const std::string& techniqueName);
    
    /// Get techniques by category (vocal, drum, mixing, mastering)
    std::vector<ProductionTechnique> getTechniquesByCategory(const std::string& category);
    
    /// Search techniques by description or keywords
    std::vector<ProductionTechnique> searchTechniques(const std::string& query);
    
    // ========================================================================
    // Natural Language Processing
    // ========================================================================
    
    /// Parse artist references from natural language
    std::vector<std::string> parseArtistReferences(const std::string& text);
    
    /// Extract style descriptors from text
    std::vector<std::string> extractStyleDescriptors(const std::string& text);
    
    /// Interpret production requests
    struct ProductionRequest {
        std::string artist;
        std::string style;
        std::string target;                   // vocals, drums, mix, master
        std::vector<std::string> modifiers;   // subtle, heavy, modern, vintage
        float intensity = 1.0f;               // how strongly to apply
    };
    
    ProductionRequest interpretRequest(const std::string& request);
    
    /// Generate suggestions based on context
    std::vector<std::string> generateSuggestions(const std::string& context);
    
    // ========================================================================
    // Advanced Analysis
    // ========================================================================
    
    /// Analyze compatibility between artists/styles
    float calculateStyleCompatibility(const std::string& artist1, const std::string& artist2);
    
    /// Get style evolution/timeline
    std::vector<std::string> getStyleEvolution(const std::string& artist);
    
    /// Get cultural/historical context
    std::string getHistoricalContext(const std::string& artist);
    
    /// Predict style trends
    std::vector<std::string> predictTrends(const std::string& genre);
    
    // ========================================================================
    // Database Management
    // ========================================================================
    
    /// Add custom artist style
    core::VoidResult addCustomArtist(const ArtistStyle& style);
    
    /// Update existing artist information
    core::VoidResult updateArtistStyle(const std::string& artist, const ArtistStyle& style);
    
    /// Add production technique
    core::VoidResult addProductionTechnique(const ProductionTechnique& technique);
    
    /// Export knowledge base to file
    core::AsyncResult<core::VoidResult> exportDatabase(const std::string& filePath) const;
    
    /// Get database statistics
    struct DatabaseStats {
        int artistCount = 0;
        int genreCount = 0;
        int techniqueCount = 0;
        std::map<std::string, int> artistsByGenre;
        std::map<std::string, int> techniquesByCategory;
    };
    
    DatabaseStats getDatabaseStats() const;

private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Load built-in comprehensive artist database
    void loadBuiltInArtists();
    
    /// Load genre characteristics
    void loadGenreDatabase();
    
    /// Load production techniques
    void loadProductionTechniques();
    
    /// Initialize natural language processing
    void initializeNLP();
    
    /// Normalize artist/genre names for lookup
    std::string normalizeString(const std::string& input) const;
    
    /// Calculate similarity between styles
    float calculateSimilarity(const ArtistStyle& style1, const ArtistStyle& style2) const;
    
    /// Extract keywords from text for matching
    std::vector<std::string> extractKeywords(const std::string& text) const;
    
    // Core databases
    std::unordered_map<std::string, ArtistStyle> artistDatabase_;
    std::unordered_map<std::string, GenreCharacteristics> genreDatabase_;
    std::unordered_map<std::string, ProductionTechnique> techniqueDatabase_;
    
    // Lookup tables and indices
    std::unordered_map<std::string, std::vector<std::string>> genreToArtists_;
    std::unordered_map<std::string, std::vector<std::string>> keywordToArtists_;
    std::unordered_map<std::string, std::vector<std::string>> eraToArtists_;
    
    // Natural language processing
    std::vector<std::string> artistNameVariants_;
    std::unordered_map<std::string, std::string> styleKeywords_;
    std::vector<std::string> productionTerms_;
    
    // State
    std::atomic<bool> isInitialized_{false};
    mutable std::shared_mutex databaseMutex_;
    
    // Statistics
    mutable DatabaseStats cachedStats_;
    mutable std::chrono::steady_clock::time_point lastStatsUpdate_;
};

// ============================================================================
// Knowledge Base Factory and Utilities
// ============================================================================

class MusicKnowledgeFactory {
public:
    /// Create knowledge base with default comprehensive database
    static std::unique_ptr<MusicKnowledgeBase> createDefault();
    
    /// Create knowledge base focused on specific genres
    static std::unique_ptr<MusicKnowledgeBase> createGenreFocused(const std::vector<std::string>& genres);
    
    /// Create knowledge base for specific era
    static std::unique_ptr<MusicKnowledgeBase> createEraFocused(const std::string& era);
};

// ============================================================================
// Style Matching Utilities
// ============================================================================

namespace style_utils {
    /// Convert style descriptors to processing parameters
    std::map<std::string, float> descriptorsToParameters(const std::vector<std::string>& descriptors);
    
    /// Generate processing chain from style characteristics
    ProcessingChain generateProcessingChain(const ArtistStyle& style, const std::string& target);
    
    /// Blend multiple artist styles
    ArtistStyle blendStyles(const std::vector<ArtistStyle>& styles, const std::vector<float>& weights);
    
    /// Extract musical characteristics from audio analysis
    std::map<std::string, float> extractCharacteristics(const std::vector<float>& audioFeatures);
}

} // namespace mixmind::ai