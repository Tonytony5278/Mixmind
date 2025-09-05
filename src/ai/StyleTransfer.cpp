#include "StyleTransfer.h"
#include "OpenAIIntegration.h"
#include "../audio/RealtimeAudioEngine.h"
#include "../core/async.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <iostream>

namespace mixmind::ai {

// ============================================================================
// Style Transfer Engine - Transform Audio to Different Styles
// ============================================================================

class StyleTransferEngine::Impl {
public:
    // AI Integration
    AudioIntelligenceEngine* aiEngine_;
    
    // Style database
    std::unordered_map<std::string, MusicStyle> styleDatabase_;
    std::vector<StyleTemplate> styleTemplates_;
    
    // Processing state
    std::atomic<bool> isProcessing_{false};
    std::mutex processingMutex_;
    
    // Audio processing components
    std::unique_ptr<SpectralProcessor> spectralProcessor_;
    std::unique_ptr<RhythmProcessor> rhythmProcessor_;
    std::unique_ptr<HarmonicProcessor> harmonicProcessor_;
    
    bool initialize() {
        aiEngine_ = &getGlobalAIEngine();
        
        // Initialize audio processors
        spectralProcessor_ = std::make_unique<SpectralProcessor>();
        rhythmProcessor_ = std::make_unique<RhythmProcessor>();
        harmonicProcessor_ = std::make_unique<HarmonicProcessor>();
        
        // Load built-in style templates
        loadBuiltInStyles();
        
        std::cout << "🎨 Style Transfer Engine initialized with " 
                  << styleTemplates_.size() << " styles" << std::endl;
        return true;
    }
    
    void loadBuiltInStyles() {
        // Jazz Style
        {
            StyleTemplate jazz;
            jazz.name = "Jazz";
            jazz.description = "Smooth, sophisticated jazz with extended harmonies";
            jazz.spectralCharacteristics.warmth = 0.8f;
            jazz.spectralCharacteristics.brightness = 0.6f;
            jazz.spectralCharacteristics.lowEndWeight = 0.7f;
            jazz.spectralCharacteristics.midPresence = 0.9f;
            jazz.spectralCharacteristics.highShimmer = 0.5f;
            
            jazz.rhythmicFeatures.swing = 0.8f;
            jazz.rhythmicFeatures.syncopation = 0.7f;
            jazz.rhythmicFeatures.groove = 0.9f;
            jazz.rhythmicFeatures.polyrhythm = 0.6f;
            
            jazz.harmonicStructure.complexity = 0.9f;
            jazz.harmonicStructure.dissonance = 0.7f;
            jazz.harmonicStructure.chromaticism = 0.8f;
            jazz.harmonicStructure.voicing = 0.9f;
            
            styleTemplates_.push_back(jazz);
        }
        
        // Electronic/EDM Style
        {
            StyleTemplate edm;
            edm.name = "Electronic";
            edm.description = "Modern electronic dance music with punchy beats";
            edm.spectralCharacteristics.warmth = 0.3f;
            edm.spectralCharacteristics.brightness = 0.9f;
            edm.spectralCharacteristics.lowEndWeight = 0.9f;
            edm.spectralCharacteristics.midPresence = 0.6f;
            edm.spectralCharacteristics.highShimmer = 0.8f;
            
            edm.rhythmicFeatures.swing = 0.1f;
            edm.rhythmicFeatures.syncopation = 0.8f;
            edm.rhythmicFeatures.groove = 0.9f;
            edm.rhythmicFeatures.polyrhythm = 0.7f;
            
            edm.harmonicStructure.complexity = 0.5f;
            edm.harmonicStructure.dissonance = 0.6f;
            edm.harmonicStructure.chromaticism = 0.4f;
            edm.harmonicStructure.voicing = 0.6f;
            
            styleTemplates_.push_back(edm);
        }
        
        // Rock Style
        {
            StyleTemplate rock;
            rock.name = "Rock";
            rock.description = "Powerful rock with driving rhythms and guitar presence";
            rock.spectralCharacteristics.warmth = 0.7f;
            rock.spectralCharacteristics.brightness = 0.8f;
            rock.spectralCharacteristics.lowEndWeight = 0.8f;
            rock.spectralCharacteristics.midPresence = 0.9f;
            rock.spectralCharacteristics.highShimmer = 0.7f;
            
            rock.rhythmicFeatures.swing = 0.2f;
            rock.rhythmicFeatures.syncopation = 0.6f;
            rock.rhythmicFeatures.groove = 0.9f;
            rock.rhythmicFeatures.polyrhythm = 0.3f;
            
            rock.harmonicStructure.complexity = 0.4f;
            rock.harmonicStructure.dissonance = 0.5f;
            rock.harmonicStructure.chromaticism = 0.3f;
            rock.harmonicStructure.voicing = 0.5f;
            
            styleTemplates_.push_back(rock);
        }
        
        // Classical Style
        {
            StyleTemplate classical;
            classical.name = "Classical";
            classical.description = "Orchestral classical with natural dynamics and space";
            classical.spectralCharacteristics.warmth = 0.9f;
            classical.spectralCharacteristics.brightness = 0.7f;
            classical.spectralCharacteristics.lowEndWeight = 0.6f;
            classical.spectralCharacteristics.midPresence = 0.8f;
            classical.spectralCharacteristics.highShimmer = 0.6f;
            
            classical.rhythmicFeatures.swing = 0.3f;
            classical.rhythmicFeatures.syncopation = 0.2f;
            classical.rhythmicFeatures.groove = 0.7f;
            classical.rhythmicFeatures.polyrhythm = 0.8f;
            
            classical.harmonicStructure.complexity = 0.8f;
            classical.harmonicStructure.dissonance = 0.4f;
            classical.harmonicStructure.chromaticism = 0.6f;
            classical.harmonicStructure.voicing = 0.9f;
            
            styleTemplates_.push_back(classical);
        }
        
        // Hip Hop Style
        {
            StyleTemplate hiphop;
            hiphop.name = "Hip Hop";
            hiphop.description = "Modern hip hop with heavy bass and crisp highs";
            hiphop.spectralCharacteristics.warmth = 0.5f;
            hiphop.spectralCharacteristics.brightness = 0.8f;
            hiphop.spectralCharacteristics.lowEndWeight = 0.95f;
            hiphop.spectralCharacteristics.midPresence = 0.7f;
            hiphop.spectralCharacteristics.highShimmer = 0.9f;
            
            hiphop.rhythmicFeatures.swing = 0.6f;
            hiphop.rhythmicFeatures.syncopation = 0.9f;
            hiphop.rhythmicFeatures.groove = 0.95f;
            hiphop.rhythmicFeatures.polyrhythm = 0.8f;
            
            hiphop.harmonicStructure.complexity = 0.3f;
            hiphop.harmonicStructure.dissonance = 0.6f;
            hiphop.harmonicStructure.chromaticism = 0.5f;
            hiphop.harmonicStructure.voicing = 0.4f;
            
            styleTemplates_.push_back(hiphop);
        }
    }
    
