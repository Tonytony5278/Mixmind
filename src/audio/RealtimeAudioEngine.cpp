#include "RealtimeAudioEngine.h"
#include "LockFreeBuffer.h"
#include "../core/async.h"
#include "../core/logging.h"
#include <portaudio.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>

namespace mixmind::audio {

// ============================================================================
// RealtimeAudioEngine - Professional DAW Audio Engine
// ============================================================================

class RealtimeAudioEngine::Impl {
public:
    // PortAudio stream and configuration
    PaStream* stream_ = nullptr;
    PaStreamParameters inputParams_;
    PaStreamParameters outputParams_;
    
    // Enhanced audio configuration
    AudioConfiguration currentConfig_;
    double sampleRate_ = 48000.0;
    unsigned long framesPerBuffer_ = 512;
    int inputChannels_ = 2;
    int outputChannels_ = 2;
    
    // PortAudio state
    bool portAudioInitialized_ = false;
    bool streamOpen_ = false;
    
    // Real-time safe audio buffers
    std::unique_ptr<AudioBufferPool> inputBufferPool_;
    std::unique_ptr<AudioBufferPool> outputBufferPool_;
    std::unique_ptr<LockFreeQueue<AudioCommand>> commandQueue_;
    
    // Enhanced performance monitoring
    AudioPerformanceStats performanceStats_;
    std::atomic<double> cpuLoad_{0.0};
    std::atomic<long> xrunCount_{0};
    std::chrono::high_resolution_clock::time_point lastXrun_;
    std::chrono::high_resolution_clock::time_point lastProcessTime_;
    
    // State management
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> shouldStop_{false};
    std::mutex configMutex_;
    
    // Enhanced callbacks and processors
    AudioInputCallback inputCallback_;
    AudioOutputCallback outputCallback_;
    AudioProcessCallback processCallback_;
    AudioErrorCallback errorCallback_;
    AudioStatusCallback statusCallback_;
    AudioXRunCallback xrunCallback_;
    std::vector<std::unique_ptr<AudioProcessor>> processors_;
    
    // Master controls
    std::atomic<double> masterVolume_{1.0};
    std::atomic<double> inputGain_{1.0};
    std::atomic<bool> inputMonitoringEnabled_{false};
    
    // Buffer management
    std::vector<float> inputBuffer_;
    std::vector<float> outputBuffer_;
    
    bool initialize() {
        return initializePortAudio().isSuccess();
    }
    
    mixmind::core::Result<void> initializePortAudio() {
        if (portAudioInitialized_) {
            return mixmind::core::Result<void>::success();
        }
        
        // Initialize PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            return mixmind::core::Result<void>::error(
                mixmind::core::ErrorCode::AudioDeviceError,
                "portaudio",
                "Failed to initialize PortAudio: " + std::string(Pa_GetErrorText(err))
            );
        }
        
        portAudioInitialized_ = true;
        performanceStats_.reset();
        
