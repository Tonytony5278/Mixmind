#include "TimeStretchService.h"
#include "../core/async.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <random>
#include <sstream>

namespace mixmind::services {

// ============================================================================
// TimeStretchService Implementation
// ============================================================================

TimeStretchService::TimeStretchService() {
    initializeBuiltInPresets();
}

TimeStretchService::~TimeStretchService() {
    if (isInitialized_.load()) {
        shutdown().get(); // Block until shutdown complete
    }
}

// ========================================================================
// IOSSService Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> TimeStretchService::initialize() {
    return core::getGlobalThreadPool().executeAsyncVoid([this]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(processorMutex_);
        
        if (isInitialized_.load()) {
            return core::VoidResult::success();
        }
        
        // Initialize SoundTouch (always available)
        auto soundTouchResult = initializeSoundTouch();
        if (!soundTouchResult) {
            lastError_ = "Failed to initialize SoundTouch: " + soundTouchResult.getErrorMessage();
            return soundTouchResult;
        }
        
        // Try to initialize RubberBand (optional)
        auto rubberBandResult = initializeRubberBand(44100, 2); // Default settings
        if (!rubberBandResult) {
            // RubberBand failed, but this is non-fatal - fall back to SoundTouch
            currentEngine_ = StretchEngine::SoundTouch;
        }
        
        // Reset performance metrics
        resetPerformanceMetrics();
        
        isInitialized_.store(true);
        
        return core::VoidResult::success();
    }, "Initializing TimeStretchService");
}

core::AsyncResult<core::VoidResult> TimeStretchService::shutdown() {
    return core::getGlobalThreadPool().executeAsyncVoid([this]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(processorMutex_);
        
        if (!isInitialized_.load()) {
            return core::VoidResult::success();
        }
        
        // Stop streaming if active
        if (isStreamingActive_.load()) {
            stopStreaming();
        }
        
        // Cleanup processors
        cleanupProcessors();
        
        isInitialized_.store(false);
        
        return core::VoidResult::success();
    }, "Shutting down TimeStretchService");
}

bool TimeStretchService::isInitialized() const {
    return isInitialized_.load();
}

std::string TimeStretchService::getServiceName() const {
    return "TimeStretchService";
}

std::string TimeStretchService::getServiceVersion() const {
    return "2.0.0";
}

TimeStretchService::ServiceInfo TimeStretchService::getServiceInfo() const {
    ServiceInfo info;
    info.name = getServiceName();
    info.version = getServiceVersion();
    info.description = "Professional audio time stretching and pitch shifting service";
    
    // Get library versions
    std::ostringstream libraryVersions;
    libraryVersions << "SoundTouch " << SOUNDTOUCH_VERSION;
    
#ifdef RUBBERBAND_ENABLED
    if (rubberBand_) {
        libraryVersions << ", RubberBand " << rubberBand_->getEngineVersion();
    }
#endif
    
    info.libraryVersion = libraryVersions.str();
    info.isInitialized = isInitialized_.load();
    info.isThreadSafe = true;
    
    info.supportedFormats = {"wav", "aiff", "flac", "mp3", "ogg", "m4a"};
    
    info.capabilities = {
        "time_stretching",
        "pitch_shifting", 
        "formant_preservation",
        "real_time_processing",
        "batch_processing",
        "content_analysis",
        "quality_presets"
    };
    
    if (rubberBand_) {
        info.capabilities.push_back("high_quality_stretching");
        info.capabilities.push_back("transient_preservation");
    }
    
    return info;
}

