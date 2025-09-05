#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <chrono>
#include <mutex>

namespace mixmind::automation {

// ============================================================================
// Real-Time Parameter Automation System
// ============================================================================

enum class AutomationMode {
    OFF,           // No automation
    READ,          // Read automation data
    WRITE,         // Write automation data
    TOUCH,         // Write when touching, read when not
    LATCH          // Write after first touch until stopped
};

enum class InterpolationType {
    NONE,          // Step/hold
    LINEAR,        // Linear interpolation
    CUBIC,         // Smooth cubic spline
    EXPONENTIAL,   // Exponential curve
    LOGARITHMIC    // Logarithmic curve
};

struct AutomationPoint {
    double timeSeconds = 0.0;      // Time position in seconds
    float value = 0.0f;            // Parameter value (0.0-1.0)
    InterpolationType interpolation = InterpolationType::LINEAR;
    float tension = 0.0f;          // Curve tension (-1.0 to 1.0)
    
    AutomationPoint() = default;
    AutomationPoint(double time, float val, InterpolationType interp = InterpolationType::LINEAR)
        : timeSeconds(time), value(val), interpolation(interp) {}
};

struct AutomationLane {
    std::string parameterId;
    std::string parameterName;
    std::string targetId;          // Plugin/processor ID
    std::vector<AutomationPoint> points;
    AutomationMode mode = AutomationMode::READ;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.5f;
    std::string units;
    bool isEnabled = true;
    bool isVisible = true;
    bool isLocked = false;
    
    // Real-time processing state
    mutable float lastValue = 0.0f;
    mutable size_t lastPointIndex = 0;
    mutable bool isDirty = false;
};

// Real-time safe automation value calculation
class AutomationProcessor {
public:
    AutomationProcessor();
    ~AutomationProcessor();
    
    // Real-time safe methods (called from audio thread)
    float calculateValue(const AutomationLane& lane, double timeSeconds) const;
    void processAutomation(const std::vector<AutomationLane*>& lanes, 
                          double timeSeconds,
                          std::function<void(const std::string&, const std::string&, float)> callback);
    
    // Configuration (called from UI thread)
    void setInterpolationQuality(int quality); // 1-4, higher = more CPU
    void setLookaheadSamples(int samples);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Parameter Automation Manager
// ============================================================================

class ParameterAutomationManager {
public:
    ParameterAutomationManager();
    ~ParameterAutomationManager();
    
    // Non-copyable
    ParameterAutomationManager(const ParameterAutomationManager&) = delete;
    ParameterAutomationManager& operator=(const ParameterAutomationManager&) = delete;
    
    // Automation lane management
    std::string createAutomationLane(const std::string& targetId, 
                                   const std::string& parameterId,
                                   const std::string& parameterName);
    bool removeAutomationLane(const std::string& laneId);
    AutomationLane* getAutomationLane(const std::string& laneId);
    const AutomationLane* getAutomationLane(const std::string& laneId) const;
    
    std::vector<std::string> getAutomationLaneIds() const;
    std::vector<AutomationLane*> getAutomationLanes();
    std::vector<const AutomationLane*> getAutomationLanes() const;
    
    // Find lanes by target
    std::vector<AutomationLane*> getAutomationLanesForTarget(const std::string& targetId);
    std::vector<const AutomationLane*> getAutomationLanesForTarget(const std::string& targetId) const;
    
    // Point manipulation
    bool addAutomationPoint(const std::string& laneId, const AutomationPoint& point);
    bool removeAutomationPoint(const std::string& laneId, size_t pointIndex);
    bool updateAutomationPoint(const std::string& laneId, size_t pointIndex, const AutomationPoint& point);
    bool moveAutomationPoint(const std::string& laneId, size_t pointIndex, double newTime, float newValue);
    
    // Bulk operations
    void addAutomationPoints(const std::string& laneId, const std::vector<AutomationPoint>& points);
    void removeAutomationPointsInRange(const std::string& laneId, double startTime, double endTime);
    void clearAutomationLane(const std::string& laneId);
    
    // Automation modes
    void setAutomationMode(const std::string& laneId, AutomationMode mode);
    AutomationMode getAutomationMode(const std::string& laneId) const;
    
    // Real-time processing
    void processAutomation(double timeSeconds, double sampleRate, int bufferSize);
    void setParameterCallback(std::function<void(const std::string&, const std::string&, float)> callback);
    
    // Recording automation
    void startRecording(const std::string& laneId);
    void stopRecording(const std::string& laneId);
    void recordParameterChange(const std::string& laneId, double timeSeconds, float value);
    bool isRecording(const std::string& laneId) const;
    
    // Touch automation
    void touchParameter(const std::string& laneId);
    void releaseParameter(const std::string& laneId);
    bool isParameterTouched(const std::string& laneId) const;
    
    // Automation editing
    void quantizeAutomation(const std::string& laneId, double quantizeValue);
    void smoothAutomation(const std::string& laneId, double startTime, double endTime, float factor);
    void scaleAutomation(const std::string& laneId, double startTime, double endTime, float scale, float offset);
    
