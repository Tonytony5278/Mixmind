// AudioEngine.cpp - Real-time audio processing with PortAudio
#include "AudioEngine.h"
#include <portaudio.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <cstring>

namespace mixmind::audio {

// ============================================================================
// AudioEngine Implementation (PIMPL)
// ============================================================================

class AudioEngine::Impl {
private:
    // PortAudio stream and configuration
    PaStream* stream_ = nullptr;
    AudioConfig config_;
    AudioCallback audioCallback_;
    ErrorCallback errorCallback_;
    
    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    
    // Performance monitoring
    mutable std::mutex statsMutex_;
    PerformanceStats stats_;
    bool performanceMonitoring_ = true;
    std::chrono::high_resolution_clock::time_point callbackStart_;
    
    // Error handling
    mutable std::mutex errorMutex_;
    std::string lastError_;
    
public:
    Impl() = default;
    
    ~Impl() {
        shutdown();
    }
    
    // ========================================================================
    // Engine Lifecycle
    // ========================================================================
    
    core::Result<void> initialize(const AudioConfig& config) {
        if (initialized_.load()) {
            return core::Error("AudioEngine already initialized");
        }
        
        // Validate configuration
        auto validationResult = validateAudioConfig(config);
        if (!validationResult.isSuccess()) {
            return validationResult;
        }
        
        // Initialize PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            return core::Error("Failed to initialize PortAudio: " + std::string(Pa_GetErrorText(err)));
        }
        
        config_ = config;
        
        // Setup stream parameters
        PaStreamParameters outputParams;
        PaStreamParameters inputParams;
        PaStreamParameters* inputParamsPtr = nullptr;
        PaStreamParameters* outputParamsPtr = nullptr;
        
        // Configure output
        if (config_.outputChannels > 0) {
            outputParams.device = (config_.deviceName.empty()) ? 
                Pa_GetDefaultOutputDevice() : findDeviceByName(config_.deviceName);
            
            if (outputParams.device == paNoDevice) {
                Pa_Terminate();
                return core::Error("No suitable output device found");
            }
            
            outputParams.channelCount = config_.outputChannels;
            outputParams.sampleFormat = paFloat32;
            outputParams.suggestedLatency = config_.enableAutoLatency ? 
                Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency : 
                config_.suggestedLatency;
            outputParams.hostApiSpecificStreamInfo = nullptr;
            outputParamsPtr = &outputParams;
        }
        
        // Configure input (if enabled)
        if (config_.enableInput && config_.inputChannels > 0) {
            inputParams.device = (config_.deviceName.empty()) ? 
                Pa_GetDefaultInputDevice() : findDeviceByName(config_.deviceName);
                
            if (inputParams.device == paNoDevice) {
                Pa_Terminate();
                return core::Error("No suitable input device found");
            }
            
            inputParams.channelCount = config_.inputChannels;
            inputParams.sampleFormat = paFloat32;
            inputParams.suggestedLatency = config_.enableAutoLatency ? 
                Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency : 
                config_.suggestedLatency;
            inputParams.hostApiSpecificStreamInfo = nullptr;
            inputParamsPtr = &inputParams;
        }
        
        // Open audio stream
        err = Pa_OpenStream(
            &stream_,
            inputParamsPtr,
            outputParamsPtr,
            config_.sampleRate,
            config_.bufferSize,
            paClipOff, // No clipping
            audioCallbackTrampoline,
            this
        );
        
        if (err != paNoError) {
            Pa_Terminate();
            return core::Error("Failed to open audio stream: " + std::string(Pa_GetErrorText(err)));
        }
        
        initialized_.store(true);
        resetPerformanceStats();
        
        std::cout << "🎵 AudioEngine initialized successfully" << std::endl;
        std::cout << "   Sample Rate: " << config_.sampleRate << " Hz" << std::endl;
        std::cout << "   Buffer Size: " << config_.bufferSize << " frames" << std::endl;
        std::cout << "   Channels: " << config_.outputChannels << " out";
        if (config_.enableInput) {
            std::cout << ", " << config_.inputChannels << " in";
        }
        std::cout << std::endl;
        
        return core::Ok();
    }
    
