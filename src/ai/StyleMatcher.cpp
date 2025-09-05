#include "StyleMatcher.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <cctype>
#include <iostream>

namespace mixmind::ai {

StyleMatcher::StyleMatcher(std::shared_ptr<MusicKnowledgeBase> knowledge)
    : knowledge_base_(knowledge) {
    initializeArtistAliases();
    precomputeStyleDistances();
}

// ============================================================================
// Reference Matching and Analysis
// ============================================================================

std::vector<StyleMatcher::ArtistMatch> StyleMatcher::findArtistReferences(const std::string& message) {
    std::vector<ArtistMatch> matches;
    
    // Convert message to lowercase for matching
    std::string lower_message = message;
    std::transform(lower_message.begin(), lower_message.end(), lower_message.begin(), ::tolower);
    
    // Extract potential artist references
    auto parsed_artists = knowledge_base_->parseArtistReferences(message);
    
    for (const auto& artist : parsed_artists) {
        ArtistMatch match;
        match.artist_name = artist;
        match.original_text = message;
        
        // Get artist style
        auto artist_style = knowledge_base_->getArtistStyle(artist);
        if (artist_style) {
            match.style = *artist_style;
            
            // Calculate confidence based on direct name match
            std::string normalized_artist = normalizeArtistName(artist);
            if (lower_message.find(normalized_artist) != std::string::npos) {
                match.confidence = 0.95f; // High confidence for direct match
            } else {
                match.confidence = 0.7f;  // Medium confidence for parsed match
            }
            
            // Find matched keywords
            for (const auto& keyword : artist_style->keywords) {
                if (lower_message.find(keyword) != std::string::npos) {
                    match.matched_keywords.push_back(keyword);
                    match.confidence += 0.05f; // Boost confidence for keyword matches
                }
            }
            
            matches.push_back(match);
        }
    }
    
    // Also check for fuzzy artist name matches
    auto fuzzy_matches = findSimilarArtistNames(message, 0.7f);
    for (const auto& fuzzy_artist : fuzzy_matches) {
        // Avoid duplicates
        bool already_matched = std::any_of(matches.begin(), matches.end(),
            [&fuzzy_artist](const ArtistMatch& existing) {
                return existing.artist_name == fuzzy_artist;
            });
            
        if (!already_matched) {
            auto artist_style = knowledge_base_->getArtistStyle(fuzzy_artist);
            if (artist_style) {
                ArtistMatch match;
                match.artist_name = fuzzy_artist;
                match.original_text = message;
                match.style = *artist_style;
                match.confidence = 0.6f; // Lower confidence for fuzzy match
                matches.push_back(match);
            }
        }
    }
    
    // Sort by confidence
    std::sort(matches.begin(), matches.end(),
        [](const ArtistMatch& a, const ArtistMatch& b) {
            return a.confidence > b.confidence;
        });
    
    updateStats(message, !matches.empty(), matches.empty() ? 0.0f : matches[0].confidence);
    return matches;
}

std::vector<StyleMatcher::GenreMatch> StyleMatcher::findGenreReferences(const std::string& message) {
    std::vector<GenreMatch> matches;
    
    std::string lower_message = message;
    std::transform(lower_message.begin(), lower_message.end(), lower_message.begin(), ::tolower);
    
    auto parsed_genres = knowledge_base_->parseGenreReferences(message);
    
    for (const auto& genre : parsed_genres) {
        GenreMatch match;
        match.genre = genre;
        
        auto genre_characteristics = knowledge_base_->getGenreCharacteristics(genre);
        if (genre_characteristics) {
            match.characteristics = *genre_characteristics;
            match.confidence = 0.8f;
            
            // Look for genre-specific indicators in the message
            std::vector<std::string> genre_indicators = {genre, 
                genre_characteristics->typical_instruments[0],
                genre_characteristics->key_features
            };
            
            for (const auto& indicator : genre_indicators) {
                std::string lower_indicator = indicator;
                std::transform(lower_indicator.begin(), lower_indicator.end(), lower_indicator.begin(), ::tolower);
                if (lower_message.find(lower_indicator) != std::string::npos) {
                    match.matched_indicators.push_back(indicator);
                    match.confidence += 0.1f;
                }
            }
            
            matches.push_back(match);
        }
    }
    
    return matches;
}

// ============================================================================
// Style Analysis and Recommendation
// ============================================================================

core::AsyncResult<core::Result<std::vector<StyleMatcher::ArtistMatch>>> 
StyleMatcher::findSimilarArtists(std::shared_ptr<core::ITrack> track, int maxResults) {
    return core::async<core::Result<std::vector<ArtistMatch>>>([this, track, maxResults]() -> core::Result<std::vector<ArtistMatch>> {
        std::vector<ArtistMatch> matches;
        
        // Analyze track characteristics
        auto track_characteristics = analyzeTrackCharacteristics(track);
        
        // Compare with all known artists
        auto all_artists = knowledge_base_->getAllArtists();
        for (const auto& artist : all_artists) {
            auto artist_style = knowledge_base_->getArtistStyle(artist);
            if (artist_style) {
                ArtistMatch match;
                match.artist_name = artist;
                match.style = *artist_style;
                match.confidence = compareWithArtistStyle(track_characteristics, *artist_style);
                
                if (match.confidence > 0.3f) { // Threshold for similarity
                    matches.push_back(match);
                }
            }
        }
        
        // Sort by confidence and limit results
        std::sort(matches.begin(), matches.end(),
            [](const ArtistMatch& a, const ArtistMatch& b) {
                return a.confidence > b.confidence;
            });
        
        if (matches.size() > maxResults) {
            matches.resize(maxResults);
        }
        
        return core::Result<std::vector<ArtistMatch>>::success(matches);
    });
}

std::vector<StyleMatcher::ProcessingRecommendation> StyleMatcher::recommendProcessing(
    std::shared_ptr<core::ITrack> track,
    const std::string& target_artist) {
    
    std::vector<ProcessingRecommendation> recommendations;
    
    if (!target_artist.empty()) {
        auto artist_style = knowledge_base_->getArtistStyle(target_artist);
        if (artist_style) {
            // Generate recommendations based on artist style
            ProcessingRecommendation vocal_rec;
            vocal_rec.type = "vocal_processing";
            vocal_rec.description = "Apply " + target_artist + " vocal characteristics";
            vocal_rec.reasoning = "Based on " + artist_style->vocals.character + " vocal style";
            vocal_rec.confidence = 0.85f;
            
            if (artist_style->vocals.character == "intimate" || 
                artist_style->vocals.character == "whispered_intimate") {
                vocal_rec.parameters["compression_ratio"] = 6.0f;
                vocal_rec.parameters["eq_presence_boost"] = 2.0f;
                vocal_rec.parameters["reverb_wet"] = 0.15f;
            } else if (artist_style->vocals.character == "raw" || 
                       artist_style->vocals.character == "dynamic") {
                vocal_rec.parameters["compression_ratio"] = 3.0f;
                vocal_rec.parameters["distortion_drive"] = 1.5f;
                vocal_rec.parameters["eq_mid_cut"] = -1.0f;
            }
            
            recommendations.push_back(vocal_rec);
            
            // Drum processing recommendation
            ProcessingRecommendation drum_rec;
            drum_rec.type = "drum_processing";
            drum_rec.description = "Apply " + target_artist + " drum characteristics";
            drum_rec.reasoning = "Based on " + artist_style->drums.character + " drum style";
            drum_rec.confidence = 0.8f;
            
            if (artist_style->drums.character == "punchy") {
                drum_rec.parameters["transient_enhancement"] = 1.3f;
                drum_rec.parameters["compression_attack"] = 5.0f;
                drum_rec.parameters["eq_punch_boost"] = 2.5f;
            }
            
            recommendations.push_back(drum_rec);
        }
    }
    
    return recommendations;
}

// ============================================================================
// Reference Blending and Morphing
// ============================================================================

StyleMatcher::BlendedStyle StyleMatcher::createBlendedStyle(
    const std::vector<std::pair<std::string, float>>& artist_weights) {
    
    BlendedStyle result;
    result.artist_weights = artist_weights;
    
    // Initialize combined style
    ArtistStyle combined;
    combined.artist = "Blended Style";
    combined.genre = "Mixed";
    
    float total_weight = 0.0f;
    std::map<std::string, float> characteristic_scores;
    
    for (const auto& [artist, weight] : artist_weights) {
        auto artist_style = knowledge_base_->getArtistStyle(artist);
        if (artist_style) {
            total_weight += weight;
            
            // Blend characteristics
            for (const auto& keyword : artist_style->keywords) {
                characteristic_scores[keyword] += weight;
            }
            
            // Weighted blending of processing parameters
            if (weight > 0.5f) { // Dominant artist influences more
                combined.vocals.character = artist_style->vocals.character;
                combined.drums.character = artist_style->drums.character;
            }
        }
    }
    
    // Normalize and set final characteristics
    for (auto& [characteristic, score] : characteristic_scores) {
        if (score / total_weight > 0.3f) { // Threshold for inclusion
            combined.keywords.push_back(characteristic);
        }
    }
    
    result.combined_style = combined;
    
    // Generate description
    std::ostringstream desc;
    desc << "Blended style combining: ";
    for (size_t i = 0; i < artist_weights.size(); ++i) {
        if (i > 0) desc << ", ";
        desc << static_cast<int>(artist_weights[i].second * 100) << "% " << artist_weights[i].first;
    }
    result.description = desc.str();
    
    return result;
}

float StyleMatcher::calculateStyleDistance(const std::string& artist1, const std::string& artist2) {
    // Check cache first
    auto cache_key = std::make_pair(artist1, artist2);
    auto it = style_distance_cache_.find(cache_key);
    if (it != style_distance_cache_.end()) {
        return it->second;
    }
    
    auto style1 = knowledge_base_->getArtistStyle(artist1);
    auto style2 = knowledge_base_->getArtistStyle(artist2);
    
    if (!style1 || !style2) {
        return 1.0f; // Maximum distance for unknown artists
    }
    
    // Calculate distance based on multiple factors
    float distance = 0.0f;
    
    // Genre similarity
    if (style1->genre != style2->genre) {
        distance += 0.3f;
    }
    
    // Era similarity
    if (style1->era != style2->era) {
        distance += 0.2f;
    }
    
    // Keyword overlap
    std::set<std::string> keywords1(style1->keywords.begin(), style1->keywords.end());
    std::set<std::string> keywords2(style2->keywords.begin(), style2->keywords.end());
    
    std::vector<std::string> intersection;
    std::set_intersection(keywords1.begin(), keywords1.end(),
                         keywords2.begin(), keywords2.end(),
                         std::back_inserter(intersection));
    
    float keyword_similarity = static_cast<float>(intersection.size()) / 
                              std::max(keywords1.size(), keywords2.size());
    distance += (1.0f - keyword_similarity) * 0.5f;
    
    // Cache the result
    style_distance_cache_[cache_key] = distance;
    style_distance_cache_[std::make_pair(artist2, artist1)] = distance; // Symmetric
    
    return distance;
}

// ============================================================================
// Natural Language Processing
// ============================================================================

StyleMatcher::ComplexStyleRequest StyleMatcher::parseComplexRequest(const std::string& message) {
    ComplexStyleRequest request;
    
    // Extract percentage-based artist references
    request.artist_references = extractWeights(message);
    
    // Parse intensity modifiers
    request.intensity = parseIntensityModifiers(message);
    
    // Extract descriptive characteristics
    std::vector<std::string> descriptors = {"bright", "warm", "punchy", "intimate", "raw", 
                                           "smooth", "crisp", "dark", "vintage", "modern"};
    for (const auto& descriptor : descriptors) {
        if (message.find(descriptor) != std::string::npos) {
            request.characteristic_requests.push_back(descriptor);
        }
    }
    
    // Extract era references
    auto era_refs = findEraReferences(message);
    if (!era_refs.empty()) {
        request.era_reference = era_refs[0].era;
    }
    
    // Extract genre influences
    auto genre_matches = findGenreReferences(message);
    for (const auto& genre_match : genre_matches) {
        request.genre_influences.push_back(genre_match.genre);
    }
    
    return request;
}

// ============================================================================
// Fuzzy Matching and Utilities
// ============================================================================

std::vector<std::string> StyleMatcher::findSimilarArtistNames(const std::string& query, float threshold) {
    std::vector<std::string> matches;
    auto all_artists = knowledge_base_->getAllArtists();
    
    std::string normalized_query = normalizeArtistName(query);
    
    for (const auto& artist : all_artists) {
        std::string normalized_artist = normalizeArtistName(artist);
        float similarity = calculateTextSimilarity(normalized_query, normalized_artist);
        
        if (similarity >= threshold) {
            matches.push_back(artist);
        }
    }
    
    return matches;
}

std::vector<StyleMatcher::EraReference> StyleMatcher::findEraReferences(const std::string& message) {
    std::vector<EraReference> era_refs;
    
    // Common era patterns
    std::map<std::string, std::vector<std::string>> era_patterns = {
        {"90s", {"1990s", "nineteen nineties", "nineties"}},
        {"80s", {"1980s", "eighties", "nineteen eighties"}},
        {"70s", {"1970s", "seventies", "nineteen seventies"}},
        {"modern", {"contemporary", "current", "today's"}},
        {"vintage", {"classic", "retro", "old school"}}
    };
    
    std::string lower_message = message;
    std::transform(lower_message.begin(), lower_message.end(), lower_message.begin(), ::tolower);
    
    for (const auto& [era, patterns] : era_patterns) {
        for (const auto& pattern : patterns) {
            if (lower_message.find(pattern) != std::string::npos) {
                EraReference ref;
                ref.era = era;
                ref.confidence = 0.8f;
                era_refs.push_back(ref);
                break; // Found this era, move to next
            }
        }
    }
    
    return era_refs;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

float StyleMatcher::calculateTextSimilarity(const std::string& text1, const std::string& text2) {
    // Simple similarity using edit distance
    int edit_distance = style_matching_utils::calculateEditDistance(text1, text2);
    int max_length = std::max(text1.length(), text2.length());
    
    if (max_length == 0) return 1.0f;
    
    return 1.0f - static_cast<float>(edit_distance) / max_length;
}

std::string StyleMatcher::normalizeArtistName(const std::string& name) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // Remove common prefixes and suffixes
    const std::vector<std::string> prefixes = {"the ", "a ", "an "};
    const std::vector<std::string> suffixes = {" band", " group"};
    
    for (const auto& prefix : prefixes) {
        if (normalized.substr(0, prefix.length()) == prefix) {
            normalized = normalized.substr(prefix.length());
            break;
        }
    }
    
    for (const auto& suffix : suffixes) {
        if (normalized.length() >= suffix.length() && 
            normalized.substr(normalized.length() - suffix.length()) == suffix) {
            normalized = normalized.substr(0, normalized.length() - suffix.length());
            break;
        }
    }
    
    return normalized;
}

std::map<std::string, float> StyleMatcher::analyzeTrackCharacteristics(std::shared_ptr<core::ITrack> track) {
    std::map<std::string, float> characteristics;
    
    // Placeholder analysis - in real implementation would analyze audio
    characteristics["energy"] = 0.7f;
    characteristics["tempo"] = 120.0f;
    characteristics["brightness"] = 0.6f;
    characteristics["warmth"] = 0.5f;
    characteristics["dynamic_range"] = 0.8f;
    
    return characteristics;
}

float StyleMatcher::compareWithArtistStyle(
    const std::map<std::string, float>& track_characteristics,
    const ArtistStyle& artist_style) {
    
    // Simplified comparison - in real implementation would be more sophisticated
    float similarity = 0.5f; // Base similarity
    
    // Check if track characteristics match artist keywords
    for (const auto& keyword : artist_style.keywords) {
        if (keyword == "energetic" && track_characteristics.at("energy") > 0.7f) {
            similarity += 0.2f;
        }
        if (keyword == "mellow" && track_characteristics.at("energy") < 0.4f) {
            similarity += 0.2f;
        }
        // Add more keyword-based matching as needed
    }
    
    return std::min(1.0f, similarity);
}

std::vector<std::pair<std::string, float>> StyleMatcher::extractWeights(const std::string& text) {
    std::vector<std::pair<std::string, float>> weights;
    
    // Look for percentage patterns like "60% Billie Eilish"
    std::regex percentage_pattern(R"((\d+)%\s*([A-Za-z\s]+))");
    std::sregex_iterator iter(text.begin(), text.end(), percentage_pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::smatch match = *iter;
        float percentage = std::stof(match[1].str()) / 100.0f;
        std::string artist = match[2].str();
        
        // Trim whitespace
        artist.erase(artist.find_last_not_of(" \n\r\t") + 1);
        
        weights.push_back({artist, percentage});
    }
    
    return weights;
}

float StyleMatcher::parseIntensityModifiers(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    if (lower_text.find("very") != std::string::npos || lower_text.find("extremely") != std::string::npos) {
        return 1.5f;
    }
    if (lower_text.find("slightly") != std::string::npos || lower_text.find("subtly") != std::string::npos) {
        return 0.5f;
    }
    if (lower_text.find("more") != std::string::npos) {
        return 1.2f;
    }
    if (lower_text.find("less") != std::string::npos) {
        return 0.8f;
    }
    
    return 1.0f; // Default intensity
}

void StyleMatcher::initializeArtistAliases() {
    // Common aliases and alternate names
    artist_aliases_["billie"] = "Billie Eilish";
    artist_aliases_["pixies"] = "The Pixies";
    artist_aliases_["beatles"] = "The Beatles";
    artist_aliases_["radiohead"] = "Radiohead";
    artist_aliases_["nirvana"] = "Nirvana";
    artist_aliases_["taylor swift"] = "Taylor Swift";
    artist_aliases_["ed sheeran"] = "Ed Sheeran";
    
    // Handle common misspellings
    artist_aliases_["billy eilish"] = "Billie Eilish";
    artist_aliases_["the pixis"] = "The Pixies";
}

void StyleMatcher::updateStats(const std::string& query, bool successful, float confidence) {
    stats_.total_queries++;
    if (successful) {
        stats_.successful_matches++;
        stats_.average_confidence = (stats_.average_confidence * (stats_.successful_matches - 1) + confidence) / stats_.successful_matches;
    } else {
        stats_.unmatched_queries.push_back(query);
    }
    
    query_history_.push_back(query);
}

void StyleMatcher::precomputeStyleDistances() {
    // Precompute distances for common artist pairs to improve performance
    auto common_artists = knowledge_base_->getAllArtists();
    for (size_t i = 0; i < common_artists.size() && i < 20; ++i) {
        for (size_t j = i + 1; j < common_artists.size() && j < 20; ++j) {
            calculateStyleDistance(common_artists[i], common_artists[j]);
        }
    }
}

StyleMatcher::MatchingStats StyleMatcher::getMatchingStatistics() {
    return stats_;
}

} // namespace mixmind::ai