    // Curve generation
    void createLinearRamp(const std::string& laneId, double startTime, float startValue, 
                         double endTime, float endValue);
    void createExponentialCurve(const std::string& laneId, double startTime, float startValue,
                               double endTime, float endValue, float curvature);
    void createSineCurve(const std::string& laneId, double startTime, double endTime,
                        float amplitude, float frequency, float phase, float offset);
    
    // Import/Export
    struct AutomationData {
        std::unordered_map<std::string, AutomationLane> lanes;
        double version = 1.0;
        std::string sessionId;
    };
    
    AutomationData exportAutomation() const;
    mixmind::core::Result<void> importAutomation(const AutomationData& data);
    mixmind::core::Result<void> saveToFile(const std::string& filePath) const;
    mixmind::core::AsyncResult<AutomationData> loadFromFile(const std::string& filePath);
    
    // Performance and statistics
    struct AutomationStats {
        size_t totalLanes = 0;
        size_t totalPoints = 0;
        size_t activeLanes = 0;
        double lastProcessingTimeMs = 0.0;
        double averageProcessingTimeMs = 0.0;
        bool hasOverruns = false;
    };
    
    AutomationStats getStats() const;
    void resetStats();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Advanced Automation Features
// ============================================================================

// Automation following (link parameters)
class AutomationLink {
public:
    AutomationLink(const std::string& sourceLaneId, const std::string& targetLaneId);
    ~AutomationLink();
    
    // Link configuration
    void setScaling(float scale, float offset);
    void setInverted(bool inverted);
    void setDelay(double delaySeconds);
    void setEnabled(bool enabled);
    
    // Processing
    void processLink(float sourceValue, double timeSeconds);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// Automation modulation (LFOs, envelopes)
class AutomationModulator {
public:
    enum class ModulatorType {
        LFO_SINE,
        LFO_TRIANGLE,
        LFO_SAWTOOTH,
        LFO_SQUARE,
        LFO_RANDOM,
        ENVELOPE_ADSR,
        ENVELOPE_CUSTOM,
        STEP_SEQUENCER
    };
    
    AutomationModulator(ModulatorType type);
    ~AutomationModulator();
    
    // Modulator parameters
    void setFrequency(float frequency);
    void setAmplitude(float amplitude);
    void setPhase(float phase);
    void setOffset(float offset);
    
    // ADSR envelope specific
    void setADSR(float attack, float decay, float sustain, float release);
    
    // Step sequencer specific
    void setStepPattern(const std::vector<float>& steps);
    void setStepLength(double stepLengthSeconds);
    
    // Processing
    float processModulation(double timeSeconds);
    void reset();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// Automation expression evaluation
class AutomationExpression {
public:
    AutomationExpression(const std::string& expression);
    ~AutomationExpression();
    
    // Variable binding
    void setVariable(const std::string& name, float value);
    void bindAutomationLane(const std::string& varName, const std::string& laneId);
    
    // Evaluation
    float evaluate(double timeSeconds);
    bool isValid() const;
    std::string getErrorMessage() const;
    
    // Supported functions: sin, cos, tan, exp, log, pow, abs, min, max, clamp, lerp, smoothstep
    // Variables: time, bpm, beat, bar, sample_rate
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Automation Timeline and Transport Integration
// ============================================================================

class AutomationTimeline {
public:
    AutomationTimeline();
    ~AutomationTimeline();
    
    // Timeline control
    void setTimePosition(double seconds);
    void setPlaying(bool playing);
    void setLooping(bool looping, double loopStart = 0.0, double loopEnd = 0.0);
    void setTempo(double bpm);
    void setTimeSignature(int numerator, int denominator);
    
    // Transport state
    double getCurrentTime() const;
    bool isPlaying() const;
    bool isLooping() const;
    double getTempo() const;
    
    // Timeline events
    using TimelineCallback = std::function<void(double timeSeconds, bool isPlaying)>;
    void setTimelineCallback(TimelineCallback callback);
    
    // Sync with external transport
    void syncWithExternalTransport(double timeSeconds, bool playing, double bpm);
    
    // Update (call from audio thread or timer)
    void update(double deltaTimeSeconds);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Real-Time Automation Engine
// ============================================================================

class RealTimeAutomationEngine {
public:
    RealTimeAutomationEngine();
    ~RealTimeAutomationEngine();
    
    // Integration with automation manager and timeline
    void setAutomationManager(std::shared_ptr<ParameterAutomationManager> manager);
    void setTimeline(std::shared_ptr<AutomationTimeline> timeline);
    
    // Real-time processing (call from audio thread)
    void processAutomation(double timeSeconds, double sampleRate, int bufferSize);
    
    // Parameter change notifications
    void setParameterChangeCallback(
        std::function<void(const std::string& targetId, const std::string& parameterId, float value)> callback);
    
    // Performance optimization
    void setThreadAffinity(int cpuCore);
    void enablePredictiveProcessing(bool enabled);
    void setProcessingPriority(int priority); // 0-99, higher = more priority
    
    // Statistics and monitoring
    struct ProcessingStats {
        double lastProcessingTimeUs = 0.0;
        double averageProcessingTimeUs = 0.0;
        double peakProcessingTimeUs = 0.0;
        size_t totalParameterUpdates = 0;
        size_t totalPointsProcessed = 0;
        bool hasTimingViolations = false;
    };
    
    ProcessingStats getProcessingStats() const;
    void resetProcessingStats();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace mixmind::automation