        MIXMIND_LOG_INFO("Real-time audio engine initialized with PortAudio v{}", Pa_GetVersionText());
        return mixmind::core::Result<void>::success();
    }
    
    std::vector<AudioDeviceInfo> getAvailableDevices() {
        std::vector<AudioDeviceInfo> devices;
        
        if (!portAudioInitialized_) {
            return devices;
        }
        
        int deviceCount = Pa_GetDeviceCount();
        if (deviceCount < 0) {
            MIXMIND_LOG_ERROR("Failed to get device count: {}", Pa_GetErrorText(deviceCount));
            return devices;
        }
        
        for (int i = 0; i < deviceCount; ++i) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
            if (!deviceInfo) continue;
            
            AudioDeviceInfo info;
            info.deviceIndex = i;
            info.name = deviceInfo->name ? deviceInfo->name : "Unknown Device";
            info.maxInputChannels = deviceInfo->maxInputChannels;
            info.maxOutputChannels = deviceInfo->maxOutputChannels;
            info.defaultSampleRate = deviceInfo->defaultSampleRate;
            info.inputLatency = deviceInfo->defaultLowInputLatency * 1000.0; // Convert to ms
            info.outputLatency = deviceInfo->defaultLowOutputLatency * 1000.0; // Convert to ms
            
            // Get host API info
            const PaHostApiInfo* hostApiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
            if (hostApiInfo) {
                info.hostApi = hostApiInfo->name;
            }
            
            // Check if default devices
            info.isDefaultInput = (i == Pa_GetDefaultInputDevice());
            info.isDefaultOutput = (i == Pa_GetDefaultOutputDevice());
            
            // Test supported sample rates
            std::vector<double> testRates = {8000, 22050, 44100, 48000, 88200, 96000, 192000};
            for (double rate : testRates) {
                if (testSampleRate(i, rate)) {
                    info.supportedSampleRates.push_back(rate);
                }
            }
            
            // Test supported buffer sizes
            std::vector<int> testSizes = {64, 128, 256, 512, 1024, 2048, 4096};
            for (int size : testSizes) {
                if (testBufferSize(i, size)) {
                    info.supportedBufferSizes.push_back(size);
                }
            }
            
            devices.push_back(info);
        }
        
        return devices;
    }
    
    bool testSampleRate(int deviceIndex, double sampleRate) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
        if (!deviceInfo) return false;
        
        PaStreamParameters inputParams = {};
        PaStreamParameters outputParams = {};
        
        if (deviceInfo->maxInputChannels > 0) {
            inputParams.device = deviceIndex;
            inputParams.channelCount = std::min(2, deviceInfo->maxInputChannels);
            inputParams.sampleFormat = paFloat32;
            inputParams.suggestedLatency = deviceInfo->defaultLowInputLatency;
        }
        
        if (deviceInfo->maxOutputChannels > 0) {
            outputParams.device = deviceIndex;
            outputParams.channelCount = std::min(2, deviceInfo->maxOutputChannels);
            outputParams.sampleFormat = paFloat32;
            outputParams.suggestedLatency = deviceInfo->defaultLowOutputLatency;
        }
        
        PaError err = Pa_IsFormatSupported(
            deviceInfo->maxInputChannels > 0 ? &inputParams : nullptr,
            deviceInfo->maxOutputChannels > 0 ? &outputParams : nullptr,
            sampleRate
        );
        
        return err == paFormatIsSupported;
    }
    
    bool testBufferSize(int deviceIndex, int bufferSize) {
        // PortAudio doesn't directly test buffer sizes, so we assume common sizes work
        return bufferSize >= 64 && bufferSize <= 4096 && (bufferSize & (bufferSize - 1)) == 0;
    }
    
    void cleanup() {
        if (stream_) {
            Pa_CloseStream(stream_);
            stream_ = nullptr;
        }
        Pa_Terminate();
        RT_LOG_INFO("RealtimeAudioEngine cleaned up");
    }
    
    // PortAudio callback - REAL-TIME THREAD
    static int portAudioCallback(
        const void* inputBuffer,
        void* outputBuffer, 
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* userData) {
        
        auto* engine = static_cast<Impl*>(userData);
        return engine->processAudio(inputBuffer, outputBuffer, framesPerBuffer, timeInfo, statusFlags);
    }
    
    int processAudio(
        const void* inputBuffer,
        void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags) {
        
        auto callbackStart = std::chrono::high_resolution_clock::now();
        
        // Handle xruns and timing issues
        if (statusFlags & (paInputUnderflow | paInputOverflow | paOutputUnderflow | paOutputOverflow)) {
            xrunCount_++;
            performanceStats_.xrunCount.fetch_add(1);
            lastXrun_ = callbackStart;
            if (xrunCallback_) {
                xrunCallback_(performanceStats_.xrunCount.load());
            }
            MIXMIND_LOG_WARNING("Audio xrun detected");
        }
        
        // Process audio commands from UI thread
        processCommands();
        
        // Get audio buffers from pool (zero-allocation)
        auto inputLease = inputBufferPool_->acquireLease();
        auto outputLease = outputBufferPool_->acquireLease();
        
        if (!inputLease || !outputLease) {
            // Buffer pool exhausted - silence output and continue
            std::memset(outputBuffer, 0, framesPerBuffer * outputChannels_ * sizeof(float));
            RT_LOG_WARNING("Buffer pool exhausted");
            return paContinue;
        }
        
        // Copy input data to our buffer format
        const float* input = static_cast<const float*>(inputBuffer);
        for (int ch = 0; ch < inputChannels_; ++ch) {
            float* channelData = inputLease->getChannelData(ch);
            for (unsigned long frame = 0; frame < framesPerBuffer; ++frame) {
                channelData[frame] = input[frame * inputChannels_ + ch];
            }
        }
        
        // Process audio through the chain
        processAudioChain(*inputLease, *outputLease, framesPerBuffer);
        
        // Apply master volume and copy output data back to PortAudio format
        float* output = static_cast<float*>(outputBuffer);
        float masterVol = static_cast<float>(masterVolume_.load());
        
        for (int ch = 0; ch < outputChannels_; ++ch) {
            const float* channelData = outputLease->getChannelData(ch);
            for (unsigned long frame = 0; frame < framesPerBuffer; ++frame) {
                float sample = channelData[frame] * masterVol;
                
                // Soft clipping
                if (sample > 1.0f) sample = 1.0f;
                else if (sample < -1.0f) sample = -1.0f;
                
                output[frame * outputChannels_ + ch] = sample;
            }
        }
        
        // Input monitoring if enabled
        if (inputMonitoringEnabled_.load() && inputBuffer) {
            const float* input = static_cast<const float*>(inputBuffer);
            float monitorGain = 0.5f; // -6dB monitoring level
            
            for (int ch = 0; ch < std::min(inputChannels_, outputChannels_); ++ch) {
                for (unsigned long frame = 0; frame < framesPerBuffer; ++frame) {
                    size_t outputIndex = frame * outputChannels_ + ch;
                    size_t inputIndex = frame * inputChannels_ + ch;
                    
                    if (outputIndex < framesPerBuffer * outputChannels_ && inputIndex < framesPerBuffer * inputChannels_) {
                        output[outputIndex] += input[inputIndex] * monitorGain;
                        
                        // Prevent clipping
                        if (output[outputIndex] > 1.0f) output[outputIndex] = 1.0f;
                        else if (output[outputIndex] < -1.0f) output[outputIndex] = -1.0f;
                    }
                }
            }
        }
        
        // Call enhanced process callback
        if (processCallback_) {
            auto timestamp = std::chrono::nanoseconds(
                static_cast<int64_t>(timeInfo->inputBufferAdcTime * 1e9)
            );
            
            processCallback_(*inputLease, *outputLease, timestamp);
        }
        
        // Update performance statistics
        auto callbackEnd = std::chrono::high_resolution_clock::now();
        updatePerformanceStats(callbackStart, callbackEnd, framesPerBuffer);
        
        return shouldStop_.load() ? paComplete : paContinue;
    }
    
    void processCommands() {
        AudioCommand cmd;
        while (commandQueue_->dequeue(cmd)) {
            switch (cmd.type) {
                case AudioCommand::SET_PARAMETER:
                    // Handle parameter changes in real-time thread
                    for (auto& processor : processors_) {
                        processor->setParameter(cmd.parameterId, cmd.value);
                    }
                    break;
                case AudioCommand::SET_BYPASS:
                    // Handle bypass changes
                    for (auto& processor : processors_) {
                        processor->setBypassed(cmd.boolValue);
                    }
                    break;
                case AudioCommand::RESET_STATE:
                    // Reset all processors
                    for (auto& processor : processors_) {
                        processor->reset();
                    }
                    break;
                default:
                    break;
            }
        }
    }
    
    void processAudioChain(AudioBufferPool::AudioBuffer& input, 
                          AudioBufferPool::AudioBuffer& output, 
                          unsigned long framesPerBuffer) {
        
        // Start with input as the source
        AudioBufferPool::AudioBuffer* currentBuffer = &input;
        
        // Process through the audio processor chain
        for (auto& processor : processors_) {
            if (!processor->isBypassed()) {
                processor->processAudio(*currentBuffer, output, framesPerBuffer);
                currentBuffer = &output;  // Chain the processing
            }
        }
        
        // If no processing occurred, copy input to output
        if (currentBuffer == &input) {
            for (int ch = 0; ch < std::min(inputChannels_, outputChannels_); ++ch) {
                std::memcpy(output.getChannelData(ch), 
                           input.getChannelData(ch), 
                           framesPerBuffer * sizeof(float));
            }
        }
        
        // Call output callback for monitoring, recording, etc.
        if (outputCallback_) {
            outputCallback_(output, framesPerBuffer);
        }
    }
    
    void updatePerformanceStats(
        std::chrono::high_resolution_clock::time_point start,
        std::chrono::high_resolution_clock::time_point end,
        unsigned long framesPerBuffer) {
        
        auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double processingMs = processingTime.count() / 1000.0;
        
        // Calculate available time for this buffer
        double availableMs = (framesPerBuffer / sampleRate_) * 1000.0;
        double cpuUsage = (processingMs / availableMs) * 100.0;
        
        performanceStats_.currentCpuUsage.store(cpuUsage);
        
        // Update average CPU usage (exponential moving average)
        double currentAvg = performanceStats_.averageCpuUsage.load();
        double newAvg = (currentAvg * 0.95) + (cpuUsage * 0.05);
        performanceStats_.averageCpuUsage.store(newAvg);
        
        // Update peak CPU usage
        double currentPeak = performanceStats_.peakCpuUsage.load();
        if (cpuUsage > currentPeak) {
            performanceStats_.peakCpuUsage.store(cpuUsage);
        }
        
        // Update legacy stats
        cpuLoad_.store(cpuUsage);
        
        // Check for overload condition
        if (cpuUsage > 90.0) {
            performanceStats_.isOverloaded.store(true);
        } else if (cpuUsage < 80.0) {
            performanceStats_.isOverloaded.store(false);
        }
        
        performanceStats_.processedBuffers.fetch_add(1);
        lastProcessTime_ = end;
    }
    
    double measureRoundTripLatency() {
        if (!stream_ || !streamOpen_) {
            return 0.0;
        }
        
        // This would implement a round-trip latency measurement
        // using a test signal and correlation detection
        // For now, estimate from device latencies
        
        const PaStreamInfo* streamInfo = Pa_GetStreamInfo(stream_);
        if (streamInfo) {
            return (streamInfo->inputLatency + streamInfo->outputLatency) * 1000.0; // Convert to ms
        }
        
        return 0.0;
    }
};