    core::AsyncResult<StyleTransferResult> transferStyle(
        const std::string& sourceDescription,
        const std::string& targetStyleName,
        float intensity) {
        
        if (isProcessing_.load()) {
            return core::executeAsyncGlobal<StyleTransferResult>([]() -> core::Result<StyleTransferResult> {
                StyleTransferResult result;
                result.success = false;
                result.errorMessage = "Style transfer already in progress";
                return core::Result<StyleTransferResult>::success(result);
            });
        }
        
        return core::executeAsyncGlobal<StyleTransferResult>(
            [this, sourceDescription, targetStyleName, intensity]() -> core::Result<StyleTransferResult> {
                
            isProcessing_.store(true);
            
            try {
                StyleTransferResult result = performStyleTransfer(
                    sourceDescription, targetStyleName, intensity);
                
                isProcessing_.store(false);
                return core::Result<StyleTransferResult>::success(result);
                
            } catch (const std::exception& e) {
                isProcessing_.store(false);
                StyleTransferResult result;
                result.success = false;
                result.errorMessage = "Style transfer failed: " + std::string(e.what());
                return core::Result<StyleTransferResult>::success(result);
            }
        });
    }
    
    StyleTransferResult performStyleTransfer(
        const std::string& sourceDescription,
        const std::string& targetStyleName,
        float intensity) {
        
        StyleTransferResult result;
        result.sourceStyle = analyzeSourceStyle(sourceDescription);
        
        // Find target style template
        auto targetIt = std::find_if(styleTemplates_.begin(), styleTemplates_.end(),
            [&targetStyleName](const StyleTemplate& style) {
                return style.name == targetStyleName;
            });
            
        if (targetIt == styleTemplates_.end()) {
            result.success = false;
            result.errorMessage = "Target style not found: " + targetStyleName;
            return result;
        }
        
        result.targetStyle = *targetIt;
        
        // Generate AI-powered style transfer plan
        auto aiPlan = generateAIStyleTransferPlan(result.sourceStyle, result.targetStyle, intensity);
        result.transformations = aiPlan;
        
        // Calculate processing parameters
        result.processingParameters = calculateProcessingParameters(
            result.sourceStyle, result.targetStyle, intensity);
        
        result.success = true;
        result.confidenceScore = calculateConfidenceScore(result.sourceStyle, result.targetStyle);
        
        std::cout << "✨ Style transfer plan generated: " 
                  << result.sourceStyle.name << " → " << result.targetStyle.name 
                  << " (confidence: " << result.confidenceScore << ")" << std::endl;
        
        return result;
    }
    
