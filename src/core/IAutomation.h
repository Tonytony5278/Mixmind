#pragma once

#include "types.h"
#include "result.h"
#include <vector>
#include <memory>
#include <functional>

namespace mixmind::core {

// ============================================================================
// Automation Interface - Parameter automation over time
// ============================================================================

class IAutomation {
public:
    virtual ~IAutomation() = default;
    
    // ========================================================================
    // Automation Identity and Info
    // ========================================================================
    
    /// Get automation ID
    virtual AutomationID getId() const = 0;
    
    /// Get associated parameter ID
    virtual ParamID getParameterId() const = 0;
    
    /// Get parameter name
    virtual std::string getParameterName() const = 0;
    
    /// Get parameter units/label
    virtual std::string getParameterLabel() const = 0;
    
    /// Get parameter range
    virtual std::pair<float, float> getParameterRange() const = 0;
    
    // ========================================================================
    // Automation Mode Control
    // ========================================================================
    
    enum class AutomationMode {
        Off,        // No automation
        Read,       // Read automation data
        Write,      // Write/record automation
        Touch,      // Touch mode (write when touching control)
        Latch,      // Latch mode (write after touching until stop)
        Trim        // Trim mode (modify existing automation)
    };
    
    /// Set automation mode
    virtual AsyncResult<VoidResult> setMode(AutomationMode mode) = 0;
    
    /// Get current automation mode
    virtual AutomationMode getMode() const = 0;
    
    /// Enable/disable automation
    virtual AsyncResult<VoidResult> setEnabled(bool enabled) = 0;
    
    /// Check if automation is enabled
    virtual bool isEnabled() const = 0;
    
    /// Check if automation is currently writing
    virtual bool isWriting() const = 0;
    
    /// Check if automation is currently being touched
    virtual bool isTouched() const = 0;
    
    // ========================================================================
    // Automation Points and Curves
    // ========================================================================
    
    struct AutomationPoint {
        TimestampSamples time;
        float value;
        float tension = 0.0f;      // Curve tension (-1.0 to 1.0)
        
        enum class CurveType {
            Linear,
            Exponential,
            Logarithmic,
            SCurve,
            Bezier,
            Step,
            Smooth
        } curveType = CurveType::Linear;
        
        bool selected = false;
        
        AutomationPoint() = default;
        AutomationPoint(TimestampSamples t, float v, CurveType curve = CurveType::Linear)
            : time(t), value(v), curveType(curve) {}
    };
    
    /// Add automation point
    virtual AsyncResult<VoidResult> addPoint(const AutomationPoint& point) = 0;
    
    /// Add multiple points
    virtual AsyncResult<VoidResult> addPoints(const std::vector<AutomationPoint>& points) = 0;
    
    /// Remove automation point at specific time
    virtual AsyncResult<VoidResult> removePoint(TimestampSamples time) = 0;
    
    /// Remove point by index
    virtual AsyncResult<VoidResult> removePointByIndex(int32_t index) = 0;
    
    /// Remove all points in time range
    virtual AsyncResult<VoidResult> removePointsInRange(TimestampSamples start, TimestampSamples end) = 0;
    
    /// Move automation point
    virtual AsyncResult<VoidResult> movePoint(int32_t pointIndex, TimestampSamples newTime, float newValue) = 0;
    
    /// Modify point curve type
    virtual AsyncResult<VoidResult> setPointCurveType(int32_t pointIndex, AutomationPoint::CurveType curveType) = 0;
    
    /// Set point tension
    virtual AsyncResult<VoidResult> setPointTension(int32_t pointIndex, float tension) = 0;
    
    // ========================================================================
    // Point Selection and Editing
    // ========================================================================
    
    /// Select automation point
    virtual VoidResult selectPoint(int32_t pointIndex, bool selected = true) = 0;
    
    /// Select all points
    virtual VoidResult selectAllPoints() = 0;
    
    /// Clear point selection
    virtual VoidResult clearSelection() = 0;
    
    /// Select points in time range
    virtual VoidResult selectPointsInRange(TimestampSamples start, TimestampSamples end) = 0;
    
    /// Get selected points
    virtual std::vector<int32_t> getSelectedPointIndices() const = 0;
    
    /// Move selected points
    virtual AsyncResult<VoidResult> moveSelectedPoints(TimestampSamples deltaTime, float deltaValue) = 0;
    
    /// Delete selected points
    virtual AsyncResult<VoidResult> deleteSelectedPoints() = 0;
    
    // ========================================================================
    // Automation Data Access
    // ========================================================================
    
    /// Get all automation points
    virtual std::vector<AutomationPoint> getAllPoints() const = 0;
    
    /// Get points in time range
    virtual std::vector<AutomationPoint> getPointsInRange(TimestampSamples start, TimestampSamples end) const = 0;
    
    /// Get number of automation points
    virtual int32_t getPointCount() const = 0;
    
