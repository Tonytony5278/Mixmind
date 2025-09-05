// AudioGenerator.cpp - Complete Implementation as per Transformation Plan
#include "AudioGenerator.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mixmind::audio::generators {

// ============================================================================
// AudioGenerator Base Class Implementation
// ============================================================================

std::string AudioGenerator::getCacheKey(const GeneratorParams& params) const {
    std::ostringstream oss;
    oss << getName() << "_"
        << params.sampleRate << "_"
        << params.channels << "_" 
        << params.tempo << "_"
        << params.bars << "_"
        << params.beatsPerBar << "_"
        << static_cast<int>(params.volume * 1000);
    return oss.str();
}

bool AudioGenerator::validateParams(const GeneratorParams& params) const {
    return params.sampleRate > 0 && 
           params.channels > 0 && 
           params.tempo > 0.0 && 
           params.bars > 0 &&
           params.beatsPerBar > 0 &&
           params.volume >= 0.0 && params.volume <= 1.0;
}

AudioGenerator::QualityMetrics AudioGenerator::analyzeQuality(const AudioBuffer& buffer) const {
    QualityMetrics metrics;
    
    metrics.rmsLevel = buffer.getRMSLevel();
    metrics.peakLevel = buffer.getPeakLevel();
    metrics.hasClipping = metrics.peakLevel >= 0.99f;
    metrics.dynamicRange = metrics.peakLevel > 0 ? (metrics.rmsLevel / metrics.peakLevel) : 0.0f;
    metrics.actualLength = buffer.lengthSeconds;
    
    // Quality threshold: RMS should be above 0.01 for audible content
    metrics.meetsThreshold = metrics.rmsLevel > 0.01f;
    
    return metrics;
}

// Utility functions
float AudioGenerator::generateSine(double phase) {
    return static_cast<float>(std::sin(2.0 * M_PI * phase));
}

float AudioGenerator::generateSaw(double phase) {
    phase = std::fmod(phase, 1.0);
    return static_cast<float>(2.0 * phase - 1.0);
}

float AudioGenerator::generateSquare(double phase) {
    phase = std::fmod(phase, 1.0);
    return phase < 0.5 ? 1.0f : -1.0f;
}

float AudioGenerator::generateNoise() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    return dis(gen);
}

float AudioGenerator::applyEnvelope(float sample, double position, double length,
                                  double attack, double decay, double sustain, double release) {
    if (position < 0.0 || position >= length) return 0.0f;
    
    double attackTime = attack * length;
    double decayTime = decay * length;
    double releaseStart = length - (release * length);
    
    float envelope = 1.0f;
    
    if (position < attackTime) {
        // Attack phase
        envelope = static_cast<float>(position / attackTime);
    } else if (position < attackTime + decayTime) {
        // Decay phase
        double decayPos = (position - attackTime) / decayTime;
        envelope = static_cast<float>(1.0 - decayPos * (1.0 - sustain));
    } else if (position < releaseStart) {
        // Sustain phase
        envelope = static_cast<float>(sustain);
    } else {
        // Release phase
        double releasePos = (position - releaseStart) / (length - releaseStart);
        envelope = static_cast<float>(sustain * (1.0 - releasePos));
    }
    
    return sample * std::max(0.0f, std::min(1.0f, envelope));
}

// DrumGenerator implementation
DrumGenerator::DrumGenerator(const DrumParams& drumParams) 
    : drumParams_(drumParams) {}

AudioBuffer DrumGenerator::generate(const GeneratorParams& params) {
    if (!validateParams(params)) {
        return AudioBuffer(params.sampleRate, params.channels);
    }
    
    switch (drumParams_.type) {
        case DrumType::Kick:   return generateKick(params);
        case DrumType::Snare:  return generateSnare(params);
        case DrumType::HiHat:  return generateHiHat(params, false);
        case DrumType::OpenHat: return generateHiHat(params, true);
        case DrumType::Crash:  return generateCrash(params);
        case DrumType::Tom:    return generateTom(params);
        case DrumType::Kit:    return generateFullKit(params);
        default:               return generateFullKit(params);
    }
}

