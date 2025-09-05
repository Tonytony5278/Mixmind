#include "ParameterAutomation.h"
#include "../core/logging.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace mixmind::automation {

// ============================================================================
// Automation Processor Implementation
// ============================================================================

class AutomationProcessor::Impl {
public:
    int interpolationQuality_ = 2;
    int lookaheadSamples_ = 0;
    
    float calculateValueLinear(const AutomationLane& lane, double timeSeconds) const {
        if (lane.points.empty()) {
            return lane.defaultValue;
        }
        
        // Binary search for surrounding points
        auto it = std::lower_bound(lane.points.begin(), lane.points.end(), timeSeconds,
            [](const AutomationPoint& point, double time) {
                return point.timeSeconds < time;
            });
        
        if (it == lane.points.begin()) {
            return lane.points.front().value;
        }
        
        if (it == lane.points.end()) {
            return lane.points.back().value;
        }
        
        // Interpolate between points
        const AutomationPoint& nextPoint = *it;
        const AutomationPoint& prevPoint = *(it - 1);
        
        double timeDiff = nextPoint.timeSeconds - prevPoint.timeSeconds;
        if (timeDiff <= 0.0) {
            return prevPoint.value;
        }
        
        double t = (timeSeconds - prevPoint.timeSeconds) / timeDiff;
        
        switch (prevPoint.interpolation) {
            case InterpolationType::NONE:
                return prevPoint.value;
                
            case InterpolationType::LINEAR:
                return prevPoint.value + t * (nextPoint.value - prevPoint.value);
                
            case InterpolationType::CUBIC: {
                // Hermite interpolation with tension
                float tension = prevPoint.tension;
                float t2 = t * t;
                float t3 = t2 * t;
                
                float h1 = 2*t3 - 3*t2 + 1;
                float h2 = -2*t3 + 3*t2;
                float h3 = t3 - 2*t2 + t;
                float h4 = t3 - t2;
                
                float tangent1 = (1 - tension) * (nextPoint.value - prevPoint.value) * 0.5f;
                float tangent2 = tangent1;
                
                return h1 * prevPoint.value + h2 * nextPoint.value + h3 * tangent1 + h4 * tangent2;
            }
            
            case InterpolationType::EXPONENTIAL: {
                float curve = std::exp(prevPoint.tension * 2.0f); // -1 to 1 maps to ~0.14 to ~7.4
                float exponentialT = (std::exp(curve * t) - 1.0f) / (std::exp(curve) - 1.0f);
                return prevPoint.value + exponentialT * (nextPoint.value - prevPoint.value);
            }
            
            case InterpolationType::LOGARITHMIC: {
                float curve = std::exp(prevPoint.tension * 2.0f);
                float logT = std::log(1.0f + curve * t) / std::log(1.0f + curve);
                return prevPoint.value + logT * (nextPoint.value - prevPoint.value);
            }
            
            default:
                return prevPoint.value + t * (nextPoint.value - prevPoint.value);
        }
    }
    
    float calculateValueHighQuality(const AutomationLane& lane, double timeSeconds) const {
        // High-quality interpolation with sub-sample precision
        // This would implement more sophisticated algorithms for professional quality
        return calculateValueLinear(lane, timeSeconds);
    }
};

AutomationProcessor::AutomationProcessor()
    : pImpl_(std::make_unique<Impl>()) {
}

AutomationProcessor::~AutomationProcessor() = default;

float AutomationProcessor::calculateValue(const AutomationLane& lane, double timeSeconds) const {
    if (pImpl_->interpolationQuality_ >= 3) {
        return pImpl_->calculateValueHighQuality(lane, timeSeconds);
    } else {
        return pImpl_->calculateValueLinear(lane, timeSeconds);
    }
}