core::VoidResult TimeStretchService::configure(const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    for (const auto& [key, value] : config) {
        config_[key] = value;
        
        // Apply configuration changes
        if (key == "default_engine") {
            if (value == "soundtouch") {
                setStretchEngine(StretchEngine::SoundTouch);
            } else if (value == "rubberband") {
                setStretchEngine(StretchEngine::RubberBand);
            } else if (value == "auto") {
                setStretchEngine(StretchEngine::Automatic);
            }
        } else if (key == "default_quality") {
            if (value == "draft") {
                setQualityPreset(QualityPreset::Draft);
            } else if (value == "low") {
                setQualityPreset(QualityPreset::Low);
            } else if (value == "standard") {
                setQualityPreset(QualityPreset::Standard);
            } else if (value == "high") {
                setQualityPreset(QualityPreset::High);
            } else if (value == "premium") {
                setQualityPreset(QualityPreset::Premium);
            }
        } else if (key == "auto_optimization") {
            setAutoOptimizationEnabled(value == "true" || value == "1");
        } else if (key == "formant_preservation") {
            setFormantPreservationEnabled(value == "true" || value == "1");
        }
    }
    
    return core::VoidResult::success();
}

std::optional<std::string> TimeStretchService::getConfigValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = config_.find(key);
    if (it != config_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

core::VoidResult TimeStretchService::resetConfiguration() {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    config_.clear();
    
    // Reset to default values
    setStretchEngine(StretchEngine::SoundTouch);
    setQualityPreset(QualityPreset::Standard);
    setProcessingMode(ProcessingMode::Realtime);
    setAutoOptimizationEnabled(false);
    setFormantPreservationEnabled(false);
    resetTimeAndPitch();
    
    return core::VoidResult::success();
}

bool TimeStretchService::isHealthy() const {
    return isInitialized_.load() && 
           (soundTouch_ != nullptr || rubberBand_ != nullptr) &&
           lastError_.empty();
}

std::string TimeStretchService::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

core::AsyncResult<core::VoidResult> TimeStretchService::runSelfTest() {
    return core::getGlobalThreadPool().executeAsyncVoid([this]() -> core::VoidResult {
        
        // Test basic initialization
        if (!isInitialized_.load()) {
            lastError_ = "Service not initialized";
            return core::VoidResult::error(
                core::ErrorCode::AudioDeviceError,
                core::ErrorCategory::audio(),
                "TimeStretchService not initialized"
            );
        }
        
        // Create test audio buffer
        core::FloatAudioBuffer testBuffer;
        testBuffer.resize(2); // Stereo
        testBuffer[0].resize(1024);
        testBuffer[1].resize(1024);
        
        // Generate test sine wave
        const double frequency = 440.0; // A4
        const double sampleRate = 44100.0;
        const double twoPi = 2.0 * M_PI;
        
        for (size_t i = 0; i < 1024; ++i) {
            double sample = std::sin(twoPi * frequency * i / sampleRate);
            testBuffer[0][i] = static_cast<float>(sample);
            testBuffer[1][i] = static_cast<float>(sample);
        }
        
        // Test SoundTouch processing
        if (soundTouch_) {
            setStretchEngine(StretchEngine::SoundTouch);
            setTimeRatio(1.5); // 1.5x speed
            
            core::FloatAudioBuffer outputBuffer;
            outputBuffer.resize(2);
            outputBuffer[0].resize(1024);
            outputBuffer[1].resize(1024);
            
            auto result = processBuffer(testBuffer, outputBuffer, static_cast<core::SampleRate>(sampleRate));
            if (!result) {
                lastError_ = "SoundTouch self-test failed: " + result.getErrorMessage();
                return result;
            }
        }
        
        // Test RubberBand processing if available
        if (rubberBand_) {
            setStretchEngine(StretchEngine::RubberBand);
            setTimeRatio(0.8); // 0.8x speed
            setPitchShift(2.0); // 2 semitones up
            
            core::FloatAudioBuffer outputBuffer;
            outputBuffer.resize(2);
            outputBuffer[0].resize(1024);
            outputBuffer[1].resize(1024);
            
            auto result = processBuffer(testBuffer, outputBuffer, static_cast<core::SampleRate>(sampleRate));
            if (!result) {
                lastError_ = "RubberBand self-test failed: " + result.getErrorMessage();
                return result;
            }
        }
        
        // Reset to defaults
        resetTimeAndPitch();
        setStretchEngine(StretchEngine::SoundTouch);
        
        lastError_.clear();
        return core::VoidResult::success();
        
    }, "Running TimeStretchService self-test");
}

TimeStretchService::PerformanceMetrics TimeStretchService::getPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(performanceMetricsMutex_);
    return performanceMetrics_;
}

