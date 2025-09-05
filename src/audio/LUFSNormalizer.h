#pragma once

#include <vector>
#include <string>
#include <memory>

// Forward declare libebur128 types to avoid including the header in public API
struct ebur128_state;

namespace mixmind::audio {

// LUFS measurement results
struct LUFSMeasurement {
    double integratedLoudness = -70.0;    // LUFS (EBU R128 integrated)
    double shortTermLoudness = -70.0;     // LUFS (3-second window)  
    double momentaryLoudness = -70.0;     // LUFS (400ms window)
    double loudnessRange = 0.0;           // LU (dynamic range)
    double truePeak = -70.0;              // dBTP (true peak level)
    
    bool isValid() const {
        return integratedLoudness > -70.0; // EBU R128 minimum measurement
    }
    
    // Check if measurements meet broadcast standards
    bool meetsBroadcastStandards() const {
        return integratedLoudness >= -24.0 && integratedLoudness <= -22.0 && // EBU R128
               truePeak <= -1.0;  // Prevent clipping
    }
    
    // Check if measurements meet streaming standards
    bool meetsStreamingStandards(double targetLUFS = -14.0) const {
        return std::abs(integratedLoudness - targetLUFS) <= 0.5; // ±0.5 LU tolerance
    }
};

// Audio buffer for LUFS processing
struct AudioData {
    std::vector<float> samples;
    int sampleRate;
    int channels;
    size_t frameCount;
    
    AudioData(int sr = 44100, int ch = 2) 
        : sampleRate(sr), channels(ch), frameCount(0) {}
    
    void resize(size_t frames) {
        frameCount = frames;
        samples.resize(frames * channels);
    }
    
    // Get interleaved sample data
    const float* data() const { return samples.data(); }
    float* data() { return samples.data(); }
    
    // Get sample at specific frame and channel
    float getSample(size_t frame, int channel) const {
        size_t index = frame * channels + channel;
        return (index < samples.size()) ? samples[index] : 0.0f;
    }
    
    void setSample(size_t frame, int channel, float value) {
        size_t index = frame * channels + channel;
        if (index < samples.size()) {
            samples[index] = value;
        }
    }
};

// LUFS normalization configuration
struct NormalizationConfig {
    double targetLUFS = -14.0;           // Target integrated loudness (streaming standard)
    double maxTruePeak = -1.0;           // Maximum true peak to prevent clipping
    bool preserveDynamics = true;        // Preserve dynamic range during normalization
    double maxGainReduction = -6.0;      // Maximum gain reduction in dB
    double maxGainIncrease = 12.0;       // Maximum gain increase in dB
    
    // Gating parameters (EBU R128 standard)
    double absoluteThreshold = -70.0;    // Absolute gating threshold
    double relativeThreshold = -10.0;    // Relative gating threshold (relative to ungated loudness)
    
    // Validation
    bool isValid() const {
        return targetLUFS >= -70.0 && targetLUFS <= 0.0 &&
               maxTruePeak >= -6.0 && maxTruePeak <= 0.0 &&
               maxGainReduction <= 0.0 && maxGainIncrease >= 0.0;
    }
};

// Main LUFS normalizer class
class LUFSNormalizer {
public:
    LUFSNormalizer();
    ~LUFSNormalizer();
    
    // Non-copyable due to libebur128 state
    LUFSNormalizer(const LUFSNormalizer&) = delete;
    LUFSNormalizer& operator=(const LUFSNormalizer&) = delete;
    
    // Initialize for specific audio format
    bool initialize(int sampleRate, int channels);
    
    // Check if properly initialized
    bool isInitialized() const { return state_ != nullptr; }
    
    // Reset state for new measurement
    void reset();
    
    // Add audio data for measurement (accumulative)
    bool addFrames(const float* audioData, size_t frameCount);
    bool addFrames(const AudioData& audio);
    
    // Get current LUFS measurements
    LUFSMeasurement getCurrentMeasurement() const;
    
    // Measure LUFS of complete audio buffer (convenience method)
    LUFSMeasurement measureLUFS(const AudioData& audio);
    
    // Normalize audio to target LUFS
    bool normalize(AudioData& audio, const NormalizationConfig& config = {});
    
    // Calculate required gain to reach target LUFS
    double calculateNormalizationGain(const LUFSMeasurement& measurement, 
                                    const NormalizationConfig& config) const;
    
    // Apply gain to audio data
    static void applyGain(AudioData& audio, double gainDb);
    
    // Utility functions
    static double lufsToDb(double lufs) { return lufs; } // LUFS is already in dB
    static double dbToLufs(double db) { return db; }     // For consistency
    
    // Convert between linear gain and dB
    static double dbToLinear(double db) { return std::pow(10.0, db / 20.0); }
    static double linearToDb(double linear) { return 20.0 * std::log10(linear); }
    
    // Get library version info
    static std::string getLibEBUR128Version();
    
    // Validate audio for LUFS measurement
    static bool isValidForMeasurement(const AudioData& audio);
    
private:
    ebur128_state* state_;
    int sampleRate_;
    int channels_;
    bool initialized_;
    
    // Internal measurement helpers
    bool getMeasurement(int mode, double* result) const;
    bool updateTruePeak(const AudioData& audio, LUFSMeasurement& measurement) const;
    
    // Gain calculation helpers
    bool exceedsGainLimits(double gainDb, const NormalizationConfig& config) const;
    double clampGain(double gainDb, const NormalizationConfig& config) const;
};

// Convenience functions for common use cases
namespace lufs {
    
    // Quick LUFS measurement
    LUFSMeasurement measure(const AudioData& audio);
    
    // Quick normalization to streaming standard (-14 LUFS)
    bool normalizeToStreaming(AudioData& audio);
    
    // Quick normalization to broadcast standard (-23 LUFS)
    bool normalizeToBroadcast(AudioData& audio);
    
    // Batch process multiple audio files
    struct BatchResult {
        std::string filename;
        LUFSMeasurement beforeMeasurement;
        LUFSMeasurement afterMeasurement;
        double appliedGain;
        bool success;
        std::string errorMessage;
    };
    
    std::vector<BatchResult> batchNormalize(
        const std::vector<std::string>& filePaths,
        const NormalizationConfig& config = {}
    );
    
    // Analyze loudness without modification
    struct LoudnessAnalysis {
        LUFSMeasurement measurement;
        bool meetsStreamingStandard;
        bool meetsBroadcastStandard;
        double recommendedGain;
        std::string recommendation;
    };
    
    LoudnessAnalysis analyzeLoudness(const AudioData& audio, double targetLUFS = -14.0);
}

} // namespace mixmind::audio