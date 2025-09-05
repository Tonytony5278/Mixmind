#include "MusicKnowledgeBase.h"
#include "../core/async.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace mixmind::ai {

MusicKnowledgeBase::MusicKnowledgeBase() {
    // Initialize empty - will be populated during initialization
}

core::AsyncResult<core::VoidResult> MusicKnowledgeBase::initialize() {
    return core::async<core::VoidResult>([this]() -> core::VoidResult {
        try {
            std::cout << "🎵 Loading Music Knowledge Database..." << std::endl;
            
            // Load comprehensive built-in artist database
            loadBuiltInArtists();
            
            // Load genre characteristics
            loadGenreDatabase();
            
            // Load production techniques
            loadProductionTechniques();
            
            // Initialize natural language processing
            initializeNLP();
            
            isInitialized_.store(true);
            
            auto stats = getDatabaseStats();
            std::cout << "✅ Music Knowledge Database loaded successfully!" << std::endl;
            std::cout << "   • " << stats.artistCount << " artists" << std::endl;
            std::cout << "   • " << stats.genreCount << " genres" << std::endl;
            std::cout << "   • " << stats.techniqueCount << " production techniques" << std::endl;
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to initialize Music Knowledge Database: " + std::string(e.what()));
        }
    });
}

void MusicKnowledgeBase::loadBuiltInArtists() {
    std::unique_lock<std::shared_mutex> lock(databaseMutex_);
    
    // ========================================================================
    // ALTERNATIVE/INDIE ROCK
    // ========================================================================
    
    // The Pixies - Garage Rock Pioneers
    ArtistStyle pixies;
    pixies.artist = "The Pixies";
    pixies.genre = "Alternative Rock";
    pixies.era = "1980s";
    pixies.overallCharacter = "Raw, dynamic, influential alternative rock with quiet-loud dynamics";
    
    pixies.vocals.description = "Raw, emotional vocals with dramatic quiet verse/loud chorus dynamics";
    pixies.vocals.effects = {"minimal_reverb", "light_compression", "distortion_on_screams"};
    pixies.vocals.parameters = {{"compression_ratio", 2.5f}, {"reverb_room", 0.3f}, {"distortion_drive", 0.7f}};
    pixies.vocals.micTechnique = "close_dynamic";
    pixies.vocals.character = "raw_emotional";
    
    pixies.drums.description = "Punchy, minimal drum kit with emphasis on snare and kick";
    pixies.drums.characteristics = {"punchy", "minimal", "garage", "dynamic"};
    pixies.drums.typicalKit = "vintage_small_kit";
    pixies.drums.processing = {{"compression_ratio", 4.0f}, {"eq_low_boost", 2.0f}, {"eq_mid_cut", -1.5f}};
    
    pixies.mixingStyle.description = "Raw, unpolished sound with prominent fuzzy bass";
    pixies.mixingStyle.plugins = {"vintage_eq", "tube_compression", "tape_saturation"};
    pixies.mixingStyle.settings = {{"bass_fuzz", 0.8f}, {"overall_grit", 0.7f}, {"dynamic_range", 12.0f}};
    
    pixies.keywords = {"garage", "raw", "dynamic", "fuzzy", "alternative", "influential", "quiet_loud"};
    pixies.technicalNotes = {{"signature_sound", "Fuzzy bass + clean guitar + dynamic vocals"}};
    pixies.typicalLoudness = -16.0f;
    pixies.typicalDynamicRange = 12.0f;
    
    artistDatabase_[normalizeString(pixies.artist)] = pixies;
    
    // ========================================================================
    // MODERN POP
    // ========================================================================
    
    // Billie Eilish - Intimate Modern Pop
    ArtistStyle billie;
    billie.artist = "Billie Eilish";
    billie.genre = "Pop";
    billie.era = "2010s";
    billie.overallCharacter = "Intimate, dark, minimalist pop with whispered vocals and spacious production";
    
    billie.vocals.description = "Intimate, close-mic whispered vocals with breathy texture";
    billie.vocals.effects = {"close_mic_eq", "subtle_reverb", "soft_compression", "de_esser"};
    billie.vocals.parameters = {{"compression_ratio", 3.0f}, {"reverb_room", 0.2f}, {"high_cut", 8000.0f}};
    billie.vocals.micTechnique = "close_intimate";
    billie.vocals.character = "whispered_intimate";
    
    billie.drums.description = "Minimalist, often programmed drums with heavy sub-bass";
    billie.drums.characteristics = {"minimalist", "electronic", "sub_heavy", "spacious"};
    billie.drums.typicalKit = "electronic_minimal";
    billie.drums.processing = {{"sub_boost", 6.0f}, {"compression_ratio", 6.0f}, {"stereo_width", 0.3f}};
    
    billie.mixingStyle.description = "Dark, spacious mix with prominent low-end and intimate vocals";
    billie.mixingStyle.plugins = {"modern_eq", "multiband_comp", "spatial_reverb"};
    billie.mixingStyle.settings = {{"bass_emphasis", 0.9f}, {"vocal_intimacy", 0.9f}, {"stereo_width", 0.8f}};
    
    billie.masteringStyle.description = "Controlled dynamics with modern loudness but preserved intimacy";
    billie.masteringStyle.plugins = {"transparent_limiter", "multiband_dynamics", "stereo_enhancer"};
    
    billie.keywords = {"intimate", "whispered", "dark", "spacious", "modern", "minimalist", "emotional"};
    billie.technicalNotes = {{"signature_sound", "Close whispered vocals + minimal beats + dark atmosphere"}};
    billie.typicalLoudness = -11.0f;
    billie.typicalDynamicRange = 6.0f;
    
    artistDatabase_[normalizeString(billie.artist)] = billie;
    
    // Katy Perry - Polished Pop
    ArtistStyle katy;
    katy.artist = "Katy Perry";
    katy.genre = "Pop";
    katy.era = "2000s";
    katy.overallCharacter = "Bright, polished, radio-ready pop with wide stereo image";
    
    katy.vocals.description = "Bright, powerful vocals with heavy processing and effects";
    katy.vocals.effects = {"bright_eq", "heavy_compression", "stereo_doubling", "vocal_effects"};
    katy.vocals.parameters = {{"compression_ratio", 5.0f}, {"eq_presence_boost", 4.0f}, {"stereo_width", 0.9f}};
    katy.vocals.micTechnique = "processed_layered";
    katy.vocals.character = "bright_powerful";
    
    katy.drums.description = "Punchy, processed drums with electronic elements";
    katy.drums.characteristics = {"punchy", "electronic", "processed", "wide"};
    katy.drums.typicalKit = "hybrid_electronic";
    katy.drums.processing = {{"compression_ratio", 8.0f}, {"eq_punch", 3.0f}, {"stereo_width", 0.8f}};
    
    katy.mixingStyle.description = "Wide, bright, heavily processed modern pop mix";
    katy.mixingStyle.plugins = {"modern_eq", "multiband_comp", "stereo_widener", "harmonic_exciter"};
    katy.mixingStyle.settings = {{"brightness", 0.8f}, {"width", 0.9f}, {"polish", 0.9f}};
    
    katy.masteringStyle.description = "Loud, bright, radio-ready mastering with heavy limiting";
    katy.masteringStyle.plugins = {"aggressive_limiter", "multiband_maximizer", "stereo_enhancer"};
    katy.masteringStyle.settings = {{"loudness", 0.95f}, {"brightness", 0.85f}, {"punch", 0.8f}};
    
    katy.keywords = {"bright", "polished", "radio", "wide", "processed", "commercial", "punchy"};
    katy.technicalNotes = {{"signature_sound", "Bright processed vocals + punchy drums + wide stereo"}};
    katy.typicalLoudness = -8.0f;
    katy.typicalDynamicRange = 4.0f;
    
    artistDatabase_[normalizeString(katy.artist)] = katy;
    
    // ========================================================================
    // GARAGE ROCK
    // ========================================================================
    
    // The White Stripes - Minimal Garage Rock
    ArtistStyle whiteStripes;
    whiteStripes.artist = "The White Stripes";
    whiteStripes.genre = "Garage Rock";
    whiteStripes.era = "2000s";
    whiteStripes.overallCharacter = "Minimal, raw, powerful garage rock duo with vintage aesthetic";
    
    whiteStripes.vocals.description = "Raw, blues-influenced vocals with attitude and energy";
    whiteStripes.vocals.effects = {"vintage_reverb", "light_distortion", "tube_warmth"};
    whiteStripes.vocals.parameters = {{"tube_drive", 0.4f}, {"reverb_spring", 0.5f}};
    whiteStripes.vocals.micTechnique = "vintage_dynamic";
    whiteStripes.vocals.character = "raw_blues";
    
    whiteStripes.drums.description = "Minimal, powerful drum kit with vintage recording aesthetic";
    whiteStripes.drums.characteristics = {"minimal", "powerful", "vintage", "roomy"};
    whiteStripes.drums.typicalKit = "vintage_minimal";
    whiteStripes.drums.processing = {{"room_reverb", 0.6f}, {"compression_ratio", 3.0f}, {"vintage_eq", 0.7f}};
    
    whiteStripes.mixingStyle.description = "Raw, room-heavy mix with vintage tape characteristics";
    whiteStripes.mixingStyle.plugins = {"vintage_console", "tape_saturation", "spring_reverb"};
    whiteStripes.mixingStyle.settings = {{"tape_saturation", 0.7f}, {"room_sound", 0.8f}, {"vintage_character", 0.9f}};
    
    whiteStripes.keywords = {"minimal", "raw", "garage", "vintage", "blues", "powerful", "duo"};
    whiteStripes.technicalNotes = {{"signature_sound", "Guitar + drums duo with vintage recording techniques"}};
    whiteStripes.typicalLoudness = -15.0f;
    whiteStripes.typicalDynamicRange = 10.0f;
    
    artistDatabase_[normalizeString(whiteStripes.artist)] = whiteStripes;
    
    // ========================================================================
    // ELECTRONIC/EXPERIMENTAL
    // ========================================================================
    
    // Daft Punk - Electronic Production Masters
    ArtistStyle daftPunk;
    daftPunk.artist = "Daft Punk";
    daftPunk.genre = "Electronic";
    daftPunk.era = "1990s";
    daftPunk.overallCharacter = "Sophisticated electronic production with French house influence";
    
    daftPunk.vocals.description = "Heavily processed, vocoder-treated vocals with robotic character";
    daftPunk.vocals.effects = {"vocoder", "harmonizer", "chorus", "delay"};
    daftPunk.vocals.parameters = {{"vocoder_intensity", 0.9f}, {"harmonizer_pitch", 0.5f}};
    daftPunk.vocals.micTechnique = "electronic_processed";
    daftPunk.vocals.character = "robotic_processed";
    
    daftPunk.drums.description = "Punchy electronic drums with French house characteristics";
    daftPunk.drums.characteristics = {"electronic", "punchy", "filtered", "groovy"};
    daftPunk.drums.typicalKit = "electronic_house";
    daftPunk.drums.processing = {{"filter_sweep", 0.6f}, {"compression_ratio", 6.0f}, {"sidechain", 0.7f}};
    
    daftPunk.mixingStyle.description = "Clean, punchy electronic mix with filter effects and sidechaining";
    daftPunk.mixingStyle.plugins = {"filters", "sidechain_comp", "stereo_effects", "harmonic_enhancement"};
    daftPunk.mixingStyle.settings = {{"filter_movement", 0.8f}, {"sidechain_pump", 0.7f}, {"clarity", 0.9f}};
    
    daftPunk.keywords = {"electronic", "french_house", "vocoder", "filtered", "sophisticated", "robotic"};
    daftPunk.technicalNotes = {{"signature_sound", "Vocoder vocals + filtered samples + punchy house beats"}};
    daftPunk.typicalLoudness = -10.0f;
    daftPunk.typicalDynamicRange = 8.0f;
    
    artistDatabase_[normalizeString(daftPunk.artist)] = daftPunk;
    
    // ========================================================================
    // Build lookup indices
    // ========================================================================
    
    for (const auto& [key, artist] : artistDatabase_) {
        // Genre index
        genreToArtists_[normalizeString(artist.genre)].push_back(artist.artist);
        
        // Keywords index
        for (const auto& keyword : artist.keywords) {
            keywordToArtists_[normalizeString(keyword)].push_back(artist.artist);
        }
        
        // Era index
        eraToArtists_[normalizeString(artist.era)].push_back(artist.artist);
    }
}

