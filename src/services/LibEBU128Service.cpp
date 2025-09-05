#include "LibEBU128Service.h"
#include <ebur128.h>  // Main libebur128 header
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace mixmind::services {

// ============================================================================
// LibEBU128Service Implementation
// ============================================================================

LibEBU128Service::LibEBU128Service() {
    // Initialize default configuration
    config_["sample_rate"] = "48000";
    config_["channels"] = "2";
    config_["analysis_modes"] = "all";
    config_["gating_enabled"] = "true";
    config_["max_analysis_duration"] = "3600"; // 1 hour max
}

LibEBU128Service::~LibEBU128Service() {
    if (isInitialized_.load()) {
        shutdown().get(); // Block until shutdown complete
    }
}

// ========================================================================
// IOSSService Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> LibEBU128Service::initialize() {
    return core::AsyncResult<core::VoidResult>::createResolved(
        core::core::Result<void>::fromLambda([this]() -> core::VoidResult {
            std::lock_guard<std::mutex> lock(ebuStateMutex_);
            
            if (isInitialized_.load()) {
                return core::core::Result<void>::success();
            }
            
            try {
                // Validate libebur128 is available
                int version = ebur128_get_version();
                if (version < EBUR128_VERSION_INT(1, 2, 0)) {
                    return core::core::Result<void>::error(
                        core::ErrorCode::UnsupportedVersion,
                        "libebur128 version too old, require >= 1.2.0"
                    );
                }
                
                // Initialize with default parameters for validation
                core::SampleRate defaultSampleRate = static_cast<core::SampleRate>(
                    std::stoi(config_["sample_rate"])
                );
                int32_t defaultChannels = std::stoi(config_["channels"]);
                
                auto result = initializeEBUState(defaultSampleRate, defaultChannels);
                if (!result.isSuccess()) {
                    return result;
                }
                
                // Clean up test state
                cleanupEBUState();
                
                isInitialized_ = true;
                return core::core::Result<void>::success();
                
            } catch (const std::exception& e) {
                lastError_ = "Failed to initialize libebur128: " + std::string(e.what());
                return core::core::Result<void>::error(
                    core::ErrorCode::InitializationFailed,
                    lastError_
                );
            }
        })
    );
}

core::AsyncResult<core::VoidResult> LibEBU128Service::shutdown() {
    return core::AsyncResult<core::VoidResult>::createResolved(
        core::core::Result<void>::fromLambda([this]() -> core::VoidResult {
            std::lock_guard<std::mutex> lock(ebuStateMutex_);
            
            if (!isInitialized_.load()) {
                return core::core::Result<void>::success();
            }
            
            try {
                // Cancel any ongoing analysis
                shouldCancel_ = true;
                
                // Stop realtime analysis
                if (isRealtimeActive_.load()) {
                    stopRealtimeAnalysis().get();
                }
                
                // Cleanup libebur128 state
                cleanupEBUState();
                
                // Clear results and metrics
                {
                    std::lock_guard<std::mutex> resultsLock(resultsMutex_);
                    analysisResults_.clear();
                }
                
                resetPerformanceMetrics();
                
                isInitialized_ = false;
                return core::core::Result<void>::success();
                
            } catch (const std::exception& e) {
                lastError_ = "Failed to shutdown libebur128: " + std::string(e.what());
                return core::core::Result<void>::error(
                    core::ErrorCode::ShutdownFailed,
                    lastError_
                );
            }
        })
    );
}

bool LibEBU128Service::isInitialized() const {
    return isInitialized_.load();
}

std::string LibEBU128Service::getServiceName() const {
    return "LibEBU128Service";
}