AudioBuffer DrumGenerator::generateKick(const GeneratorParams& params) {
    AudioBuffer buffer(params.sampleRate, params.channels);
    size_t totalFrames = params.getTotalFrames();
    buffer.resize(totalFrames * params.channels);
    
    // Generate kick pattern (every beat)
    double beatLength = 60.0 / params.tempo;
    double totalLength = params.getTotalLengthSeconds();
    
    for (int bar = 0; bar < params.bars; ++bar) {
        for (int beat = 0; beat < params.beatsPerBar; ++beat) {
            double kickTime = bar * params.getBarLengthSeconds() + beat * beatLength;
            
            // Generate kick drum sound (sine wave with pitch envelope)
            double kickDuration = 0.2; // 200ms kick
            size_t kickSamples = static_cast<size_t>(kickDuration * params.sampleRate);
            size_t kickStart = static_cast<size_t>(kickTime * params.sampleRate);
            
            for (size_t i = 0; i < kickSamples && (kickStart + i) < totalFrames; ++i) {
                double t = static_cast<double>(i) / params.sampleRate;
                double phase = 0.0;
                
                // Frequency envelope: 60Hz to 40Hz
                double freq = 60.0 - 20.0 * (t / kickDuration);
                phase += freq * t;
                
                float sample = generateSine(phase);
                sample = applyEnvelope(sample, t, kickDuration, 0.001, 0.05, 0.3, 0.8);
                sample *= params.volume * drumParams_.velocity;
                
                for (int ch = 0; ch < params.channels; ++ch) {
                    buffer.setSample(kickStart + i, ch, sample);
                }
            }
        }
    }
    
    return buffer;
}

AudioBuffer DrumGenerator::generateSnare(const GeneratorParams& params) {
    AudioBuffer buffer(params.sampleRate, params.channels);
    size_t totalFrames = params.getTotalFrames();
    buffer.resize(totalFrames * params.channels);
    
    // Generate snare on beats 2 and 4
    double beatLength = 60.0 / params.tempo;
    
    for (int bar = 0; bar < params.bars; ++bar) {
        for (int beat : {1, 3}) { // Beats 2 and 4 (0-indexed)
            if (beat >= params.beatsPerBar) continue;
            
            double snareTime = bar * params.getBarLengthSeconds() + beat * beatLength;
            
            // Generate snare sound (noise + tone)
            double snareDuration = 0.15; // 150ms snare
            size_t snareSamples = static_cast<size_t>(snareDuration * params.sampleRate);
            size_t snareStart = static_cast<size_t>(snareTime * params.sampleRate);
            
            for (size_t i = 0; i < snareSamples && (snareStart + i) < totalFrames; ++i) {
                double t = static_cast<double>(i) / params.sampleRate;
                
                // Mix of noise and 200Hz tone
                float noiseSample = generateNoise() * 0.7f;
                float toneSample = generateSine(200.0 * t) * 0.3f;
                
                float sample = noiseSample + toneSample;
                sample = applyEnvelope(sample, t, snareDuration, 0.001, 0.03, 0.2, 0.7);
                sample *= params.volume * drumParams_.velocity * drumParams_.snap;
                
                for (int ch = 0; ch < params.channels; ++ch) {
                    buffer.setSample(snareStart + i, ch, sample);
                }
            }
        }
    }
    
    return buffer;
}

AudioBuffer DrumGenerator::generateHiHat(const GeneratorParams& params, bool open) {
    AudioBuffer buffer(params.sampleRate, params.channels);
    size_t totalFrames = params.getTotalFrames();
    buffer.resize(totalFrames * params.channels);
    
    // Generate hi-hat on off-beats
    double beatLength = 60.0 / params.tempo;
    double hatDuration = open ? 0.3 : 0.05; // Open hat lasts longer
    
    for (int bar = 0; bar < params.bars; ++bar) {
        for (int subdivision = 0; subdivision < params.beatsPerBar * 2; ++subdivision) {
            double hatTime = bar * params.getBarLengthSeconds() + subdivision * (beatLength * 0.5);
            
            size_t hatSamples = static_cast<size_t>(hatDuration * params.sampleRate);
            size_t hatStart = static_cast<size_t>(hatTime * params.sampleRate);
            
            for (size_t i = 0; i < hatSamples && (hatStart + i) < totalFrames; ++i) {
                double t = static_cast<double>(i) / params.sampleRate;
                
                // High-frequency filtered noise
                float sample = generateNoise();
                
                // Simple high-pass filter effect (emphasize high frequencies)
                sample *= (1.0f + 3.0f * drumParams_.tone);
                
                sample = applyEnvelope(sample, t, hatDuration, 0.001, 0.02, 0.1, 0.9);
                sample *= params.volume * drumParams_.velocity * 0.6f;
                
                for (int ch = 0; ch < params.channels; ++ch) {
                    buffer.setSample(hatStart + i, ch, sample);
                }
            }
        }
    }
    
    return buffer;
}

