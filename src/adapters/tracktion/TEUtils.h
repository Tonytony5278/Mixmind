#pragma once

#include "../../core/types.h"
#include "../../core/result.h"
#include <tracktion_engine/tracktion_engine.h>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <string>
#include <vector>
#include <optional>

namespace mixmind::adapters::tracktion {

// Namespace alias for Tracktion Engine
namespace te = tracktion;

// ============================================================================
// Type Conversions
// ============================================================================

class TETypeConverter {
public:
    // ========================================================================
    // Time Conversions
    // ========================================================================
    
    /// Convert samples to TE time units
    static te::BeatPosition samplesToBeats(core::TimestampSamples samples, double sampleRate, double bpm);
    
    /// Convert TE time units to samples
    static core::TimestampSamples beatsToSamples(te::BeatPosition beats, double sampleRate, double bpm);
    
    /// Convert seconds to TE time units
    static te::TimePosition secondsToTime(core::TimestampSeconds seconds);
    
    /// Convert TE time units to seconds
    static core::TimestampSeconds timeToSeconds(te::TimePosition time);
    
    /// Convert samples to seconds
    static core::TimestampSeconds samplesToSeconds(core::TimestampSamples samples, core::SampleRate sampleRate);
    
    /// Convert seconds to samples
    static core::TimestampSamples secondsToSamples(core::TimestampSeconds seconds, core::SampleRate sampleRate);
    
    // ========================================================================
    // Audio Format Conversions
    // ========================================================================
    
    /// Convert our audio format to JUCE format
    static juce::AudioFormat* getJuceAudioFormat(core::AudioFormat format);
    
    /// Convert JUCE format to our audio format
    static std::optional<core::AudioFormat> getAudioFormat(const juce::AudioFormat* format);
    
    /// Convert our sample rate to double
    static double sampleRateToDouble(core::SampleRate sampleRate);
    
    /// Convert double to our sample rate type
    static core::SampleRate doubleToSampleRate(double sampleRate);
    
    // ========================================================================
    // Buffer Conversions
    // ========================================================================
    
    /// Convert our audio buffer to JUCE audio buffer
    static juce::AudioBuffer<float> convertToJuceBuffer(const core::FloatAudioBuffer& buffer);
    
    /// Convert JUCE audio buffer to our buffer
    static core::FloatAudioBuffer convertFromJuceBuffer(const juce::AudioBuffer<float>& buffer);
    
    /// Convert our MIDI buffer to TE MIDI buffer
    static te::MidiMessageSequence convertToTEMidiSequence(const core::MidiBuffer& buffer);
    
    /// Convert TE MIDI sequence to our MIDI buffer
    static core::MidiBuffer convertFromTEMidiSequence(const te::MidiMessageSequence& sequence);
    
    // ========================================================================
    // Plugin Type Conversions
    // ========================================================================
    
    /// Convert TE plugin type to our plugin type
    static core::PluginType convertPluginType(te::Plugin::Type teType);
    
    /// Convert our plugin type to TE plugin type
    static te::Plugin::Type convertToTEPluginType(core::PluginType type);
    
    /// Convert TE plugin category to our category
    static core::PluginCategory convertPluginCategory(const juce::String& teCategory);
    
    /// Convert our plugin category to TE category string
    static juce::String convertToTEPluginCategory(core::PluginCategory category);
    
    // ========================================================================
    // String Conversions
    // ========================================================================
    
    /// Convert std::string to juce::String
    static juce::String toJuceString(const std::string& str);
    
    /// Convert juce::String to std::string
    static std::string fromJuceString(const juce::String& str);
    
    /// Convert file path for cross-platform compatibility
    static juce::File convertFilePath(const std::string& path);
    
    /// Convert juce::File to std::string path
    static std::string convertFromFile(const juce::File& file);
    
    // ========================================================================
    // Transport State Conversions
    // ========================================================================
    
    /// Convert TE transport state to our transport state
    static core::TransportState convertTransportState(te::TransportControl::PlayState teState);
    
    /// Convert our transport state to TE state
    static te::TransportControl::PlayState convertToTETransportState(core::TransportState state);
    
    /// Convert TE loop mode to our loop mode
    static core::LoopMode convertLoopMode(te::LoopInfo::LoopMode teMode);
    
    /// Convert our loop mode to TE loop mode
    static te::LoopInfo::LoopMode convertToTELoopMode(core::LoopMode mode);
    
    // ========================================================================
    // Color Conversions
    // ========================================================================
    
    /// Convert hex color string to JUCE Colour
    static juce::Colour convertToJuceColour(const std::string& hexColor);
    
    /// Convert JUCE Colour to hex color string
    static std::string convertFromJuceColour(juce::Colour colour);
    
    // ========================================================================
    // Parameter Conversions
    // ========================================================================
    
    /// Convert TE parameter info to our parameter info
    static core::IPluginInstance::ParameterInfo convertParameterInfo(const te::AutomatableParameter::Ptr& teParam);
    
    /// Normalize parameter value (0.0-1.0)
    static float normalizeParameterValue(float value, float minValue, float maxValue);
    
