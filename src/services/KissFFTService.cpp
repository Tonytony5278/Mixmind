#include "KissFFTService.h"
#include "../core/async.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <numeric>

namespace mixmind::services {

// ============================================================================
// KissFFTService Implementation
// ============================================================================

KissFFTService::KissFFTService() {
    // Initialize default window
    window_ = generateWindow(fftSize_, windowType_);
}

KissFFTService::~KissFFTService() {
    shutdown().get(); // Ensure proper cleanup
}

// ========================================================================
// IOSSService Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> KissFFTService::initialize() {
    return core::executeAsyncGlobal<core::Result<void>>([this]() -> core::Result<void> {
        try {
            // Initialize FFT configurations
            auto result = initializeFFTConfig();
            if (!result.isSuccess()) {
                std::lock_guard<std::mutex> lock(errorMutex_);
                lastError_ = result.getError();
                return result;
            }
            
            // Generate initial window
            window_ = generateWindow(fftSize_, windowType_);
            
            // Reset state
            {
                std::lock_guard<std::mutex> lock(resultsMutex_);
                analysisResults_.clear();
            }
            
            {
                std::lock_guard<std::mutex> lock(metricsMutex_);
                performanceMetrics_ = PerformanceMetrics{};
                performanceMetrics_.initializationTime = std::chrono::system_clock::now();
            }
            
            isInitialized_.store(true);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Initialization failed: " + std::string(e.what());
            return core::core::Result<void>::failure(lastError_);
        }
    });
}

core::AsyncResult<core::VoidResult> KissFFTService::shutdown() {
    return core::executeAsyncGlobal<core::Result<void>>([this]() -> core::Result<void> {
        try {
            // Stop any active operations
            isAnalyzing_.store(false);
            shouldCancel_.store(true);
            
            if (isRealtimeActive_.load()) {
                stopRealtimeAnalysis().get();
            }
            
            // Cleanup FFT resources
            cleanupFFTConfig();
            
            // Clear data
            {
                std::lock_guard<std::mutex> lock(resultsMutex_);
                analysisResults_.clear();
            }
            
            {
                std::lock_guard<std::mutex> lock(realtimeMutex_);
                realtimeBuffer_.clear();
                latestSpectrum_ = SpectrumData{};
                spectrumCallback_ = nullptr;
            }
            
            isInitialized_.store(false);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Shutdown failed: " + std::string(e.what());
            return core::core::Result<void>::failure(lastError_);
        }
    });
}

bool KissFFTService::isInitialized() const {
    return isInitialized_.load();
}

std::string KissFFTService::getServiceName() const {
    return "KissFFT Spectrum Analysis Service";
}

std::string KissFFTService::getServiceVersion() const {
    return "1.3.1"; // KissFFT version
}

IOSSService::ServiceInfo KissFFTService::getServiceInfo() const {
    ServiceInfo info;
    info.name = getServiceName();
    info.version = getServiceVersion();
    info.description = "High-performance FFT analysis and spectrum processing using KissFFT";
    info.vendor = "KissFFT Project";
    info.category = "Audio Analysis";
    info.capabilities = {
        "Forward FFT", "Inverse FFT", "Power Spectrum", "Spectral Density",
        "Real-time Analysis", "Windowing Functions", "Spectral Features",
        "Pitch Detection", "Frequency Filtering", "Convolution"
    };
    info.supportedSampleRates = {8000, 11025, 22050, 44100, 48000, 88200, 96000, 176400, 192000};
    info.maxChannels = 32;
    info.latencySamples = getLatencySamples();
    
    return info;
}

core::VoidResult KissFFTService::configure(const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    for (const auto& pair : config) {
        config_[pair.first] = pair.second;
        
        // Apply specific configurations
        if (pair.first == "fft_size") {
            int32_t fftSize = std::stoi(pair.second);
            auto result = setFFTSize(fftSize);
            if (!result.isSuccess()) {
                return result;
            }
        }
        else if (pair.first == "window_type") {
            if (pair.second == "rectangular") setWindowType(WindowType::Rectangular);
            else if (pair.second == "hanning") setWindowType(WindowType::Hanning);
            else if (pair.second == "hamming") setWindowType(WindowType::Hamming);
            else if (pair.second == "blackman") setWindowType(WindowType::Blackman);
            else if (pair.second == "kaiser") setWindowType(WindowType::Kaiser);
        }
        else if (pair.first == "window_overlap") {
            float overlap = std::stof(pair.second);
            setWindowOverlap(overlap);
        }
        else if (pair.first == "zero_padding") {
            bool enabled = (pair.second == "true" || pair.second == "1");
            setZeroPaddingEnabled(enabled);
        }
    }
    
    return core::VoidResult::success();
}