void TimeStretchService::resetPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(performanceMetricsMutex_);
    performanceMetrics_ = PerformanceMetrics{};
}

// ========================================================================
// IAudioProcessingService Implementation
// ========================================================================

core::VoidResult TimeStretchService::processBuffer(
    core::FloatAudioBuffer& buffer,
    core::SampleRate sampleRate) {
    
    // Process in-place by creating temporary output buffer
    core::FloatAudioBuffer outputBuffer;
    outputBuffer.resize(buffer.size());
    for (size_t ch = 0; ch < buffer.size(); ++ch) {
        outputBuffer[ch].resize(buffer[ch].size());
    }
    
    auto result = processBuffer(buffer, outputBuffer, sampleRate);
    if (result) {
        buffer = std::move(outputBuffer);
    }
    
    return result;
}

core::VoidResult TimeStretchService::processBuffer(
    const core::FloatAudioBuffer& inputBuffer,
    core::FloatAudioBuffer& outputBuffer,
    core::SampleRate sampleRate) {
    
    if (!isInitialized_.load()) {
        return core::VoidResult::error(
            core::ErrorCode::AudioDeviceError,
            core::ErrorCategory::audio(),
            "TimeStretchService not initialized"
        );
    }
    
    if (inputBuffer.empty() || inputBuffer[0].empty()) {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::audio(),
            "Empty input buffer"
        );
    }
    
    // Validate parameters
    auto validation = validateParameters();
    if (!validation) {
        return validation;
    }
    
    std::lock_guard<std::mutex> lock(processorMutex_);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    core::VoidResult result;
    
    // Choose processing engine
    StretchEngine engineToUse = currentEngine_;
    if (engineToUse == StretchEngine::Automatic) {
        engineToUse = getRecommendedEngine(contentType_);
    }
    
    // Process based on selected engine
    switch (engineToUse) {
        case StretchEngine::SoundTouch:
            result = processSoundTouch(inputBuffer, outputBuffer);
            break;
            
        case StretchEngine::RubberBand:
            if (rubberBand_) {
                result = processRubberBand(inputBuffer, outputBuffer);
            } else {
                // Fall back to SoundTouch
                result = processSoundTouch(inputBuffer, outputBuffer);
            }
            break;
            
        default:
            result = processSoundTouch(inputBuffer, outputBuffer);
            break;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto processingTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    updatePerformanceMetrics(processingTime, result.isSuccess());
    
    if (!result) {
        lastError_ = result.getErrorMessage();
    }
    
    return result;
}

core::VoidResult TimeStretchService::setParameters(const std::unordered_map<std::string, double>& parameters) {
    for (const auto& [key, value] : parameters) {
        if (key == "time_ratio") {
            setTimeRatio(value);
        } else if (key == "pitch_ratio") {
            setPitchRatio(value);
        } else if (key == "pitch_semitones") {
            setPitchShift(value);
        } else if (key == "formant_preservation") {
            setFormantPreservationEnabled(value > 0.5);
        } else if (key == "transient_preservation") {
            setTransientPreservation(value);
        } else if (key == "phase_coherence") {
            setPhaseCoherenceEnabled(value > 0.5);
        }
    }
    
    return core::VoidResult::success();
}

std::unordered_map<std::string, double> TimeStretchService::getParameters() const {
    std::unordered_map<std::string, double> params;
    
    params["time_ratio"] = timeRatio_.load();
    params["pitch_ratio"] = pitchRatio_.load();
    params["pitch_semitones"] = pitchRatioToSemitones(pitchRatio_.load());
    params["formant_preservation"] = formantPreservation_.load() ? 1.0 : 0.0;
    params["transient_preservation"] = transientPreservation_.load();
    params["phase_coherence"] = phaseCoherence_.load() ? 1.0 : 0.0;
    params["auto_optimization"] = autoOptimization_.load() ? 1.0 : 0.0;
    
    return params;
}

