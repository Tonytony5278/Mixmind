#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include "AudioBufferPool.h"
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>

// Forward declare PortAudio types
typedef void PaStream;
typedef struct PaDeviceInfo PaDeviceInfo;

namespace mixmind::audio {

// ============================================================================
// Audio Configuration & Types with Real PortAudio Support
// ============================================================================

enum class AudioDriverType {
    PORTAUDIO_DEFAULT,
    ASIO,        // Windows
    COREAUDIO,   // macOS  
    ALSA,        // Linux
    WASAPI,      // Windows
    DIRECTSOUND  // Windows
};

enum class AudioSampleFormat {
    FLOAT32,
    INT32,
    INT24,
    INT16
};

struct AudioDeviceInfo {
    int deviceIndex = -1;
    std::string name;
    std::string hostApi;
    int maxInputChannels = 0;
    int maxOutputChannels = 0;
    double defaultSampleRate = 44100.0;
    std::vector<double> supportedSampleRates;
    std::vector<int> supportedBufferSizes;
    bool isDefaultInput = false;
    bool isDefaultOutput = false;
    double inputLatency = 0.0;
    double outputLatency = 0.0;
    bool supportsExclusiveMode = false;
};

struct AudioConfiguration {
    // Device selection
    int inputDeviceIndex = -1;   // -1 for default
    int outputDeviceIndex = -1;  // -1 for default
    
    // Audio format
    double sampleRate = 44100.0;
    int bufferSize = 512;
    int inputChannels = 2;
    int outputChannels = 2;
    AudioSampleFormat sampleFormat = AudioSampleFormat::FLOAT32;
    
    // Driver preferences
    AudioDriverType preferredDriver = AudioDriverType::PORTAUDIO_DEFAULT;
    
    // Performance settings
    bool enableExclusiveMode = false;
    int suggestedInputLatency = 0;  // 0 for default
    int suggestedOutputLatency = 0; // 0 for default
    
    // Processing options
    bool enableInputMonitoring = false;
    bool enableMetronome = false;
    double masterVolume = 1.0;
    
    // Legacy compatibility
    unsigned long framesPerBuffer() const { return static_cast<unsigned long>(bufferSize); }
    bool lowLatencyMode = true;
    int inputDevice() const { return inputDeviceIndex; }
    int outputDevice() const { return outputDeviceIndex; }
    double inputLatency() const { return suggestedInputLatency / 1000.0; }
    double outputLatency() const { return suggestedOutputLatency / 1000.0; }
};

struct AudioPerformanceStats {
    std::atomic<double> inputLatencyMs{0.0};
    std::atomic<double> outputLatencyMs{0.0};
    std::atomic<double> roundTripLatencyMs{0.0};
    std::atomic<double> currentCpuUsage{0.0};
    std::atomic<double> averageCpuUsage{0.0};
    std::atomic<double> peakCpuUsage{0.0};
    std::atomic<int> xrunCount{0};
    std::atomic<int> processedBuffers{0};
    std::atomic<int> droppedBuffers{0};
    std::atomic<bool> isOverloaded{false};
    std::atomic<bool> isRunning{false};
    std::chrono::steady_clock::time_point startTime;
    
    AudioPerformanceStats() : startTime(std::chrono::steady_clock::now()) {}
    
    void reset() {
        inputLatencyMs = 0.0;
        outputLatencyMs = 0.0;
        roundTripLatencyMs = 0.0;
        currentCpuUsage = 0.0;
        averageCpuUsage = 0.0;
        peakCpuUsage = 0.0;
        xrunCount = 0;
        processedBuffers = 0;
        droppedBuffers = 0;
        isOverloaded = false;
        startTime = std::chrono::steady_clock::now();
    }
    