std::optional<std::string> KissFFTService::getConfigValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    auto it = config_.find(key);
    return (it != config_.end()) ? std::optional<std::string>(it->second) : std::nullopt;
}

bool KissFFTService::isHealthy() const {
    return isInitialized_.load() && forwardConfig_ != nullptr && inverseConfig_ != nullptr;
}

std::string KissFFTService::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

core::AsyncResult<core::VoidResult> KissFFTService::runSelfTest() {
    return core::executeAsyncGlobal<core::Result<void>>([this]() -> core::Result<void> {
        try {
            if (!isInitialized()) {
                return core::core::Result<void>::failure("Service not initialized");
            }
            
            // Test basic FFT operations
            std::vector<float> testSignal(fftSize_, 0.0f);
            
            // Generate test sine wave
            float frequency = 440.0f; // A4
            float sampleRate = 44100.0f;
            for (int i = 0; i < fftSize_; ++i) {
                testSignal[i] = std::sin(2.0f * M_PI * frequency * i / sampleRate);
            }
            
            // Test forward FFT
            auto forwardResult = forwardFFT(testSignal);
            if (!forwardResult.isSuccess()) {
                return core::core::Result<void>::failure("Forward FFT test failed: " + forwardResult.getError());
            }
            
            // Test inverse FFT
            auto inverseResult = inverseFFT(forwardResult.getValue());
            if (!inverseResult.isSuccess()) {
                return core::core::Result<void>::failure("Inverse FFT test failed: " + inverseResult.getError());
            }
            
            // Test spectrum computation
            auto spectrumResult = computePowerSpectrum(testSignal, static_cast<core::SampleRate>(sampleRate));
            if (!spectrumResult.isSuccess()) {
                return core::core::Result<void>::failure("Spectrum computation test failed: " + spectrumResult.getError());
            }
            
            // Verify spectrum contains expected peak at 440 Hz
            const auto& spectrum = spectrumResult.getValue();
            bool foundPeak = false;
            for (size_t i = 0; i < spectrum.frequencies.size(); ++i) {
                if (std::abs(spectrum.frequencies[i] - frequency) < 10.0f && spectrum.magnitudesDB[i] > -20.0f) {
                    foundPeak = true;
                    break;
                }
            }
            
            if (!foundPeak) {
                return core::core::Result<void>::failure("Expected frequency peak not found in spectrum");
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::core::Result<void>::failure("Self-test failed: " + std::string(e.what()));
        }
    });
}

IOSSService::PerformanceMetrics KissFFTService::getPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return performanceMetrics_;
}

void KissFFTService::resetPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    performanceMetrics_ = PerformanceMetrics{};
    performanceMetrics_.resetTime = std::chrono::system_clock::now();
}

// ========================================================================
// FFT Configuration
// ========================================================================

core::VoidResult KissFFTService::setFFTSize(int32_t fftSize) {
    if (!isPowerOfTwo(fftSize) || fftSize < 32 || fftSize > 32768) {
        return core::core::Result<void>::failure("FFT size must be a power of 2 between 32 and 32768");
    }
    
    std::lock_guard<std::mutex> lock(fftMutex_);
    
    if (fftSize_ != fftSize) {
        fftSize_ = fftSize;
        
        // Regenerate window
        window_ = generateWindow(fftSize_, windowType_);
        
        // Reinitialize FFT configurations if already initialized
        if (isInitialized_.load()) {
            cleanupFFTConfig();
            auto result = initializeFFTConfig();
            if (!result.isSuccess()) {
                return result;
            }
        }
    }
    
    return core::VoidResult::success();
}