AudioBuffer DrumGenerator::generateFullKit(const GeneratorParams& params) {
    // Generate each drum component and mix them
    DrumParams kickParams = drumParams_; kickParams.type = DrumType::Kick;
    DrumParams snareParams = drumParams_; snareParams.type = DrumType::Snare;  
    DrumParams hatParams = drumParams_; hatParams.type = DrumType::HiHat;
    
    DrumGenerator kickGen(kickParams);
    DrumGenerator snareGen(snareParams);
    DrumGenerator hatGen(hatParams);
    
    auto kickBuffer = kickGen.generate(params);
    auto snareBuffer = snareGen.generate(params);
    auto hatBuffer = hatGen.generate(params);
    
    // Mix all components
    AudioBuffer mixedBuffer(params.sampleRate, params.channels);
    size_t totalSamples = kickBuffer.samples.size();
    mixedBuffer.samples.resize(totalSamples);
    
    for (size_t i = 0; i < totalSamples; ++i) {
        float mixed = kickBuffer.samples[i] + snareBuffer.samples[i] + hatBuffer.samples[i];
        mixedBuffer.samples[i] = std::max(-1.0f, std::min(1.0f, mixed)); // Basic limiting
    }
    
    mixedBuffer.lengthSeconds = params.getTotalLengthSeconds();
    return mixedBuffer;
}

// BassGenerator implementation
BassGenerator::BassGenerator(const BassParams& bassParams)
    : bassParams_(bassParams) {}

AudioBuffer BassGenerator::generate(const GeneratorParams& params) {
    if (!validateParams(params)) {
        return AudioBuffer(params.sampleRate, params.channels);
    }
    
    switch (bassParams_.type) {
        case BassType::SubBass:      return generateSubBass(params);
        case BassType::ElectricBass: return generateElectricBass(params);
        case BassType::SynthBass:    return generateSynthBass(params);
        case BassType::AcousticBass: return generateAcousticBass(params);
        default:                     return generateSynthBass(params);
    }
}

AudioBuffer BassGenerator::generateSynthBass(const GeneratorParams& params) {
    AudioBuffer buffer(params.sampleRate, params.channels);
    size_t totalFrames = params.getTotalFrames();
    buffer.resize(totalFrames * params.channels);
    
    // Generate bass pattern
    auto bassNotes = generateBassPattern(params);
    
    // Render each note
    for (const auto& note : bassNotes) {
        renderBassNote(buffer, note, params);
    }
    
    return buffer;
}

AudioBuffer BassGenerator::generateSubBass(const GeneratorParams& params) {
    AudioBuffer buffer(params.sampleRate, params.channels);
    size_t totalFrames = params.getTotalFrames();
    buffer.resize(totalFrames * params.channels);
    
    // Simple sub bass - sine wave at very low frequency
    double beatLength = 60.0 / params.tempo;
    
    for (int bar = 0; bar < params.bars; ++bar) {
        for (int beat = 0; beat < params.beatsPerBar; ++beat) {
            double noteTime = bar * params.getBarLengthSeconds() + beat * beatLength;
            double noteDuration = beatLength * 0.8; // Slightly shorter than beat
            
            float freq = midiNoteToFreq(bassParams_.rootNote - 12); // One octave lower
            size_t noteStart = static_cast<size_t>(noteTime * params.sampleRate);
            size_t noteSamples = static_cast<size_t>(noteDuration * params.sampleRate);
            
            for (size_t i = 0; i < noteSamples && (noteStart + i) < totalFrames; ++i) {
                double t = static_cast<double>(i) / params.sampleRate;
                double phase = freq * t;
                
                float sample = generateSine(phase);
                sample = applyEnvelope(sample, t, noteDuration, bassParams_.attack, 0.1, 0.8, bassParams_.release);
                sample *= params.volume * 0.8f;
                
                for (int ch = 0; ch < params.channels; ++ch) {
                    size_t index = (noteStart + i) * params.channels + ch;
                    if (index < buffer.samples.size()) {
                        buffer.samples[index] += sample; // Add to existing content
                    }
                }
            }
        }
    }
    
    return buffer;
}

