#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include "../audio/LockFreeBuffer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>

namespace mixmind::ai {

// ============================================================================
// Music Style Definitions
// ============================================================================

struct SpectralCharacteristics {
    float warmth = 0.5f;        // Low-frequency warmth (0.0 = cold, 1.0 = warm)
    float brightness = 0.5f;    // High-frequency presence (0.0 = dull, 1.0 = bright)
    float lowEndWeight = 0.5f;  // Sub-bass and bass emphasis (0.0 = thin, 1.0 = heavy)
    float midPresence = 0.5f;   // Midrange clarity (0.0 = scooped, 1.0 = forward)
    float highShimmer = 0.5f;   // High-frequency air and sparkle (0.0 = dark, 1.0 = airy)
};

struct RhythmicFeatures {
    float swing = 0.0f;         // Swing rhythm amount (0.0 = straight, 1.0 = heavy swing)
    float syncopation = 0.5f;   // Off-beat emphasis (0.0 = on-beat, 1.0 = highly syncopated)
    float groove = 0.5f;        // Rhythmic pocket and feel (0.0 = rigid, 1.0 = deep groove)
    float polyrhythm = 0.0f;    // Complex rhythm layering (0.0 = simple, 1.0 = polyrhythmic)
};

struct HarmonicStructure {
    float complexity = 0.5f;    // Chord complexity (0.0 = simple triads, 1.0 = extended harmonies)
    float dissonance = 0.5f;    // Harmonic tension (0.0 = consonant, 1.0 = dissonant)
    float chromaticism = 0.5f;  // Use of non-diatonic notes (0.0 = diatonic, 1.0 = chromatic)
    float voicing = 0.5f;       // Chord voicing sophistication (0.0 = basic, 1.0 = advanced)
};

struct MusicStyle {
    std::string name;
    std::string description;
    SpectralCharacteristics spectral;
    RhythmicFeatures rhythmic;
    HarmonicStructure harmonic;
    
    // Additional style metadata
    std::vector<std::string> keyCharacteristics;
    std::vector<std::string> typicalInstruments;
    std::vector<std::string> commonEffects;
    float typicalTempo = 120.0f;
    std::string timeSignature = "4/4";
};

using StyleTemplate = MusicStyle; // Alias for clarity

// ============================================================================
// Style Transfer Types
// ============================================================================

enum class TransformationType {
    SPECTRAL_WARMTH,
    SPECTRAL_BRIGHTNESS,
    SPECTRAL_LOW_END,
    SPECTRAL_MID_PRESENCE,
    SPECTRAL_HIGH_SHIMMER,
    
    RHYTHMIC_SWING,
    RHYTHMIC_SYNCOPATION,
    RHYTHMIC_GROOVE,
    RHYTHMIC_POLYRHYTHM,
    
    HARMONIC_COMPLEXITY,
    HARMONIC_DISSONANCE,
    HARMONIC_CHROMATICISM,
    HARMONIC_VOICING,
    
    DYNAMIC_RANGE,
    DYNAMIC_COMPRESSION,
    DYNAMIC_TRANSIENTS,
    
    SPATIAL_WIDTH,
    SPATIAL_DEPTH,
    SPATIAL_HEIGHT
};

struct StyleTransformation {
    TransformationType type;
    std::string parameter;
    float sourceValue = 0.0f;
    float targetValue = 0.0f;
    float intensity = 1.0f;
    std::string description;
    bool isRecommended = true;
};

struct EQAdjustment {
    float frequency = 1000.0f;
    float gain = 0.0f;          // dB
    float q = 1.0f;
    std::string description;
};

struct StyleProcessingParameters {
    // EQ adjustments for spectral shaping
    std::vector<EQAdjustment> eqAdjustments;
    
    // Compression for dynamic control
    float compressionRatio = 1.0f;
    float compressionThreshold = -6.0f; // dB
    float compressionAttack = 10.0f;    // ms
    float compressionRelease = 100.0f;  // ms
    
    // Saturation/distortion for harmonic content
    float saturationAmount = 0.0f;
    std::string saturationType = "tape"; // "tube", "tape", "digital"
    
    // Spatial effects
    float reverbAmount = 0.0f;
    std::string reverbType = "hall"; // "hall", "room", "plate", "spring"
    float delayAmount = 0.0f;
    float delayTime = 250.0f; // ms
    
    // Modulation effects
    float chorusAmount = 0.0f;
    float flangerAmount = 0.0f;
    float phaserAmount = 0.0f;
};

struct StyleTransferResult {
    bool success = false;
    std::string errorMessage;
    
    StyleTemplate sourceStyle;
    StyleTemplate targetStyle;
    