core::VoidResult TimeStretchService::resetState() {
    std::lock_guard<std::mutex> lock(processorMutex_);
    
    if (soundTouch_) {
        soundTouch_->clear();
        soundTouch_->flush();
    }
    
    if (rubberBand_) {
        rubberBand_->reset();
    }
    
    return core::VoidResult::success();
}

int32_t TimeStretchService::getLatencySamples() const {
    // Return maximum latency among active processors
    int32_t maxLatency = 0;
    
    if (soundTouch_ && currentEngine_ == StretchEngine::SoundTouch) {
        maxLatency = std::max(maxLatency, static_cast<int32_t>(soundTouch_->getSetting(SETTING_NOMINAL_OUTPUT_SEQUENCE)));
    }
    
    if (rubberBand_ && currentEngine_ == StretchEngine::RubberBand) {
        maxLatency = std::max(maxLatency, static_cast<int32_t>(rubberBand_->getLatency()));
    }
    
    return maxLatency;
}

// ========================================================================
// Time Stretching Engines
// ========================================================================

core::VoidResult TimeStretchService::setStretchEngine(StretchEngine engine) {
    // Validate engine availability
    if (engine == StretchEngine::RubberBand && !rubberBand_) {
        // Try to initialize RubberBand
        auto result = initializeRubberBand(streamingSampleRate_ > 0 ? streamingSampleRate_ : 44100, 
                                          streamingChannels_ > 0 ? streamingChannels_ : 2);
        if (!result) {
            // Fall back to SoundTouch
            currentEngine_ = StretchEngine::SoundTouch;
            return core::VoidResult::error(
                core::ErrorCode::NotSupported,
                core::ErrorCategory::audio(),
                "RubberBand not available, falling back to SoundTouch"
            );
        }
    }
    
    currentEngine_ = engine;
    return core::VoidResult::success();
}

TimeStretchService::StretchEngine TimeStretchService::getStretchEngine() const {
    return currentEngine_;
}

std::vector<TimeStretchService::StretchEngine> TimeStretchService::getAvailableEngines() const {
    std::vector<StretchEngine> engines = {StretchEngine::SoundTouch, StretchEngine::Automatic};
    
    if (rubberBand_) {
        engines.push_back(StretchEngine::RubberBand);
    }
    
    return engines;
}

// ========================================================================
// Time and Pitch Control
// ========================================================================

core::VoidResult TimeStretchService::setTimeRatio(double ratio) {
    if (ratio <= 0.0 || ratio > 10.0) {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::audio(),
            "Time ratio out of valid range (0.0, 10.0]"
        );
    }
    
    timeRatio_.store(ratio);
    
    // Update processors
    std::lock_guard<std::mutex> lock(processorMutex_);
    
    if (soundTouch_) {
        soundTouch_->setTempo(static_cast<float>(1.0 / ratio));
    }
    
    if (rubberBand_) {
        rubberBand_->setTimeRatio(ratio);
    }
    
    return core::VoidResult::success();
}

double TimeStretchService::getTimeRatio() const {
    return timeRatio_.load();
}

core::VoidResult TimeStretchService::setPitchShift(double semitones) {
    if (std::abs(semitones) > 48.0) {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::audio(),
            "Pitch shift out of valid range [-48, +48] semitones"
        );
    }
    
    double ratio = semitonesToPitchRatio(semitones);
    return setPitchRatio(ratio);
}

double TimeStretchService::getPitchShift() const {
    return pitchRatioToSemitones(pitchRatio_.load());
}

core::VoidResult TimeStretchService::setPitchRatio(double ratio) {
    if (ratio <= 0.0 || ratio > 16.0) {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::audio(),
            "Pitch ratio out of valid range (0.0, 16.0]"
        );
    }
    
    pitchRatio_.store(ratio);
    
    // Update processors
    std::lock_guard<std::mutex> lock(processorMutex_);
    
    if (soundTouch_) {
        soundTouch_->setPitch(static_cast<float>(ratio));
    }
    
    if (rubberBand_) {
        rubberBand_->setPitchScale(ratio);
    }
    
    return core::VoidResult::success();
}

double TimeStretchService::getPitchRatio() const {
    return pitchRatio_.load();
}

