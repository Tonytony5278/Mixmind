#include "IntelligentProcessor.h"
#include "../core/async.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <regex>

namespace mixmind::ai {

IntelligentProcessor::IntelligentProcessor(std::shared_ptr<MusicKnowledgeBase> knowledge)
    : knowledgeBase_(knowledge) {
    initializeCharacteristicMappings();
    std::cout << "🎛️ Intelligent Audio Processor initialized with AI-powered processing" << std::endl;
}

// ========================================================================
// Artist-Style Processing
// ========================================================================

core::AsyncResult<core::VoidResult> IntelligentProcessor::applyArtistStyle(
    std::shared_ptr<core::ITrack> track,
    const std::string& artist,
    float intensity
) {
    return core::async<core::VoidResult>([this, track, artist, intensity]() -> core::VoidResult {
        try {
            if (!knowledgeBase_ || !knowledgeBase_->isReady()) {
                return core::VoidResult::failure("Music Knowledge Base not ready");
            }
            
            auto artistStyle = knowledgeBase_->getArtistStyle(artist);
            if (!artistStyle) {
                return core::VoidResult::failure("Artist '" + artist + "' not found in database");
            }
            
            std::cout << "🎵 Applying " << artist << " style processing..." << std::endl;
            std::cout << "   Character: " << artistStyle->overallCharacter << std::endl;
            std::cout << "   Genre: " << artistStyle->genre << " (" << artistStyle->era << ")" << std::endl;
            
            std::vector<std::string> appliedEffects;
            
            // Apply artist-specific EQ curve
            applyArtistEQ(track, *artistStyle, intensity);
            appliedEffects.push_back("EQ");
            
            // Apply artist-specific compression
            applyArtistCompression(track, *artistStyle, intensity);
            appliedEffects.push_back("Compression");
            
            // Apply spatial effects (reverb, delay)
            applyArtistSpatialEffects(track, *artistStyle, intensity);
            appliedEffects.push_back("Spatial Effects");
            
            // Apply saturation/distortion if characteristic of the artist
            if (artistStyle->overallCharacter.find("raw") != std::string::npos ||
                artistStyle->overallCharacter.find("gritty") != std::string::npos) {
                applyArtistSaturation(track, *artistStyle, intensity);
                appliedEffects.push_back("Saturation");
            }
            
            // Apply stereo processing
            applyArtistStereoProcessing(track, *artistStyle, intensity);
            appliedEffects.push_back("Stereo Processing");
            
            // Log processing action
            logProcessingAction(track, "apply_artist_style", artist, 
                {{"intensity", intensity}}, generateProcessingExplanation(*artistStyle, appliedEffects));
            
            std::cout << "✅ Successfully applied " << artist << " style processing" << std::endl;
            std::cout << "   Effects applied: ";
            for (size_t i = 0; i < appliedEffects.size(); ++i) {
                std::cout << appliedEffects[i];
                if (i < appliedEffects.size() - 1) std::cout << " → ";
            }
            std::cout << std::endl;
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to apply artist style: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> IntelligentProcessor::applyVocalChain(
    std::shared_ptr<core::ITrack> track,
    const std::string& artist,
    float intensity
) {
    return core::async<core::VoidResult>([this, track, artist, intensity]() -> core::VoidResult {
        try {
            auto artistStyle = knowledgeBase_->getArtistStyle(artist);
            if (!artistStyle) {
                return core::VoidResult::failure("Artist '" + artist + "' not found");
            }
            
            std::cout << "🎤 Applying " << artist << " vocal processing..." << std::endl;
            std::cout << "   Vocal style: " << artistStyle->vocals.character << std::endl;
            std::cout << "   Mic technique: " << artistStyle->vocals.micTechnique << std::endl;
            
            // Apply vocal-specific processing based on artist characteristics
            if (artistStyle->vocals.character == "intimate" || artistStyle->vocals.character == "whispered_intimate") {
                // Billie Eilish style - close-mic, heavily compressed
                applyCompression(track, 6.0f * intensity, -15.0f, 5.0f, 50.0f);
                applyEQ(track, {
                    {100.0f, -2.0f * intensity, 1.0f},   // Reduce rumble
                    {800.0f, 1.5f * intensity, 0.8f},    // Boost lower mids for warmth  
                    {3000.0f, 2.0f * intensity, 1.2f},   // Presence boost
                    {8000.0f, -1.0f * intensity, 0.8f}   // Slight high cut for intimacy
                });
                applyReverb(track, 0.2f, 0.1f * intensity, 0.8f); // Subtle, short reverb
                std::cout << "   Applied intimate vocal processing" << std::endl;
                
            } else if (artistStyle->vocals.character == "powerful") {
                // Katy Perry style - bright and punchy
                applyCompression(track, 5.0f * intensity, -12.0f, 3.0f, 80.0f);
                applyEQ(track, {
                    {200.0f, -1.0f * intensity, 0.8f},   // Clean up low mids
                    {2000.0f, 1.5f * intensity, 1.0f},   // Vocal clarity
                    {5000.0f, 2.5f * intensity, 1.2f},   // Brightness
                    {10000.0f, 1.0f * intensity, 0.8f}   // Air
                });
                applyReverb(track, 0.6f, 0.25f * intensity, 1.2f); // More spacious reverb
                std::cout << "   Applied powerful vocal processing" << std::endl;
                
            } else if (artistStyle->vocals.character == "raw_emotional") {
                // The Pixies style - minimal processing, natural dynamics
                applyCompression(track, 2.5f * intensity, -10.0f, 20.0f, 200.0f); // Light compression
                applyEQ(track, {
                    {400.0f, 2.0f * intensity, 1.5f},    // Mid boost for aggression
                    {1200.0f, 1.5f * intensity, 1.0f},   // Upper mid clarity
                    {6000.0f, -0.5f * intensity, 0.8f}   // Slight high cut
                });
                applyReverb(track, 0.3f, 0.05f * intensity, 0.6f); // Very dry
                if (intensity > 0.7f) {
                    applyDistortion(track, 0.3f * intensity, "tape"); // Add grit for screams
                }
                std::cout << "   Applied raw emotional vocal processing" << std::endl;
            }
            
            logProcessingAction(track, "apply_vocal_chain", artist, 
                {{"intensity", intensity}}, "Applied " + artist + " vocal processing chain");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to apply vocal chain: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> IntelligentProcessor::processNaturalLanguageRequest(
    std::shared_ptr<core::ITrack> track,
    const std::string& request
) {
    return core::async<core::VoidResult>([this, track, request]() -> core::VoidResult {
        try {
            std::cout << "🧠 Processing request: \"" << request << "\"" << std::endl;
            
            // Parse the request using the music knowledge base
            auto productionRequest = knowledgeBase_->interpretRequest(request);
            
            std::cout << "   Detected artist: " << productionRequest.artist << std::endl;
            std::cout << "   Target element: " << productionRequest.target << std::endl;
            std::cout << "   Style: " << productionRequest.style << std::endl;
            std::cout << "   Intensity: " << productionRequest.intensity << std::endl;
            
            // Apply processing based on interpreted request
            if (!productionRequest.artist.empty()) {
                if (productionRequest.target == "vocals") {
                    return applyVocalChain(track, productionRequest.artist, productionRequest.intensity).get();
                } else {
                    return applyArtistStyle(track, productionRequest.artist, productionRequest.intensity).get();
                }
            } else if (!productionRequest.style.empty()) {
                // Apply descriptive processing
                return applyCharacteristic(track, productionRequest.style, productionRequest.intensity).get();
            } else {
                // Try to extract artist references from the text
                auto artistRefs = knowledgeBase_->parseArtistReferences(request);
                if (!artistRefs.empty()) {
                    return applyArtistStyle(track, artistRefs[0], productionRequest.intensity).get();
                }
            }
            
            return core::VoidResult::failure("Could not understand processing request");
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to process natural language request: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> IntelligentProcessor::applyCharacteristic(
    std::shared_ptr<core::ITrack> track,
    const std::string& characteristic,
    float intensity
) {
    return core::async<core::VoidResult>([this, track, characteristic, intensity]() -> core::VoidResult {
        try {
            std::cout << "🎛️ Making track sound more " << characteristic << "..." << std::endl;
            
            auto params = descriptorToParameters(characteristic);
            
            if (characteristic == "bright" || characteristic == "brighter") {
                // Boost high frequencies
                applyEQ(track, {
                    {3000.0f, 1.5f * intensity, 1.0f},   // Presence
                    {8000.0f, 2.0f * intensity, 0.8f},   // Brightness
                    {12000.0f, 1.0f * intensity, 0.6f}   // Air
                });
                std::cout << "   Applied brightness enhancement" << std::endl;
                
            } else if (characteristic == "warm" || characteristic == "warmer") {
                // Boost low-mids, slight high cut
                applyEQ(track, {
                    {200.0f, 1.0f * intensity, 0.8f},    // Warmth
                    {500.0f, 1.5f * intensity, 1.0f},    // Body
                    {8000.0f, -1.0f * intensity, 0.8f}   // Gentle high cut
                });
                applyDistortion(track, 0.1f * intensity, "tube"); // Subtle saturation
                std::cout << "   Applied warmth enhancement" << std::endl;
                
            } else if (characteristic == "punchy" || characteristic == "punchier") {
                // Enhance transients and mids
                applyCompression(track, 3.0f * intensity, -8.0f, 10.0f, 100.0f);
                applyEQ(track, {
                    {100.0f, 2.0f * intensity, 1.2f},    // Low punch
                    {1000.0f, 1.5f * intensity, 1.0f},   // Mid punch
                    {3000.0f, 1.0f * intensity, 1.5f}    // Attack
                });
                std::cout << "   Applied punch enhancement" << std::endl;
                
            } else if (characteristic == "aggressive" || characteristic == "harder") {
                // Add distortion and mid boost
                applyDistortion(track, 0.4f * intensity, "overdrive");
                applyEQ(track, {
                    {800.0f, 3.0f * intensity, 1.5f},    // Aggressive mids
                    {2000.0f, 2.0f * intensity, 1.2f},   // Upper mid aggression
                    {6000.0f, 1.0f * intensity, 1.0f}    // Edge
                });
                applyCompression(track, 6.0f * intensity, -10.0f, 5.0f, 50.0f); // Heavy compression
                std::cout << "   Applied aggressive processing" << std::endl;
                
            } else if (characteristic == "wide" || characteristic == "wider") {
                // Stereo width enhancement
                applyStereoWidth(track, 1.5f * intensity);
                std::cout << "   Applied stereo width enhancement" << std::endl;
                
            } else if (characteristic == "intimate" || characteristic == "closer") {
                // Close, compressed sound
                applyCompression(track, 6.0f * intensity, -15.0f, 5.0f, 50.0f);
                applyEQ(track, {
                    {800.0f, 1.5f * intensity, 0.8f},    // Warmth
                    {3000.0f, 2.0f * intensity, 1.0f},   // Presence
                    {10000.0f, -1.5f * intensity, 0.8f}  // Roll off highs
                });
                applyStereoWidth(track, 0.7f); // Narrow the stereo image
                std::cout << "   Applied intimate processing" << std::endl;
                
            } else {
                return core::VoidResult::failure("Unknown characteristic: " + characteristic);
            }
            
            logProcessingAction(track, "apply_characteristic", characteristic, 
                {{"intensity", intensity}}, "Made track sound more " + characteristic);
            
            std::cout << "✅ Successfully applied " << characteristic << " characteristic" << std::endl;
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to apply characteristic: " + std::string(e.what()));
        }
    });
}

// ========================================================================
// Internal Processing Methods
// ========================================================================

void IntelligentProcessor::applyArtistEQ(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity) {
    // Convert artist style to EQ curve
    auto eqCurve = processing_utils::styleToEQCurve(style);
    
    // Scale intensity
    for (auto& band : eqCurve) {
        std::get<1>(band) *= intensity; // Scale gain by intensity
    }
    
    applyEQ(track, eqCurve);
    std::cout << "   Applied " << style.artist << " EQ curve" << std::endl;
}

void IntelligentProcessor::applyArtistCompression(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity) {
    auto [ratio, threshold, attack, release] = processing_utils::styleToCompression(style);
    
    // Scale compression intensity
    float scaledRatio = 1.0f + (ratio - 1.0f) * intensity;
    
    applyCompression(track, scaledRatio, threshold, attack, release);
    std::cout << "   Applied " << style.artist << " compression (" << scaledRatio << ":1)" << std::endl;
}

void IntelligentProcessor::applyArtistSpatialEffects(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity) {
    auto [roomSize, wetLevel, decayTime] = processing_utils::styleToReverb(style);
    
    // Scale reverb by intensity
    float scaledWetLevel = wetLevel * intensity;
    
    applyReverb(track, roomSize, scaledWetLevel, decayTime);
    std::cout << "   Applied " << style.artist << " spatial effects" << std::endl;
}

void IntelligentProcessor::applyArtistSaturation(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity) {
    // Determine saturation type and amount based on style
    float driveAmount = 0.3f * intensity;
    std::string saturationType = "tube";
    
    if (style.overallCharacter.find("raw") != std::string::npos) {
        saturationType = "tape";
        driveAmount = 0.5f * intensity;
    } else if (style.overallCharacter.find("aggressive") != std::string::npos) {
        saturationType = "overdrive";
        driveAmount = 0.4f * intensity;
    }
    
    applyDistortion(track, driveAmount, saturationType);
    std::cout << "   Applied " << saturationType << " saturation (" << driveAmount << ")" << std::endl;
}

void IntelligentProcessor::applyArtistStereoProcessing(std::shared_ptr<core::ITrack> track, const ArtistStyle& style, float intensity) {
    float width = 1.0f; // Default stereo width
    
    if (style.overallCharacter.find("intimate") != std::string::npos) {
        width = 0.8f; // Narrower for intimate feel
    } else if (style.overallCharacter.find("wide") != std::string::npos || 
               style.genre == "Pop") {
        width = 1.3f; // Wider for modern pop
    }
    
    // Apply intensity scaling
    width = 1.0f + (width - 1.0f) * intensity;
    
    applyStereoWidth(track, width);
    std::cout << "   Applied stereo width: " << width << std::endl;
}

// ========================================================================
// Specific Effect Application (Mock Implementations)
// ========================================================================

void IntelligentProcessor::applyCompression(
    std::shared_ptr<core::ITrack> track,
    float ratio,
    float threshold,
    float attack,
    float release
) {
    // In a real implementation, this would interface with the DAW's plugin system
    // For now, we'll log the processing that would be applied
    std::cout << "   🎛️ Compressor: " << ratio << ":1, " << threshold << "dB, " 
              << attack << "ms attack, " << release << "ms release" << std::endl;
}

void IntelligentProcessor::applyEQ(
    std::shared_ptr<core::ITrack> track,
    const std::vector<std::tuple<float, float, float>>& bands
) {
    std::cout << "   🎛️ EQ: ";
    for (const auto& band : bands) {
        auto [freq, gain, q] = band;
        std::cout << freq << "Hz " << (gain > 0 ? "+" : "") << gain << "dB ";
    }
    std::cout << std::endl;
}

void IntelligentProcessor::applyReverb(
    std::shared_ptr<core::ITrack> track,
    float roomSize,
    float wetLevel,
    float decayTime
) {
    std::cout << "   🎛️ Reverb: Room=" << roomSize << ", Wet=" << (wetLevel * 100) 
              << "%, Decay=" << decayTime << "s" << std::endl;
}

void IntelligentProcessor::applyDistortion(
    std::shared_ptr<core::ITrack> track,
    float drive,
    const std::string& type
) {
    std::cout << "   🎛️ " << type << " saturation: Drive=" << (drive * 100) << "%" << std::endl;
}

void IntelligentProcessor::applyStereoWidth(
    std::shared_ptr<core::ITrack> track,
    float width
) {
    std::cout << "   🎛️ Stereo width: " << (width * 100) << "%" << std::endl;
}

// ========================================================================
// Processing Analysis and Mapping
// ========================================================================

std::map<std::string, float> IntelligentProcessor::descriptorToParameters(const std::string& descriptor) const {
    auto it = characteristicEQCurves_.find(descriptor);
    if (it != characteristicEQCurves_.end()) {
        std::map<std::string, float> params;
        params["descriptor"] = 1.0f;
        return params;
    }
    return {};
}

std::string IntelligentProcessor::generateProcessingExplanation(
    const ArtistStyle& style,
    const std::vector<std::string>& appliedEffects
) const {
    std::ostringstream explanation;
    explanation << "Applied " << style.artist << " style processing:\n";
    explanation << "• Character: " << style.overallCharacter << "\n";
    explanation << "• Effects: ";
    
    for (size_t i = 0; i < appliedEffects.size(); ++i) {
        explanation << appliedEffects[i];
        if (i < appliedEffects.size() - 1) explanation << " → ";
    }
    
    return explanation.str();
}

std::string IntelligentProcessor::explainProcessing(const std::string& artistOrStyle) const {
    if (!knowledgeBase_) return "Knowledge base not available";
    
    auto artist = knowledgeBase_->getArtistStyle(artistOrStyle);
    if (artist) {
        std::ostringstream explanation;
        explanation << "🎵 " << artist->artist << " Processing Style:\n\n";
        explanation << "Era: " << artist->era << " " << artist->genre << "\n";
        explanation << "Character: " << artist->overallCharacter << "\n\n";
        
        explanation << "Vocal Style:\n";
        explanation << "• " << artist->vocals.description << "\n";
        explanation << "• Mic technique: " << artist->vocals.micTechnique << "\n\n";
        
        explanation << "Typical Processing:\n";
        explanation << "• " << artist->mixingStyle.description << "\n";
        explanation << "• Target loudness: " << artist->typicalLoudness << " LUFS\n";
        explanation << "• Dynamic range: " << artist->typicalDynamicRange << " dB\n\n";
        
        explanation << "Keywords: ";
        for (size_t i = 0; i < artist->keywords.size(); ++i) {
            explanation << artist->keywords[i];
            if (i < artist->keywords.size() - 1) explanation << ", ";
        }
        
        return explanation.str();
    }
    
    return "Style or artist not found in database";
}

// ========================================================================
// State Management
// ========================================================================

void IntelligentProcessor::initializeCharacteristicMappings() {
    // Initialize EQ curves for different characteristics
    characteristicEQCurves_["bright"] = {{3000.0f, 2.0f, 1.0f}, {8000.0f, 3.0f, 0.8f}};
    characteristicEQCurves_["warm"] = {{200.0f, 1.5f, 0.8f}, {500.0f, 2.0f, 1.0f}, {8000.0f, -1.5f, 0.8f}};
    characteristicEQCurves_["punchy"] = {{100.0f, 2.0f, 1.2f}, {1000.0f, 1.5f, 1.0f}};
    
    // Initialize compression settings
    characteristicCompression_["aggressive"] = {6.0f, -10.0f};
    characteristicCompression_["smooth"] = {3.0f, -15.0f};
    characteristicCompression_["punchy"] = {4.0f, -8.0f};
    
    // Initialize reverb settings (room, wet, decay)
    characteristicReverb_["spacious"] = {0.8f, 0.3f, 2.0f};
    characteristicReverb_["intimate"] = {0.2f, 0.1f, 0.8f};
    characteristicReverb_["dry"] = {0.1f, 0.05f, 0.5f};
}

void IntelligentProcessor::logProcessingAction(
    std::shared_ptr<core::ITrack> track,
    const std::string& actionType,
    const std::string& target,
    const std::map<std::string, float>& settings,
    const std::string& description
) {
    ProcessingAction action;
    action.actionType = actionType;
    action.targetArtist = target;
    action.appliedSettings = settings;
    action.description = description;
    action.timestamp = std::chrono::steady_clock::now();
    
    // Store in processing history (using mock track ID for now)
    core::TrackID trackId = reinterpret_cast<core::TrackID>(track.get());
    processingHistory_[trackId].push_back(action);
    
    // Keep history manageable
    if (processingHistory_[trackId].size() > 10) {
        processingHistory_[trackId].erase(processingHistory_[trackId].begin());
    }
}

// ========================================================================
// Processing Utilities Implementation
// ========================================================================

namespace processing_utils {

std::vector<std::tuple<float, float, float>> styleToEQCurve(const ArtistStyle& style) {
    std::vector<std::tuple<float, float, float>> curve;
    
    // Generate EQ curve based on style characteristics
    if (style.overallCharacter.find("bright") != std::string::npos) {
        curve.push_back({3000.0f, 2.0f, 1.0f});
        curve.push_back({8000.0f, 2.5f, 0.8f});
    } else if (style.overallCharacter.find("warm") != std::string::npos) {
        curve.push_back({200.0f, 1.5f, 0.8f});
        curve.push_back({500.0f, 2.0f, 1.0f});
        curve.push_back({8000.0f, -1.0f, 0.8f});
    } else if (style.overallCharacter.find("aggressive") != std::string::npos) {
        curve.push_back({800.0f, 3.0f, 1.5f});
        curve.push_back({2000.0f, 2.0f, 1.2f});
    } else if (style.overallCharacter.find("intimate") != std::string::npos) {
        curve.push_back({800.0f, 1.5f, 0.8f});
        curve.push_back({3000.0f, 2.0f, 1.0f});
        curve.push_back({10000.0f, -1.5f, 0.8f});
    } else {
        // Default balanced curve
        curve.push_back({1000.0f, 0.5f, 1.0f});
    }
    
    return curve;
}

std::tuple<float, float, float, float> styleToCompression(const ArtistStyle& style) {
    // Default compression settings
    float ratio = 3.0f;
    float threshold = -12.0f;
    float attack = 10.0f;
    float release = 100.0f;
    
    if (style.overallCharacter.find("intimate") != std::string::npos) {
        ratio = 6.0f;
        threshold = -15.0f;
        attack = 5.0f;
        release = 50.0f;
    } else if (style.overallCharacter.find("aggressive") != std::string::npos) {
        ratio = 5.0f;
        threshold = -10.0f;
        attack = 3.0f;
        release = 80.0f;
    } else if (style.overallCharacter.find("raw") != std::string::npos) {
        ratio = 2.5f;
        threshold = -8.0f;
        attack = 20.0f;
        release = 200.0f;
    }
    
    return {ratio, threshold, attack, release};
}

std::tuple<float, float, float> styleToReverb(const ArtistStyle& style) {
    float roomSize = 0.5f;
    float wetLevel = 0.15f;
    float decayTime = 1.2f;
    
    if (style.overallCharacter.find("intimate") != std::string::npos) {
        roomSize = 0.2f;
        wetLevel = 0.1f;
        decayTime = 0.8f;
    } else if (style.overallCharacter.find("spacious") != std::string::npos || 
               style.genre == "Pop") {
        roomSize = 0.8f;
        wetLevel = 0.25f;
        decayTime = 2.0f;
    } else if (style.overallCharacter.find("raw") != std::string::npos) {
        roomSize = 0.3f;
        wetLevel = 0.05f;
        decayTime = 0.6f;
    }
    
    return {roomSize, wetLevel, decayTime};
}

std::map<std::string, float> scaleIntensity(
    const std::map<std::string, float>& baseSettings,
    float intensity
) {
    std::map<std::string, float> scaledSettings;
    
    for (const auto& [key, value] : baseSettings) {
        scaledSettings[key] = value * intensity;
    }
    
    return scaledSettings;
}

} // namespace processing_utils

} // namespace mixmind::ai