    StyleTemplate analyzeSourceStyle(const std::string& description) {
        StyleTemplate sourceStyle;
        sourceStyle.name = "Source";
        sourceStyle.description = description;
        
        // AI-powered style analysis
        AudioAnalysisContext context;
        context.additionalInfo = description;
        
        ChatRequest request;
        request.model = "gpt-4";
        request.temperature = 0.3;
        
        ChatMessage systemMsg;
        systemMsg.role = "system";
        systemMsg.content = R"(
You are an expert music analyst. Analyze the described audio and provide detailed style characteristics.
Focus on spectral balance, rhythmic features, and harmonic complexity.
Respond with specific numerical assessments on a 0.0-1.0 scale.
)";
        
        ChatMessage userMsg;
        userMsg.role = "user";
        userMsg.content = "Analyze the style characteristics of: " + description + 
                         "\n\nProvide assessments for: warmth, brightness, low-end weight, " +
                         "mid presence, swing, syncopation, harmonic complexity, and overall groove.";
        
        request.messages = {systemMsg, userMsg};
        
        // For now, use default values - in production, parse AI response
        sourceStyle.spectralCharacteristics.warmth = 0.5f;
        sourceStyle.spectralCharacteristics.brightness = 0.5f;
        sourceStyle.spectralCharacteristics.lowEndWeight = 0.5f;
        sourceStyle.spectralCharacteristics.midPresence = 0.5f;
        sourceStyle.spectralCharacteristics.highShimmer = 0.5f;
        
        sourceStyle.rhythmicFeatures.swing = 0.5f;
        sourceStyle.rhythmicFeatures.syncopation = 0.5f;
        sourceStyle.rhythmicFeatures.groove = 0.5f;
        sourceStyle.rhythmicFeatures.polyrhythm = 0.5f;
        
        sourceStyle.harmonicStructure.complexity = 0.5f;
        sourceStyle.harmonicStructure.dissonance = 0.5f;
        sourceStyle.harmonicStructure.chromaticism = 0.5f;
        sourceStyle.harmonicStructure.voicing = 0.5f;
        
        return sourceStyle;
    }
    
    std::vector<StyleTransformation> generateAIStyleTransferPlan(
        const StyleTemplate& source,
        const StyleTemplate& target,
        float intensity) {
        
        std::vector<StyleTransformation> transformations;
        
        // Spectral transformations
        if (std::abs(target.spectralCharacteristics.warmth - source.spectralCharacteristics.warmth) > 0.1f) {
            StyleTransformation warmth;
            warmth.type = TransformationType::SPECTRAL_WARMTH;
            warmth.parameter = "warmth";
            warmth.sourceValue = source.spectralCharacteristics.warmth;
            warmth.targetValue = target.spectralCharacteristics.warmth;
            warmth.intensity = intensity;
            warmth.description = "Adjust tonal warmth to match " + target.name + " style";
            transformations.push_back(warmth);
        }
        
        if (std::abs(target.spectralCharacteristics.brightness - source.spectralCharacteristics.brightness) > 0.1f) {
            StyleTransformation brightness;
            brightness.type = TransformationType::SPECTRAL_BRIGHTNESS;
            brightness.parameter = "brightness";
            brightness.sourceValue = source.spectralCharacteristics.brightness;
            brightness.targetValue = target.spectralCharacteristics.brightness;
            brightness.intensity = intensity;
            brightness.description = "Modify high-frequency content for " + target.name + " character";
            transformations.push_back(brightness);
        }
        
        // Rhythmic transformations
        if (std::abs(target.rhythmicFeatures.swing - source.rhythmicFeatures.swing) > 0.1f) {
            StyleTransformation swing;
            swing.type = TransformationType::RHYTHMIC_SWING;
            swing.parameter = "swing";
            swing.sourceValue = source.rhythmicFeatures.swing;
            swing.targetValue = target.rhythmicFeatures.swing;
            swing.intensity = intensity;
            swing.description = "Apply " + target.name + " rhythmic swing characteristics";
            transformations.push_back(swing);
        }
        
        // Dynamic transformations
        StyleTransformation dynamics;
        dynamics.type = TransformationType::DYNAMIC_RANGE;
        dynamics.parameter = "dynamics";
        dynamics.sourceValue = 0.5f; // Assume moderate dynamics
        dynamics.targetValue = (target.name == "Classical") ? 0.9f : 
                              (target.name == "Electronic") ? 0.3f : 0.6f;
        dynamics.intensity = intensity;
        dynamics.description = "Adjust dynamic range for " + target.name + " aesthetics";
        transformations.push_back(dynamics);
        
        return transformations;
    }
    