void AutomationProcessor::processAutomation(const std::vector<AutomationLane*>& lanes, 
                                          double timeSeconds,
                                          std::function<void(const std::string&, const std::string&, float)> callback) {
    
    for (auto* lane : lanes) {
        if (!lane || !lane->isEnabled || lane->mode == AutomationMode::OFF) {
            continue;
        }
        
        float value = calculateValue(*lane, timeSeconds);
        
        // Apply value scaling
        float scaledValue = lane->minValue + value * (lane->maxValue - lane->minValue);
        
        // Clamp to valid range
        scaledValue = std::clamp(scaledValue, lane->minValue, lane->maxValue);
        
        // Update cached value
        lane->lastValue = scaledValue;
        
        // Notify callback
        if (callback) {
            callback(lane->targetId, lane->parameterId, scaledValue);
        }
    }
}

void AutomationProcessor::setInterpolationQuality(int quality) {
    pImpl_->interpolationQuality_ = std::clamp(quality, 1, 4);
}

void AutomationProcessor::setLookaheadSamples(int samples) {
    pImpl_->lookaheadSamples_ = std::max(0, samples);
}

// ============================================================================
// Parameter Automation Manager Implementation  
// ============================================================================

class ParameterAutomationManager::Impl {
public:
    std::unordered_map<std::string, std::unique_ptr<AutomationLane>> lanes_;
    std::unordered_map<std::string, bool> recordingLanes_;
    std::unordered_map<std::string, bool> touchedParameters_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> touchStartTimes_;
    
    std::function<void(const std::string&, const std::string&, float)> parameterCallback_;
    AutomationProcessor processor_;
    
    mutable std::mutex lanesMutex_;
    std::atomic<size_t> nextLaneId_{1};
    
    // Performance stats
    mutable std::atomic<double> lastProcessingTimeMs_{0.0};
    mutable std::atomic<double> averageProcessingTimeMs_{0.0};
    mutable std::atomic<size_t> totalProcessingCalls_{0};
    mutable std::atomic<bool> hasOverruns_{false};
    
    std::string generateLaneId() {
        return "lane_" + std::to_string(nextLaneId_.fetch_add(1));
    }
    
    void updateProcessingStats(double processingTimeMs) {
        lastProcessingTimeMs_.store(processingTimeMs);
        
        // Update running average
        size_t calls = totalProcessingCalls_.fetch_add(1);
        double currentAvg = averageProcessingTimeMs_.load();
        double newAvg = (currentAvg * calls + processingTimeMs) / (calls + 1);
        averageProcessingTimeMs_.store(newAvg);
        
        // Check for overruns (more than 10% of available time at 44.1kHz/512 samples)
        if (processingTimeMs > 1.16) { // ~10% of 11.6ms available time
            hasOverruns_.store(true);
        }
    }
};

ParameterAutomationManager::ParameterAutomationManager()
    : pImpl_(std::make_unique<Impl>()) {
}

ParameterAutomationManager::~ParameterAutomationManager() = default;

std::string ParameterAutomationManager::createAutomationLane(const std::string& targetId, 
                                                           const std::string& parameterId,
                                                           const std::string& parameterName) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    std::string laneId = pImpl_->generateLaneId();
    
    auto lane = std::make_unique<AutomationLane>();
    lane->parameterId = parameterId;
    lane->parameterName = parameterName;
    lane->targetId = targetId;
    lane->mode = AutomationMode::READ;
    
    pImpl_->lanes_[laneId] = std::move(lane);
    
    MIXMIND_LOG_INFO("Created automation lane: {} for {}.{}", laneId, targetId, parameterId);
    
    return laneId;
}

bool ParameterAutomationManager::removeAutomationLane(const std::string& laneId) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it == pImpl_->lanes_.end()) {
        return false;
    }
    
    pImpl_->lanes_.erase(it);
    pImpl_->recordingLanes_.erase(laneId);
    
    MIXMIND_LOG_INFO("Removed automation lane: {}", laneId);
    return true;
}

AutomationLane* ParameterAutomationManager::getAutomationLane(const std::string& laneId) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    return (it != pImpl_->lanes_.end()) ? it->second.get() : nullptr;
}