// ============================================================================
// Style Matching Utilities Implementation
// ============================================================================

namespace mixmind::ai::style_matching_utils {

int calculateEditDistance(const std::string& s1, const std::string& s2) {
    const size_t len1 = s1.length();
    const size_t len2 = s2.length();
    
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));
    
    for (size_t i = 0; i <= len1; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= len2; ++j) dp[0][j] = j;
    
    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i-1][j] + 1,      // deletion
                dp[i][j-1] + 1,      // insertion
                dp[i-1][j-1] + cost  // substitution
            });
        }
    }
    
    return dp[len1][len2];
}

std::vector<std::string> extractQuotedStrings(const std::string& text) {
    std::vector<std::string> quoted;
    std::regex quote_pattern(R"(["']([^"']+)["'])");
    std::sregex_iterator iter(text.begin(), text.end(), quote_pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        quoted.push_back((*iter)[1].str());
    }
    
    return quoted;
}

float parsePercentage(const std::string& text) {
    std::regex percentage_pattern(R"((\d+)%)");
    std::smatch match;
    
    if (std::regex_search(text, match, percentage_pattern)) {
        return std::stof(match[1].str()) / 100.0f;
    }
    
    // Handle word-based percentages
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    if (lower_text.find("half") != std::string::npos) return 0.5f;
    if (lower_text.find("quarter") != std::string::npos) return 0.25f;
    if (lower_text.find("mostly") != std::string::npos) return 0.8f;
    if (lower_text.find("little") != std::string::npos) return 0.2f;
    
    return 1.0f; // Default full intensity
}

} // namespace mixmind::ai::style_matching_utils