void MusicKnowledgeBase::loadGenreDatabase() {
    std::unique_lock<std::shared_mutex> lock(databaseMutex_);
    
    // Alternative Rock
    GenreCharacteristics altRock;
    altRock.genre = "Alternative Rock";
    altRock.description = "Independent rock music that emerged from underground scenes";
    altRock.keyArtists = {"The Pixies", "Nirvana", "Radiohead", "Pearl Jam"};
    altRock.commonElements = {
        {"dynamics", "Quiet-loud song structures"},
        {"production", "Raw, unpolished sound"},
        {"instruments", "Guitar-driven with unconventional song structures"}
    };
    genreDatabase_[normalizeString(altRock.genre)] = altRock;
    
    // Pop
    GenreCharacteristics pop;
    pop.genre = "Pop";
    pop.description = "Popular music designed for mass appeal and radio play";
    pop.keyArtists = {"Katy Perry", "Taylor Swift", "Billie Eilish", "Ariana Grande"};
    pop.commonElements = {
        {"structure", "Verse-chorus-verse-chorus-bridge-chorus"},
        {"production", "Polished, radio-ready sound"},
        {"vocals", "Prominent, often heavily processed"}
    };
    genreDatabase_[normalizeString(pop.genre)] = pop;
    
    // Garage Rock
    GenreCharacteristics garage;
    garage.genre = "Garage Rock";
    garage.description = "Raw, energetic rock music with lo-fi production aesthetic";
    garage.keyArtists = {"The White Stripes", "The Strokes", "The Black Keys"};
    garage.commonElements = {
        {"production", "Raw, minimal recording techniques"},
        {"energy", "High energy, live feel"},
        {"instruments", "Basic rock instrumentation, often minimal"}
    };
    genreDatabase_[normalizeString(garage.genre)] = garage;
}