const AutomationLane* ParameterAutomationManager::getAutomationLane(const std::string& laneId) const {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    return (it != pImpl_->lanes_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> ParameterAutomationManager::getAutomationLaneIds() const {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    std::vector<std::string> ids;
    ids.reserve(pImpl_->lanes_.size());
    
    for (const auto& [laneId, lane] : pImpl_->lanes_) {
        ids.push_back(laneId);
    }
    
    return ids;
}

std::vector<AutomationLane*> ParameterAutomationManager::getAutomationLanes() {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    std::vector<AutomationLane*> lanes;
    lanes.reserve(pImpl_->lanes_.size());
    
    for (const auto& [laneId, lane] : pImpl_->lanes_) {
        lanes.push_back(lane.get());
    }
    
    return lanes;
}

std::vector<const AutomationLane*> ParameterAutomationManager::getAutomationLanes() const {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    std::vector<const AutomationLane*> lanes;
    lanes.reserve(pImpl_->lanes_.size());
    
    for (const auto& [laneId, lane] : pImpl_->lanes_) {
        lanes.push_back(lane.get());
    }
    
    return lanes;
}

std::vector<AutomationLane*> ParameterAutomationManager::getAutomationLanesForTarget(const std::string& targetId) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    std::vector<AutomationLane*> targetLanes;
    
    for (const auto& [laneId, lane] : pImpl_->lanes_) {
        if (lane->targetId == targetId) {
            targetLanes.push_back(lane.get());
        }
    }
    
    return targetLanes;
}

bool ParameterAutomationManager::addAutomationPoint(const std::string& laneId, const AutomationPoint& point) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it == pImpl_->lanes_.end()) {
        return false;
    }
    
    auto& points = it->second->points;
    
    // Insert point in time-sorted order
    auto insertIt = std::upper_bound(points.begin(), points.end(), point,
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.timeSeconds < b.timeSeconds;
        });
    
    points.insert(insertIt, point);
    it->second->isDirty = true;
    
    return true;
}

bool ParameterAutomationManager::removeAutomationPoint(const std::string& laneId, size_t pointIndex) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it == pImpl_->lanes_.end() || pointIndex >= it->second->points.size()) {
        return false;
    }
    
    it->second->points.erase(it->second->points.begin() + pointIndex);
    it->second->isDirty = true;
    
    return true;
}

bool ParameterAutomationManager::updateAutomationPoint(const std::string& laneId, size_t pointIndex, const AutomationPoint& point) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it == pImpl_->lanes_.end() || pointIndex >= it->second->points.size()) {
        return false;
    }
    
    it->second->points[pointIndex] = point;
    
    // Re-sort if time changed
    std::sort(it->second->points.begin(), it->second->points.end(),
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.timeSeconds < b.timeSeconds;
        });
    
    it->second->isDirty = true;
    
    return true;
}

void ParameterAutomationManager::processAutomation(double timeSeconds, double sampleRate, int bufferSize) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Get active lanes (read-only access)
    std::vector<AutomationLane*> activeLanes;
    {
        std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
        activeLanes.reserve(pImpl_->lanes_.size());
        
        for (const auto& [laneId, lane] : pImpl_->lanes_) {
            if (lane->isEnabled && lane->mode != AutomationMode::OFF) {
                activeLanes.push_back(lane.get());
            }
        }
    }
    
    // Process automation (real-time safe)
    pImpl_->processor_.processAutomation(activeLanes, timeSeconds, pImpl_->parameterCallback_);
    
    // Update performance stats
    auto endTime = std::chrono::high_resolution_clock::now();
    double processingTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    pImpl_->updateProcessingStats(processingTimeMs);
}

void ParameterAutomationManager::setParameterCallback(
    std::function<void(const std::string&, const std::string&, float)> callback) {
    
    pImpl_->parameterCallback_ = std::move(callback);
}

