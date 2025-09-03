#pragma once

#include "TEAdapter.h"
#include "../../core/IAutomation.h"
#include "../../core/types.h"
#include "../../core/result.h"
#include <tracktion_engine/tracktion_engine.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TE Automation Adapter - Tracktion Engine implementation of IAutomation
// ============================================================================

class TEAutomation : public TEAdapter, public core::IAutomation {
public:
    explicit TEAutomation(te::Engine& engine);
    ~TEAutomation() override = default;
    
    // Non-copyable, movable
    TEAutomation(const TEAutomation&) = delete;
    TEAutomation& operator=(const TEAutomation&) = delete;
    TEAutomation(TEAutomation&&) = default;
    TEAutomation& operator=(TEAutomation&&) = default;
    
    // ========================================================================
    // IAutomation Implementation
    // ========================================================================
    
    // Automation Lane Management
    core::AsyncResult<core::Result<core::AutomationLaneID>> createAutomationLane(
        core::TrackID trackId,
        const std::string& parameterName,
        AutomationTarget target = AutomationTarget::TrackVolume
    ) override;
    
    core::AsyncResult<core::Result<core::AutomationLaneID>> createPluginAutomationLane(
        core::PluginInstanceID pluginId,
        int32_t parameterId
    ) override;
    
    core::AsyncResult<core::VoidResult> deleteAutomationLane(core::AutomationLaneID laneId) override;
    
    core::AsyncResult<core::Result<std::vector<AutomationLaneInfo>>> getAutomationLanes(
        std::optional<core::TrackID> trackId = std::nullopt
    ) const override;
    
    core::AsyncResult<core::Result<AutomationLaneInfo>> getAutomationLane(
        core::AutomationLaneID laneId
    ) const override;
    
    // Automation Point Management
    core::AsyncResult<core::Result<core::AutomationPointID>> addAutomationPoint(
        core::AutomationLaneID laneId,
        core::TimePosition time,
        float value,
        CurveType curveType = CurveType::Linear
    ) override;
    
    core::AsyncResult<core::VoidResult> removeAutomationPoint(
        core::AutomationLaneID laneId,
        core::AutomationPointID pointId
    ) override;
    
    core::AsyncResult<core::VoidResult> updateAutomationPoint(
        core::AutomationLaneID laneId,
        core::AutomationPointID pointId,
        std::optional<core::TimePosition> newTime = std::nullopt,
        std::optional<float> newValue = std::nullopt,
        std::optional<CurveType> newCurveType = std::nullopt
    ) override;
    
    core::AsyncResult<core::Result<std::vector<AutomationPoint>>> getAutomationPoints(
        core::AutomationLaneID laneId,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) const override;
    
    core::AsyncResult<core::VoidResult> clearAutomationPoints(
        core::AutomationLaneID laneId,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) override;
    
    // Automation Value Interpolation
    core::AsyncResult<core::Result<float>> getAutomationValueAtTime(
        core::AutomationLaneID laneId,
        core::TimePosition time
    ) const override;
    
    core::AsyncResult<core::Result<std::vector<float>>> getAutomationValuesInRange(
        core::AutomationLaneID laneId,
        core::TimePosition startTime,
        core::TimePosition endTime,
        int32_t sampleCount
    ) const override;
    
    // Automation Recording
    core::AsyncResult<core::VoidResult> startAutomationRecording(
        core::AutomationLaneID laneId,
        AutomationRecordMode mode = AutomationRecordMode::Touch
    ) override;
    
    core::AsyncResult<core::VoidResult> stopAutomationRecording(core::AutomationLaneID laneId) override;
    
    core::AsyncResult<core::VoidResult> pauseAutomationRecording(core::AutomationLaneID laneId) override;
    
    bool isAutomationRecording(core::AutomationLaneID laneId) const override;
    
    core::AsyncResult<core::VoidResult> setAutomationRecordMode(
        core::AutomationLaneID laneId,
        AutomationRecordMode mode
    ) override;
    
    // Automation Playback Control
    core::AsyncResult<core::VoidResult> setAutomationEnabled(
        core::AutomationLaneID laneId,
        bool enabled
    ) override;
    