// ============================================================================
// RealtimeAudioEngine Public Interface
// ============================================================================

RealtimeAudioEngine::RealtimeAudioEngine() 
    : pImpl_(std::make_unique<Impl>()) {
}

RealtimeAudioEngine::~RealtimeAudioEngine() {
    stop();
}

bool RealtimeAudioEngine::initialize(const AudioConfig& config) {
    std::lock_guard<std::mutex> lock(pImpl_->configMutex_);
    
    // Update configuration
    pImpl_->sampleRate_ = config.sampleRate;
    pImpl_->framesPerBuffer_ = config.framesPerBuffer;
    pImpl_->inputChannels_ = config.inputChannels;
    pImpl_->outputChannels_ = config.outputChannels;
    
    return pImpl_->initialize();
}

bool RealtimeAudioEngine::start() {
    if (pImpl_->isRunning_.load()) {
        return true; // Already running
    }
    
    // Open PortAudio stream
    PaError err = Pa_OpenStream(
        &pImpl_->stream_,
        &pImpl_->inputParams_,
        &pImpl_->outputParams_,
        pImpl_->sampleRate_,
        pImpl_->framesPerBuffer_,
        paNoFlag,
        Impl::portAudioCallback,
        pImpl_.get()
    );
    
    if (err != paNoError) {
        RT_LOG_ERROR("Failed to open PortAudio stream");
        return false;
    }
    
    // Start the stream
    err = Pa_StartStream(pImpl_->stream_);
    if (err != paNoError) {
        RT_LOG_ERROR("Failed to start PortAudio stream");
        Pa_CloseStream(pImpl_->stream_);
        pImpl_->stream_ = nullptr;
        return false;
    }
    
    pImpl_->isRunning_.store(true);
    pImpl_->shouldStop_.store(false);
    
    RT_LOG_INFO("RealtimeAudioEngine started successfully");
    return true;
}