    // Legacy compatibility
    double cpuLoad() const { return currentCpuUsage.load(); }
    long xrunCount_legacy() const { return static_cast<long>(xrunCount.load()); }
    double sampleRate = 0.0;
    unsigned long framesPerBuffer = 0;
    int inputChannels = 0;
    int outputChannels = 0;
    double inputLatency() const { return inputLatencyMs.load(); }
    double outputLatency() const { return outputLatencyMs.load(); }
};

// Legacy typedef for compatibility
using AudioConfig = AudioConfiguration;
using AudioStats = AudioPerformanceStats;

// ============================================================================
// Audio Processing Interfaces
// ============================================================================

// Audio processing callback function type
using AudioProcessCallback = std::function<void(
    const AudioBufferPool::AudioBuffer& input,
    AudioBufferPool::AudioBuffer& output,
    std::chrono::nanoseconds timestamp
)>;

// Audio event callbacks
using AudioErrorCallback = std::function<void(const std::string& error)>;
using AudioStatusCallback = std::function<void(const std::string& status)>;
using AudioXRunCallback = std::function<void(int xrunCount)>;

class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;
    
    // Initialize processor with audio configuration
    virtual bool initialize(double sampleRate, unsigned long maxFramesPerBuffer) = 0;
    
    // Process audio (real-time thread safe)
    virtual void processAudio(
        const AudioBufferPool::AudioBuffer& input,
        AudioBufferPool::AudioBuffer& output,
        unsigned long framesPerBuffer) = 0;
    
    // Enhanced processing with timestamp support
    virtual void processAudioEnhanced(
        const AudioBufferPool::AudioBuffer& input,
        AudioBufferPool::AudioBuffer& output,
        std::chrono::nanoseconds timestamp) {
        // Default implementation calls legacy method
        processAudio(input, output, input.size() / input.channels);
    }
    
    // Parameter control (thread-safe)
    virtual void setParameter(int parameterId, float value) = 0;
    virtual float getParameter(int parameterId) const = 0;
    
    // Enhanced parameter control
    virtual void setParameter(const std::string& parameterName, float value) {
        // Default implementation - derived classes can override
        for (int i = 0; i < getParameterCount(); ++i) {
            if (parameterName == getParameterName(i)) {
                setParameter(i, value);
                break;
            }
        }
    }
    
    // State management
    virtual void setBypassed(bool bypassed) = 0;
    virtual bool isBypassed() const = 0;
    virtual void reset() = 0;
    
    // Processor information
    virtual const char* getName() const = 0;
    virtual int getParameterCount() const = 0;
    virtual const char* getParameterName(int parameterId) const = 0;
    
    // Performance monitoring
    virtual double getCurrentCpuUsage() const { return 0.0; }
    virtual int getLatencySamples() const { return 0; }
    virtual void resetPerformanceCounters() {}
    
    // Advanced features
    virtual bool isActive() const = 0;
    virtual void setActive(bool active) = 0;
};

// ============================================================================
// Audio Callbacks
// ============================================================================

using AudioInputCallback = std::function<void(
    const AudioBufferPool::AudioBuffer& inputBuffer, 
    unsigned long framesPerBuffer)>;

using AudioOutputCallback = std::function<void(
    const AudioBufferPool::AudioBuffer& outputBuffer, 
    unsigned long framesPerBuffer)>;

// ============================================================================
// Real-time Audio Engine - Professional DAW Core with PortAudio
// ============================================================================

class RealtimeAudioEngine {
public:
    RealtimeAudioEngine();
    ~RealtimeAudioEngine();
    
    // Non-copyable, non-movable
    RealtimeAudioEngine(const RealtimeAudioEngine&) = delete;
    RealtimeAudioEngine& operator=(const RealtimeAudioEngine&) = delete;
    RealtimeAudioEngine(RealtimeAudioEngine&&) = delete;
    RealtimeAudioEngine& operator=(RealtimeAudioEngine&&) = delete;
    
    // Initialization and cleanup
    mixmind::core::Result<void> initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Engine lifecycle (enhanced)
    bool initialize(const AudioConfig& config = AudioConfig{});
    mixmind::core::Result<void> initializePortAudio();
    bool start();
    void stop();
    bool isRunning() const;
    