core::VoidResult TimeStretchService::setTimeAndPitchRatios(double timeRatio, double pitchRatio) {
    auto timeResult = setTimeRatio(timeRatio);
    if (!timeResult) {
        return timeResult;
    }
    
    auto pitchResult = setPitchRatio(pitchRatio);
    if (!pitchResult) {
        return pitchResult;
    }
    
    return core::VoidResult::success();
}

core::VoidResult TimeStretchService::resetTimeAndPitch() {
    return setTimeAndPitchRatios(1.0, 1.0);
}

// ========================================================================
// Internal Implementation
// ========================================================================

core::VoidResult TimeStretchService::initializeSoundTouch() {
    try {
        soundTouch_ = std::make_unique<soundtouch::SoundTouch>();
        
        // Configure default settings
        soundTouch_->setSampleRate(44100);
        soundTouch_->setChannels(2);
        
        // Apply current settings
        soundTouch_->setSetting(SETTING_USE_AA_FILTER, soundTouchSettings_.useAntiAliasing ? 1 : 0);
        soundTouch_->setSetting(SETTING_USE_QUICKSEEK, soundTouchSettings_.useQuickSeek ? 1 : 0);
        
        if (soundTouchSettings_.sequenceMs > 0) {
            soundTouch_->setSetting(SETTING_SEQUENCE_MS, soundTouchSettings_.sequenceMs);
        }
        if (soundTouchSettings_.seekWindowMs > 0) {
            soundTouch_->setSetting(SETTING_SEEKWINDOW_MS, soundTouchSettings_.seekWindowMs);
        }
        if (soundTouchSettings_.overlapMs > 0) {
            soundTouch_->setSetting(SETTING_OVERLAP_MS, soundTouchSettings_.overlapMs);
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::AudioDeviceError,
            core::ErrorCategory::audio(),
            std::string("Failed to initialize SoundTouch: ") + e.what()
        );
    }
}

core::VoidResult TimeStretchService::initializeRubberBand(core::SampleRate sampleRate, int32_t channels) {
#ifdef RUBBERBAND_ENABLED
    try {
        rubberBand_ = std::make_unique<RubberBand::RubberBandStretcher>(
            sampleRate, 
            channels, 
            rubberBandSettings_.options,
            1.0, // Initial time ratio
            1.0  // Initial pitch scale
        );
        
        if (rubberBandSettings_.debugLevel > 0) {
            rubberBand_->setDebugLevel(rubberBandSettings_.debugLevel);
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::AudioDeviceError,
            core::ErrorCategory::audio(),
            std::string("Failed to initialize RubberBand: ") + e.what()
        );
    }
#else
    return core::VoidResult::error(
        core::ErrorCode::NotSupported,
        core::ErrorCategory::audio(),
        "RubberBand not available in this build"
    );
#endif
}

void TimeStretchService::cleanupProcessors() {
    soundTouch_.reset();
    
#ifdef RUBBERBAND_ENABLED
    rubberBand_.reset();
#endif
}