    /// Denormalize parameter value
    static float denormalizeParameterValue(float normalizedValue, float minValue, float maxValue);
    
    // ========================================================================
    // Error Conversions
    // ========================================================================
    
    /// Convert JUCE Result to our ErrorCode
    static core::ErrorCode convertErrorCode(const juce::Result& result);
    
    /// Convert exception to our ErrorCode
    static core::ErrorCode convertExceptionToErrorCode(const std::exception& e);
    
    /// Create error message from TE operation
    static std::string createErrorMessage(const std::string& operation, const juce::String& teMessage);
};

// ============================================================================
// TE Engine Utilities
// ============================================================================

class TEEngineUtils {
public:
    /// Initialize TE engine with our settings
    static std::unique_ptr<te::Engine> createEngine();
    
    /// Configure TE engine for optimal performance
    static void optimizeEngine(te::Engine& engine);
    
    /// Get TE engine version information
    static std::string getEngineVersion();
    
    /// Check TE engine capabilities
    struct EngineCapabilities {
        bool supportsVST = false;
        bool supportsVST3 = false;
        bool supportsAU = false;
        bool supportsLADSPA = false;
        bool supportsRack = false;
        std::vector<std::string> supportedAudioFormats;
        std::vector<core::SampleRate> supportedSampleRates;
        int32_t maxChannels = 0;
    };
    
    static EngineCapabilities getEngineCapabilities(te::Engine& engine);
    
    /// Setup TE audio device for testing/development
    static juce::Result setupDefaultAudioDevice(te::Engine& engine);
    
    /// Get TE engine statistics
    struct EngineStats {
        double cpuUsage = 0.0;
        size_t memoryUsage = 0;
        int32_t activeProjects = 0;
        int32_t loadedPlugins = 0;
        double sampleRate = 0.0;
        int32_t bufferSize = 0;
    };
    
    static EngineStats getEngineStats(te::Engine& engine);
};

// ============================================================================
// TE Progress Callback Adapter
// ============================================================================

class TEProgressCallback {
public:
    explicit TEProgressCallback(core::ProgressCallback coreCallback);
    
    /// Convert to TE ThreadPoolJobWithProgress callback
    std::function<bool(float)> asTECallback();
    
    /// Convert to JUCE ProgressCallback
    std::function<void(float, const juce::String&)> asJuceCallback();
    
private:
    core::ProgressCallback coreCallback_;
};

// ============================================================================
// TE Thread Safety Helpers
// ============================================================================

class TEThreadSafety {
public:
    /// Execute operation on message thread
    template<typename T>
    static T executeOnMessageThread(std::function<T()> operation);
    
    /// Execute operation on audio thread (if safe)
    template<typename T>
    static T executeOnAudioThread(std::function<T()> operation);
    
    /// Check if current thread is message thread
    static bool isMessageThread();
    
    /// Check if current thread is audio thread
    static bool isAudioThread();
    
    /// Assert message thread
    static void assertMessageThread(const std::string& operation);
    
    /// Assert not audio thread (for UI operations)
    static void assertNotAudioThread(const std::string& operation);
};

// ============================================================================
// TE Resource Management
// ============================================================================

class TEResourceManager {
public:
    /// RAII wrapper for TE objects
    template<typename T>
    class TEGuard {
    public:
        explicit TEGuard(T* object) : object_(object) {}
        ~TEGuard() { cleanup(); }
        
        TEGuard(const TEGuard&) = delete;
        TEGuard& operator=(const TEGuard&) = delete;
        
        TEGuard(TEGuard&& other) noexcept : object_(other.object_) {
            other.object_ = nullptr;
        }
        
        TEGuard& operator=(TEGuard&& other) noexcept {
            if (this != &other) {
                cleanup();
                object_ = other.object_;
                other.object_ = nullptr;
            }
            return *this;
        }
        
        T* get() const { return object_; }
        T& operator*() const { return *object_; }
        T* operator->() const { return object_; }
        
        T* release() {
            T* temp = object_;
            object_ = nullptr;
            return temp;
        }
        
    private:
        T* object_;
        void cleanup() { 
            // TE objects are typically managed by smart pointers
            // or the engine, so cleanup is usually automatic
        }
    };
    
    /// Create scoped TE object guard
    template<typename T>
    static TEGuard<T> createGuard(T* object) {
        return TEGuard<T>(object);
    }
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T>
inline T TEThreadSafety::executeOnMessageThread(std::function<T()> operation) {
    if (isMessageThread()) {
        return operation();
    } else {
        // Use JUCE MessageManager to execute on message thread
        T result{};
        juce::WaitableEvent event;
        
        juce::MessageManager::callAsync([&]() {
            result = operation();
            event.signal();
        });
        
        event.wait();
        return result;
    }
}

template<typename T>
inline T TEThreadSafety::executeOnAudioThread(std::function<T()> operation) {
    // For now, just execute directly
    // In a full implementation, this would need proper audio thread handling
    return operation();
}

} // namespace mixmind::adapters::tracktion