    core::Result<void> start() {
        if (!initialized_.load()) {
            return core::Error("AudioEngine not initialized");
        }
        
        if (running_.load()) {
            return core::Error("AudioEngine already running");
        }
        
        PaError err = Pa_StartStream(stream_);
        if (err != paNoError) {
            return core::Error("Failed to start audio stream: " + std::string(Pa_GetErrorText(err)));
        }
        
        running_.store(true);
        resetPerformanceStats();
        
        std::cout << "🎵 AudioEngine started - real-time audio processing active" << std::endl;
        return core::Ok();
    }
    
    core::Result<void> stop() {
        if (!running_.load()) {
            return core::Ok(); // Already stopped
        }
        
        PaError err = Pa_StopStream(stream_);
        if (err != paNoError) {
            return core::Error("Failed to stop audio stream: " + std::string(Pa_GetErrorText(err)));
        }
        
        running_.store(false);
        std::cout << "🎵 AudioEngine stopped" << std::endl;
        return core::Ok();
    }
    
    void shutdown() {
        if (running_.load()) {
            stop();
        }
        
        if (stream_) {
            Pa_CloseStream(stream_);
            stream_ = nullptr;
        }
        
        if (initialized_.load()) {
            Pa_Terminate();
            initialized_.store(false);
            std::cout << "🎵 AudioEngine shutdown complete" << std::endl;
        }
    }
    
    bool isRunning() const {
        return running_.load();
    }
    
    bool isInitialized() const {
        return initialized_.load();
    }
    
    // ========================================================================
    // Audio Processing
    // ========================================================================
    
    void setAudioCallback(AudioCallback callback) {
        audioCallback_ = callback;
    }
    
    void clearAudioCallback() {
        audioCallback_ = nullptr;
    }
    
    AudioConfig getConfig() const {
        return config_;
    }
    
    LatencyInfo getLatencyInfo() const {
        LatencyInfo info;
        
        if (stream_) {
            const PaStreamInfo* streamInfo = Pa_GetStreamInfo(stream_);
            if (streamInfo) {
                info.inputLatency = streamInfo->inputLatency;
                info.outputLatency = streamInfo->outputLatency;
                info.totalLatency = info.inputLatency + info.outputLatency;
                info.actualSampleRate = static_cast<int>(streamInfo->sampleRate);
            }
            info.actualBufferSize = config_.bufferSize;
        }
        
        return info;
    }
    
    // ========================================================================
    // Performance Monitoring  
    // ========================================================================
    
    PerformanceStats getPerformanceStats() const {
        std::lock_guard<std::mutex> lock(statsMutex_);
        
        PerformanceStats stats = stats_;
        
        // Get CPU load from PortAudio
        if (stream_) {
            stats.cpuLoad = Pa_GetStreamCpuLoad(stream_);
        }
        
        return stats;
    }
    
    void resetPerformanceStats() {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = PerformanceStats{};
    }
    
    void setPerformanceMonitoring(bool enabled) {
        performanceMonitoring_ = enabled;
    }
    
    // ========================================================================
    // Error Handling
    // ========================================================================
    
    std::string getLastError() const {
        std::lock_guard<std::mutex> lock(errorMutex_);
        return lastError_;
    }
    
    void setErrorCallback(ErrorCallback callback) {
        errorCallback_ = callback;
    }
    
private:
    // ========================================================================
    // PortAudio Callback
    // ========================================================================
    
    static int audioCallbackTrampoline(const void* inputBuffer, void* outputBuffer,
                                     unsigned long framesPerBuffer,
                                     const PaStreamCallbackTimeInfo* timeInfo,
                                     PaStreamCallbackFlags statusFlags,
                                     void* userData) {
        auto* impl = static_cast<Impl*>(userData);
        return impl->audioCallbackImpl(inputBuffer, outputBuffer, framesPerBuffer, 
                                     timeInfo, statusFlags);
    }
    
