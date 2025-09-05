#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace mixmind::audio::generators {

// Audio sample data structure
struct AudioBuffer {
    std::vector<float> samples;
    int sampleRate;
    int channels;
    double lengthSeconds;
    
    AudioBuffer(int sr = 44100, int ch = 2) 
        : sampleRate(sr), channels(ch), lengthSeconds(0.0) {}
        
    void resize(size_t numSamples) {
        samples.resize(numSamples);
        lengthSeconds = static_cast<double>(numSamples / channels) / sampleRate;
    }
    
    size_t getFrameCount() const {
        return samples.size() / channels;
    }
    
    // Get sample at specific frame and channel
    float getSample(size_t frame, int channel) const {
        size_t index = frame * channels + channel;
        return (index < samples.size()) ? samples[index] : 0.0f;
    }
    
    // Set sample at specific frame and channel
    void setSample(size_t frame, int channel, float value) {
        size_t index = frame * channels + channel;
        if (index < samples.size()) {
            samples[index] = value;
        }
    }
    
    // RMS calculation for validation
    float getRMSLevel() const {
        if (samples.empty()) return 0.0f;
        
        double sum = 0.0;
        for (float sample : samples) {
            sum += sample * sample;
        }
        return static_cast<float>(std::sqrt(sum / samples.size()));
    }
    
    // Peak level calculation
    float getPeakLevel() const {
        float peak = 0.0f;
        for (float sample : samples) {
            peak = std::max(peak, std::abs(sample));
        }
        return peak;
    }
};

// Generator parameters
struct GeneratorParams {
    int sampleRate = 44100;
    int channels = 2;
    double tempo = 120.0;
    int bars = 8;
    int beatsPerBar = 4;
    double volume = 0.7; // 0.0 to 1.0
    
    // Derived properties
    double getBarLengthSeconds() const {
        return (60.0 / tempo) * beatsPerBar;
    }
    
    double getTotalLengthSeconds() const {
        return getBarLengthSeconds() * bars;
    }
    
    size_t getTotalFrames() const {
        return static_cast<size_t>(getTotalLengthSeconds() * sampleRate);
    }
};

// Abstract base class for audio generators
class AudioGenerator {
public:
    virtual ~AudioGenerator() = default;
    
    // Generate audio buffer
    virtual AudioBuffer generate(const GeneratorParams& params) = 0;
    
    // Generator metadata
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::vector<std::string> getTags() const = 0;
    
    // Cache management
    virtual std::string getCacheKey(const GeneratorParams& params) const;
    virtual bool supportsCaching() const { return true; }
    
    // Parameter validation
    virtual bool validateParams(const GeneratorParams& params) const;
    
    // Quality metrics
    struct QualityMetrics {
        float rmsLevel;
        float peakLevel; 
        bool hasClipping;
        float dynamicRange;
        double actualLength;
        bool meetsThreshold;
    };
    
    virtual QualityMetrics analyzeQuality(const AudioBuffer& buffer) const;
    
protected:
    // Utility functions for generators
    static float generateSine(double phase);
    static float generateSaw(double phase);
    static float generateSquare(double phase);
    static float generateNoise();
    static float applyEnvelope(float sample, double position, double length, 
                              double attack = 0.01, double decay = 0.1, 
                              double sustain = 0.7, double release = 0.2);
};

// Drum generator implementation  
class DrumGenerator : public AudioGenerator {
public:
    enum class DrumType {
        Kick,
        Snare, 
        HiHat,
        OpenHat,
        Crash,
        Tom,
        Kit // Full drum kit pattern
    };
    
    struct DrumParams {
        DrumType type = DrumType::Kit;
        float pitch = 1.0f;    // Pitch multiplier
        float snap = 0.5f;     // Snappiness (0-1)
        float tone = 0.5f;     // Tone control (0-1)
        bool swing = false;    // Swing timing
        float velocity = 0.8f; // Hit velocity
    };
    
    DrumGenerator(const DrumParams& drumParams = {});
    
    AudioBuffer generate(const GeneratorParams& params) override;
    std::string getName() const override { return "DrumGenerator"; }
    std::string getDescription() const override { return "Built-in drum machine with various drum sounds"; }
    std::vector<std::string> getTags() const override { return {"drums", "percussion", "rhythm"}; }
    
private:
    DrumParams drumParams_;
    
    // Individual drum sound generators
    AudioBuffer generateKick(const GeneratorParams& params);
    AudioBuffer generateSnare(const GeneratorParams& params);
    AudioBuffer generateHiHat(const GeneratorParams& params, bool open = false);
    AudioBuffer generateCrash(const GeneratorParams& params);
    AudioBuffer generateTom(const GeneratorParams& params);
    AudioBuffer generateFullKit(const GeneratorParams& params);
    
    // Drum pattern helpers
    struct DrumHit {
        double time;
        DrumType type;
        float velocity;
    };
    
    std::vector<DrumHit> generateDrumPattern(const GeneratorParams& params);
    void renderDrumHit(AudioBuffer& buffer, const DrumHit& hit, const GeneratorParams& params);
};

// Bass generator implementation
class BassGenerator : public AudioGenerator {
public:
    enum class BassType {
        SubBass,
        ElectricBass,
        SynthBass,
        AcousticBass
    };
    
    enum class Pattern {
        FourOnFloor,
        Syncopated,
        Walking,
        Arpeggiated,
        Custom
    };
    
    struct BassParams {
        BassType type = BassType::SynthBass;
        Pattern pattern = Pattern::FourOnFloor;
        float rootNote = 36.0f;    // MIDI note number (C2)
        float cutoff = 0.5f;       // Filter cutoff (0-1)
        float resonance = 0.2f;    // Filter resonance (0-1)
        float attack = 0.01f;      // Envelope attack
        float release = 0.3f;      // Envelope release
        std::vector<int> customPattern; // For Pattern::Custom
    };
    
    BassGenerator(const BassParams& bassParams = {});
    
    AudioBuffer generate(const GeneratorParams& params) override;
    std::string getName() const override { return "BassGenerator"; }
    std::string getDescription() const override { return "Built-in bass synthesizer with various bass types"; }
    std::vector<std::string> getTags() const override { return {"bass", "synthesizer", "low-end"}; }
    
private:
    BassParams bassParams_;
    
    // Bass sound generators
    AudioBuffer generateSubBass(const GeneratorParams& params);
    AudioBuffer generateElectricBass(const GeneratorParams& params);
    AudioBuffer generateSynthBass(const GeneratorParams& params);
    AudioBuffer generateAcousticBass(const GeneratorParams& params);
    
    // Pattern generators
    struct BassNote {
        double time;
        float note;     // MIDI note number
        float velocity;
        double duration;
    };
    
    std::vector<BassNote> generateBassPattern(const GeneratorParams& params);
    void renderBassNote(AudioBuffer& buffer, const BassNote& note, const GeneratorParams& params);
    
    // Synthesis components
    float midiNoteToFreq(float midiNote) const;
    float applyLowPassFilter(float sample, float& filterState, float cutoff) const;
};

// Factory for creating generators
class GeneratorFactory {
public:
    static std::unique_ptr<AudioGenerator> createDrumGenerator(const DrumGenerator::DrumParams& params = {});
    static std::unique_ptr<AudioGenerator> createBassGenerator(const BassGenerator::BassParams& params = {});
    
    // Get list of available generators
    static std::vector<std::string> getAvailableGenerators();
    static std::unique_ptr<AudioGenerator> createGenerator(const std::string& name);
};

} // namespace mixmind::audio::generators