std::vector<BassGenerator::BassNote> BassGenerator::generateBassPattern(const GeneratorParams& params) {
    std::vector<BassNote> notes;
    double beatLength = 60.0 / params.tempo;
    
    for (int bar = 0; bar < params.bars; ++bar) {
        double barStart = bar * params.getBarLengthSeconds();
        
        switch (bassParams_.pattern) {
            case Pattern::FourOnFloor:
                // Bass on every beat
                for (int beat = 0; beat < params.beatsPerBar; ++beat) {
                    notes.push_back({
                        barStart + beat * beatLength,
                        bassParams_.rootNote,
                        0.8f,
                        beatLength * 0.8
                    });
                }
                break;
                
            case Pattern::Syncopated:
                // Bass on beats 1 and 3.5
                notes.push_back({barStart, bassParams_.rootNote, 0.9f, beatLength * 0.8});
                if (params.beatsPerBar >= 4) {
                    notes.push_back({barStart + 2.5 * beatLength, bassParams_.rootNote + 7, 0.7f, beatLength * 0.4});
                }
                break;
                
            case Pattern::Walking:
                // Walking bass line
                float walkingNotes[] = {0, 2, 4, 5}; // Scale degrees
                for (int beat = 0; beat < std::min(params.beatsPerBar, 4); ++beat) {
                    notes.push_back({
                        barStart + beat * beatLength,
                        bassParams_.rootNote + walkingNotes[beat],
                        0.7f,
                        beatLength * 0.9
                    });
                }
                break;
                
            default:
                // Default to four-on-floor
                for (int beat = 0; beat < params.beatsPerBar; ++beat) {
                    notes.push_back({
                        barStart + beat * beatLength,
                        bassParams_.rootNote,
                        0.8f,
                        beatLength * 0.8
                    });
                }
                break;
        }
    }
    
    return notes;
}

void BassGenerator::renderBassNote(AudioBuffer& buffer, const BassNote& note, const GeneratorParams& params) {
    float freq = midiNoteToFreq(note.note);
    size_t noteStart = static_cast<size_t>(note.time * params.sampleRate);
    size_t noteSamples = static_cast<size_t>(note.duration * params.sampleRate);
    size_t totalFrames = buffer.getFrameCount();
    
    float filterState = 0.0f;
    
    for (size_t i = 0; i < noteSamples && (noteStart + i) < totalFrames; ++i) {
        double t = static_cast<double>(i) / params.sampleRate;
        double phase = freq * t;
        
        // Generate saw wave for synthetic bass
        float sample = generateSaw(phase);
        
        // Apply low-pass filter
        sample = applyLowPassFilter(sample, filterState, bassParams_.cutoff);
        
        // Apply envelope
        sample = applyEnvelope(sample, t, note.duration, bassParams_.attack, 0.1, 0.7, bassParams_.release);
        sample *= params.volume * note.velocity;
        
        for (int ch = 0; ch < params.channels; ++ch) {
            size_t index = (noteStart + i) * params.channels + ch;
            if (index < buffer.samples.size()) {
                buffer.samples[index] += sample; // Add to existing content
            }
        }
    }
}

float BassGenerator::midiNoteToFreq(float midiNote) const {
    return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}

float BassGenerator::applyLowPassFilter(float sample, float& filterState, float cutoff) const {
    // Simple one-pole low-pass filter
    float alpha = cutoff * cutoff; // Simple cutoff mapping
    filterState += alpha * (sample - filterState);
    return filterState;
}

// Implement other bass types with simpler versions
AudioBuffer BassGenerator::generateElectricBass(const GeneratorParams& params) {
    // Reuse synth bass for now - could be enhanced with different waveform
    return generateSynthBass(params);
}

AudioBuffer BassGenerator::generateAcousticBass(const GeneratorParams& params) {
    // Reuse synth bass for now - could be enhanced with different envelope/timbre
    return generateSynthBass(params);
}

// Factory implementation
std::unique_ptr<AudioGenerator> GeneratorFactory::createDrumGenerator(const DrumGenerator::DrumParams& params) {
    return std::make_unique<DrumGenerator>(params);
}

std::unique_ptr<AudioGenerator> GeneratorFactory::createBassGenerator(const BassGenerator::BassParams& params) {
    return std::make_unique<BassGenerator>(params);
}

std::vector<std::string> GeneratorFactory::getAvailableGenerators() {
    return {"DrumGenerator", "BassGenerator"};
}

std::unique_ptr<AudioGenerator> GeneratorFactory::createGenerator(const std::string& name) {
    if (name == "DrumGenerator") {
        return createDrumGenerator();
    } else if (name == "BassGenerator") {
        return createBassGenerator();
    }
    return nullptr;
}

} // namespace mixmind::audio::generators