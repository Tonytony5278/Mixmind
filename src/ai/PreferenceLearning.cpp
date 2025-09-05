#include "PreferenceLearning.h"
#include "../core/logging.h"
#include "../services/RealOpenAIService.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <random>
#include <thread>

using json = nlohmann::json;

namespace mixmind::ai {

// ============================================================================
// PreferenceLearning Implementation
// ============================================================================

class PreferenceLearning::Impl {
public:
    std::unordered_map<std::string, UserPreference> preferences;
    std::map<std::string, MixProfile> genreProfiles;
    std::vector<WorkflowPattern> workflowPatterns;
    std::vector<Pattern> detectedPatterns;
    UserProfile userProfile;
    LearningAnalytics analytics;
    
    // Configuration
    bool contextualLearningEnabled = true;
    bool realTimeAdaptationEnabled = false;
    float learningRate = 1.0f;
    float confidenceThreshold = 0.6f;
    
    // Callbacks
    SuggestionCallback suggestionCallback;
    PreferenceUpdateCallback preferenceUpdateCallback;
    PatternDetectionCallback patternDetectionCallback;
    
    // Services
    std::shared_ptr<services::RealOpenAIService> aiService;
    
    // Thread safety
    mutable std::mutex preferenceMutex;
    mutable std::mutex analyticsMutex;
    
    Impl() {
        aiService = std::make_shared<services::RealOpenAIService>();
        initializeDefaultProfiles();
    }
    
    void initializeDefaultProfiles() {
        // Create default genre profiles based on industry standards
        
        // Hip-Hop Profile
        MixProfile hiphop;
        hiphop.genre = "Hip-Hop";
        hiphop.avgLoudness = -14.0f; // Loud, competitive
        hiphop.dynamicRange = 6.0f;  // Heavily compressed
        hiphop.bassEnergyRatio = 0.35f; // Heavy bass
        hiphop.preferredPlugins = {"SSL Compressor", "Pultec EQ", "Waves RBass", "Auto-Tune"};
        hiphop.compressionStyle.ratio = 6.0f;
        hiphop.compressionStyle.attack = 3.0f;
        hiphop.compressionStyle.release = 50.0f;
        genreProfiles["Hip-Hop"] = hiphop;
        
        // Rock Profile
        MixProfile rock;
        rock.genre = "Rock";
        rock.avgLoudness = -12.0f;
        rock.dynamicRange = 8.0f;
        rock.bassEnergyRatio = 0.25f;
        rock.midEnergyRatio = 0.55f;
        rock.preferredPlugins = {"1176 Compressor", "Neve 1073 EQ", "Plate Reverb", "Tube Screamer"};
        rock.compressionStyle.ratio = 4.0f;
        rock.compressionStyle.attack = 10.0f;
        rock.compressionStyle.release = 100.0f;
        genreProfiles["Rock"] = rock;
        
        // Electronic Profile
        MixProfile electronic;
        electronic.genre = "Electronic";
        electronic.avgLoudness = -10.0f; // Very loud
        electronic.dynamicRange = 5.0f;
        electronic.bassEnergyRatio = 0.30f;
        electronic.highEnergyRatio = 0.35f;
        electronic.preferredPlugins = {"OTT", "Serum", "FabFilter Pro-L2", "Valhalla Shimmer"};
        electronic.compressionStyle.ratio = 8.0f;
        electronic.compressionStyle.attack = 1.0f;
        electronic.compressionStyle.release = 30.0f;
        genreProfiles["Electronic"] = electronic;
        
        // Jazz Profile
        MixProfile jazz;
        jazz.genre = "Jazz";
        jazz.avgLoudness = -20.0f; // More dynamic
        jazz.dynamicRange = 14.0f;
        jazz.stereoWidth = 0.9f;
        jazz.preferredPlugins = {"Vintage Tube EQ", "Vintage Compressor", "Hall Reverb", "Tape Saturation"};
        jazz.compressionStyle.ratio = 2.5f;
        jazz.compressionStyle.attack = 30.0f;
        jazz.compressionStyle.release = 200.0f;
        genreProfiles["Jazz"] = jazz;
    }
    