    std::vector<StyleTransformation> transformations;
    StyleProcessingParameters processingParameters;
    
    float confidenceScore = 0.0f;
    std::string aiAnalysis;
    std::vector<std::string> recommendations;
};

// ============================================================================
// Audio Processing Components for Style Transfer
// ============================================================================

class SpectralProcessor {
public:
    SpectralProcessor();
    ~SpectralProcessor();
    
    void processSpectralCharacteristics(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output,
        const SpectralCharacteristics& target);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

class RhythmProcessor {
public:
    RhythmProcessor();
    ~RhythmProcessor();
    
    void processRhythmicFeatures(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output,
        const RhythmicFeatures& target);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

class HarmonicProcessor {
public:
    HarmonicProcessor();
    ~HarmonicProcessor();
    
    void processHarmonicStructure(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output,
        const HarmonicStructure& target);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Style Transfer Engine - Main Interface
// ============================================================================

class StyleTransferEngine {
public:
    StyleTransferEngine();
    ~StyleTransferEngine();
    
    // Non-copyable
    StyleTransferEngine(const StyleTransferEngine&) = delete;
    StyleTransferEngine& operator=(const StyleTransferEngine&) = delete;
    
    // Initialize style transfer system
    bool initialize();
    
    // Main style transfer function
    core::AsyncResult<StyleTransferResult> transferStyle(
        const std::string& sourceDescription,
        const std::string& targetStyleName,
        float intensity = 1.0f);
    
    // Style management
    std::vector<StyleTemplate> getAvailableStyles() const;
    bool addCustomStyle(const StyleTemplate& style);
    bool removeStyle(const std::string& styleName);
    
    // Analysis functions
    StyleTemplate analyzeAudioStyle(const std::string& audioDescription);
    float calculateStyleSimilarity(const StyleTemplate& style1, const StyleTemplate& style2);
    
    // Processing status
    bool isProcessing() const;
    void cancelProcessing();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Built-in Style Presets
// ============================================================================

namespace presets {
    // Popular music styles
    extern const StyleTemplate JAZZ_SMOOTH;
    extern const StyleTemplate JAZZ_FUSION;
    extern const StyleTemplate ROCK_CLASSIC;
    extern const StyleTemplate ROCK_MODERN;
    extern const StyleTemplate ELECTRONIC_HOUSE;
    extern const StyleTemplate ELECTRONIC_DUBSTEP;
    extern const StyleTemplate HIP_HOP_BOOM_BAP;
    extern const StyleTemplate HIP_HOP_TRAP;
    extern const StyleTemplate CLASSICAL_ORCHESTRAL;
    extern const StyleTemplate CLASSICAL_CHAMBER;
    extern const StyleTemplate FOLK_ACOUSTIC;
    extern const StyleTemplate BLUES_TRADITIONAL;
    extern const StyleTemplate REGGAE_ROOTS;
    extern const StyleTemplate LATIN_SALSA;
    extern const StyleTemplate AMBIENT_ATMOSPHERIC;
    
    // Get all built-in presets
    std::vector<StyleTemplate> getAllPresets();
}

// ============================================================================
// Style Transfer Utilities
// ============================================================================

namespace utils {
    // Convert style characteristics to human-readable descriptions
    std::string describeSpectralCharacteristics(const SpectralCharacteristics& spectral);
    std::string describeRhythmicFeatures(const RhythmicFeatures& rhythmic);
    std::string describeHarmonicStructure(const HarmonicStructure& harmonic);
    
    // Style analysis helpers
    float calculateSpectralDistance(const SpectralCharacteristics& a, const SpectralCharacteristics& b);
    float calculateRhythmicDistance(const RhythmicFeatures& a, const RhythmicFeatures& b);
    float calculateHarmonicDistance(const HarmonicStructure& a, const HarmonicStructure& b);
    
    // Processing parameter generation
    StyleProcessingParameters generateOptimalParameters(
        const StyleTemplate& source, 
        const StyleTemplate& target,
        float intensity = 1.0f);
    
    // Style recommendation system
    struct StyleRecommendation {
        std::string styleName;
        float compatibility;
        std::string reasoning;
    };
    
    std::vector<StyleRecommendation> recommendStyles(
        const StyleTemplate& sourceStyle,
        int maxRecommendations = 5);
}

// ============================================================================
// Global Style Transfer Engine
// ============================================================================

// Get global style transfer engine (singleton)
StyleTransferEngine& getGlobalStyleEngine();

// Shutdown style engine (call at app exit)
void shutdownGlobalStyleEngine();

} // namespace mixmind::ai