    int audioCallbackImpl(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags) {
        
        // Performance monitoring
        auto callbackStart = std::chrono::high_resolution_clock::now();
        
        // Clear output buffer initially  
        float* output = static_cast<float*>(outputBuffer);
        if (output) {
            std::memset(output, 0, framesPerBuffer * config_.outputChannels * sizeof(float));
        }
        
        // Handle stream errors
        if (statusFlags & (paInputUnderflow | paInputOverflow | paOutputUnderflow | paOutputOverflow)) {
            handleStreamErrors(statusFlags);
        }
        
        // Call user audio callback if set
        if (audioCallback_) {
            try {
                const float* input = static_cast<const float*>(inputBuffer);
                audioCallback_(input, output, framesPerBuffer);
            } catch (const std::exception& e) {
                handleCallbackError("Audio callback exception: " + std::string(e.what()));
                return paAbort;
            } catch (...) {
                handleCallbackError("Unknown audio callback exception");
                return paAbort;
            }
        }
        
        // Update performance statistics
        if (performanceMonitoring_) {
            updatePerformanceStats(callbackStart);
        }
        
        return paContinue;
    }
    
    void handleStreamErrors(PaStreamCallbackFlags statusFlags) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        
        if (statusFlags & paInputUnderflow) {
            stats_.underruns++;
        }
        if (statusFlags & paInputOverflow) {
            stats_.overruns++;  
        }
        if (statusFlags & paOutputUnderflow) {
            stats_.underruns++;
        }
        if (statusFlags & paOutputOverflow) {
            stats_.overruns++;
        }
    }
    
    void handleCallbackError(const std::string& error) {
        {
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = error;
        }
        
        if (errorCallback_) {
            errorCallback_(error);
        }
        
        std::cerr << "AudioEngine error: " << error << std::endl;
    }
    
    void updatePerformanceStats(std::chrono::high_resolution_clock::time_point callbackStart) {
        auto callbackEnd = std::chrono::high_resolution_clock::now();
        auto callbackDuration = std::chrono::duration<double, std::milli>(callbackEnd - callbackStart).count();
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.totalCallbacks++;
        
        // Update timing statistics
        stats_.averageCallbackTime = (stats_.averageCallbackTime * (stats_.totalCallbacks - 1) + callbackDuration) 
                                   / stats_.totalCallbacks;
        stats_.maxCallbackTime = std::max(stats_.maxCallbackTime, callbackDuration);
    }
    
    // ========================================================================
    // Device Management Helpers
    // ========================================================================
    
    PaDeviceIndex findDeviceByName(const std::string& name) const {
        int deviceCount = Pa_GetDeviceCount();
        
        for (PaDeviceIndex i = 0; i < deviceCount; ++i) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
            if (deviceInfo && deviceInfo->name == name) {
                return i;
            }
        }
        
        return paNoDevice;
    }
};

// ============================================================================
// AudioEngine Public Interface
// ============================================================================