    /// Get automation point by index
    virtual std::optional<AutomationPoint> getPoint(int32_t index) const = 0;
    
    /// Find closest point to time
    virtual int32_t findClosestPoint(TimestampSamples time) const = 0;
    
    /// Get value at specific time (interpolated)
    virtual float getValueAtTime(TimestampSamples time) const = 0;
    
    /// Get interpolated values for time range
    virtual std::vector<float> getValuesInRange(TimestampSamples start, TimestampSamples end, int32_t sampleCount) const = 0;
    
    // ========================================================================
    // Automation Recording
    // ========================================================================
    
    /// Start recording automation
    virtual AsyncResult<VoidResult> startRecording() = 0;
    
    /// Stop recording automation
    virtual AsyncResult<VoidResult> stopRecording() = 0;
    
    /// Record automation value at current time
    virtual AsyncResult<VoidResult> recordValue(float value) = 0;
    
    /// Set recording thinning threshold (removes redundant points)
    virtual VoidResult setRecordingThinning(float threshold) = 0;
    
    /// Get recording thinning threshold
    virtual float getRecordingThinning() const = 0;
    
    /// Enable/disable recording quantization
    virtual VoidResult setRecordingQuantization(bool enabled) = 0;
    
    /// Check if recording quantization is enabled
    virtual bool isRecordingQuantization() const = 0;
    
    // ========================================================================
    // Automation Lanes and Layers
    // ========================================================================
    
    /// Get number of automation lanes
    virtual int32_t getLaneCount() const = 0;
    
    /// Add new automation lane
    virtual AsyncResult<Result<int32_t>> addLane() = 0;
    
    /// Remove automation lane
    virtual AsyncResult<VoidResult> removeLane(int32_t laneIndex) = 0;
    
    /// Get current active lane
    virtual int32_t getActiveLane() const = 0;
    
    /// Set active lane
    virtual VoidResult setActiveLane(int32_t laneIndex) = 0;
    
    /// Merge lanes
    virtual AsyncResult<VoidResult> mergeLanes(const std::vector<int32_t>& laneIndices, int32_t targetLane) = 0;
    
    // ========================================================================
    // Automation Editing Operations
    // ========================================================================
    
    /// Clear all automation data
    virtual AsyncResult<VoidResult> clear() = 0;
    
    /// Clear automation in time range
    virtual AsyncResult<VoidResult> clearRange(TimestampSamples start, TimestampSamples end) = 0;
    
    /// Copy automation from time range
    virtual std::vector<AutomationPoint> copyRange(TimestampSamples start, TimestampSamples end) const = 0;
    
    /// Paste automation at specific time
    virtual AsyncResult<VoidResult> pasteAtTime(const std::vector<AutomationPoint>& points, TimestampSamples time) = 0;
    
    /// Scale automation values in range
    virtual AsyncResult<VoidResult> scaleValuesInRange(TimestampSamples start, TimestampSamples end, float scaleFactor) = 0;
    
    /// Offset automation values in range
    virtual AsyncResult<VoidResult> offsetValuesInRange(TimestampSamples start, TimestampSamples end, float offset) = 0;
    
    /// Reverse automation in time range
    virtual AsyncResult<VoidResult> reverseRange(TimestampSamples start, TimestampSamples end) = 0;
    
    /// Smooth automation in range (reduce jitter)
    virtual AsyncResult<VoidResult> smoothRange(TimestampSamples start, TimestampSamples end, float strength = 0.5f) = 0;
    
    /// Quantize automation points to grid
    virtual AsyncResult<VoidResult> quantizeToGrid(TimestampSamples gridSize, TimestampSamples start = 0, TimestampSamples end = 0) = 0;
    
    // ========================================================================
    // Automation Patterns and Generation
    // ========================================================================
    
    enum class PatternType {
        Sine,
        Triangle,
        Square,
        Sawtooth,
        Random,
        Ramp,
        Custom
    };
    
    struct PatternConfig {
        PatternType type;
        TimestampSamples startTime;
        TimestampSamples duration;
        float minValue;
        float maxValue;
        float frequency = 1.0f;     // Hz
        float phase = 0.0f;         // 0.0 to 1.0
        float amplitude = 1.0f;
        float offset = 0.0f;
        int32_t steps = 16;         // For stepped patterns
        uint32_t randomSeed = 0;    // For random patterns
    };
    
    /// Generate automation pattern
    virtual AsyncResult<VoidResult> generatePattern(const PatternConfig& config) = 0;
    
    /// Create LFO automation
    virtual AsyncResult<VoidResult> createLFO(PatternType waveform, float frequency, float depth, TimestampSamples startTime, TimestampSamples duration) = 0;
    
    /// Create automation ramp
    virtual AsyncResult<VoidResult> createRamp(float startValue, float endValue, TimestampSamples startTime, TimestampSamples duration) = 0;
    