    StyleProcessingParameters calculateProcessingParameters(
        const StyleTemplate& source,
        const StyleTemplate& target,
        float intensity) {
        
        StyleProcessingParameters params;
        
        // EQ adjustments
        params.eqAdjustments.push_back({
            100.0f, // frequency
            (target.spectralCharacteristics.lowEndWeight - source.spectralCharacteristics.lowEndWeight) * intensity * 3.0f, // gain
            1.0f,   // Q
            "Low-end adjustment for " + target.name + " character"
        });
        
        params.eqAdjustments.push_back({
            1000.0f, // frequency
            (target.spectralCharacteristics.midPresence - source.spectralCharacteristics.midPresence) * intensity * 2.0f, // gain
            2.0f,    // Q
            "Mid-range presence for " + target.name + " clarity"
        });
        
        params.eqAdjustments.push_back({
            8000.0f, // frequency
            (target.spectralCharacteristics.brightness - source.spectralCharacteristics.brightness) * intensity * 2.5f, // gain
            1.5f,    // Q
            "High-frequency character for " + target.name + " style"
        });
        
        // Compression settings
        params.compressionRatio = 1.0f + (4.0f - 1.0f) * 
            ((target.name == "Rock" || target.name == "Electronic") ? 0.7f : 0.3f) * intensity;
        params.compressionThreshold = -12.0f + (6.0f * (1.0f - intensity));
        params.compressionAttack = (target.name == "Electronic") ? 1.0f : 10.0f;
        params.compressionRelease = (target.name == "Electronic") ? 50.0f : 150.0f;
        
        // Saturation/distortion
        params.saturationAmount = (target.name == "Rock") ? 0.3f * intensity : 0.1f * intensity;
        params.saturationType = (target.name == "Rock") ? "tube" : "tape";
        
        // Spatial processing
        params.reverbAmount = (target.name == "Classical") ? 0.4f : 
                             (target.name == "Electronic") ? 0.1f : 0.2f;
        params.reverbType = (target.name == "Classical") ? "hall" :
                           (target.name == "Electronic") ? "plate" : "room";
        
        return params;
    }
    
    float calculateConfidenceScore(const StyleTemplate& source, const StyleTemplate& target) {
        // Calculate similarity between source and target styles
        float spectralDiff = std::abs(target.spectralCharacteristics.warmth - source.spectralCharacteristics.warmth) +
                            std::abs(target.spectralCharacteristics.brightness - source.spectralCharacteristics.brightness);
        
        float rhythmicDiff = std::abs(target.rhythmicFeatures.swing - source.rhythmicFeatures.swing) +
                            std::abs(target.rhythmicFeatures.syncopation - source.rhythmicFeatures.syncopation);
        
        float harmonicDiff = std::abs(target.harmonicStructure.complexity - source.harmonicStructure.complexity);
        
        float totalDifference = (spectralDiff + rhythmicDiff + harmonicDiff) / 6.0f;
        
        // Higher confidence for larger differences (more dramatic transformation)
        return std::min(1.0f, 0.3f + totalDifference);
    }
};

// ============================================================================
// Audio Processing Components
// ============================================================================

class SpectralProcessor::Impl {
public:
    // FFT processing for spectral manipulation
    void processSpectralCharacteristics(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output,
        const SpectralCharacteristics& target) {
        
        // For now, simple gain adjustments - in production, use FFT
        float warmthGain = 0.5f + target.warmth * 0.5f;
        float brightnessGain = 0.5f + target.brightness * 0.5f;
        
        for (size_t ch = 0; ch < input.channels; ++ch) {
            const float* inputChannel = input.getChannelData(ch);
            float* outputChannel = output.getChannelData(ch);
            
            for (size_t sample = 0; sample < input.capacity; ++sample) {
                // Simple spectral shaping approximation
                float processed = inputChannel[sample];
                processed *= warmthGain; // Low-freq emphasis
                processed = std::tanh(processed * brightnessGain); // High-freq character
                outputChannel[sample] = processed;
            }
        }
    }
};