std::string LibEBU128Service::getServiceVersion() const {
    int version = ebur128_get_version();
    int major = (version >> 16) & 0xFF;
    int minor = (version >> 8) & 0xFF;
    int patch = version & 0xFF;
    
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

LibEBU128Service::ServiceInfo LibEBU128Service::getServiceInfo() const {
    ServiceInfo info;
    info.name = getServiceName();
    info.version = "1.0.0";  // Our wrapper version
    info.description = "LUFS and True Peak audio analysis using libebur128";
    info.libraryVersion = getServiceVersion();
    info.isInitialized = isInitialized();
    info.isThreadSafe = false;  // libebur128 is not thread-safe per instance
    
    info.supportedFormats = {"wav", "flac", "mp3", "aiff", "ogg"};
    info.capabilities = {
        "integrated_loudness", "momentary_loudness", "short_term_loudness",
        "loudness_range", "true_peak", "sample_peak", "ebu_r128_compliance",
        "atsc_a85_compliance", "realtime_analysis", "batch_processing"
    };
    
    return info;
}

core::VoidResult LibEBU128Service::configure(const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    for (const auto& [key, value] : config) {
        config_[key] = value;
    }
    
    return validateConfiguration();
}

std::optional<std::string> LibEBU128Service::getConfigValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    auto it = config_.find(key);
    return (it != config_.end()) ? std::make_optional(it->second) : std::nullopt;
}

core::VoidResult LibEBU128Service::resetConfiguration() {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.clear();
    
    // Restore defaults
    config_["sample_rate"] = "48000";
    config_["channels"] = "2";
    config_["analysis_modes"] = "all";
    config_["gating_enabled"] = "true";
    config_["max_analysis_duration"] = "3600";
    
    return core::core::Result<void>::success();
}

bool LibEBU128Service::isHealthy() const {
    return isInitialized() && lastError_.empty();
}

std::string LibEBU128Service::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

// ========================================================================
// IAudioAnalysisService Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> LibEBU128Service::analyzeBuffer(
    const core::FloatAudioBuffer& buffer,
    core::SampleRate sampleRate,
    core::ProgressCallback progress
) {
    return core::AsyncResult<core::VoidResult>::createAsync([this, buffer, sampleRate, progress]() -> core::VoidResult {
        if (!isInitialized()) {
            return core::core::Result<void>::error(
                core::ErrorCode::InvalidState,
                "Service not initialized"
            );
        }
        
        if (buffer.channels.empty()) {
            return core::core::Result<void>::error(
                core::ErrorCode::InvalidParameter,
                "Empty audio buffer"
            );
        }
        
        auto startTime = std::chrono::steady_clock::now();
        isAnalyzing_ = true;
        shouldCancel_ = false;
        
        try {
            std::lock_guard<std::mutex> lock(ebuStateMutex_);
            
            // Initialize EBU state for this analysis
            auto result = initializeEBUState(sampleRate, static_cast<int32_t>(buffer.channels.size()));
            if (!result.isSuccess()) {
                isAnalyzing_ = false;
                return result;
            }
            
            // Prepare interleaved audio data
            size_t frameCount = buffer.channels[0].size();
            int32_t channelCount = static_cast<int32_t>(buffer.channels.size());
            std::vector<float> interleavedSamples(frameCount * channelCount);
            
            // Interleave samples
            for (size_t frame = 0; frame < frameCount; ++frame) {
                if (shouldCancel_.load()) {
                    cleanupEBUState();
                    isAnalyzing_ = false;
                    return core::core::Result<void>::error(
                        core::ErrorCode::OperationCancelled,
                        "Analysis cancelled"
                    );
                }
                
                for (int32_t channel = 0; channel < channelCount; ++channel) {
                    interleavedSamples[frame * channelCount + channel] = buffer.channels[channel][frame];
                }
                
                // Report progress
                if (progress && frame % 1024 == 0) {
                    core::ProgressInfo info;
                    info.progress = static_cast<float>(frame) / frameCount;
                    info.message = "Analyzing audio buffer";
                    info.canCancel = true;
                    
                    if (!progress(info)) {
                        shouldCancel_ = true;
                    }
                }
            }
            
            // Add frames to libebur128
            int ebuResult = ebur128_add_frames_float(ebuState_, interleavedSamples.data(), frameCount);
            if (ebuResult != EBUR128_SUCCESS) {
                cleanupEBUState();
                isAnalyzing_ = false;
                return core::core::Result<void>::error(
                    convertEBUError(ebuResult),
                    "Failed to add frames to libebur128"
                );
            }
            
            // Compute measurements
            result = computeAllMeasurements();
            cleanupEBUState();
            
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            updatePerformanceMetrics(duration.count(), result.isSuccess());
            
            isAnalyzing_ = false;
            return result;
            
        } catch (const std::exception& e) {
            isAnalyzing_ = false;
            cleanupEBUState();
            
            std::string error = "Buffer analysis failed: " + std::string(e.what());
            lastError_ = error;
            
            return core::core::Result<void>::error(
                core::ErrorCode::ProcessingFailed,
                error
            );
        }
    });
}