    bool isAutomationEnabled(core::AutomationLaneID laneId) const override;
    
    core::AsyncResult<core::VoidResult> setAutomationLocked(
        core::AutomationLaneID laneId,
        bool locked
    ) override;
    
    bool isAutomationLocked(core::AutomationLaneID laneId) const override;
    
    // Automation Editing Operations
    core::AsyncResult<core::VoidResult> scaleAutomationValues(
        core::AutomationLaneID laneId,
        float scaleFactor,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) override;
    
    core::AsyncResult<core::VoidResult> offsetAutomationValues(
        core::AutomationLaneID laneId,
        float offset,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) override;
    
    core::AsyncResult<core::VoidResult> smoothAutomation(
        core::AutomationLaneID laneId,
        float strength,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) override;
    
    core::AsyncResult<core::VoidResult> quantizeAutomationPoints(
        core::AutomationLaneID laneId,
        core::TimeDuration gridSize,
        float strength = 1.0f
    ) override;
    
    // Automation Templates and Patterns
    core::AsyncResult<core::VoidResult> saveAutomationPattern(
        core::AutomationLaneID laneId,
        const std::string& patternName,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) override;
    
    core::AsyncResult<core::VoidResult> loadAutomationPattern(
        core::AutomationLaneID laneId,
        const std::string& patternName,
        core::TimePosition insertTime
    ) override;
    
    core::AsyncResult<core::Result<std::vector<std::string>>> getAutomationPatterns() const override;
    
    core::AsyncResult<core::VoidResult> deleteAutomationPattern(const std::string& patternName) override;
    
    // LFO and Generator Automation
    core::AsyncResult<core::Result<core::AutomationGeneratorID>> addAutomationGenerator(
        core::AutomationLaneID laneId,
        AutomationGeneratorType type,
        const AutomationGeneratorSettings& settings
    ) override;
    
    core::AsyncResult<core::VoidResult> removeAutomationGenerator(
        core::AutomationLaneID laneId,
        core::AutomationGeneratorID generatorId
    ) override;
    
    core::AsyncResult<core::VoidResult> updateAutomationGenerator(
        core::AutomationLaneID laneId,
        core::AutomationGeneratorID generatorId,
        const AutomationGeneratorSettings& settings
    ) override;
    
    core::AsyncResult<core::Result<std::vector<AutomationGeneratorInfo>>> getAutomationGenerators(
        core::AutomationLaneID laneId
    ) const override;
    
    // Automation Follows
    core::AsyncResult<core::VoidResult> setAutomationFollows(
        core::AutomationLaneID sourceLaneId,
        core::AutomationLaneID targetLaneId,
        AutomationFollowMode mode = AutomationFollowMode::Scale
    ) override;
    
    core::AsyncResult<core::VoidResult> removeAutomationFollows(
        core::AutomationLaneID sourceLaneId,
        core::AutomationLaneID targetLaneId
    ) override;
    
    core::AsyncResult<core::Result<std::vector<AutomationFollowInfo>>> getAutomationFollows(
        core::AutomationLaneID laneId
    ) const override;
    
    // Bulk Operations
    core::AsyncResult<core::VoidResult> copyAutomationData(
        core::AutomationLaneID sourceLaneId,
        core::AutomationLaneID targetLaneId,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) override;
    
    core::AsyncResult<core::VoidResult> moveAutomationData(
        core::AutomationLaneID laneId,
        core::TimeDuration timeOffset,
        std::optional<core::TimePosition> startTime = std::nullopt,
        std::optional<core::TimePosition> endTime = std::nullopt
    ) override;
    
    core::AsyncResult<core::VoidResult> deleteMultipleAutomationLanes(
        const std::vector<core::AutomationLaneID>& laneIds
    ) override;
    
    // Global Automation Settings
    core::AsyncResult<core::VoidResult> setGlobalAutomationEnabled(bool enabled) override;
    bool isGlobalAutomationEnabled() const override;
    
    core::AsyncResult<core::VoidResult> setAutomationRecordingEnabled(bool enabled) override;
    bool isAutomationRecordingEnabled() const override;
    