core::VoidResult TimeStretchService::processSoundTouch(
    const core::FloatAudioBuffer& inputBuffer,
    core::FloatAudioBuffer& outputBuffer) {
    
    if (!soundTouch_) {
        return core::VoidResult::error(
            core::ErrorCode::AudioDeviceError,
            core::ErrorCategory::audio(),
            "SoundTouch processor not available"
        );
    }
    
    try {
        // Configure SoundTouch for current buffer
        soundTouch_->setChannels(static_cast<uint32_t>(inputBuffer.size()));
        soundTouch_->clear();
        
        // Apply current time and pitch settings
        soundTouch_->setTempo(static_cast<float>(1.0 / timeRatio_.load()));
        soundTouch_->setPitch(static_cast<float>(pitchRatio_.load()));
        
        // Convert input buffer to interleaved format
        size_t inputSamples = inputBuffer[0].size();
        size_t channels = inputBuffer.size();
        std::vector<float> interleavedInput(inputSamples * channels);
        
        for (size_t sample = 0; sample < inputSamples; ++sample) {
            for (size_t ch = 0; ch < channels; ++ch) {
                interleavedInput[sample * channels + ch] = inputBuffer[ch][sample];
            }
        }
        
        // Process audio
        soundTouch_->putSamples(interleavedInput.data(), static_cast<uint32_t>(inputSamples));
        soundTouch_->flush();
        
        // Get output samples
        uint32_t outputSamples = soundTouch_->numSamples();
        if (outputSamples > 0) {
            std::vector<float> interleavedOutput(outputSamples * channels);
            uint32_t receivedSamples = soundTouch_->receiveSamples(interleavedOutput.data(), outputSamples);
            
            // Prepare output buffer
            outputBuffer.resize(channels);
            for (size_t ch = 0; ch < channels; ++ch) {
                outputBuffer[ch].resize(receivedSamples);
            }
            
            // Convert back to channel-separated format
            for (uint32_t sample = 0; sample < receivedSamples; ++sample) {
                for (size_t ch = 0; ch < channels; ++ch) {
                    outputBuffer[ch][sample] = interleavedOutput[sample * channels + ch];
                }
            }
        } else {
            // No output samples - create empty buffer
            outputBuffer.resize(channels);
            for (size_t ch = 0; ch < channels; ++ch) {
                outputBuffer[ch].clear();
            }
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::AudioDeviceError,
            core::ErrorCategory::audio(),
            std::string("SoundTouch processing error: ") + e.what()
        );
    }
}

core::VoidResult TimeStretchService::processRubberBand(
    const core::FloatAudioBuffer& inputBuffer,
    core::FloatAudioBuffer& outputBuffer) {
    
#ifdef RUBBERBAND_ENABLED
    if (!rubberBand_) {
        return core::VoidResult::error(
            core::ErrorCode::AudioDeviceError,
            core::ErrorCategory::audio(),
            "RubberBand processor not available"
        );
    }
    
    try {
        // Update RubberBand settings
        rubberBand_->setTimeRatio(timeRatio_.load());
        rubberBand_->setPitchScale(pitchRatio_.load());
        
        // Prepare input data
        size_t inputSamples = inputBuffer[0].size();
        size_t channels = inputBuffer.size();
        
        std::vector<const float*> inputPtrs(channels);
        for (size_t ch = 0; ch < channels; ++ch) {
            inputPtrs[ch] = inputBuffer[ch].data();
        }
        
        // Process audio
        rubberBand_->study(inputPtrs.data(), inputSamples, true); // Final chunk
        rubberBand_->process(inputPtrs.data(), inputSamples, true);
        
        // Get output
        size_t available = rubberBand_->available();
        if (available > 0) {
            outputBuffer.resize(channels);
            for (size_t ch = 0; ch < channels; ++ch) {
                outputBuffer[ch].resize(available);
            }
            
            std::vector<float*> outputPtrs(channels);
            for (size_t ch = 0; ch < channels; ++ch) {
                outputPtrs[ch] = outputBuffer[ch].data();
            }
            
            size_t retrieved = rubberBand_->retrieve(outputPtrs.data(), available);
            
            // Resize to actual retrieved samples
            for (size_t ch = 0; ch < channels; ++ch) {
                outputBuffer[ch].resize(retrieved);
            }
        } else {
            // No output samples
            outputBuffer.resize(channels);
            for (size_t ch = 0; ch < channels; ++ch) {
                outputBuffer[ch].clear();
            }
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::AudioDeviceError,
            core::ErrorCategory::audio(),
            std::string("RubberBand processing error: ") + e.what()
        );
    }
#else
    return core::VoidResult::error(
        core::ErrorCode::NotSupported,
        core::ErrorCategory::audio(),
        "RubberBand not available in this build"
    );
#endif
}

void TimeStretchService::updatePerformanceMetrics(double processingTime, bool success) {
    std::lock_guard<std::mutex> lock(performanceMetricsMutex_);
    
    performanceMetrics_.totalOperations++;
    if (!success) {
        performanceMetrics_.failedOperations++;
    }
    
    // Update timing statistics
    if (performanceMetrics_.totalOperations == 1) {
        performanceMetrics_.averageProcessingTime = processingTime;
        performanceMetrics_.peakProcessingTime = processingTime;
    } else {
        // Exponential moving average
        const double alpha = 0.1;
        performanceMetrics_.averageProcessingTime = 
            (1.0 - alpha) * performanceMetrics_.averageProcessingTime + alpha * processingTime;
        
        performanceMetrics_.peakProcessingTime = std::max(performanceMetrics_.peakProcessingTime, processingTime);
    }
}