SpectralProcessor::SpectralProcessor() : pImpl_(std::make_unique<Impl>()) {}
SpectralProcessor::~SpectralProcessor() = default;

void SpectralProcessor::processSpectralCharacteristics(
    const audio::AudioBufferPool::AudioBuffer& input,
    audio::AudioBufferPool::AudioBuffer& output,
    const SpectralCharacteristics& target) {
    pImpl_->processSpectralCharacteristics(input, output, target);
}

class RhythmProcessor::Impl {
public:
    void processRhythmicFeatures(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output,
        const RhythmicFeatures& target) {
        
        // Rhythm processing would involve tempo detection, beat tracking, etc.
        // For now, simple copy with groove-based gain modulation
        
        for (size_t ch = 0; ch < input.channels; ++ch) {
            const float* inputChannel = input.getChannelData(ch);
            float* outputChannel = output.getChannelData(ch);
            
            for (size_t sample = 0; sample < input.capacity; ++sample) {
                float grooveMod = 1.0f + target.groove * 0.1f * std::sin(sample * 0.001f);
                outputChannel[sample] = inputChannel[sample] * grooveMod;
            }
        }
    }
};

RhythmProcessor::RhythmProcessor() : pImpl_(std::make_unique<Impl>()) {}
RhythmProcessor::~RhythmProcessor() = default;

void RhythmProcessor::processRhythmicFeatures(
    const audio::AudioBufferPool::AudioBuffer& input,
    audio::AudioBufferPool::AudioBuffer& output,
    const RhythmicFeatures& target) {
    pImpl_->processRhythmicFeatures(input, output, target);
}

class HarmonicProcessor::Impl {
public:
    void processHarmonicStructure(
        const audio::AudioBufferPool::AudioBuffer& input,
        audio::AudioBufferPool::AudioBuffer& output,
        const HarmonicStructure& target) {
        
        // Harmonic processing for chord voicing and complexity
        for (size_t ch = 0; ch < input.channels; ++ch) {
            const float* inputChannel = input.getChannelData(ch);
            float* outputChannel = output.getChannelData(ch);
            
            for (size_t sample = 0; sample < input.capacity; ++sample) {
                float harmonicColor = 1.0f + target.complexity * 0.2f;
                outputChannel[sample] = inputChannel[sample] * harmonicColor;
            }
        }
    }
};

HarmonicProcessor::HarmonicProcessor() : pImpl_(std::make_unique<Impl>()) {}
HarmonicProcessor::~HarmonicProcessor() = default;

void HarmonicProcessor::processHarmonicStructure(
    const audio::AudioBufferPool::AudioBuffer& input,
    audio::AudioBufferPool::AudioBuffer& output,
    const HarmonicStructure& target) {
    pImpl_->processHarmonicStructure(input, output, target);
}

// ============================================================================
// StyleTransferEngine Public Interface
// ============================================================================

StyleTransferEngine::StyleTransferEngine() : pImpl_(std::make_unique<Impl>()) {}
StyleTransferEngine::~StyleTransferEngine() = default;

bool StyleTransferEngine::initialize() {
    return pImpl_->initialize();
}

core::AsyncResult<StyleTransferResult> StyleTransferEngine::transferStyle(
    const std::string& sourceDescription,
    const std::string& targetStyleName,
    float intensity) {
    return pImpl_->transferStyle(sourceDescription, targetStyleName, intensity);
}

std::vector<StyleTemplate> StyleTransferEngine::getAvailableStyles() const {
    return pImpl_->styleTemplates_;
}

bool StyleTransferEngine::isProcessing() const {
    return pImpl_->isProcessing_.load();
}

// Global style transfer engine
static std::unique_ptr<StyleTransferEngine> g_styleEngine;
static std::mutex g_styleEngineMutex;

StyleTransferEngine& getGlobalStyleEngine() {
    std::lock_guard<std::mutex> lock(g_styleEngineMutex);
    if (!g_styleEngine) {
        g_styleEngine = std::make_unique<StyleTransferEngine>();
    }
    return *g_styleEngine;
}

void shutdownGlobalStyleEngine() {
    std::lock_guard<std::mutex> lock(g_styleEngineMutex);
    g_styleEngine.reset();
}

} // namespace mixmind::ai