void ParameterAutomationManager::startRecording(const std::string& laneId) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it != pImpl_->lanes_.end()) {
        it->second->mode = AutomationMode::WRITE;
        pImpl_->recordingLanes_[laneId] = true;
        
        MIXMIND_LOG_INFO("Started recording automation for lane: {}", laneId);
    }
}

void ParameterAutomationManager::stopRecording(const std::string& laneId) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it != pImpl_->lanes_.end()) {
        it->second->mode = AutomationMode::READ;
        pImpl_->recordingLanes_[laneId] = false;
        
        MIXMIND_LOG_INFO("Stopped recording automation for lane: {}", laneId);
    }
}

void ParameterAutomationManager::recordParameterChange(const std::string& laneId, double timeSeconds, float value) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto laneIt = pImpl_->lanes_.find(laneId);
    auto recordIt = pImpl_->recordingLanes_.find(laneId);
    
    if (laneIt != pImpl_->lanes_.end() && recordIt != pImpl_->recordingLanes_.end() && recordIt->second) {
        AutomationPoint point(timeSeconds, value);
        
        // Add point in time-sorted order
        auto& points = laneIt->second->points;
        auto insertIt = std::upper_bound(points.begin(), points.end(), point,
            [](const AutomationPoint& a, const AutomationPoint& b) {
                return a.timeSeconds < b.timeSeconds;
            });
        
        points.insert(insertIt, point);
        laneIt->second->isDirty = true;
    }
}

bool ParameterAutomationManager::isRecording(const std::string& laneId) const {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->recordingLanes_.find(laneId);
    return (it != pImpl_->recordingLanes_.end()) ? it->second : false;
}

void ParameterAutomationManager::touchParameter(const std::string& laneId) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    pImpl_->touchedParameters_[laneId] = true;
    pImpl_->touchStartTimes_[laneId] = std::chrono::steady_clock::now();
    
    // Switch to write mode for touch automation
    auto laneIt = pImpl_->lanes_.find(laneId);
    if (laneIt != pImpl_->lanes_.end() && laneIt->second->mode == AutomationMode::TOUCH) {
        startRecording(laneId);
    }
}

void ParameterAutomationManager::releaseParameter(const std::string& laneId) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    pImpl_->touchedParameters_[laneId] = false;
    
    // Switch back to read mode for touch automation
    auto laneIt = pImpl_->lanes_.find(laneId);
    if (laneIt != pImpl_->lanes_.end() && laneIt->second->mode == AutomationMode::TOUCH) {
        stopRecording(laneId);
    }
}

bool ParameterAutomationManager::isParameterTouched(const std::string& laneId) const {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->touchedParameters_.find(laneId);
    return (it != pImpl_->touchedParameters_.end()) ? it->second : false;
}

void ParameterAutomationManager::setAutomationMode(const std::string& laneId, AutomationMode mode) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it != pImpl_->lanes_.end()) {
        it->second->mode = mode;
    }
}

AutomationMode ParameterAutomationManager::getAutomationMode(const std::string& laneId) const {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    return (it != pImpl_->lanes_.end()) ? it->second->mode : AutomationMode::OFF;
}

void ParameterAutomationManager::createLinearRamp(const std::string& laneId, double startTime, float startValue, 
                                                 double endTime, float endValue) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it == pImpl_->lanes_.end()) {
        return;
    }
    
    // Remove existing points in range
    auto& points = it->second->points;
    points.erase(std::remove_if(points.begin(), points.end(),
        [startTime, endTime](const AutomationPoint& point) {
            return point.timeSeconds >= startTime && point.timeSeconds <= endTime;
        }), points.end());
    
    // Add start and end points
    AutomationPoint startPoint(startTime, startValue);
    AutomationPoint endPoint(endTime, endValue);
    
    points.push_back(startPoint);
    points.push_back(endPoint);
    
    // Re-sort
    std::sort(points.begin(), points.end(),
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.timeSeconds < b.timeSeconds;
        });
    
    it->second->isDirty = true;
    
    MIXMIND_LOG_INFO("Created linear ramp for lane {}: {:.3f}@{:.3f}s -> {:.3f}@{:.3f}s", 
                    laneId, startValue, startTime, endValue, endTime);
}