void MusicKnowledgeBase::loadProductionTechniques() {
    std::unique_lock<std::shared_mutex> lock(databaseMutex_);
    
    // Intimate Vocal Processing (Billie Eilish style)
    ProductionTechnique intimateVocals;
    intimateVocals.name = "Intimate Vocal Processing";
    intimateVocals.description = "Close-mic recording with subtle processing for intimate feel";
    intimateVocals.category = "vocal";
    intimateVocals.steps = {
        "Record very close to microphone (2-4 inches)",
        "Apply gentle high-frequency cut around 8kHz",
        "Use soft compression with 3:1 ratio",
        "Add subtle room reverb with short decay",
        "Apply de-esser if needed"
    };
    intimateVocals.parameters = {
        {"mic_distance", 3.0f},
        {"high_cut_freq", 8000.0f},
        {"compression_ratio", 3.0f},
        {"reverb_room", 0.2f}
    };
    intimateVocals.associatedArtists = {"Billie Eilish", "Lana Del Rey"};
    techniqueDatabase_[normalizeString(intimateVocals.name)] = intimateVocals;
    
    // Garage Rock Drums
    ProductionTechnique garageDrums;
    garageDrums.name = "Garage Rock Drums";
    garageDrums.description = "Raw, minimal drum recording with room sound";
    garageDrums.category = "drum";
    garageDrums.steps = {
        "Use minimal microphone setup",
        "Emphasize room sound and natural reverb",
        "Apply moderate compression for punch",
        "Boost low-mid frequencies for body",
        "Maintain dynamic range"
    };
    garageDrums.parameters = {
        {"room_reverb", 0.6f},
        {"compression_ratio", 3.0f},
        {"eq_low_mid_boost", 2.0f}
    };
    garageDrums.associatedArtists = {"The White Stripes", "The Pixies"};
    techniqueDatabase_[normalizeString(garageDrums.name)] = garageDrums;
}

