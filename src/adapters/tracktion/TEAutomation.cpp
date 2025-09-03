#include "TEAutomation.h"
#include "TEUtils.h"
#include <tracktion_engine/tracktion_engine.h>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TEAutomation Implementation
// ============================================================================

TEAutomation::TEAutomation(te::Engine& engine)
    : TEAdapter(engine)
{
    updateAutomationLaneMapping();
}

// Automation Lane Management

core::AsyncResult<core::Result<core::AutomationLaneID>> TEAutomation::createAutomationLane(
    core::TrackID trackId,
    const std::string& parameterName,
    AutomationTarget target
) {
    return executeAsync<core::Result<core::AutomationLaneID>>([this, trackId, parameterName, target]() -> core::Result<core::AutomationLaneID> {
        try {
            te::Edit* edit = getCurrentEdit();
            if (!edit) {
                return core::Result<core::AutomationLaneID>::failure("No active edit");
            }
            
            // Find the track
            te::Track* track = nullptr;
            for (auto* t : edit->getTrackList()) {
                if (t->getIndexInEditTrackList() == static_cast<int>(trackId.value())) {
                    track = t;
                    break;
                }
            }
            
            if (!track) {
                return core::Result<core::AutomationLaneID>::failure("Track not found");
            }
            
            // Get the automatable parameter
            te::AutomatableParameter* param = convertAutomationTargetToTEParameter(trackId, target);
            if (!param) {
                return core::Result<core::AutomationLaneID>::failure("Automation target not found");
            }
            
            // Create automation curve
            auto curve = new te::AutomationCurve(*param);
            param->attachToModifierSource(*curve);
            
            // Generate lane ID and add to mapping
            auto laneId = generateAutomationLaneID();
            
            {
                std::unique_lock<std::shared_mutex> lock(automationLaneMapMutex_);
                automationLaneMap_[laneId] = curve;
                reverseAutomationLaneMap_[curve] = laneId;
            }
            
            // Initialize recording state
            {
                std::unique_lock<std::shared_mutex> lock(recordingStateMutex_);
                recordingState_[laneId] = false;
                recordingModes_[laneId] = AutomationRecordMode::Touch;
            }
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::LaneCreated, laneId, "Automation lane created: " + parameterName);
            
            return core::Result<core::AutomationLaneID>::success(laneId);
            
        } catch (const std::exception& e) {
            return core::Result<core::AutomationLaneID>::failure(
                "Failed to create automation lane: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<core::AutomationLaneID>> TEAutomation::createPluginAutomationLane(
    core::PluginInstanceID pluginId,
    int32_t parameterId
) {
    return executeAsync<core::Result<core::AutomationLaneID>>([this, pluginId, parameterId]() -> core::Result<core::AutomationLaneID> {
        try {
            te::Edit* edit = getCurrentEdit();
            if (!edit) {
                return core::Result<core::AutomationLaneID>::failure("No active edit");
            }
            
            // Find the plugin (this requires coordination with TEPlugin)
            // For now, search through all tracks and plugins
            te::Plugin* targetPlugin = nullptr;
            te::AutomatableParameter* param = nullptr;
            
            for (auto* track : edit->getTrackList()) {
                if (auto* audioTrack = dynamic_cast<te::AudioTrack*>(track)) {
                    for (auto* plugin : audioTrack->pluginList) {
                        if (plugin) {
                            // Match plugin somehow - this requires plugin instance ID mapping
                            // For now, use parameter index to find the parameter
                            if (parameterId < plugin->getNumAutomatableParameters()) {
                                param = plugin->getAutomatableParameter(parameterId);
                                targetPlugin = plugin;
                                break;
                            }
                        }
                    }
                    if (targetPlugin) break;
                }
            }
            
            if (!param || !targetPlugin) {
                return core::Result<core::AutomationLaneID>::failure("Plugin parameter not found");
            }
            
            // Create automation curve
            auto curve = new te::AutomationCurve(*param);
            param->attachToModifierSource(*curve);
            
            // Generate lane ID and add to mapping
            auto laneId = generateAutomationLaneID();
            
            {
                std::unique_lock<std::shared_mutex> lock(automationLaneMapMutex_);
                automationLaneMap_[laneId] = curve;
                reverseAutomationLaneMap_[curve] = laneId;
            }
            
            // Initialize recording state
            {
                std::unique_lock<std::shared_mutex> lock(recordingStateMutex_);
                recordingState_[laneId] = false;
                recordingModes_[laneId] = AutomationRecordMode::Touch;
            }
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::LaneCreated, laneId, 
                               "Plugin automation lane created for parameter " + std::to_string(parameterId));
            
            return core::Result<core::AutomationLaneID>::success(laneId);
            
        } catch (const std::exception& e) {
            return core::Result<core::AutomationLaneID>::failure(
                "Failed to create plugin automation lane: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> TEAutomation::deleteAutomationLane(core::AutomationLaneID laneId) {
    return executeAsync<core::VoidResult>([this, laneId]() -> core::VoidResult {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::VoidResult::failure("Automation lane not found");
            }
            
            // Detach from parameter
            if (auto* param = getAutomatableParameter(laneId)) {
                param->detachFromModifierSource(*curve);
            }
            
            // Remove from mappings
            {
                std::unique_lock<std::shared_mutex> lock(automationLaneMapMutex_);
                reverseAutomationLaneMap_.erase(curve);
                automationLaneMap_.erase(laneId);
            }
            
            // Remove from recording state
            {
                std::unique_lock<std::shared_mutex> lock(recordingStateMutex_);
                recordingState_.erase(laneId);
                recordingModes_.erase(laneId);
            }
            
            // Remove automation points and generators
            {
                std::unique_lock<std::shared_mutex> lock(automationPointMapMutex_);
                automationPointMap_.erase(laneId);
            }
            
            {
                std::unique_lock<std::shared_mutex> lock(automationGeneratorMapMutex_);
                automationGeneratorMap_.erase(laneId);
            }
            
            // Delete the curve
            delete curve;
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::LaneDeleted, laneId, "Automation lane deleted");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to delete automation lane: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<std::vector<TEAutomation::AutomationLaneInfo>>> TEAutomation::getAutomationLanes(
    std::optional<core::TrackID> trackId
) const {
    return executeAsync<core::Result<std::vector<AutomationLaneInfo>>>([this, trackId]() -> core::Result<std::vector<AutomationLaneInfo>> {
        try {
            std::vector<AutomationLaneInfo> lanes;
            
            std::shared_lock<std::shared_mutex> lock(automationLaneMapMutex_);
            for (const auto& pair : automationLaneMap_) {
                if (pair.second) {
                    AutomationLaneInfo laneInfo = convertTECurveToLaneInfo(pair.second);
                    
                    // Filter by track if specified
                    if (trackId && laneInfo.trackId != trackId) {
                        continue;
                    }
                    
                    lanes.push_back(laneInfo);
                }
            }
            
            return core::Result<std::vector<AutomationLaneInfo>>::success(std::move(lanes));
            
        } catch (const std::exception& e) {
            return core::Result<std::vector<AutomationLaneInfo>>::failure(
                "Failed to get automation lanes: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<TEAutomation::AutomationLaneInfo>> TEAutomation::getAutomationLane(
    core::AutomationLaneID laneId
) const {
    return executeAsync<core::Result<AutomationLaneInfo>>([this, laneId]() -> core::Result<AutomationLaneInfo> {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::Result<AutomationLaneInfo>::failure("Automation lane not found");
            }
            
            return core::Result<AutomationLaneInfo>::success(convertTECurveToLaneInfo(curve));
            
        } catch (const std::exception& e) {
            return core::Result<AutomationLaneInfo>::failure(
                "Failed to get automation lane: " + std::string(e.what()));
        }
    });
}

// Automation Point Management

core::AsyncResult<core::Result<core::AutomationPointID>> TEAutomation::addAutomationPoint(
    core::AutomationLaneID laneId,
    core::TimePosition time,
    float value,
    CurveType curveType
) {
    return executeAsync<core::Result<core::AutomationPointID>>([this, laneId, time, value, curveType]() -> core::Result<core::AutomationPointID> {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::Result<core::AutomationPointID>::failure("Automation lane not found");
            }
            
            // Convert time to TE time
            double teTime = TEUtils::convertTimePosition(time);
            
            // Convert curve type
            te::CurveSource::Type teCurveType = convertCurveTypeToTE(curveType);
            
            // Add control point to curve
            curve->addControlPoint(teTime, value, teCurveType);
            
            // Generate point ID and add to mapping
            auto pointId = generateAutomationPointID();
            
            // Find the added control point (TE doesn't return it directly)
            te::AutomationCurve::ControlPoint* controlPoint = nullptr;
            for (int i = 0; i < curve->getNumControlPoints(); ++i) {
                auto* point = curve->getControlPoint(i);
                if (point && std::abs(point->time - teTime) < 0.001 && std::abs(point->value - value) < 0.001f) {
                    controlPoint = point;
                    break;
                }
            }
            
            if (controlPoint) {
                std::unique_lock<std::shared_mutex> lock(automationPointMapMutex_);
                automationPointMap_[laneId][pointId] = controlPoint;
            }
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::PointAdded, laneId, 
                               "Automation point added at time " + std::to_string(teTime));
            
            return core::Result<core::AutomationPointID>::success(pointId);
            
        } catch (const std::exception& e) {
            return core::Result<core::AutomationPointID>::failure(
                "Failed to add automation point: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> TEAutomation::removeAutomationPoint(
    core::AutomationLaneID laneId,
    core::AutomationPointID pointId
) {
    return executeAsync<core::VoidResult>([this, laneId, pointId]() -> core::VoidResult {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::VoidResult::failure("Automation lane not found");
            }
            
            // Find the control point
            te::AutomationCurve::ControlPoint* controlPoint = nullptr;
            {
                std::shared_lock<std::shared_mutex> lock(automationPointMapMutex_);
                auto laneIt = automationPointMap_.find(laneId);
                if (laneIt != automationPointMap_.end()) {
                    auto pointIt = laneIt->second.find(pointId);
                    if (pointIt != laneIt->second.end()) {
                        controlPoint = pointIt->second;
                    }
                }
            }
            
            if (!controlPoint) {
                return core::VoidResult::failure("Automation point not found");
            }
            
            // Remove control point from curve
            for (int i = 0; i < curve->getNumControlPoints(); ++i) {
                if (curve->getControlPoint(i) == controlPoint) {
                    curve->removeControlPoint(i);
                    break;
                }
            }
            
            // Remove from mapping
            {
                std::unique_lock<std::shared_mutex> lock(automationPointMapMutex_);
                auto laneIt = automationPointMap_.find(laneId);
                if (laneIt != automationPointMap_.end()) {
                    laneIt->second.erase(pointId);
                }
            }
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::PointRemoved, laneId, "Automation point removed");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to remove automation point: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> TEAutomation::updateAutomationPoint(
    core::AutomationLaneID laneId,
    core::AutomationPointID pointId,
    std::optional<core::TimePosition> newTime,
    std::optional<float> newValue,
    std::optional<CurveType> newCurveType
) {
    return executeAsync<core::VoidResult>([this, laneId, pointId, newTime, newValue, newCurveType]() -> core::VoidResult {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::VoidResult::failure("Automation lane not found");
            }
            
            // Find the control point
            te::AutomationCurve::ControlPoint* controlPoint = nullptr;
            int pointIndex = -1;
            {
                std::shared_lock<std::shared_mutex> lock(automationPointMapMutex_);
                auto laneIt = automationPointMap_.find(laneId);
                if (laneIt != automationPointMap_.end()) {
                    auto pointIt = laneIt->second.find(pointId);
                    if (pointIt != laneIt->second.end()) {
                        controlPoint = pointIt->second;
                        
                        // Find index
                        for (int i = 0; i < curve->getNumControlPoints(); ++i) {
                            if (curve->getControlPoint(i) == controlPoint) {
                                pointIndex = i;
                                break;
                            }
                        }
                    }
                }
            }
            
            if (!controlPoint || pointIndex < 0) {
                return core::VoidResult::failure("Automation point not found");
            }
            
            // Update point properties
            if (newTime) {
                controlPoint->time = TEUtils::convertTimePosition(*newTime);
            }
            
            if (newValue) {
                controlPoint->value = *newValue;
            }
            
            if (newCurveType) {
                controlPoint->curveType = convertCurveTypeToTE(*newCurveType);
            }
            
            // Mark curve as changed
            curve->sendChangeMessage();
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::PointUpdated, laneId, "Automation point updated");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to update automation point: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<std::vector<TEAutomation::AutomationPoint>>> TEAutomation::getAutomationPoints(
    core::AutomationLaneID laneId,
    std::optional<core::TimePosition> startTime,
    std::optional<core::TimePosition> endTime
) const {
    return executeAsync<core::Result<std::vector<AutomationPoint>>>([this, laneId, startTime, endTime]() -> core::Result<std::vector<AutomationPoint>> {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::Result<std::vector<AutomationPoint>>::failure("Automation lane not found");
            }
            
            std::vector<AutomationPoint> points;
            
            double teStartTime = startTime ? TEUtils::convertTimePosition(*startTime) : -std::numeric_limits<double>::max();
            double teEndTime = endTime ? TEUtils::convertTimePosition(*endTime) : std::numeric_limits<double>::max();
            
            for (int i = 0; i < curve->getNumControlPoints(); ++i) {
                auto* controlPoint = curve->getControlPoint(i);
                if (controlPoint && controlPoint->time >= teStartTime && controlPoint->time <= teEndTime) {
                    points.push_back(convertTEControlPointToCore(*controlPoint));
                }
            }
            
            return core::Result<std::vector<AutomationPoint>>::success(std::move(points));
            
        } catch (const std::exception& e) {
            return core::Result<std::vector<AutomationPoint>>::failure(
                "Failed to get automation points: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> TEAutomation::clearAutomationPoints(
    core::AutomationLaneID laneId,
    std::optional<core::TimePosition> startTime,
    std::optional<core::TimePosition> endTime
) {
    return executeAsync<core::VoidResult>([this, laneId, startTime, endTime]() -> core::VoidResult {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::VoidResult::failure("Automation lane not found");
            }
            
            double teStartTime = startTime ? TEUtils::convertTimePosition(*startTime) : -std::numeric_limits<double>::max();
            double teEndTime = endTime ? TEUtils::convertTimePosition(*endTime) : std::numeric_limits<double>::max();
            
            // Remove points in reverse order to avoid index issues
            for (int i = curve->getNumControlPoints() - 1; i >= 0; --i) {
                auto* controlPoint = curve->getControlPoint(i);
                if (controlPoint && controlPoint->time >= teStartTime && controlPoint->time <= teEndTime) {
                    curve->removeControlPoint(i);
                }
            }
            
            // Clear from point mapping (simplified - would need to track which points were removed)
            if (!startTime && !endTime) {
                std::unique_lock<std::shared_mutex> lock(automationPointMapMutex_);
                automationPointMap_.erase(laneId);
            }
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::PointsCleared, laneId, "Automation points cleared");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to clear automation points: " + std::string(e.what()));
        }
    });
}

// Automation Value Interpolation

core::AsyncResult<core::Result<float>> TEAutomation::getAutomationValueAtTime(
    core::AutomationLaneID laneId,
    core::TimePosition time
) const {
    return executeAsync<core::Result<float>>([this, laneId, time]() -> core::Result<float> {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::Result<float>::failure("Automation lane not found");
            }
            
            double teTime = TEUtils::convertTimePosition(time);
            float value = curve->getValueAtTime(teTime);
            
            return core::Result<float>::success(value);
            
        } catch (const std::exception& e) {
            return core::Result<float>::failure(
                "Failed to get automation value: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<std::vector<float>>> TEAutomation::getAutomationValuesInRange(
    core::AutomationLaneID laneId,
    core::TimePosition startTime,
    core::TimePosition endTime,
    int32_t sampleCount
) const {
    return executeAsync<core::Result<std::vector<float>>>([this, laneId, startTime, endTime, sampleCount]() -> core::Result<std::vector<float>> {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::Result<std::vector<float>>::failure("Automation lane not found");
            }
            
            std::vector<float> values;
            values.reserve(sampleCount);
            
            double teStartTime = TEUtils::convertTimePosition(startTime);
            double teEndTime = TEUtils::convertTimePosition(endTime);
            double timeStep = (teEndTime - teStartTime) / (sampleCount - 1);
            
            for (int32_t i = 0; i < sampleCount; ++i) {
                double currentTime = teStartTime + (i * timeStep);
                float value = curve->getValueAtTime(currentTime);
                values.push_back(value);
            }
            
            return core::Result<std::vector<float>>::success(std::move(values));
            
        } catch (const std::exception& e) {
            return core::Result<std::vector<float>>::failure(
                "Failed to get automation values in range: " + std::string(e.what()));
        }
    });
}

// Automation Recording

core::AsyncResult<core::VoidResult> TEAutomation::startAutomationRecording(
    core::AutomationLaneID laneId,
    AutomationRecordMode mode
) {
    return executeAsync<core::VoidResult>([this, laneId, mode]() -> core::VoidResult {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::VoidResult::failure("Automation lane not found");
            }
            
            // Set recording state
            {
                std::unique_lock<std::shared_mutex> lock(recordingStateMutex_);
                recordingState_[laneId] = true;
                recordingModes_[laneId] = mode;
            }
            
            // Configure TE automation recording
            if (auto* param = getAutomatableParameter(laneId)) {
                param->setAutomationActive(true);
                
                // Set recording mode
                switch (mode) {
                    case AutomationRecordMode::Write:
                        // Overwrite existing automation
                        break;
                    case AutomationRecordMode::Touch:
                        // Record only when parameter is being touched
                        break;
                    case AutomationRecordMode::Latch:
                        // Record from first touch until stopped
                        break;
                }
            }
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::RecordingStarted, laneId, "Automation recording started");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to start automation recording: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> TEAutomation::stopAutomationRecording(core::AutomationLaneID laneId) {
    return executeAsync<core::VoidResult>([this, laneId]() -> core::VoidResult {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::VoidResult::failure("Automation lane not found");
            }
            
            // Set recording state
            {
                std::unique_lock<std::shared_mutex> lock(recordingStateMutex_);
                recordingState_[laneId] = false;
            }
            
            // Stop TE automation recording
            if (auto* param = getAutomatableParameter(laneId)) {
                // TE handles recording stop automatically
            }
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::RecordingStopped, laneId, "Automation recording stopped");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to stop automation recording: " + std::string(e.what()));
        }
    });
}

bool TEAutomation::isAutomationRecording(core::AutomationLaneID laneId) const {
    std::shared_lock<std::shared_mutex> lock(recordingStateMutex_);
    auto it = recordingState_.find(laneId);
    return (it != recordingState_.end()) ? it->second : false;
}

// Automation Playback Control

core::AsyncResult<core::VoidResult> TEAutomation::setAutomationEnabled(
    core::AutomationLaneID laneId,
    bool enabled
) {
    return executeAsync<core::VoidResult>([this, laneId, enabled]() -> core::VoidResult {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::VoidResult::failure("Automation lane not found");
            }
            
            if (auto* param = getAutomatableParameter(laneId)) {
                param->setAutomationActive(enabled);
            }
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::EnabledChanged, laneId, 
                               enabled ? "Automation enabled" : "Automation disabled");
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to set automation enabled: " + std::string(e.what()));
        }
    });
}

bool TEAutomation::isAutomationEnabled(core::AutomationLaneID laneId) const {
    if (auto* param = getAutomatableParameter(laneId)) {
        return param->isAutomationActive();
    }
    return false;
}

// Automation Editing Operations

core::AsyncResult<core::VoidResult> TEAutomation::scaleAutomationValues(
    core::AutomationLaneID laneId,
    float scaleFactor,
    std::optional<core::TimePosition> startTime,
    std::optional<core::TimePosition> endTime
) {
    return executeAsync<core::VoidResult>([this, laneId, scaleFactor, startTime, endTime]() -> core::VoidResult {
        try {
            te::AutomationCurve* curve = getTEAutomationCurve(laneId);
            if (!curve) {
                return core::VoidResult::failure("Automation lane not found");
            }
            
            double teStartTime = startTime ? TEUtils::convertTimePosition(*startTime) : -std::numeric_limits<double>::max();
            double teEndTime = endTime ? TEUtils::convertTimePosition(*endTime) : std::numeric_limits<double>::max();
            
            // Scale points in range
            for (int i = 0; i < curve->getNumControlPoints(); ++i) {
                auto* controlPoint = curve->getControlPoint(i);
                if (controlPoint && controlPoint->time >= teStartTime && controlPoint->time <= teEndTime) {
                    controlPoint->value = std::clamp(controlPoint->value * scaleFactor, 0.0f, 1.0f);
                }
            }
            
            // Mark curve as changed
            curve->sendChangeMessage();
            
            // Emit automation event
            emitAutomationEvent(AutomationEventType::ValuesScaled, laneId, 
                               "Automation values scaled by " + std::to_string(scaleFactor));
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to scale automation values: " + std::string(e.what()));
        }
    });
}

// Global Automation Settings

core::AsyncResult<core::VoidResult> TEAutomation::setGlobalAutomationEnabled(bool enabled) {
    return executeAsync<core::VoidResult>([this, enabled]() -> core::VoidResult {
        try {
            globalAutomationEnabled_.store(enabled);
            
            // Apply to current edit if available
            if (te::Edit* edit = getCurrentEdit()) {
                edit->setAutomationReadMode(enabled ? te::Edit::AutomationReadMode::reads : te::Edit::AutomationReadMode::off);
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to set global automation enabled: " + std::string(e.what()));
        }
    });
}

bool TEAutomation::isGlobalAutomationEnabled() const {
    return globalAutomationEnabled_.load();
}

// Event Callbacks

void TEAutomation::setAutomationEventCallback(AutomationEventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    automationEventCallback_ = std::move(callback);
}

void TEAutomation::clearAutomationEventCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    automationEventCallback_ = nullptr;
}

// Protected Implementation Methods

te::AutomationCurve* TEAutomation::getTEAutomationCurve(core::AutomationLaneID laneId) const {
    std::shared_lock<std::shared_mutex> lock(automationLaneMapMutex_);
    auto it = automationLaneMap_.find(laneId);
    return (it != automationLaneMap_.end()) ? it->second : nullptr;
}

te::AutomatableParameter* TEAutomation::getAutomatableParameter(core::AutomationLaneID laneId) const {
    if (te::AutomationCurve* curve = getTEAutomationCurve(laneId)) {
        return &curve->getOwnerParameter();
    }
    return nullptr;
}

te::AutomatableParameter* TEAutomation::convertAutomationTargetToTEParameter(
    core::TrackID trackId,
    AutomationTarget target
) const {
    te::Edit* edit = getCurrentEdit();
    if (!edit) return nullptr;
    
    // Find the track
    te::Track* track = nullptr;
    for (auto* t : edit->getTrackList()) {
        if (t->getIndexInEditTrackList() == static_cast<int>(trackId.value())) {
            track = t;
            break;
        }
    }
    
    if (!track) return nullptr;
    
    // Convert target to TE parameter
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*>(track)) {
        switch (target) {
            case AutomationTarget::TrackVolume:
                return &audioTrack->getVolumeParameter();
            case AutomationTarget::TrackPan:
                return &audioTrack->getPanParameter();
            case AutomationTarget::TrackMute:
                return &audioTrack->getMuteParameter();
            case AutomationTarget::TrackSolo:
                return &audioTrack->getSoloParameter();
            case AutomationTarget::SendLevel:
                // Would need send index
                break;
            default:
                break;
        }
    }
    
    return nullptr;
}

te::CurveSource::Type TEAutomation::convertCurveTypeToTE(CurveType curveType) const {
    switch (curveType) {
        case CurveType::Linear:
            return te::CurveSource::linear;
        case CurveType::Exponential:
            return te::CurveSource::exponential;
        case CurveType::Logarithmic:
            return te::CurveSource::logarithmic;
        case CurveType::SCurve:
            return te::CurveSource::sCurve;
        case CurveType::Stepped:
            return te::CurveSource::stepped;
        default:
            return te::CurveSource::linear;
    }
}

CurveType TEAutomation::convertTECurveTypeToCore(te::CurveSource::Type teCurveType) const {
    switch (teCurveType) {
        case te::CurveSource::linear:
            return CurveType::Linear;
        case te::CurveSource::exponential:
            return CurveType::Exponential;
        case te::CurveSource::logarithmic:
            return CurveType::Logarithmic;
        case te::CurveSource::sCurve:
            return CurveType::SCurve;
        case te::CurveSource::stepped:
            return CurveType::Stepped;
        default:
            return CurveType::Linear;
    }
}

TEAutomation::AutomationLaneInfo TEAutomation::convertTECurveToLaneInfo(te::AutomationCurve* curve) const {
    AutomationLaneInfo info;
    
    // Find lane ID
    {
        std::shared_lock<std::shared_mutex> lock(automationLaneMapMutex_);
        auto it = reverseAutomationLaneMap_.find(curve);
        if (it != reverseAutomationLaneMap_.end()) {
            info.laneId = it->second;
        }
    }
    
    auto& param = curve->getOwnerParameter();
    info.parameterName = param.getParameterName().toStdString();
    info.displayName = param.getParameterName().toStdString();
    info.enabled = param.isAutomationActive();
    info.locked = false; // TE doesn't have direct lock concept
    
    // Find track ID (simplified)
    info.trackId = core::TrackID{0}; // Would need proper track lookup
    
    // Determine automation target
    info.target = AutomationTarget::TrackVolume; // Would need proper target detection
    
    return info;
}

TEAutomation::AutomationPoint TEAutomation::convertTEControlPointToCore(const te::AutomationCurve::ControlPoint& point) const {
    AutomationPoint corePoint;
    corePoint.time = TEUtils::convertFromTETime(point.time);
    corePoint.value = point.value;
    corePoint.curveType = convertTECurveTypeToCore(point.curveType);
    
    return corePoint;
}

void TEAutomation::updateAutomationLaneMapping() {
    // This would scan current edit for existing automation curves
    // and build the mapping - implementation depends on when this is called
}

core::AutomationLaneID TEAutomation::generateAutomationLaneID() {
    return core::AutomationLaneID{nextAutomationLaneId_.fetch_add(1)};
}

core::AutomationPointID TEAutomation::generateAutomationPointID() {
    return core::AutomationPointID{nextAutomationPointId_.fetch_add(1)};
}

core::AutomationGeneratorID TEAutomation::generateAutomationGeneratorID() {
    return core::AutomationGeneratorID{nextAutomationGeneratorId_.fetch_add(1)};
}

void TEAutomation::emitAutomationEvent(AutomationEventType eventType, core::AutomationLaneID laneId, const std::string& details) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (automationEventCallback_) {
        automationEventCallback_(eventType, laneId, details);
    }
}

te::Edit* TEAutomation::getCurrentEdit() const {
    std::lock_guard<std::mutex> lock(editMutex_);
    if (!currentEdit_) {
        currentEdit_ = engine_.getUIBehaviour().getCurrentlyFocusedEdit();
    }
    return currentEdit_;
}

// Additional methods would be implemented following the same patterns...
// Due to space constraints, showing the core structure and key functionality

} // namespace mixmind::adapters::tracktion