core::VoidResult TimeStretchService::validateParameters() const {
    double timeRatio = timeRatio_.load();
    double pitchRatio = pitchRatio_.load();
    
    if (!areParametersValid(timeRatio, pitchRatio)) {
        return core::VoidResult::error(
            core::ErrorCode::InvalidParameter,
            core::ErrorCategory::audio(),
            "Invalid time stretch parameters"
        );
    }
    
    return core::VoidResult::success();
}

void TimeStretchService::initializeBuiltInPresets() {
    builtInPresets_.clear();
    
    // Speech presets
    {
        TimeStretchPreset preset;
        preset.name = "Speech Standard";
        preset.description = "Optimized for speech content with good intelligibility";
        preset.engine = StretchEngine::SoundTouch;
        preset.quality = QualityPreset::Standard;
        preset.contentType = ContentType::Speech;
        preset.processingMode = ProcessingMode::Realtime;
        preset.formantPreservation = true;
        preset.transientPreservation = 0.8;
        preset.phaseCoherence = true;
        builtInPresets_.push_back(preset);
    }
    
    // Music presets
    {
        TimeStretchPreset preset;
        preset.name = "Music High Quality";
        preset.description = "High quality stretching for musical content";
        preset.engine = StretchEngine::RubberBand;
        preset.quality = QualityPreset::High;
        preset.contentType = ContentType::Music;
        preset.processingMode = ProcessingMode::Offline;
        preset.formantPreservation = false;
        preset.transientPreservation = 0.9;
        preset.phaseCoherence = true;
        builtInPresets_.push_back(preset);
    }
    
    // Real-time preset
    {
        TimeStretchPreset preset;
        preset.name = "Real-time Preview";
        preset.description = "Fast processing for real-time preview";
        preset.engine = StretchEngine::SoundTouch;
        preset.quality = QualityPreset::Low;
        preset.contentType = ContentType::Unknown;
        preset.processingMode = ProcessingMode::Preview;
        preset.formantPreservation = false;
        preset.transientPreservation = 0.5;
        preset.phaseCoherence = false;
        builtInPresets_.push_back(preset);
    }
}

// ========================================================================
// Utility Functions
// ========================================================================

double TimeStretchService::timeRatioToTempoChange(double timeRatio) {
    return (1.0 / timeRatio - 1.0) * 100.0;
}

double TimeStretchService::tempoChangeToTimeRatio(double tempoChangePercent) {
    return 1.0 / (1.0 + tempoChangePercent / 100.0);
}

double TimeStretchService::semitonesToPitchRatio(double semitones) {
    return std::pow(2.0, semitones / 12.0);
}

double TimeStretchService::pitchRatioToSemitones(double pitchRatio) {
    return 12.0 * std::log2(pitchRatio);
}

bool TimeStretchService::areParametersValid(double timeRatio, double pitchRatio) {
    return timeRatio > 0.0 && timeRatio <= 10.0 &&
           pitchRatio > 0.0 && pitchRatio <= 16.0;
}

TimeStretchService::StretchEngine TimeStretchService::getRecommendedEngine(ContentType contentType) {
    switch (contentType) {
        case ContentType::Speech:
            return StretchEngine::SoundTouch; // Faster, good for speech
        case ContentType::Music:
        case ContentType::Harmonic:
            return StretchEngine::RubberBand; // Higher quality for music
        case ContentType::Percussion:
        case ContentType::Transient:
            return StretchEngine::RubberBand; // Better transient handling
        default:
            return StretchEngine::SoundTouch; // Safe default
    }
}

TimeStretchService::QualityPreset TimeStretchService::getRecommendedRealtimeQuality() {
    return QualityPreset::Low; // Balance between quality and performance
}

} // namespace mixmind::services