    // ========================================================================
    // Automation Templates and Presets
    // ========================================================================
    
    /// Save automation as template
    virtual AsyncResult<VoidResult> saveAsTemplate(const std::string& templateName, const std::string& description = "") = 0;
    
    /// Load automation template
    virtual AsyncResult<VoidResult> loadTemplate(const std::string& templateName) = 0;
    
    /// Get available automation templates
    virtual std::vector<std::string> getAvailableTemplates() const = 0;
    
    /// Export automation data
    virtual AsyncResult<VoidResult> exportToFile(const std::string& filePath) const = 0;
    
    /// Import automation data
    virtual AsyncResult<VoidResult> importFromFile(const std::string& filePath) = 0;
    
    // ========================================================================
    // Automation State and Persistence
    // ========================================================================
    
    /// Save automation state
    virtual AsyncResult<VoidResult> saveState(std::vector<uint8_t>& data) const = 0;
    
    /// Load automation state
    virtual AsyncResult<VoidResult> loadState(const std::vector<uint8_t>& data) = 0;
    
    /// Get automation length (time of last point)
    virtual TimestampSamples getLength() const = 0;
    
    /// Check if automation has any data
    virtual bool hasData() const = 0;
    
    /// Check if automation is empty
    virtual bool isEmpty() const = 0;
    
    // ========================================================================
    // Real-time Control Integration
    // ========================================================================
    
    /// Set real-time parameter value (for touch/latch modes)
    virtual AsyncResult<VoidResult> setRealtimeValue(float value) = 0;
    
    /// Get current real-time value
    virtual float getCurrentValue() const = 0;
    
    /// Touch parameter (start touch mode)
    virtual AsyncResult<VoidResult> touchParameter() = 0;
    
    /// Release parameter (end touch mode)
    virtual AsyncResult<VoidResult> releaseParameter() = 0;
    
    /// Set touch sensitivity
    virtual VoidResult setTouchSensitivity(float sensitivity) = 0;
    
    /// Get touch sensitivity
    virtual float getTouchSensitivity() const = 0;
    
    // ========================================================================
    // MIDI Control Integration
    // ========================================================================
    
    /// Map MIDI CC to automation
    virtual VoidResult mapMIDICC(int32_t channel, int32_t ccNumber) = 0;
    
    /// Unmap MIDI CC
    virtual VoidResult unmapMIDICC() = 0;
    
    /// Get MIDI CC mapping
    virtual std::optional<std::pair<int32_t, int32_t>> getMIDICCMapping() const = 0;
    
    /// Set MIDI learn mode
    virtual VoidResult setMIDILearnMode(bool enabled) = 0;
    
    /// Check if in MIDI learn mode
    virtual bool isMIDILearnMode() const = 0;
    
    // ========================================================================
    // Automation Display and Visualization
    // ========================================================================
    
    /// Set display range for UI
    virtual VoidResult setDisplayRange(float minValue, float maxValue) = 0;
    
    /// Get display range
    virtual std::pair<float, float> getDisplayRange() const = 0;
    
    /// Set automation color for UI
    virtual VoidResult setColor(const std::string& color) = 0;
    
    /// Get automation color
    virtual std::string getColor() const = 0;
    
    /// Set automation height for UI
    virtual VoidResult setHeight(int32_t height) = 0;
    
    /// Get automation height
    virtual int32_t getHeight() const = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class AutomationEvent {
        PointAdded,
        PointRemoved,
        PointMoved,
        PointSelected,
        ModeChanged,
        RecordingStarted,
        RecordingStopped,
        ValueChanged,
        TemplateLoaded,
        DataCleared
    };
    
    using AutomationEventCallback = std::function<void(AutomationEvent event, const std::string& details)>;
    
    /// Subscribe to automation events
    virtual void addEventListener(AutomationEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(AutomationEventCallback callback) = 0;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Set automation resolution (samples per point for dense automation)
    virtual VoidResult setResolution(int32_t samplesPerPoint) = 0;
    
    /// Get automation resolution
    virtual int32_t getResolution() const = 0;
    
    /// Enable/disable automation smoothing
    virtual VoidResult setSmoothingEnabled(bool enabled) = 0;
    
    /// Check if smoothing is enabled
    virtual bool isSmoothingEnabled() const = 0;
    
    /// Set smoothing amount
    virtual VoidResult setSmoothingAmount(float amount) = 0;
    
    /// Get smoothing amount
    virtual float getSmoothingAmount() const = 0;
    
    /// Set automation override (temporary manual control)
    virtual AsyncResult<VoidResult> setOverride(float value, bool enabled = true) = 0;
    
    /// Clear automation override
    virtual AsyncResult<VoidResult> clearOverride() = 0;
    
    /// Check if override is active
    virtual bool isOverrideActive() const = 0;
    
    /// Get override value
    virtual float getOverrideValue() const = 0;
};

} // namespace mixmind::core