    core::AsyncResult<core::VoidResult> setAutomationReadMode(AutomationReadMode mode) override;
    AutomationReadMode getAutomationReadMode() const override;
    
    // Event Callbacks
    void setAutomationEventCallback(AutomationEventCallback callback) override;
    void clearAutomationEventCallback() override;

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Get Tracktion Engine automation curve by lane ID
    te::AutomationCurve* getTEAutomationCurve(core::AutomationLaneID laneId) const;
    
    /// Get automatable parameter for lane
    te::AutomatableParameter* getAutomatableParameter(core::AutomationLaneID laneId) const;
    
    /// Convert automation target to TE parameter
    te::AutomatableParameter* convertAutomationTargetToTEParameter(
        core::TrackID trackId,
        AutomationTarget target
    ) const;
    
    /// Convert curve type to TE curve type
    te::CurveSource::Type convertCurveTypeToTE(CurveType curveType) const;
    
    /// Convert TE curve type to our curve type
    CurveType convertTECurveTypeToCore(te::CurveSource::Type teCurveType) const;
    
    /// Convert automation generator type to TE modulator type
    te::ModifierSource::Type convertGeneratorTypeToTE(AutomationGeneratorType type) const;
    
    /// Convert TE automation curve to AutomationLaneInfo
    AutomationLaneInfo convertTECurveToLaneInfo(te::AutomationCurve* curve) const;
    
    /// Convert TE control point to AutomationPoint
    AutomationPoint convertTEControlPointToCore(const te::AutomationCurve::ControlPoint& point) const;
    
    /// Update automation lane mapping
    void updateAutomationLaneMapping();
    
    /// Generate unique automation lane ID
    core::AutomationLaneID generateAutomationLaneID();
    
    /// Generate unique automation point ID
    core::AutomationPointID generateAutomationPointID();
    
    /// Generate unique automation generator ID
    core::AutomationGeneratorID generateAutomationGeneratorID();
    
    /// Emit automation event
    void emitAutomationEvent(AutomationEventType eventType, core::AutomationLaneID laneId, const std::string& details);
    
    /// Get current edit for automation operations
    te::Edit* getCurrentEdit() const;

private:
    // Automation lane mapping
    std::unordered_map<core::AutomationLaneID, te::AutomationCurve*> automationLaneMap_;
    std::unordered_map<te::AutomationCurve*, core::AutomationLaneID> reverseAutomationLaneMap_;
    mutable std::shared_mutex automationLaneMapMutex_;
    
    // Automation point mapping
    std::unordered_map<core::AutomationLaneID, std::unordered_map<core::AutomationPointID, te::AutomationCurve::ControlPoint*>> automationPointMap_;
    mutable std::shared_mutex automationPointMapMutex_;
    
    // Automation generator mapping
    std::unordered_map<core::AutomationLaneID, std::unordered_map<core::AutomationGeneratorID, te::ModifierSource*>> automationGeneratorMap_;
    mutable std::shared_mutex automationGeneratorMapMutex_;
    
    // ID generation
    std::atomic<uint32_t> nextAutomationLaneId_{1};
    std::atomic<uint32_t> nextAutomationPointId_{1};
    std::atomic<uint32_t> nextAutomationGeneratorId_{1};
    
    // Recording state
    std::unordered_map<core::AutomationLaneID, bool> recordingState_;
    std::unordered_map<core::AutomationLaneID, AutomationRecordMode> recordingModes_;
    mutable std::shared_mutex recordingStateMutex_;
    
    // Global automation state
    std::atomic<bool> globalAutomationEnabled_{true};
    std::atomic<bool> automationRecordingEnabled_{true};
    AutomationReadMode automationReadMode_ = AutomationReadMode::Read;
    mutable std::mutex globalStateMutex_;
    
    // Event callback
    AutomationEventCallback automationEventCallback_;
    std::mutex callbackMutex_;
    
    // Current edit reference
    mutable te::Edit* currentEdit_ = nullptr;
    mutable std::mutex editMutex_;
    
    // Automation patterns storage
    std::unordered_map<std::string, std::vector<AutomationPoint>> automationPatterns_;
    mutable std::shared_mutex patternsMutex_;
};

} // namespace mixmind::adapters::tracktion