AudioEngine::AudioEngine() : pImpl(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() = default;

core::Result<void> AudioEngine::initialize(const AudioConfig& config) {
    return pImpl->initialize(config);
}

core::Result<void> AudioEngine::start() {
    return pImpl->start();
}

core::Result<void> AudioEngine::stop() {
    return pImpl->stop();
}

void AudioEngine::shutdown() {
    pImpl->shutdown();
}

bool AudioEngine::isRunning() const {
    return pImpl->isRunning();
}

bool AudioEngine::isInitialized() const {
    return pImpl->isInitialized();
}

void AudioEngine::setAudioCallback(AudioCallback callback) {
    pImpl->setAudioCallback(callback);
}

void AudioEngine::clearAudioCallback() {
    pImpl->clearAudioCallback();
}

AudioConfig AudioEngine::getConfig() const {
    return pImpl->getConfig();
}

AudioEngine::LatencyInfo AudioEngine::getLatencyInfo() const {
    return pImpl->getLatencyInfo();
}

AudioEngine::PerformanceStats AudioEngine::getPerformanceStats() const {
    return pImpl->getPerformanceStats();
}

void AudioEngine::resetPerformanceStats() {
    pImpl->resetPerformanceStats();
}

void AudioEngine::setPerformanceMonitoring(bool enabled) {
    pImpl->setPerformanceMonitoring(enabled);
}

std::string AudioEngine::getLastError() const {
    return pImpl->getLastError();
}

void AudioEngine::setErrorCallback(ErrorCallback callback) {
    pImpl->setErrorCallback(callback);
}

// ============================================================================
// Static Device Management Functions
// ============================================================================

std::vector<AudioDeviceInfo> AudioEngine::getAvailableDevices() {
    std::vector<AudioDeviceInfo> devices;
    
    // Initialize PortAudio temporarily if not already initialized
    bool wasInitialized = (Pa_GetVersionInfo() != nullptr);
    if (!wasInitialized) {
        if (Pa_Initialize() != paNoError) {
            return devices; // Return empty list on error
        }
    }
    
    int deviceCount = Pa_GetDeviceCount();
    PaDeviceIndex defaultInput = Pa_GetDefaultInputDevice();
    PaDeviceIndex defaultOutput = Pa_GetDefaultOutputDevice();
    
    for (PaDeviceIndex i = 0; i < deviceCount; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;
        
        AudioDeviceInfo device;
        device.deviceIndex = i;
        device.name = info->name;
        device.maxInputChannels = info->maxInputChannels;
        device.maxOutputChannels = info->maxOutputChannels;
        device.defaultSampleRate = info->defaultSampleRate;
        device.lowInputLatency = info->defaultLowInputLatency;
        device.lowOutputLatency = info->defaultLowOutputLatency;
        device.highInputLatency = info->defaultHighInputLatency;
        device.highOutputLatency = info->defaultHighOutputLatency;
        device.isDefault = (i == defaultInput || i == defaultOutput);
        
        devices.push_back(device);
    }
    
    if (!wasInitialized) {
        Pa_Terminate();
    }
    
    return devices;
}

AudioDeviceInfo AudioEngine::getDefaultInputDevice() {
    auto devices = getAvailableDevices();
    PaDeviceIndex defaultIndex = Pa_GetDefaultInputDevice();
    
    for (const auto& device : devices) {
        if (device.deviceIndex == defaultIndex) {
            return device;
        }
    }
    
    return AudioDeviceInfo{}; // Return empty if not found
}

AudioDeviceInfo AudioEngine::getDefaultOutputDevice() {
    auto devices = getAvailableDevices();
    PaDeviceIndex defaultIndex = Pa_GetDefaultOutputDevice();
    
    for (const auto& device : devices) {
        if (device.deviceIndex == defaultIndex) {
            return device;
        }
    }
    
    return AudioDeviceInfo{}; // Return empty if not found
}

std::optional<AudioDeviceInfo> AudioEngine::findDevice(const std::string& name) {
    auto devices = getAvailableDevices();
    
    for (const auto& device : devices) {
        if (device.name == name) {
            return device;
        }
    }
    
    return std::nullopt;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string sampleRateToString(int sampleRate) {
    if (sampleRate >= 1000) {
        return std::to_string(sampleRate / 1000) + "." + 
               std::to_string((sampleRate % 1000) / 100) + " kHz";
    }
    return std::to_string(sampleRate) + " Hz";
}

double bufferSizeToLatency(int bufferSize, int sampleRate) {
    return static_cast<double>(bufferSize) / sampleRate;
}

int latencyToBufferSize(double latency, int sampleRate) {
    return static_cast<int>(latency * sampleRate);
}

core::Result<void> validateAudioConfig(const AudioConfig& config) {
    if (config.sampleRate <= 0) {
        return core::Error("Invalid sample rate: must be positive");
    }
    
    if (config.bufferSize <= 0) {
        return core::Error("Invalid buffer size: must be positive");
    }
    
    if (config.outputChannels < 0) {
        return core::Error("Invalid output channels: cannot be negative");
    }
    
    if (config.inputChannels < 0) {
        return core::Error("Invalid input channels: cannot be negative");
    }
    
    if (config.outputChannels == 0 && (!config.enableInput || config.inputChannels == 0)) {
        return core::Error("Must have at least one input or output channel");
    }
    
    // Validate common sample rates
    std::vector<int> validSampleRates = {8000, 11025, 16000, 22050, 44100, 48000, 88200, 96000, 176400, 192000};
    bool validSampleRate = std::find(validSampleRates.begin(), validSampleRates.end(), config.sampleRate) != validSampleRates.end();
    
    if (!validSampleRate) {
        return core::Error("Unusual sample rate: " + std::to_string(config.sampleRate) + " Hz - may not be supported");
    }
    
    // Validate buffer sizes (powers of 2 are most efficient)
    if (config.bufferSize < 64 || config.bufferSize > 8192) {
        return core::Error("Buffer size out of recommended range (64-8192 frames)");
    }
    
    return core::Ok();
}

} // namespace mixmind::audio