    Value calculateWeightedPreference(const std::vector<Value>& values) {
        if (values.empty()) return Value(0.0f);
        
        // More recent values have higher weight
        float totalWeight = 0.0f;
        float weightedSum = 0.0f;
        
        for (size_t i = 0; i < values.size(); ++i) {
            float weight = std::pow(0.9f, values.size() - 1 - i); // Exponential decay
            totalWeight += weight;
            
            if (values[i].type == Value::FLOAT) {
                weightedSum += static_cast<float>(values[i]) * weight;
            }
        }
        
        return Value(weightedSum / totalWeight);
    }
    
    std::vector<UserPreference*> findRelevantPreferences(const std::string& task, const Context& context) {
        std::vector<UserPreference*> relevant;
        
        std::lock_guard<std::mutex> lock(preferenceMutex);
        for (auto& [key, pref] : preferences) {
            if (pref.confidence > confidenceThreshold) {
                // Check if preference is relevant to current context
                if (pref.contextualPreferences.find(context.getCurrentGenre()) != pref.contextualPreferences.end()) {
                    relevant.push_back(&pref);
                }
            }
        }
        
        return relevant;
    }
    
    std::string inferCurrentTask(const Context& context) {
        // Analyze context to infer what user is likely doing
        if (context.mixingPhase == "tracking") {
            return "recording";
        } else if (context.mixingPhase == "mixing") {
            if (context.activePlugins.empty()) {
                return "setting_up_mix";
            } else {
                // Check plugin types to infer specific mixing task
                bool hasEQ = std::any_of(context.activePlugins.begin(), context.activePlugins.end(),
                    [](const std::string& plugin) { 
                        return plugin.find("EQ") != std::string::npos || plugin.find("Pro-Q") != std::string::npos;
                    });
                
                bool hasCompressor = std::any_of(context.activePlugins.begin(), context.activePlugins.end(),
                    [](const std::string& plugin) { 
                        return plugin.find("Comp") != std::string::npos || plugin.find("1176") != std::string::npos;
                    });
                
                if (hasEQ && hasCompressor) {
                    return "detailed_mixing";
                } else if (hasEQ) {
                    return "eq_balancing";
                } else if (hasCompressor) {
                    return "dynamics_processing";
                }
            }
            return "mixing";
        } else if (context.mixingPhase == "mastering") {
            return "mastering";
        }
        
        return "general_production";
    }
};

PreferenceLearning::PreferenceLearning() : pImpl_(std::make_unique<Impl>()) {
    MIXMIND_LOG_INFO("Preference learning system initialized - ready to adapt to user workflow");
}

PreferenceLearning::~PreferenceLearning() = default;

void PreferenceLearning::observeUserAction(const std::string& action, const Value& value, const Context& context) {
    std::lock_guard<std::mutex> lock(pImpl_->preferenceMutex);
    
    auto& pref = pImpl_->preferences[action];
    pref.category = "user_action";
    pref.parameter = action;
    pref.historicalValues.push_back(value);
    
    // Keep only recent history (last 100 values)
    if (pref.historicalValues.size() > 100) {
        pref.historicalValues.erase(pref.historicalValues.begin());
    }
    
    // Update preferred value
    pref.preferredValue = pImpl_->calculateWeightedPreference(pref.historicalValues);
    
    // Context-aware learning
    if (pImpl_->contextualLearningEnabled) {
        std::string genre = context.getCurrentGenre();
        if (!genre.empty()) {
            pref.contextualPreferences[genre] = value;
        }
    }
    
    // Update confidence (increases with more observations, up to 1.0)
    pref.confidence = std::min(1.0f, pref.confidence + (0.05f * pImpl_->learningRate));
    pref.usageCount++;
    pref.lastUpdated = std::chrono::system_clock::now();
    
    // Update analytics
    std::lock_guard<std::mutex> analyticsLock(pImpl_->analyticsMutex);
    pImpl_->analytics.totalActionsObserved++;
    pImpl_->analytics.totalPreferencesLearned = pImpl_->preferences.size();
    
    MIXMIND_LOG_DEBUG("Learned preference for action '{}' with confidence {:.2f}", action, pref.confidence);
    
    // Notify callback
    if (pImpl_->preferenceUpdateCallback) {
        pImpl_->preferenceUpdateCallback(action, pref.preferredValue);
    }
}

void PreferenceLearning::observeParameterChange(const std::string& pluginId, const std::string& parameter, 
                                               float value, const Context& context) {
    std::string key = pluginId + "::" + parameter;
    observeUserAction(key, Value(value), context);
    
    // Also learn plugin-specific patterns
    std::string pluginKey = "plugin_usage::" + pluginId;
    observeUserAction(pluginKey, Value(1.0f), context);
    
    // Update plugin usage analytics
    std::lock_guard<std::mutex> lock(pImpl_->analyticsMutex);
    pImpl_->analytics.mostUsedPlugins[pluginId]++;
}

void PreferenceLearning::observePluginSelection(const std::string& pluginName, const Context& context) {
    std::string task = pImpl_->inferCurrentTask(context);
    std::string key = "plugin_for_task::" + task + "::" + pluginName;
    
    observeUserAction(key, Value(1.0f), context);
    
    MIXMIND_LOG_DEBUG("Observed plugin selection: {} for task: {}", pluginName, task);
}

void PreferenceLearning::offerPredictiveAction(const Context& context) {
    auto suggestions = generateAdaptiveSuggestions(context);
    
    if (!suggestions.empty() && pImpl_->suggestionCallback) {
        pImpl_->suggestionCallback(suggestions);
    }
}

std::vector<PreferenceLearning::Suggestion> PreferenceLearning::generateAdaptiveSuggestions(const Context& context) {
    std::vector<Suggestion> suggestions;
    
    std::string currentTask = pImpl_->inferCurrentTask(context);
    auto relevantPrefs = pImpl_->findRelevantPreferences(currentTask, context);
    
    for (auto* pref : relevantPrefs) {
        if (pref->confidence > pImpl_->confidenceThreshold) {
            Suggestion suggestion(Suggestion::PARAMETER_ADJUSTMENT, 
                                "Apply Learned Preference",
                                "Based on your usual workflow, would you like me to " + pref->parameter + "?",
                                pref->confidence);
            
            suggestion.parameters["action"] = Value(pref->parameter);
            suggestion.parameters["value"] = pref->preferredValue;
            suggestion.potentialImpact = pref->confidence * 10.0f;
            
            suggestions.push_back(suggestion);
        }
    }
    
    // Genre-specific suggestions
    std::string genre = context.getCurrentGenre();
    if (!genre.empty() && pImpl_->genreProfiles.find(genre) != pImpl_->genreProfiles.end()) {
        const auto& profile = pImpl_->genreProfiles[genre];
        
        // Suggest genre-appropriate plugins
        for (const auto& plugin : profile.preferredPlugins) {
            bool alreadyLoaded = std::find(context.activePlugins.begin(), 
                                         context.activePlugins.end(), plugin) != context.activePlugins.end();
            
            if (!alreadyLoaded) {
                Suggestion suggestion(Suggestion::PLUGIN_RECOMMENDATION,
                                    "Genre-Appropriate Plugin",
                                    "For " + genre + ", you often use " + plugin + ". Load it?",
                                    0.8f);
                
                suggestion.parameters["plugin"] = Value(plugin);
                suggestion.potentialImpact = 7.0f;
                suggestions.push_back(suggestion);
            }
        }
        
        // Suggest mixing adjustments based on genre profile
        if (currentTask == "mixing" || currentTask == "detailed_mixing") {
            if (profile.avgLoudness > -16.0f) { // Loud genre
                Suggestion suggestion(Suggestion::MIX_GUIDANCE,
                                    "Genre Loudness Target",
                                    genre + " typically targets " + std::to_string(profile.avgLoudness) + " LUFS",
                                    0.9f);
                suggestions.push_back(suggestion);
            }
        }
    }
    
    // Update analytics
    std::lock_guard<std::mutex> lock(pImpl_->analyticsMutex);
    pImpl_->analytics.totalSuggestionsGenerated += suggestions.size();
    if (!suggestions.empty()) {
        float totalConfidence = 0.0f;
        for (const auto& s : suggestions) {
            totalConfidence += s.confidence;
        }
        pImpl_->analytics.averageSuggestionConfidence = totalConfidence / suggestions.size();
    }
    
    return suggestions;
}

std::map<std::string, MixProfile> PreferenceLearning::learnMixingStyles() {
    // Analyze user's historical mixing decisions to learn their style per genre
    std::map<std::string, MixProfile> learnedProfiles;
    
    std::lock_guard<std::mutex> lock(pImpl_->preferenceMutex);
    
    // Group preferences by genre
    std::map<std::string, std::vector<UserPreference*>> genrePreferences;
    
    for (auto& [key, pref] : pImpl_->preferences) {
        for (const auto& [genre, value] : pref.contextualPreferences) {
            genrePreferences[genre].push_back(&pref);
        }
    }
    
    // Create learned profile for each genre
    for (const auto& [genre, prefs] : genrePreferences) {
        MixProfile profile;
        profile.genre = genre;
        
        // Analyze compression preferences
        for (auto* pref : prefs) {
            if (pref->parameter.find("compressor_ratio") != std::string::npos) {
                profile.compressionStyle.ratio = static_cast<float>(pref->preferredValue);
            } else if (pref->parameter.find("compressor_attack") != std::string::npos) {
                profile.compressionStyle.attack = static_cast<float>(pref->preferredValue);
            } else if (pref->parameter.find("compressor_release") != std::string::npos) {
                profile.compressionStyle.release = static_cast<float>(pref->preferredValue);
            }
        }
        
        // Learn preferred plugins for this genre
        for (auto* pref : prefs) {
            if (pref->parameter.find("plugin_for_task") != std::string::npos) {
                // Extract plugin name from parameter
                size_t lastColon = pref->parameter.find_last_of("::");
                if (lastColon != std::string::npos && lastColon < pref->parameter.length() - 2) {
                    std::string pluginName = pref->parameter.substr(lastColon + 2);
                    if (pref->confidence > 0.7f) {
                        profile.preferredPlugins.push_back(pluginName);
                    }
                }
            }
        }
        
        learnedProfiles[genre] = profile;
    }
    
    // Update internal profiles with learned data
    for (const auto& [genre, profile] : learnedProfiles) {
        if (pImpl_->genreProfiles.find(genre) != pImpl_->genreProfiles.end()) {
            // Merge with existing profile
            auto& existing = pImpl_->genreProfiles[genre];
            
            // Blend learned values with defaults
            float blendFactor = 0.7f; // Favor learned preferences
            if (profile.compressionStyle.ratio > 0) {
                existing.compressionStyle.ratio = existing.compressionStyle.ratio * (1 - blendFactor) + 
                                                profile.compressionStyle.ratio * blendFactor;
            }
            
            // Add learned plugins to preferred list
            for (const auto& plugin : profile.preferredPlugins) {
                if (std::find(existing.preferredPlugins.begin(), existing.preferredPlugins.end(), plugin) 
                    == existing.preferredPlugins.end()) {
                    existing.preferredPlugins.push_back(plugin);
                }
            }
        } else {
            pImpl_->genreProfiles[genre] = profile;
        }
    }
    
    MIXMIND_LOG_INFO("Learned mixing styles for {} genres", learnedProfiles.size());
    return learnedProfiles;
}

MixProfile PreferenceLearning::getMixProfileForGenre(const std::string& genre) const {
    std::lock_guard<std::mutex> lock(pImpl_->preferenceMutex);
    
    auto it = pImpl_->genreProfiles.find(genre);
    if (it != pImpl_->genreProfiles.end()) {
        return it->second;
    }
    
    // Return default profile if genre not found
    MixProfile defaultProfile;
    defaultProfile.genre = genre;
    return defaultProfile;
}

std::vector<std::string> PreferenceLearning::getPreferredPluginsForTask(const std::string& task, const std::string& genre) {
    std::vector<std::string> preferredPlugins;
    
    std::lock_guard<std::mutex> lock(pImpl_->preferenceMutex);
    
    // Look for task-specific plugin preferences
    std::string searchKey = "plugin_for_task::" + task + "::";
    
    std::vector<std::pair<std::string, float>> pluginConfidences;
    
    for (const auto& [key, pref] : pImpl_->preferences) {
        if (key.find(searchKey) == 0 && pref.confidence > pImpl_->confidenceThreshold) {
            // Extract plugin name
            std::string pluginName = key.substr(searchKey.length());
            
            float confidence = pref.confidence;
            
            // Boost confidence if this matches current genre preferences
            if (!genre.empty() && pref.contextualPreferences.find(genre) != pref.contextualPreferences.end()) {
                confidence *= 1.5f; // 50% boost for genre match
            }
            
            pluginConfidences.emplace_back(pluginName, confidence);
        }
    }
    
    // Sort by confidence
    std::sort(pluginConfidences.begin(), pluginConfidences.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Return top plugins
    for (const auto& [plugin, confidence] : pluginConfidences) {
        preferredPlugins.push_back(plugin);
        if (preferredPlugins.size() >= 5) break; // Limit to top 5
    }
    
    // If no learned preferences, fall back to genre defaults
    if (preferredPlugins.empty() && !genre.empty()) {
        auto it = pImpl_->genreProfiles.find(genre);
        if (it != pImpl_->genreProfiles.end()) {
            return it->second.preferredPlugins;
        }
    }
    
    return preferredPlugins;
}

PreferenceLearning::UserProfile PreferenceLearning::getUserProfile() const {
    std::lock_guard<std::mutex> lock(pImpl_->preferenceMutex);
    return pImpl_->userProfile;
}

void PreferenceLearning::exportUserProfile(const std::string& filePath) const {
    try {
        json profileJson;
        
        std::lock_guard<std::mutex> lock(pImpl_->preferenceMutex);
        
        // Export preferences
        json preferencesJson;
        for (const auto& [key, pref] : pImpl_->preferences) {
            json prefJson;
            prefJson["category"] = pref.category;
            prefJson["parameter"] = pref.parameter;
            prefJson["confidence"] = pref.confidence;
            prefJson["usageCount"] = pref.usageCount;
            
            // Export historical values (sample)
            json historyJson = json::array();
            for (size_t i = 0; i < std::min(pref.historicalValues.size(), size_t(20)); ++i) {
                const auto& value = pref.historicalValues[i];
                if (value.type == Value::FLOAT) {
                    historyJson.push_back(static_cast<float>(value));
                }
            }
            prefJson["recentValues"] = historyJson;
            
            // Export contextual preferences
            json contextJson;
            for (const auto& [context, value] : pref.contextualPreferences) {
                if (value.type == Value::FLOAT) {
                    contextJson[context] = static_cast<float>(value);
                }
            }
            prefJson["contextual"] = contextJson;
            
            preferencesJson[key] = prefJson;
        }
        
        profileJson["preferences"] = preferencesJson;
        
        // Export genre profiles
        json genreProfilesJson;
        for (const auto& [genre, profile] : pImpl_->genreProfiles) {
            json genreJson;
            genreJson["avgLoudness"] = profile.avgLoudness;
            genreJson["dynamicRange"] = profile.dynamicRange;
            genreJson["stereoWidth"] = profile.stereoWidth;
            genreJson["compressionRatio"] = profile.compressionStyle.ratio;
            genreJson["compressionAttack"] = profile.compressionStyle.attack;
            genreJson["compressionRelease"] = profile.compressionStyle.release;
            genreJson["preferredPlugins"] = profile.preferredPlugins;
            
            genreProfilesJson[genre] = genreJson;
        }
        
        profileJson["genreProfiles"] = genreProfilesJson;
        profileJson["version"] = "1.0";
        profileJson["exportTime"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Write to file
        std::ofstream file(filePath);
        file << profileJson.dump(2);
        
        MIXMIND_LOG_INFO("User profile exported to: {}", filePath);
        
    } catch (const std::exception& e) {
        MIXMIND_LOG_ERROR("Failed to export user profile: {}", e.what());
    }
}

PreferenceLearning::LearningAnalytics PreferenceLearning::getAnalytics() const {
    std::lock_guard<std::mutex> lock(pImpl_->analyticsMutex);
    return pImpl_->analytics;
}

void PreferenceLearning::setLearningRate(float rate) {
    pImpl_->learningRate = std::clamp(rate, 0.1f, 2.0f);
    MIXMIND_LOG_INFO("Learning rate set to: {:.2f}", pImpl_->learningRate);
}

void PreferenceLearning::setSuggestionCallback(SuggestionCallback callback) {
    pImpl_->suggestionCallback = callback;
}

void PreferenceLearning::setPreferenceUpdateCallback(PreferenceUpdateCallback callback) {
    pImpl_->preferenceUpdateCallback = callback;
}

} // namespace mixmind::ai