void ParameterAutomationManager::createExponentialCurve(const std::string& laneId, double startTime, float startValue,
                                                       double endTime, float endValue, float curvature) {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    auto it = pImpl_->lanes_.find(laneId);
    if (it == pImpl_->lanes_.end()) {
        return;
    }
    
    // Remove existing points in range
    auto& points = it->second->points;
    points.erase(std::remove_if(points.begin(), points.end(),
        [startTime, endTime](const AutomationPoint& point) {
            return point.timeSeconds >= startTime && point.timeSeconds <= endTime;
        }), points.end());
    
    // Generate exponential curve points
    const int numPoints = std::max(2, static_cast<int>((endTime - startTime) * 10)); // 10 points per second
    
    for (int i = 0; i < numPoints; ++i) {
        double t = static_cast<double>(i) / (numPoints - 1);
        double time = startTime + t * (endTime - startTime);
        
        // Exponential interpolation
        float exponentialT = (std::exp(curvature * t) - 1.0f) / (std::exp(curvature) - 1.0f);
        float value = startValue + exponentialT * (endValue - startValue);
        
        AutomationPoint point(time, value, InterpolationType::LINEAR);
        points.push_back(point);
    }
    
    // Re-sort
    std::sort(points.begin(), points.end(),
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.timeSeconds < b.timeSeconds;
        });
    
    it->second->isDirty = true;
    
    MIXMIND_LOG_INFO("Created exponential curve for lane {}: curvature={:.2f}", laneId, curvature);
}

ParameterAutomationManager::AutomationStats ParameterAutomationManager::getStats() const {
    std::lock_guard<std::mutex> lock(pImpl_->lanesMutex_);
    
    AutomationStats stats;
    stats.totalLanes = pImpl_->lanes_.size();
    stats.lastProcessingTimeMs = pImpl_->lastProcessingTimeMs_.load();
    stats.averageProcessingTimeMs = pImpl_->averageProcessingTimeMs_.load();
    stats.hasOverruns = pImpl_->hasOverruns_.load();
    
    // Count total points and active lanes
    for (const auto& [laneId, lane] : pImpl_->lanes_) {
        stats.totalPoints += lane->points.size();
        if (lane->isEnabled && lane->mode != AutomationMode::OFF) {
            stats.activeLanes++;
        }
    }
    
    return stats;
}

void ParameterAutomationManager::resetStats() {
    pImpl_->lastProcessingTimeMs_.store(0.0);
    pImpl_->averageProcessingTimeMs_.store(0.0);
    pImpl_->totalProcessingCalls_.store(0);
    pImpl_->hasOverruns_.store(false);
}

// ============================================================================
// Real-Time Automation Engine Implementation
// ============================================================================

class RealTimeAutomationEngine::Impl {
public:
    std::shared_ptr<ParameterAutomationManager> automationManager_;
    std::shared_ptr<AutomationTimeline> timeline_;
    
    std::function<void(const std::string&, const std::string&, float)> parameterCallback_;
    
    // Performance stats
    mutable std::atomic<double> lastProcessingTimeUs_{0.0};
    mutable std::atomic<double> averageProcessingTimeUs_{0.0};
    mutable std::atomic<double> peakProcessingTimeUs_{0.0};
    mutable std::atomic<size_t> totalParameterUpdates_{0};
    mutable std::atomic<size_t> totalPointsProcessed_{0};
    mutable std::atomic<bool> hasTimingViolations_{false};
    mutable std::atomic<size_t> processingCalls_{0};
    