int32_t KissFFTService::getFFTSize() const {
    return fftSize_;
}

core::VoidResult KissFFTService::setWindowType(WindowType windowType) {
    std::lock_guard<std::mutex> lock(fftMutex_);
    
    if (windowType_ != windowType) {
        windowType_ = windowType;
        window_ = generateWindow(fftSize_, windowType_);
    }
    
    return core::VoidResult::success();
}

KissFFTService::WindowType KissFFTService::getWindowType() const {
    return windowType_;
}

core::VoidResult KissFFTService::setWindowOverlap(float overlap) {
    if (overlap < 0.0f || overlap >= 1.0f) {
        return core::core::Result<void>::failure("Window overlap must be between 0.0 and 0.95");
    }
    
    windowOverlap_ = overlap;
    return core::VoidResult::success();
}

float KissFFTService::getWindowOverlap() const {
    return windowOverlap_;
}

// ========================================================================
// Spectrum Analysis
// ========================================================================

core::Result<KissFFTService::SpectrumData> KissFFTService::computePowerSpectrum(
    const std::vector<float>& audioData,
    core::SampleRate sampleRate
) {
    if (!isInitialized()) {
        return core::Result<SpectrumData>::failure("Service not initialized");
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // Prepare input buffer
        auto inputBuffer = prepareInputBuffer(audioData);
        
        // Perform FFT
        std::vector<kiss_fft_cpx> outputBuffer(fftSize_);
        
        {
            std::lock_guard<std::mutex> lock(fftMutex_);
            kiss_fft(forwardConfig_, inputBuffer.data(), outputBuffer.data());
        }
        
        // Process FFT output to spectrum data
        auto spectrum = processFFTOutput(outputBuffer, sampleRate);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        updatePerformanceMetrics(duration, true);
        
        return core::Result<SpectrumData>::success(std::move(spectrum));
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        updatePerformanceMetrics(duration, false);
        
        return core::Result<SpectrumData>::failure("Power spectrum computation failed: " + std::string(e.what()));
    }
}

core::Result<std::vector<std::complex<float>>> KissFFTService::forwardFFT(const std::vector<float>& timeData) {
    if (!isInitialized()) {
        return core::Result<std::vector<std::complex<float>>>::failure("Service not initialized");
    }
    
    try {
        // Prepare input buffer
        auto inputBuffer = prepareInputBuffer(timeData);
        
        // Perform FFT
        std::vector<kiss_fft_cpx> outputBuffer(fftSize_);
        
        {
            std::lock_guard<std::mutex> lock(fftMutex_);
            kiss_fft(forwardConfig_, inputBuffer.data(), outputBuffer.data());
        }
        
        // Convert to std::complex
        std::vector<std::complex<float>> result;
        result.reserve(fftSize_);
        
        for (const auto& sample : outputBuffer) {
            result.emplace_back(sample.r, sample.i);
        }
        
        return core::Result<std::vector<std::complex<float>>>::success(std::move(result));
        
    } catch (const std::exception& e) {
        return core::Result<std::vector<std::complex<float>>>::failure("Forward FFT failed: " + std::string(e.what()));
    }
}

core::Result<std::vector<float>> KissFFTService::inverseFFT(const std::vector<std::complex<float>>& freqData) {
    if (!isInitialized()) {
        return core::Result<std::vector<float>>::failure("Service not initialized");
    }
    
    try {
        // Convert from std::complex to kiss_fft_cpx
        std::vector<kiss_fft_cpx> inputBuffer;
        inputBuffer.reserve(freqData.size());
        
        for (const auto& sample : freqData) {
            kiss_fft_cpx kfft_sample;
            kfft_sample.r = sample.real();
            kfft_sample.i = sample.imag();
            inputBuffer.push_back(kfft_sample);
        }
        
        // Perform inverse FFT
        std::vector<kiss_fft_cpx> outputBuffer(fftSize_);
        
        {
            std::lock_guard<std::mutex> lock(fftMutex_);
            kiss_fft(inverseConfig_, inputBuffer.data(), outputBuffer.data());
        }
        
        // Convert to real values and normalize
        std::vector<float> result;
        result.reserve(fftSize_);
        
        float normalization = 1.0f / static_cast<float>(fftSize_);
        for (const auto& sample : outputBuffer) {
            result.push_back(sample.r * normalization);
        }
        
        return core::Result<std::vector<float>>::success(std::move(result));
        
    } catch (const std::exception& e) {
        return core::Result<std::vector<float>>::failure("Inverse FFT failed: " + std::string(e.what()));
    }
}