    // Stream management
    mixmind::core::Result<void> openStream(const AudioConfiguration& config);
    mixmind::core::Result<void> closeStream();
    mixmind::core::Result<void> startStream();
    mixmind::core::Result<void> stopStream();
    bool isStreamOpen() const;
    bool isStreamRunning() const;
    
    // Device enumeration and selection
    std::vector<AudioDeviceInfo> getAvailableDevices();
    AudioDeviceInfo getDeviceInfo(int deviceIndex);
    std::vector<AudioDeviceInfo> getInputDevices();
    std::vector<AudioDeviceInfo> getOutputDevices();
    AudioDeviceInfo getDefaultInputDevice();
    AudioDeviceInfo getDefaultOutputDevice();
    
    // Configuration
    void setConfiguration(const AudioConfiguration& config);
    AudioConfiguration getConfiguration() const;
    mixmind::core::Result<void> validateConfiguration(const AudioConfiguration& config);
    
    // Buffer size and sample rate
    std::vector<int> getSupportedBufferSizes(int deviceIndex = -1);
    std::vector<double> getSupportedSampleRates(int deviceIndex = -1);
    mixmind::core::Result<void> setBufferSize(int bufferSize);
    mixmind::core::Result<void> setSampleRate(double sampleRate);
    
    // Audio processing chain
    void addProcessor(std::unique_ptr<AudioProcessor> processor);
    void removeProcessor(int processorId);
    void clearProcessors();
    
    // Processing callback registration
    void setProcessCallback(AudioProcessCallback callback);
    void setErrorCallback(AudioErrorCallback callback);
    void setStatusCallback(AudioStatusCallback callback);
    void setXRunCallback(AudioXRunCallback callback);
    
    // Real-time parameter control
    void setParameter(int processorId, int parameterId, float value);
    void setParameterByName(int processorId, const std::string& parameterName, float value);
    
    // Audio I/O callbacks (legacy compatibility)
    void setInputCallback(AudioInputCallback callback);
    void setOutputCallback(AudioOutputCallback callback);
    
    // Performance monitoring
    AudioStats getStats() const;
    const AudioPerformanceStats& getPerformanceStats() const;
    void resetPerformanceStats();
    double measureRoundTripLatency();
    
    // Audio buffer management
    AudioBufferPool& getBufferPool();
    const AudioBufferPool& getBufferPool() const;
    
    // Master controls
    void setMasterVolume(double volume);
    double getMasterVolume() const;
    void setInputGain(double gain);
    double getInputGain() const;
    void setInputMonitoring(bool enabled);
    bool isInputMonitoringEnabled() const;
    
    // Device management (enhanced)
    std::vector<std::string> getInputDevices() const;
    std::vector<std::string> getOutputDevices() const;
    bool setInputDevice(int deviceId);
    bool setOutputDevice(int deviceId);
    
    // Advanced features
    mixmind::core::Result<void> enableExclusiveMode(bool enable);
    bool isExclusiveModeEnabled() const;
    mixmind::core::Result<void> setThreadPriority(int priority);
    
    // Diagnostics and testing
    struct AudioTestResult {
        bool deviceAccessible = false;
        bool formatSupported = false;
        bool latencyAcceptable = false;
        double measuredInputLatency = 0.0;
        double measuredOutputLatency = 0.0;
        double measuredRoundTripLatency = 0.0;
        std::string errorMessage;
        std::vector<std::string> warnings;
    };
    
    AudioTestResult testConfiguration(const AudioConfiguration& config);
    mixmind::core::Result<void> performLatencyTest();
    
    // Host API information
    struct HostApiInfo {
        std::string name;
        int deviceCount;
        int defaultInputDevice;
        int defaultOutputDevice;
        bool supportsExclusiveMode;
        bool supportsCallbackMode;
        bool supportsBlockingMode;
    };
    