void MusicKnowledgeBase::initializeNLP() {
    // Build artist name variants for better recognition
    for (const auto& [key, artist] : artistDatabase_) {
        artistNameVariants_.push_back(artist.artist);
        artistNameVariants_.push_back(key);  // normalized version
        
        // Add common abbreviations or alternate spellings
        if (artist.artist == "The Pixies") {
            artistNameVariants_.push_back("pixies");
        }
        if (artist.artist == "The White Stripes") {
            artistNameVariants_.push_back("white stripes");
        }
    }
    
    // Common style keywords mapping
    styleKeywords_ = {
        {"bright", "brightness"},
        {"dark", "darkness"},
        {"intimate", "intimacy"},
        {"raw", "rawness"},
        {"polished", "polish"},
        {"garage", "garage_rock"},
        {"fuzzy", "fuzz"},
        {"clean", "clarity"},
        {"punchy", "punch"},
        {"spacious", "space"}
    };
    
    // Production terms for NLP
    productionTerms_ = {
        "vocals", "drums", "guitar", "bass", "mix", "master", "reverb", "compression",
        "EQ", "distortion", "delay", "chorus", "flanger", "phaser", "limiter",
        "compressor", "equalizer", "effects", "processing"
    };
}

std::optional<ArtistStyle> MusicKnowledgeBase::getArtistStyle(const std::string& artist) {
    std::shared_lock<std::shared_mutex> lock(databaseMutex_);
    
    std::string normalizedArtist = normalizeString(artist);
    auto it = artistDatabase_.find(normalizedArtist);
    
    if (it != artistDatabase_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::vector<ArtistStyle> MusicKnowledgeBase::getArtistsByGenre(const std::string& genre) {
    std::shared_lock<std::shared_mutex> lock(databaseMutex_);
    std::vector<ArtistStyle> results;
    
    for (const auto& [key, artist] : artistDatabase_) {
        if (normalizeString(artist.genre) == normalizeString(genre)) {
            results.push_back(artist);
        }
    }
    
    return results;
}

std::vector<std::string> MusicKnowledgeBase::parseArtistReferences(const std::string& text) {
    std::vector<std::string> foundArtists;
    std::string lowerText = normalizeString(text);
    
    for (const auto& artistName : artistNameVariants_) {
        if (lowerText.find(normalizeString(artistName)) != std::string::npos) {
            // Find the actual artist name from the database
            for (const auto& [key, artist] : artistDatabase_) {
                if (key == normalizeString(artistName) || normalizeString(artist.artist) == normalizeString(artistName)) {
                    foundArtists.push_back(artist.artist);
                    break;
                }
            }
        }
    }
    
    // Remove duplicates
    std::sort(foundArtists.begin(), foundArtists.end());
    foundArtists.erase(std::unique(foundArtists.begin(), foundArtists.end()), foundArtists.end());
    
    return foundArtists;
}

MusicKnowledgeBase::ProductionRequest MusicKnowledgeBase::interpretRequest(const std::string& request) {
    ProductionRequest result;
    std::string lowerRequest = normalizeString(request);
    
    // Parse artist references
    auto artists = parseArtistReferences(request);
    if (!artists.empty()) {
        result.artist = artists[0];  // Take first found artist
    }
    
    // Extract target (vocals, drums, etc.)
    for (const auto& term : productionTerms_) {
        if (lowerRequest.find(normalizeString(term)) != std::string::npos) {
            result.target = term;
            break;
        }
    }
    
    // Extract modifiers
    std::vector<std::string> modifiers = {"subtle", "heavy", "light", "strong", "gentle", "aggressive"};
    for (const auto& modifier : modifiers) {
        if (lowerRequest.find(modifier) != std::string::npos) {
            result.modifiers.push_back(modifier);
        }
    }
    
    // Set intensity based on modifiers
    if (std::find(result.modifiers.begin(), result.modifiers.end(), "heavy") != result.modifiers.end() ||
        std::find(result.modifiers.begin(), result.modifiers.end(), "aggressive") != result.modifiers.end()) {
        result.intensity = 1.5f;
    } else if (std::find(result.modifiers.begin(), result.modifiers.end(), "subtle") != result.modifiers.end() ||
               std::find(result.modifiers.begin(), result.modifiers.end(), "gentle") != result.modifiers.end()) {
        result.intensity = 0.5f;
    }
    
    return result;
}

std::vector<std::string> MusicKnowledgeBase::getVocalChain(const std::string& artist) {
    auto style = getArtistStyle(artist);
    if (!style) return {};
    
    std::vector<std::string> chain;
    
    // Build processing chain based on artist's vocal style
    for (const auto& effect : style->vocals.effects) {
        chain.push_back(effect);
    }
    
    // Add specific recommendations
    if (style->vocals.character == "intimate") {
        chain.push_back("Close-mic recording (2-4 inches from mic)");
        chain.push_back("Gentle high-frequency rolloff around 8kHz");
        chain.push_back("Soft compression with 3:1 ratio");
    } else if (style->vocals.character == "raw_emotional") {
        chain.push_back("Dynamic microphone for natural compression");
        chain.push_back("Light compression to preserve dynamics");
        chain.push_back("Room reverb for space");
    } else if (style->vocals.character == "bright_powerful") {
        chain.push_back("Condenser microphone for detail");
        chain.push_back("Presence boost around 5-7kHz");
        chain.push_back("Heavy compression for consistency");
        chain.push_back("Stereo doubling for width");
    }
    
    return chain;
}

std::string MusicKnowledgeBase::normalizeString(const std::string& input) const {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    
    // Remove common prefixes
    if (result.substr(0, 4) == "the ") {
        result = result.substr(4);
    }
    
    return result;
}

MusicKnowledgeBase::DatabaseStats MusicKnowledgeBase::getDatabaseStats() const {
    std::shared_lock<std::shared_mutex> lock(databaseMutex_);
    
    DatabaseStats stats;
    stats.artistCount = artistDatabase_.size();
    stats.genreCount = genreDatabase_.size();
    stats.techniqueCount = techniqueDatabase_.size();
    
    // Count artists by genre
    for (const auto& [key, artist] : artistDatabase_) {
        stats.artistsByGenre[artist.genre]++;
    }
    
    // Count techniques by category
    for (const auto& [key, technique] : techniqueDatabase_) {
        stats.techniquesByCategory[technique.category]++;
    }
    
    return stats;
}

bool MusicKnowledgeBase::hasArtist(const std::string& artist) const {
    std::shared_lock<std::shared_mutex> lock(databaseMutex_);
    return artistDatabase_.find(normalizeString(artist)) != artistDatabase_.end();
}

std::vector<std::string> MusicKnowledgeBase::getAllArtists() const {
    std::shared_lock<std::shared_mutex> lock(databaseMutex_);
    std::vector<std::string> artists;
    
    for (const auto& [key, artist] : artistDatabase_) {
        artists.push_back(artist.artist);
    }
    
    std::sort(artists.begin(), artists.end());
    return artists;
}

} // namespace mixmind::ai