void RealtimeAudioEngine::stop() {
    if (!pImpl_->isRunning_.load()) {
        return;
    }
    
    pImpl_->shouldStop_.store(true);
    
    if (pImpl_->stream_) {
        Pa_StopStream(pImpl_->stream_);
        Pa_CloseStream(pImpl_->stream_);
        pImpl_->stream_ = nullptr;
    }
    
    pImpl_->isRunning_.store(false);
    RT_LOG_INFO("RealtimeAudioEngine stopped");
}

bool RealtimeAudioEngine::isRunning() const {
    return pImpl_->isRunning_.load();
}

void RealtimeAudioEngine::addProcessor(std::unique_ptr<AudioProcessor> processor) {
    std::lock_guard<std::mutex> lock(pImpl_->configMutex_);
    
    // Initialize processor with current configuration
    processor->initialize(pImpl_->sampleRate_, pImpl_->framesPerBuffer_);
    pImpl_->processors_.push_back(std::move(processor));
}

void RealtimeAudioEngine::setParameter(int processorId, int parameterId, float value) {
    AudioCommand cmd(AudioCommand::SET_PARAMETER, parameterId, value);
    pImpl_->commandQueue_->enqueue(cmd);
}

void RealtimeAudioEngine::setInputCallback(AudioInputCallback callback) {
    pImpl_->inputCallback_ = std::move(callback);
}