// ========================================================================
// Real-time Spectrum Analysis
// ========================================================================

core::AsyncResult<core::VoidResult> KissFFTService::startRealtimeAnalysis(
    core::SampleRate sampleRate,
    int32_t bufferSize
) {
    return core::executeAsyncGlobal<core::Result<void>>([this, sampleRate, bufferSize]() -> core::Result<void> {
        try {
            if (!isInitialized()) {
                return core::core::Result<void>::failure("Service not initialized");
            }
            
            if (isRealtimeActive_.load()) {
                return core::core::Result<void>::failure("Real-time analysis already active");
            }
            
            // Initialize real-time buffer
            {
                std::lock_guard<std::mutex> lock(realtimeMutex_);
                realtimeBuffer_.clear();
                realtimeBuffer_.reserve(fftSize_);
                
                // Initialize latest spectrum
                latestSpectrum_ = SpectrumData{};
                latestSpectrum_.sampleRate = static_cast<float>(sampleRate);
                latestSpectrum_.fftSize = fftSize_;
            }
            
            isRealtimeActive_.store(true);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::core::Result<void>::failure("Failed to start real-time analysis: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> KissFFTService::stopRealtimeAnalysis() {
    return core::executeAsyncGlobal<core::Result<void>>([this]() -> core::Result<void> {
        try {
            isRealtimeActive_.store(false);
            
            {
                std::lock_guard<std::mutex> lock(realtimeMutex_);
                realtimeBuffer_.clear();
                spectrumCallback_ = nullptr;
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::core::Result<void>::failure("Failed to stop real-time analysis: " + std::string(e.what()));
        }
    });
}

core::VoidResult KissFFTService::processRealtimeFrame(const float* samples, int32_t sampleCount) {
    if (!isRealtimeActive_.load()) {
        return core::core::Result<void>::failure("Real-time analysis not active");
    }
    
    try {
        std::lock_guard<std::mutex> lock(realtimeMutex_);
        
        // Add new samples to buffer
        for (int32_t i = 0; i < sampleCount; ++i) {
            realtimeBuffer_.push_back(samples[i]);
        }
        
        // Process if we have enough samples
        while (realtimeBuffer_.size() >= static_cast<size_t>(fftSize_)) {
            // Extract frame
            std::vector<float> frame(realtimeBuffer_.begin(), realtimeBuffer_.begin() + fftSize_);
            
            // Compute spectrum
            auto spectrumResult = computePowerSpectrum(frame, static_cast<core::SampleRate>(latestSpectrum_.sampleRate));
            
            if (spectrumResult.isSuccess()) {
                latestSpectrum_ = spectrumResult.getValue();
                latestSpectrum_.analysisTime = std::chrono::duration<double>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                
                // Call callback if set
                if (spectrumCallback_) {
                    spectrumCallback_(latestSpectrum_);
                }
            }
            
            // Advance buffer by hop size
            int32_t hopSize = static_cast<int32_t>(fftSize_ * (1.0f - windowOverlap_));
            realtimeBuffer_.erase(realtimeBuffer_.begin(), realtimeBuffer_.begin() + hopSize);
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::core::Result<void>::failure("Real-time frame processing failed: " + std::string(e.what()));
    }
}

KissFFTService::SpectrumData KissFFTService::getLatestSpectrum() const {
    std::lock_guard<std::mutex> lock(realtimeMutex_);
    return latestSpectrum_;
}

void KissFFTService::setSpectrumCallback(SpectrumCallback callback) {
    std::lock_guard<std::mutex> lock(realtimeMutex_);
    spectrumCallback_ = std::move(callback);
}

// ========================================================================
// Spectral Features Extraction
// ========================================================================

KissFFTService::SpectralFeatures KissFFTService::extractSpectralFeatures(
    const SpectrumData& spectrum,
    const SpectralFeatures* previousFeatures
) const {
    SpectralFeatures features;
    
    if (spectrum.magnitudes.empty() || spectrum.frequencies.empty()) {
        return features;
    }
    
    // Compute spectral centroid
    features.spectralCentroid = computeSpectralCentroid(spectrum);
    
    // Compute spectral spread (bandwidth)
    float centroid = features.spectralCentroid;
    float spread = 0.0f;
    float totalMagnitude = 0.0f;
    
    for (size_t i = 0; i < spectrum.magnitudes.size(); ++i) {
        float freq = spectrum.frequencies[i];
        float mag = spectrum.magnitudes[i];
        
        spread += mag * (freq - centroid) * (freq - centroid);
        totalMagnitude += mag;
    }
    
    features.spectralSpread = (totalMagnitude > 0.0f) ? std::sqrt(spread / totalMagnitude) : 0.0f;
    
    // Compute spectral rolloff
    features.spectralRolloff = computeSpectralRolloff(spectrum);
    
    // Compute spectral flux (if previous features available)
    if (previousFeatures) {
        // This would require storing previous spectrum data
        // Simplified version - compute energy difference
        float currentEnergy = 0.0f;
        for (float mag : spectrum.magnitudes) {
            currentEnergy += mag * mag;
        }
        // Would need previous energy to compute flux
        features.spectralFlux = 0.0f; // Placeholder
    }
    
    // Zero crossing rate would be computed from time-domain signal
    features.zeroCrossingRate = 0.0f; // Placeholder - needs time domain data
    
    return features;
}

float KissFFTService::computeSpectralCentroid(const SpectrumData& spectrum) const {
    if (spectrum.magnitudes.empty() || spectrum.frequencies.empty()) {
        return 0.0f;
    }
    
    float weightedSum = 0.0f;
    float totalMagnitude = 0.0f;
    
    for (size_t i = 0; i < spectrum.magnitudes.size(); ++i) {
        float freq = spectrum.frequencies[i];
        float mag = spectrum.magnitudes[i];
        
        weightedSum += freq * mag;
        totalMagnitude += mag;
    }
    
    return (totalMagnitude > 0.0f) ? weightedSum / totalMagnitude : 0.0f;
}

float KissFFTService::computeSpectralRolloff(const SpectrumData& spectrum, float percentage) const {
    if (spectrum.magnitudes.empty() || spectrum.frequencies.empty()) {
        return 0.0f;
    }
    
    // Calculate total energy
    float totalEnergy = 0.0f;
    for (float mag : spectrum.magnitudes) {
        totalEnergy += mag * mag;
    }
    
    if (totalEnergy == 0.0f) {
        return 0.0f;
    }
    
    // Find frequency where energy reaches percentage of total
    float cumulativeEnergy = 0.0f;
    float targetEnergy = totalEnergy * percentage;
    
    for (size_t i = 0; i < spectrum.magnitudes.size(); ++i) {
        cumulativeEnergy += spectrum.magnitudes[i] * spectrum.magnitudes[i];
        
        if (cumulativeEnergy >= targetEnergy) {
            return spectrum.frequencies[i];
        }
    }
    
    // Return highest frequency if not found
    return spectrum.frequencies.back();
}

// ========================================================================
// Windowing Functions
// ========================================================================

std::vector<float> KissFFTService::generateWindow(int32_t size, WindowType windowType) const {
    std::vector<float> window(size);
    
    switch (windowType) {
        case WindowType::Rectangular:
            std::fill(window.begin(), window.end(), 1.0f);
            break;
            
        case WindowType::Hanning:
            for (int32_t i = 0; i < size; ++i) {
                window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
            }
            break;
            
        case WindowType::Hamming:
            for (int32_t i = 0; i < size; ++i) {
                window[i] = 0.54f - 0.46f * std::cos(2.0f * M_PI * i / (size - 1));
            }
            break;
            
        case WindowType::Blackman:
            for (int32_t i = 0; i < size; ++i) {
                float phase = 2.0f * M_PI * i / (size - 1);
                window[i] = 0.42f - 0.5f * std::cos(phase) + 0.08f * std::cos(2.0f * phase);
            }
            break;
            
        case WindowType::Bartlett:
            for (int32_t i = 0; i < size; ++i) {
                if (i < size / 2) {
                    window[i] = 2.0f * i / (size - 1);
                } else {
                    window[i] = 2.0f - 2.0f * i / (size - 1);
                }
            }
            break;
            
        default:
            // Default to Hanning
            for (int32_t i = 0; i < size; ++i) {
                window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
            }
            break;
    }
    
    return window;
}

// ========================================================================
// Utility Functions
// ========================================================================

float KissFFTService::magnitudeToDecibels(float magnitude, float reference) {
    if (magnitude <= 0.0f) {
        return -120.0f; // Very small value
    }
    return 20.0f * std::log10(magnitude / reference);
}

float KissFFTService::decibelsToMagnitude(float decibels, float reference) {
    return reference * std::pow(10.0f, decibels / 20.0f);
}

bool KissFFTService::isPowerOfTwo(int32_t size) {
    return size > 0 && (size & (size - 1)) == 0;
}

int32_t KissFFTService::nextPowerOfTwo(int32_t size) {
    if (size <= 0) return 1;
    
    int32_t result = 1;
    while (result < size) {
        result <<= 1;
    }
    return result;
}

// ========================================================================
// Protected Implementation Methods
// ========================================================================

core::VoidResult KissFFTService::initializeFFTConfig() {
    try {
        // Cleanup any existing configurations
        cleanupFFTConfig();
        
        // Create forward FFT configuration
        forwardConfig_ = kiss_fft_alloc(fftSize_, 0, nullptr, nullptr);
        if (!forwardConfig_) {
            return core::core::Result<void>::failure("Failed to allocate forward FFT configuration");
        }
        
        // Create inverse FFT configuration
        inverseConfig_ = kiss_fft_alloc(fftSize_, 1, nullptr, nullptr);
        if (!inverseConfig_) {
            kiss_fft_free(forwardConfig_);
            forwardConfig_ = nullptr;
            return core::core::Result<void>::failure("Failed to allocate inverse FFT configuration");
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::core::Result<void>::failure("FFT configuration failed: " + std::string(e.what()));
    }
}

void KissFFTService::cleanupFFTConfig() {
    if (forwardConfig_) {
        kiss_fft_free(forwardConfig_);
        forwardConfig_ = nullptr;
    }
    
    if (inverseConfig_) {
        kiss_fft_free(inverseConfig_);
        inverseConfig_ = nullptr;
    }
}

std::vector<kiss_fft_cpx> KissFFTService::prepareInputBuffer(const std::vector<float>& input) {
    std::vector<kiss_fft_cpx> buffer(fftSize_);
    
    // Copy input data (with windowing)
    int32_t inputSize = std::min(static_cast<int32_t>(input.size()), fftSize_);
    
    for (int32_t i = 0; i < inputSize; ++i) {
        buffer[i].r = input[i] * window_[i];
        buffer[i].i = 0.0f;
    }
    
    // Zero padding if enabled and input is smaller than FFT size
    if (zeroPaddingEnabled_ && inputSize < fftSize_) {
        for (int32_t i = inputSize; i < fftSize_; ++i) {
            buffer[i].r = 0.0f;
            buffer[i].i = 0.0f;
        }
    }
    
    return buffer;
}

KissFFTService::SpectrumData KissFFTService::processFFTOutput(
    const std::vector<kiss_fft_cpx>& fftOutput,
    core::SampleRate sampleRate
) {
    SpectrumData spectrum;
    spectrum.sampleRate = static_cast<float>(sampleRate);
    spectrum.fftSize = fftSize_;
    spectrum.analysisTime = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // Process only the first half of the FFT (positive frequencies)
    int32_t numBins = fftSize_ / 2 + 1;
    
    spectrum.frequencies.reserve(numBins);
    spectrum.magnitudes.reserve(numBins);
    spectrum.magnitudesDB.reserve(numBins);
    spectrum.phases.reserve(numBins);
    spectrum.complex.reserve(numBins);
    
    float freqResolution = static_cast<float>(sampleRate) / fftSize_;
    
    for (int32_t i = 0; i < numBins; ++i) {
        // Frequency bin
        spectrum.frequencies.push_back(i * freqResolution);
        
        // Complex representation
        std::complex<float> complexSample(fftOutput[i].r, fftOutput[i].i);
        spectrum.complex.push_back(complexSample);
        
        // Magnitude
        float magnitude = std::abs(complexSample);
        spectrum.magnitudes.push_back(magnitude);
        
        // Magnitude in dB
        spectrum.magnitudesDB.push_back(magnitudeToDecibels(magnitude));
        
        // Phase
        spectrum.phases.push_back(std::arg(complexSample));
    }
    
    return spectrum;
}

void KissFFTService::updatePerformanceMetrics(double processingTime, bool success) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    performanceMetrics_.totalOperations++;
    performanceMetrics_.totalProcessingTime += processingTime;
    
    if (success) {
        performanceMetrics_.successfulOperations++;
    } else {
        performanceMetrics_.failedOperations++;
    }
    
    if (performanceMetrics_.totalOperations > 0) {
        performanceMetrics_.averageProcessingTime = 
            performanceMetrics_.totalProcessingTime / performanceMetrics_.totalOperations;
    }
    
    performanceMetrics_.lastOperationTime = std::chrono::system_clock::now();
}

// ========================================================================
// IAudioAnalysisService Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> KissFFTService::analyzeBuffer(
    const core::FloatAudioBuffer& buffer,
    core::SampleRate sampleRate,
    core::ProgressCallback progress
) {
    return core::executeAsyncGlobal<core::Result<void>>([this, &buffer, sampleRate, progress]() -> core::Result<void> {
        try {
            isAnalyzing_.store(true);
            shouldCancel_.store(false);
            
            // Process each channel
            int32_t numChannels = buffer.getNumChannels();
            int32_t numSamples = buffer.getNumSamples();
            
            for (int32_t channel = 0; channel < numChannels; ++channel) {
                if (shouldCancel_.load()) {
                    isAnalyzing_.store(false);
                    return core::Result<void>::failure("Analysis cancelled");
                }
                
                // Extract channel data
                std::vector<float> channelData(numSamples);
                std::copy(buffer.getReadPointer(channel), 
                         buffer.getReadPointer(channel) + numSamples,
                         channelData.begin());
                
                // Compute spectrum
                auto spectrumResult = computePowerSpectrum(channelData, sampleRate);
                if (!spectrumResult.isSuccess()) {
                    isAnalyzing_.store(false);
                    return core::core::Result<void>::failure("Spectrum analysis failed: " + spectrumResult.getError());
                }
                
                // Extract spectral features
                auto features = extractSpectralFeatures(spectrumResult.getValue());
                
                // Store results
                {
                    std::lock_guard<std::mutex> lock(resultsMutex_);
                    std::string channelPrefix = "channel_" + std::to_string(channel) + "_";
                    
                    analysisResults_[channelPrefix + "spectral_centroid"] = features.spectralCentroid;
                    analysisResults_[channelPrefix + "spectral_spread"] = features.spectralSpread;
                    analysisResults_[channelPrefix + "spectral_rolloff"] = features.spectralRolloff;
                    analysisResults_[channelPrefix + "spectral_flux"] = features.spectralFlux;
                }
                
                // Update progress
                if (progress) {
                    float progressValue = static_cast<float>(channel + 1) / numChannels;
                    progress(progressValue);
                }
            }
            
            isAnalyzing_.store(false);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            isAnalyzing_.store(false);
            return core::core::Result<void>::failure("Buffer analysis failed: " + std::string(e.what()));
        }
    });
}

std::unordered_map<std::string, double> KissFFTService::getAnalysisResults() const {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    return analysisResults_;
}

void KissFFTService::clearResults() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    analysisResults_.clear();
}

bool KissFFTService::isAnalyzing() const {
    return isAnalyzing_.load();
}

core::VoidResult KissFFTService::cancelAnalysis() {
    shouldCancel_.store(true);
    return core::VoidResult::success();
}

int32_t KissFFTService::getLatencySamples() const {
    return fftSize_ / 2; // Half FFT size is typical latency
}

} // namespace mixmind::services