core::AsyncResult<core::VoidResult> LibEBU128Service::analyzeFile(
    const std::string& filePath,
    core::ProgressCallback progress
) {
    return loadAndAnalyzeAudioFile(filePath, progress);
}

std::unordered_map<std::string, double> LibEBU128Service::getAnalysisResults() const {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    return analysisResults_;
}

void LibEBU128Service::clearResults() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    analysisResults_.clear();
}

bool LibEBU128Service::isAnalyzing() const {
    return isAnalyzing_.load();
}

core::VoidResult LibEBU128Service::cancelAnalysis() {
    shouldCancel_ = true;
    return core::core::Result<void>::success();
}

// ========================================================================
// EBU R128 Specific Methods
// ========================================================================

core::Result<double> LibEBU128Service::getIntegratedLoudness() const {
    std::lock_guard<std::mutex> lock(ebuStateMutex_);
    
    if (!ebuState_) {
        return core::Result<double>::error(
            core::ErrorCode::InvalidState,
            "No active analysis session"
        );
    }
    
    double loudness;
    int result = ebur128_loudness_global(ebuState_, &loudness);
    
    if (result != EBUR128_SUCCESS) {
        return core::Result<double>::error(
            convertEBUError(result),
            "Failed to get integrated loudness"
        );
    }
    
    return core::Result<double>::success(loudness);
}

core::Result<double> LibEBU128Service::getLoudnessRange() const {
    std::lock_guard<std::mutex> lock(ebuStateMutex_);
    
    if (!ebuState_) {
        return core::Result<double>::error(
            core::ErrorCode::InvalidState,
            "No active analysis session"
        );
    }
    
    double range;
    int result = ebur128_loudness_range(ebuState_, &range);
    
    if (result != EBUR128_SUCCESS) {
        return core::Result<double>::error(
            convertEBUError(result),
            "Failed to get loudness range"
        );
    }
    
    return core::Result<double>::success(range);
}

core::Result<std::vector<double>> LibEBU128Service::getTruePeaks() const {
    std::lock_guard<std::mutex> lock(ebuStateMutex_);
    
    if (!ebuState_) {
        return core::Result<std::vector<double>>::error(
            core::ErrorCode::InvalidState,
            "No active analysis session"
        );
    }
    
    unsigned int channels = ebur128_get_channels(ebuState_);
    std::vector<double> peaks(channels);
    
    for (unsigned int i = 0; i < channels; ++i) {
        double peak;
        int result = ebur128_true_peak(ebuState_, i, &peak);
        
        if (result != EBUR128_SUCCESS) {
            return core::Result<std::vector<double>>::error(
                convertEBUError(result),
                "Failed to get true peak for channel " + std::to_string(i)
            );
        }
        
        peaks[i] = 20.0 * std::log10(peak); // Convert to dBTP
    }
    
    return core::Result<std::vector<double>>::success(std::move(peaks));
}

// ========================================================================
// Internal Implementation
// ========================================================================

core::VoidResult LibEBU128Service::initializeEBUState(core::SampleRate sampleRate, int32_t channels) {
    if (ebuState_) {
        cleanupEBUState();
    }
    
    ebuState_ = ebur128_init(
        static_cast<unsigned int>(channels),
        static_cast<unsigned long>(sampleRate),
        analysisModes_
    );
    
    if (!ebuState_) {
        return core::core::Result<void>::error(
            core::ErrorCode::InitializationFailed,
            "Failed to initialize libebur128 state"
        );
    }
    
    currentSampleRate_ = sampleRate;
    currentChannels_ = channels;
    
    return core::core::Result<void>::success();
}

void LibEBU128Service::cleanupEBUState() {
    if (ebuState_) {
        ebur128_destroy(&ebuState_);
        ebuState_ = nullptr;
    }
}