    void updateProcessingStats(std::chrono::microseconds processingTime, size_t parameterUpdates, size_t pointsProcessed) {
        double timeUs = processingTime.count();
        
        lastProcessingTimeUs_.store(timeUs);
        totalParameterUpdates_.fetch_add(parameterUpdates);
        totalPointsProcessed_.fetch_add(pointsProcessed);
        
        // Update average
        size_t calls = processingCalls_.fetch_add(1);
        double currentAvg = averageProcessingTimeUs_.load();
        double newAvg = (currentAvg * calls + timeUs) / (calls + 1);
        averageProcessingTimeUs_.store(newAvg);
        
        // Update peak
        double currentPeak = peakProcessingTimeUs_.load();
        if (timeUs > currentPeak) {
            peakProcessingTimeUs_.store(timeUs);
        }
        
        // Check for timing violations (>50% of available time at 44.1kHz/512 samples)
        if (timeUs > 5800.0) { // ~50% of 11.6ms available time in microseconds
            hasTimingViolations_.store(true);
        }
    }
};

RealTimeAutomationEngine::RealTimeAutomationEngine()
    : pImpl_(std::make_unique<Impl>()) {
}

RealTimeAutomationEngine::~RealTimeAutomationEngine() = default;

void RealTimeAutomationEngine::setAutomationManager(std::shared_ptr<ParameterAutomationManager> manager) {
    pImpl_->automationManager_ = manager;
}

void RealTimeAutomationEngine::setTimeline(std::shared_ptr<AutomationTimeline> timeline) {
    pImpl_->timeline_ = timeline;
}

void RealTimeAutomationEngine::processAutomation(double timeSeconds, double sampleRate, int bufferSize) {
    if (!pImpl_->automationManager_) {
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Set parameter callback for counting
    size_t parameterUpdates = 0;
    auto countingCallback = [this, &parameterUpdates](const std::string& targetId, const std::string& parameterId, float value) {
        parameterUpdates++;
        if (pImpl_->parameterCallback_) {
            pImpl_->parameterCallback_(targetId, parameterId, value);
        }
    };
    
    // Temporarily set counting callback
    pImpl_->automationManager_->setParameterCallback(countingCallback);
    
    // Process automation
    pImpl_->automationManager_->processAutomation(timeSeconds, sampleRate, bufferSize);
    
    // Restore original callback
    pImpl_->automationManager_->setParameterCallback(pImpl_->parameterCallback_);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Get processed points count from automation manager stats
    auto stats = pImpl_->automationManager_->getStats();
    
    pImpl_->updateProcessingStats(processingTime, parameterUpdates, stats.totalPoints);
}

void RealTimeAutomationEngine::setParameterChangeCallback(
    std::function<void(const std::string&, const std::string&, float)> callback) {
    
    pImpl_->parameterCallback_ = std::move(callback);
}

RealTimeAutomationEngine::ProcessingStats RealTimeAutomationEngine::getProcessingStats() const {
    ProcessingStats stats;
    stats.lastProcessingTimeUs = pImpl_->lastProcessingTimeUs_.load();
    stats.averageProcessingTimeUs = pImpl_->averageProcessingTimeUs_.load();
    stats.peakProcessingTimeUs = pImpl_->peakProcessingTimeUs_.load();
    stats.totalParameterUpdates = pImpl_->totalParameterUpdates_.load();
    stats.totalPointsProcessed = pImpl_->totalPointsProcessed_.load();
    stats.hasTimingViolations = pImpl_->hasTimingViolations_.load();
    
    return stats;
}

void RealTimeAutomationEngine::resetProcessingStats() {
    pImpl_->lastProcessingTimeUs_.store(0.0);
    pImpl_->averageProcessingTimeUs_.store(0.0);
    pImpl_->peakProcessingTimeUs_.store(0.0);
    pImpl_->totalParameterUpdates_.store(0);
    pImpl_->totalPointsProcessed_.store(0);
    pImpl_->hasTimingViolations_.store(false);
    pImpl_->processingCalls_.store(0);
}

} // namespace mixmind::automation