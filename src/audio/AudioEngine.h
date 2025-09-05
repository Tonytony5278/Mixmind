#pragma once

#include "../core/result.h"
#include <functional>
#include <memory>
#include <string>
#include <atomic>

namespace mixmind::audio {

// Audio configuration structure
struct AudioConfig {
    int sampleRate = 48000;
    int bufferSize = 512;        // Frames per buffer
    int inputChannels = 2;
    int outputChannels = 2;
    std::string deviceName;      // Empty = default device
    bool enableInput = false;    // Input disabled by default for output-only apps
    
    // Performance settings
    bool useExclusiveMode = false;
    bool enableAutoLatency = true;
    double suggestedLatency = 0.0; // Auto-detect if 0
};

// Audio callback function type
using AudioCallback = std::function<void(
    const float* inputBuffer,    // Input audio samples (can be nullptr)
    float* outputBuffer,         // Output audio samples 
    unsigned long frameCount     // Number of frames to process
)>;

// Audio device information
struct AudioDeviceInfo {
    int deviceIndex;
    std::string name;
    int maxInputChannels;
    int maxOutputChannels;
    double defaultSampleRate;
    double lowInputLatency;
    double lowOutputLatency;
    double highInputLatency;
    double highOutputLatency;
    bool isDefault;
};

// Audio engine for real-time audio processing
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    
    // Non-copyable, non-movable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;
    
    // ========================================================================
    // Engine Lifecycle
    // ========================================================================
    
    /// Initialize audio engine with configuration
    core::Result<void> initialize(const AudioConfig& config);
    
    /// Start audio processing
    core::Result<void> start();
    
    /// Stop audio processing  
    core::Result<void> stop();
    
    /// Shutdown audio engine
    void shutdown();
    
    /// Check if engine is running
    bool isRunning() const;
    
    /// Check if engine is initialized
    bool isInitialized() const;
    
    // ========================================================================
    // Audio Processing
    // ========================================================================
    
    /// Set audio processing callback
    void setAudioCallback(AudioCallback callback);
    
    /// Clear audio processing callback
    void clearAudioCallback();
    
    /// Get current audio configuration
    AudioConfig getConfig() const;
    
    /// Get actual latency information
    struct LatencyInfo {
        double inputLatency = 0.0;   // seconds
        double outputLatency = 0.0;  // seconds  
        double totalLatency = 0.0;   // seconds
        int actualBufferSize = 0;    // frames
        int actualSampleRate = 0;    // Hz
    };
    
    LatencyInfo getLatencyInfo() const;
    
    // ========================================================================
    // Device Management
    // ========================================================================
    
    /// Get list of available audio devices
    static std::vector<AudioDeviceInfo> getAvailableDevices();
    
    /// Get default input device
    static AudioDeviceInfo getDefaultInputDevice();
    
    /// Get default output device  
    static AudioDeviceInfo getDefaultOutputDevice();
    
    /// Find device by name
    static std::optional<AudioDeviceInfo> findDevice(const std::string& name);
    
    // ========================================================================
    // Performance Monitoring
    // ========================================================================
    
    /// Performance statistics
    struct PerformanceStats {
        double cpuLoad = 0.0;           // 0.0 to 1.0
        unsigned long totalCallbacks = 0;
        unsigned long underruns = 0;    // Buffer underruns
        unsigned long overruns = 0;     // Buffer overruns  
        double averageCallbackTime = 0.0; // milliseconds
        double maxCallbackTime = 0.0;   // milliseconds
    };
    
    PerformanceStats getPerformanceStats() const;
    
    /// Reset performance statistics
    void resetPerformanceStats();
    
    /// Enable/disable performance monitoring
    void setPerformanceMonitoring(bool enabled);
    
    // ========================================================================
    // Error Handling
    // ========================================================================
    
    /// Get last error message
    std::string getLastError() const;
    
    /// Error callback for handling audio errors
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    /// Set error callback
    void setErrorCallback(ErrorCallback callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================================
// Audio Engine Utilities
// ============================================================================

/// Convert sample rate to string
std::string sampleRateToString(int sampleRate);

/// Convert buffer size to latency estimate
double bufferSizeToLatency(int bufferSize, int sampleRate);

/// Convert latency to buffer size estimate  
int latencyToBufferSize(double latency, int sampleRate);

/// Validate audio configuration
core::Result<void> validateAudioConfig(const AudioConfig& config);

} // namespace mixmind::audio