void RealtimeAudioEngine::setOutputCallback(AudioOutputCallback callback) {
    pImpl_->outputCallback_ = std::move(callback);
}

AudioStats RealtimeAudioEngine::getStats() const {
    AudioStats stats;
    stats.cpuLoad = pImpl_->cpuLoad_.load();
    stats.xrunCount = pImpl_->xrunCount_.load();
    stats.isRunning = pImpl_->isRunning_.load();
    stats.sampleRate = pImpl_->sampleRate_;
    stats.framesPerBuffer = pImpl_->framesPerBuffer_;
    stats.inputChannels = pImpl_->inputChannels_;
    stats.outputChannels = pImpl_->outputChannels_;
    
    if (pImpl_->stream_) {
        const PaStreamInfo* streamInfo = Pa_GetStreamInfo(pImpl_->stream_);
        if (streamInfo) {
            stats.inputLatency = streamInfo->inputLatency;
            stats.outputLatency = streamInfo->outputLatency;
        }
    }
    
    return stats;
}

// Enhanced Methods Implementation
mixmind::core::Result<void> RealtimeAudioEngine::initialize() {
    return pImpl_->initializePortAudio();
}

void RealtimeAudioEngine::shutdown() {
    pImpl_->cleanup();
}

bool RealtimeAudioEngine::isInitialized() const {
    return pImpl_->portAudioInitialized_;
}

std::vector<AudioDeviceInfo> RealtimeAudioEngine::getAvailableDevices() {
    return pImpl_->getAvailableDevices();
}

AudioDeviceInfo RealtimeAudioEngine::getDeviceInfo(int deviceIndex) {
    auto devices = getAvailableDevices();
    for (const auto& device : devices) {
        if (device.deviceIndex == deviceIndex) {
            return device;
        }
    }
    return AudioDeviceInfo{};
}