core::VoidResult LibEBU128Service::computeAllMeasurements() {
    if (!ebuState_) {
        return core::core::Result<void>::error(
            core::ErrorCode::InvalidState,
            "No EBU state available"
        );
    }
    
    std::lock_guard<std::mutex> lock(resultsMutex_);
    analysisResults_.clear();
    
    try {
        // Integrated loudness
        if (analysisModes_ & EBUR128_MODE_I) {
            auto result = getIntegratedLoudness();
            if (result.isSuccess()) {
                analysisResults_["integrated_loudness"] = result.getValue();
            }
        }
        
        // Loudness range
        if (analysisModes_ & EBUR128_MODE_LRA) {
            auto result = getLoudnessRange();
            if (result.isSuccess()) {
                analysisResults_["loudness_range"] = result.getValue();
            }
        }
        
        // True peaks
        if (analysisModes_ & EBUR128_MODE_TRUE_PEAK) {
            auto result = getTruePeaks();
            if (result.isSuccess()) {
                auto peaks = result.getValue();
                double maxPeak = *std::max_element(peaks.begin(), peaks.end());
                analysisResults_["max_true_peak"] = maxPeak;
                
                for (size_t i = 0; i < peaks.size(); ++i) {
                    analysisResults_["true_peak_ch_" + std::to_string(i)] = peaks[i];
                }
            }
        }
        
        return core::core::Result<void>::success();
        
    } catch (const std::exception& e) {
        return core::core::Result<void>::error(
            core::ErrorCode::ProcessingFailed,
            "Failed to compute measurements: " + std::string(e.what())
        );
    }
}

core::ErrorCode LibEBU128Service::convertEBUError(int ebuError) const {
    switch (ebuError) {
        case EBUR128_SUCCESS:
            return core::ErrorCode::Success;
        case EBUR128_ERROR_NOMEM:
            return core::ErrorCode::OutOfMemory;
        case EBUR128_ERROR_INVALID_MODE:
        case EBUR128_ERROR_INVALID_CHANNEL_INDEX:
            return core::ErrorCode::InvalidParameter;
        case EBUR128_ERROR_NO_CHANGE:
            return core::ErrorCode::InvalidOperation;
        default:
            return core::ErrorCode::UnknownError;
    }
}

void LibEBU128Service::updatePerformanceMetrics(double processingTime, bool success) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    performanceMetrics_.totalOperations++;
    if (!success) {
        performanceMetrics_.failedOperations++;
    }
    
    if (performanceMetrics_.totalOperations == 1) {
        performanceMetrics_.averageProcessingTime = processingTime;
    } else {
        performanceMetrics_.averageProcessingTime = 
            (performanceMetrics_.averageProcessingTime * (performanceMetrics_.totalOperations - 1) + processingTime) 
            / performanceMetrics_.totalOperations;
    }
    
    if (processingTime > performanceMetrics_.peakProcessingTime) {
        performanceMetrics_.peakProcessingTime = processingTime;
    }
}

LibEBU128Service::PerformanceMetrics LibEBU128Service::getPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return performanceMetrics_;
}

void LibEBU128Service::resetPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    performanceMetrics_ = PerformanceMetrics{};
}

core::VoidResult LibEBU128Service::validateConfiguration() const {
    try {
        // Validate sample rate
        int sampleRate = std::stoi(config_.at("sample_rate"));
        if (sampleRate < 8000 || sampleRate > 192000) {
            return core::core::Result<void>::error(
                core::ErrorCode::InvalidParameter,
                "Invalid sample rate: " + std::to_string(sampleRate)
            );
        }
        
        // Validate channels
        int channels = std::stoi(config_.at("channels"));
        if (channels < 1 || channels > 32) {
            return core::core::Result<void>::error(
                core::ErrorCode::InvalidParameter,
                "Invalid channel count: " + std::to_string(channels)
            );
        }
        
        return core::core::Result<void>::success();
        
    } catch (const std::exception& e) {
        return core::core::Result<void>::error(
            core::ErrorCode::InvalidParameter,
            "Configuration validation failed: " + std::string(e.what())
        );
    }
}

// Placeholder for file analysis implementation
core::AsyncResult<core::VoidResult> LibEBU128Service::loadAndAnalyzeAudioFile(const std::string& filePath, core::ProgressCallback progress) {
    return core::AsyncResult<core::VoidResult>::createResolved(
        core::core::Result<void>::error(
            core::ErrorCode::NotImplemented,
            "File analysis not yet implemented - requires audio file loading"
        )
    );
}

// Additional placeholder implementations for remaining methods...
// Following the same patterns established above

} // namespace mixmind::services