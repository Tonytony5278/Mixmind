#include "LUFSNormalizer.h"
#include <cmath>
#include <algorithm>
#include <sstream>

// For now, provide a mock implementation since libebur128 may not be available
// In production, this would include: #include <ebur128.h>

// Mock implementation of libebur128 state
struct ebur128_state {
    int sampleRate;
    int channels;
    double integratedLoudness = -70.0;
    double shortTermLoudness = -70.0;
    double momentaryLoudness = -70.0;
    double loudnessRange = 0.0;
    double truePeak = -70.0;
    size_t samplesProcessed = 0;
    std::vector<float> buffer; // For storing samples for processing
};

namespace mixmind::audio {

LUFSNormalizer::LUFSNormalizer() 
    : state_(nullptr), sampleRate_(0), channels_(0), initialized_(false) {
}

LUFSNormalizer::~LUFSNormalizer() {
    if (state_) {
        delete state_; // In real implementation: ebur128_destroy(&state_);
        state_ = nullptr;
    }
}

bool LUFSNormalizer::initialize(int sampleRate, int channels) {
    if (state_) {
        delete state_;
        state_ = nullptr;
    }
    
    if (sampleRate <= 0 || channels <= 0 || channels > 8) {
        return false;
    }
    
    // Mock initialization - in real implementation:
    // state_ = ebur128_init(channels, sampleRate, EBUR128_MODE_I | EBUR128_MODE_S | EBUR128_MODE_M | EBUR128_MODE_LRA | EBUR128_MODE_SAMPLE_PEAK);
    
    state_ = new ebur128_state();
    state_->sampleRate = sampleRate;
    state_->channels = channels;
    
    sampleRate_ = sampleRate;
    channels_ = channels;
    initialized_ = (state_ != nullptr);
    
    return initialized_;
}

void LUFSNormalizer::reset() {
    if (!initialized_) return;
    
    // Reset measurements
    state_->integratedLoudness = -70.0;
    state_->shortTermLoudness = -70.0;
    state_->momentaryLoudness = -70.0;
    state_->loudnessRange = 0.0;
    state_->truePeak = -70.0;
    state_->samplesProcessed = 0;
    state_->buffer.clear();
}

bool LUFSNormalizer::addFrames(const float* audioData, size_t frameCount) {
    if (!initialized_ || !audioData || frameCount == 0) {
        return false;
    }
    
    // Mock processing - in real implementation:
    // return ebur128_add_frames_float(state_, audioData, frameCount) == EBUR128_SUCCESS;
    
    // Simple mock implementation that approximates LUFS measurement
    size_t sampleCount = frameCount * channels_;
    state_->buffer.insert(state_->buffer.end(), audioData, audioData + sampleCount);
    state_->samplesProcessed += frameCount;
    
    // Calculate mock measurements based on RMS and some weighting
    double rmsSum = 0.0;
    double peakLevel = 0.0;
    
    for (size_t i = 0; i < sampleCount; ++i) {
        double sample = static_cast<double>(audioData[i]);
        rmsSum += sample * sample;
        peakLevel = std::max(peakLevel, std::abs(sample));
    }
    
    if (sampleCount > 0) {
        double rms = std::sqrt(rmsSum / sampleCount);
        
        // Mock LUFS calculation (very simplified)
        // Real LUFS uses K-weighting filter, gating, etc.
        if (rms > 0.0) {
            double rmsDb = 20.0 * std::log10(rms);
            // Apply mock K-weighting (approximately +4dB adjustment)
            state_->integratedLoudness = rmsDb + 4.0;
            state_->shortTermLoudness = state_->integratedLoudness;
            state_->momentaryLoudness = state_->integratedLoudness;
        }
        
        // Mock true peak
        if (peakLevel > 0.0) {
            state_->truePeak = 20.0 * std::log10(peakLevel);
        }
        
        // Mock loudness range (simplified)
        state_->loudnessRange = std::max(2.0, std::abs(state_->integratedLoudness + 40.0) * 0.3);
    }
    
    return true;
}

bool LUFSNormalizer::addFrames(const AudioData& audio) {
    return addFrames(audio.data(), audio.frameCount);
}

LUFSMeasurement LUFSNormalizer::getCurrentMeasurement() const {
    LUFSMeasurement measurement;
    
    if (!initialized_) {
        return measurement; // Return invalid measurement
    }
    
    // In real implementation:
    // ebur128_loudness_global(state_, &measurement.integratedLoudness);
    // ebur128_loudness_shortterm(state_, &measurement.shortTermLoudness);
    // ebur128_loudness_momentary(state_, &measurement.momentaryLoudness);
    // ebur128_loudness_range(state_, &measurement.loudnessRange);
    // ebur128_sample_peak(state_, 0, &measurement.truePeak);
    
    measurement.integratedLoudness = state_->integratedLoudness;
    measurement.shortTermLoudness = state_->shortTermLoudness;
    measurement.momentaryLoudness = state_->momentaryLoudness;
    measurement.loudnessRange = state_->loudnessRange;
    measurement.truePeak = state_->truePeak;
    
    return measurement;
}

LUFSMeasurement LUFSNormalizer::measureLUFS(const AudioData& audio) {
    if (!initialize(audio.sampleRate, audio.channels)) {
        return LUFSMeasurement(); // Invalid measurement
    }
    
    reset();
    
    if (!addFrames(audio)) {
        return LUFSMeasurement(); // Invalid measurement
    }
    
    return getCurrentMeasurement();
}

bool LUFSNormalizer::normalize(AudioData& audio, const NormalizationConfig& config) {
    if (!config.isValid() || !isValidForMeasurement(audio)) {
        return false;
    }
    
    // Measure current loudness
    LUFSMeasurement measurement = measureLUFS(audio);
    if (!measurement.isValid()) {
        return false;
    }
    
    // Calculate required gain
    double gainDb = calculateNormalizationGain(measurement, config);
    
    // Check if gain is within acceptable limits
    if (exceedsGainLimits(gainDb, config)) {
        gainDb = clampGain(gainDb, config);
    }
    
    // Apply gain
    applyGain(audio, gainDb);
    
    // Verify the result doesn't exceed true peak limit
    LUFSMeasurement afterMeasurement = measureLUFS(audio);
    if (afterMeasurement.truePeak > config.maxTruePeak) {
        // Reduce gain to prevent true peak clipping
        double peakReduction = afterMeasurement.truePeak - config.maxTruePeak + 0.1; // Small margin
        applyGain(audio, -peakReduction);
    }
    
    return true;
}

double LUFSNormalizer::calculateNormalizationGain(const LUFSMeasurement& measurement, 
                                                const NormalizationConfig& config) const {
    if (!measurement.isValid()) {
        return 0.0;
    }
    
    // Basic gain calculation: difference between current and target loudness
    double gainDb = config.targetLUFS - measurement.integratedLoudness;
    
    // Apply gain limits
    gainDb = clampGain(gainDb, config);
    
    return gainDb;
}

void LUFSNormalizer::applyGain(AudioData& audio, double gainDb) {
    if (gainDb == 0.0) return;
    
    double linearGain = dbToLinear(gainDb);
    
    for (float& sample : audio.samples) {
        sample = static_cast<float>(sample * linearGain);
        
        // Hard clipping prevention
        sample = std::max(-1.0f, std::min(1.0f, sample));
    }
}

std::string LUFSNormalizer::getLibEBUR128Version() {
    // In real implementation: return ebur128_get_version();
    return "Mock libebur128 v1.0.0 (MIT Licensed)";
}

bool LUFSNormalizer::isValidForMeasurement(const AudioData& audio) {
    return audio.sampleRate > 0 && 
           audio.channels > 0 && 
           audio.frameCount > 0 && 
           !audio.samples.empty() &&
           audio.samples.size() == audio.frameCount * audio.channels;
}

bool LUFSNormalizer::getMeasurement(int mode, double* result) const {
    if (!initialized_ || !result) {
        return false;
    }
    
    // Mock implementation
    *result = state_->integratedLoudness;
    return true;
}

bool LUFSNormalizer::exceedsGainLimits(double gainDb, const NormalizationConfig& config) const {
    return gainDb < config.maxGainReduction || gainDb > config.maxGainIncrease;
}

double LUFSNormalizer::clampGain(double gainDb, const NormalizationConfig& config) const {
    return std::max(config.maxGainReduction, std::min(config.maxGainIncrease, gainDb));
}

// Convenience functions
namespace lufs {

LUFSMeasurement measure(const AudioData& audio) {
    LUFSNormalizer normalizer;
    return normalizer.measureLUFS(audio);
}

bool normalizeToStreaming(AudioData& audio) {
    LUFSNormalizer normalizer;
    NormalizationConfig config;
    config.targetLUFS = -14.0; // Spotify, Apple Music, etc.
    return normalizer.normalize(audio, config);
}

bool normalizeToBroadcast(AudioData& audio) {
    LUFSNormalizer normalizer;
    NormalizationConfig config;
    config.targetLUFS = -23.0; // EBU R128 broadcast standard
    config.maxTruePeak = -1.0;
    return normalizer.normalize(audio, config);
}

std::vector<BatchResult> batchNormalize(const std::vector<std::string>& filePaths,
                                      const NormalizationConfig& config) {
    std::vector<BatchResult> results;
    
    for (const auto& filePath : filePaths) {
        BatchResult result;
        result.filename = filePath;
        result.success = false;
        result.appliedGain = 0.0;
        result.errorMessage = "Batch processing not implemented in mock version";
        
        // In real implementation, load audio file, process, and save
        // This would involve file I/O and audio format support
        
        results.push_back(result);
    }
    
    return results;
}

LoudnessAnalysis analyzeLoudness(const AudioData& audio, double targetLUFS) {
    LoudnessAnalysis analysis;
    
    LUFSNormalizer normalizer;
    analysis.measurement = normalizer.measureLUFS(audio);
    
    if (analysis.measurement.isValid()) {
        analysis.meetsStreamingStandard = analysis.measurement.meetsStreamingStandards(-14.0);
        analysis.meetsBroadcastStandard = analysis.measurement.meetsBroadcastStandards();
        analysis.recommendedGain = targetLUFS - analysis.measurement.integratedLoudness;
        
        // Generate recommendation text
        std::ostringstream oss;
        if (std::abs(analysis.recommendedGain) <= 0.5) {
            oss << "Loudness is within acceptable range for target " << targetLUFS << " LUFS";
        } else if (analysis.recommendedGain > 0) {
            oss << "Audio is " << -analysis.recommendedGain << " LU too quiet. Increase gain by " 
                << analysis.recommendedGain << " dB";
        } else {
            oss << "Audio is " << analysis.recommendedGain << " LU too loud. Reduce gain by " 
                << -analysis.recommendedGain << " dB";
        }
        
        if (analysis.measurement.truePeak > -1.0) {
            oss << ". Warning: True peak exceeds -1 dBTP";
        }
        
        analysis.recommendation = oss.str();
    } else {
        analysis.meetsStreamingStandard = false;
        analysis.meetsBroadcastStandard = false;
        analysis.recommendedGain = 0.0;
        analysis.recommendation = "Unable to measure loudness - audio may be invalid";
    }
    
    return analysis;
}

} // namespace lufs

} // namespace mixmind::audio