std::vector<AudioDeviceInfo> RealtimeAudioEngine::getInputDevices() {
    auto devices = getAvailableDevices();
    std::vector<AudioDeviceInfo> inputDevices;
    
    for (const auto& device : devices) {
        if (device.maxInputChannels > 0) {
            inputDevices.push_back(device);
        }
    }
    
    return inputDevices;
}

std::vector<AudioDeviceInfo> RealtimeAudioEngine::getOutputDevices() {
    auto devices = getAvailableDevices();
    std::vector<AudioDeviceInfo> outputDevices;
    
    for (const auto& device : devices) {
        if (device.maxOutputChannels > 0) {
            outputDevices.push_back(device);
        }
    }
    
    return outputDevices;
}

void RealtimeAudioEngine::setProcessCallback(AudioProcessCallback callback) {
    pImpl_->processCallback_ = std::move(callback);
}

void RealtimeAudioEngine::setErrorCallback(AudioErrorCallback callback) {
    pImpl_->errorCallback_ = std::move(callback);
}

void RealtimeAudioEngine::setStatusCallback(AudioStatusCallback callback) {
    pImpl_->statusCallback_ = std::move(callback);
}

void RealtimeAudioEngine::setXRunCallback(AudioXRunCallback callback) {
    pImpl_->xrunCallback_ = std::move(callback);
}

const AudioPerformanceStats& RealtimeAudioEngine::getPerformanceStats() const {
    return pImpl_->performanceStats_;
}

void RealtimeAudioEngine::resetPerformanceStats() {
    pImpl_->performanceStats_.reset();
}

double RealtimeAudioEngine::measureRoundTripLatency() {
    return pImpl_->measureRoundTripLatency();
}

AudioBufferPool& RealtimeAudioEngine::getBufferPool() {
    return *pImpl_->inputBufferPool_; // Return input pool as primary
}

const AudioBufferPool& RealtimeAudioEngine::getBufferPool() const {
    return *pImpl_->inputBufferPool_; // Return input pool as primary
}

void RealtimeAudioEngine::setMasterVolume(double volume) {
    pImpl_->masterVolume_.store(std::clamp(volume, 0.0, 2.0));
}

double RealtimeAudioEngine::getMasterVolume() const {
    return pImpl_->masterVolume_.load();
}

void RealtimeAudioEngine::setInputGain(double gain) {
    pImpl_->inputGain_.store(std::clamp(gain, 0.0, 4.0));
}

double RealtimeAudioEngine::getInputGain() const {
    return pImpl_->inputGain_.load();
}

void RealtimeAudioEngine::setInputMonitoring(bool enabled) {
    pImpl_->inputMonitoringEnabled_.store(enabled);
}

bool RealtimeAudioEngine::isInputMonitoringEnabled() const {
    return pImpl_->inputMonitoringEnabled_.load();
}

void RealtimeAudioEngine::setConfiguration(const AudioConfiguration& config) {
    std::lock_guard<std::mutex> lock(pImpl_->configMutex_);
    pImpl_->currentConfig_ = config;
}

AudioConfiguration RealtimeAudioEngine::getConfiguration() const {
    std::lock_guard<std::mutex> lock(pImpl_->configMutex_);
    return pImpl_->currentConfig_;
}

bool RealtimeAudioEngine::isStreamOpen() const {
    return pImpl_->streamOpen_;
}

bool RealtimeAudioEngine::isStreamRunning() const {
    return pImpl_->isRunning_.load();
}

// Global audio engine instance
static std::unique_ptr<RealtimeAudioEngine> g_audioEngine;

RealtimeAudioEngine& getGlobalAudioEngine() {
    if (!g_audioEngine) {
        g_audioEngine = std::make_unique<RealtimeAudioEngine>();
    }
    return *g_audioEngine;
}

void shutdownGlobalAudioEngine() {
    g_audioEngine.reset();
}

} // namespace mixmind::audio