    std::vector<HostApiInfo> getHostApis();
    HostApiInfo getCurrentHostApi();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Global Audio Engine Access
// ============================================================================

// Get the global audio engine instance (singleton)
RealtimeAudioEngine& getGlobalAudioEngine();

// Shutdown the global audio engine (call at app exit)
void shutdownGlobalAudioEngine();

// ============================================================================
// Built-in Audio Processors
// ============================================================================

class BasicGainProcessor : public AudioProcessor {
public:
    enum Parameters {
        PARAM_GAIN = 0,
        PARAM_MUTE = 1,
        PARAM_COUNT
    };
    
    BasicGainProcessor() = default;
    
    bool initialize(double sampleRate, unsigned long maxFramesPerBuffer) override {
        sampleRate_ = sampleRate;
        maxBufferSize_ = maxFramesPerBuffer;
        isActive_ = true;
        return true;
    }
    
    void processAudio(
        const AudioBufferPool::AudioBuffer& input,
        AudioBufferPool::AudioBuffer& output,
        unsigned long framesPerBuffer) override {
        
        if (bypassed_ || muted_) {
            // Silence output
            for (size_t ch = 0; ch < input.channels; ++ch) {
                std::memset(output.getChannelData(ch), 0, 
                          framesPerBuffer * sizeof(float));
            }
            return;
        }
        
        float gain = gain_.load();
        for (size_t ch = 0; ch < input.channels; ++ch) {
            const float* inputChannel = input.getChannelData(ch);
            float* outputChannel = output.getChannelData(ch);
            
            for (unsigned long frame = 0; frame < framesPerBuffer; ++frame) {
                outputChannel[frame] = inputChannel[frame] * gain;
            }
        }
    }
    
    void setParameter(int parameterId, float value) override {
        switch (parameterId) {
            case PARAM_GAIN:
                gain_.store(value);
                break;
            case PARAM_MUTE:
                muted_ = (value > 0.5f);
                break;
        }
    }
    
    float getParameter(int parameterId) const override {
        switch (parameterId) {
            case PARAM_GAIN: return gain_.load();
            case PARAM_MUTE: return muted_ ? 1.0f : 0.0f;
            default: return 0.0f;
        }
    }
    
    void setBypassed(bool bypassed) override { bypassed_ = bypassed; }
    bool isBypassed() const override { return bypassed_; }
    void reset() override { /* No state to reset for gain */ }
    
    const char* getName() const override { return "Basic Gain"; }
    int getParameterCount() const override { return PARAM_COUNT; }
    const char* getParameterName(int parameterId) const override {
        switch (parameterId) {
            case PARAM_GAIN: return "Gain";
            case PARAM_MUTE: return "Mute";
            default: return "Unknown";
        }
    }
    
    // Enhanced AudioProcessor interface implementation
    bool isActive() const override { return isActive_.load(); }
    void setActive(bool active) override { isActive_.store(active); }
    double getCurrentCpuUsage() const override { return lastCpuUsage_.load(); }
    int getLatencySamples() const override { return 0; } // No latency for gain
    
    void processAudioEnhanced(
        const AudioBufferPool::AudioBuffer& input,
        AudioBufferPool::AudioBuffer& output,
        std::chrono::nanoseconds timestamp) override {
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        processAudio(input, output, input.size() / input.channels);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        double cpuUsage = (processingTime.count() / 1000.0) / ((input.size() / input.channels) / sampleRate_ * 1000.0) * 100.0;
        lastCpuUsage_.store(cpuUsage);
    }
    
private:
    std::atomic<float> gain_{1.0f};
    std::atomic<bool> muted_{false};
    std::atomic<bool> bypassed_{false};
    std::atomic<bool> isActive_{false};
    std::atomic<double> lastCpuUsage_{0.0};
    double sampleRate_ = 48000.0;
    unsigned long maxBufferSize_ = 512;
};

} // namespace mixmind::audio