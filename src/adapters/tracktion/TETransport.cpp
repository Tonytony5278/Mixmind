#include "TETransport.h"
#include "TEUtils.h"
#include <regex>
#include <sstream>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TE Transport Listener - Internal class for TE callbacks
// ============================================================================

class TETransportListener : public te::TransportControl::Listener {
public:
    explicit TETransportListener(TETransport& transport) : transport_(transport) {}
    
    void playStateChanged(bool isPlaying) override {
        if (isPlaying) {
            transport_.notifyTransportEvent(core::TransportEvent::StateChanged, "Started playing");
        } else {
            transport_.notifyTransportEvent(core::TransportEvent::StateChanged, "Stopped playing");
        }
    }
    
    void recordStateChanged(bool isRecording) override {
        if (isRecording) {
            transport_.notifyTransportEvent(core::TransportEvent::StateChanged, "Started recording");
        } else {
            transport_.notifyTransportEvent(core::TransportEvent::StateChanged, "Stopped recording");
        }
    }
    
    void positionChanged(te::TimePosition newPosition) override {
        transport_.notifyTransportEvent(core::TransportEvent::PositionChanged, 
                                       "Position: " + std::to_string(newPosition.inSeconds()));
    }
    
private:
    TETransport& transport_;
};

// ============================================================================
// TETransport Implementation
// ============================================================================

TETransport::TETransport(te::Engine& engine, te::Edit& edit) 
    : TEAdapter(engine), edit_(edit), transportControl_(edit.getTransport())
{
    setupTransportCallbacks();
    updateTransportInfo();
}

TETransport::~TETransport() {
    cleanupTransportCallbacks();
}

// ========================================================================
// Basic Transport Control
// ========================================================================

core::AsyncResult<core::VoidResult> TETransport::play() {
    return executeAsyncVoid([this]() -> core::VoidResult {
        try {
            if (isPaused_.load()) {
                // Resume from paused position
                transportControl_.setPosition(pausedPosition_, false);
                isPaused_ = false;
            }
            
            transportControl_.play(false);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to start playback: " + std::string(e.what())
            );
        }
    }, "Start playback");
}

core::AsyncResult<core::VoidResult> TETransport::stop() {
    return executeAsyncVoid([this]() -> core::VoidResult {
        try {
            transportControl_.stop(false, false);
            isPaused_ = false;
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to stop transport: " + std::string(e.what())
            );
        }
    }, "Stop transport");
}

core::AsyncResult<core::VoidResult> TETransport::pause() {
    return executeAsyncVoid([this]() -> core::VoidResult {
        try {
            if (isPlaying()) {
                pausedPosition_ = transportControl_.getCurrentPosition();
                transportControl_.stop(false, false);
                isPaused_ = true;
                
                notifyTransportEvent(core::TransportEvent::StateChanged, "Paused");
                return core::VoidResult::success();
            } else {
                return core::VoidResult::error(
                    core::ErrorCode::InvalidState,
                    "Transport is not playing"
                );
            }
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to pause transport: " + std::string(e.what())
            );
        }
    }, "Pause transport");
}

core::AsyncResult<core::VoidResult> TETransport::record() {
    return executeAsyncVoid([this]() -> core::VoidResult {
        try {
            transportControl_.record(false, false);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to start recording: " + std::string(e.what())
            );
        }
    }, "Start recording");
}

core::AsyncResult<core::VoidResult> TETransport::togglePlayPause() {
    if (isPlaying() || isPaused()) {
        return isPaused() ? play() : pause();
    } else {
        return play();
    }
}

core::AsyncResult<core::VoidResult> TETransport::toggleRecord() {
    if (isRecording()) {
        return stop();
    } else {
        return record();
    }
}

// ========================================================================
// Position Control
// ========================================================================

core::AsyncResult<core::VoidResult> TETransport::locate(core::TimestampSamples position) {
    return executeAsyncVoid([this, position]() -> core::VoidResult {
        try {
            double sampleRate = edit_.engine.getDeviceManager().getSampleRate();
            te::TimePosition tePosition = te::TimePosition::fromSeconds(
                TETypeConverter::samplesToSeconds(position, static_cast<core::SampleRate>(sampleRate))
            );
            
            transportControl_.setPosition(tePosition, true);
            
            if (isPaused_.load()) {
                pausedPosition_ = tePosition;
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to locate to position: " + std::string(e.what())
            );
        }
    }, "Locate to position");
}

core::AsyncResult<core::VoidResult> TETransport::locateSeconds(core::TimestampSeconds seconds) {
    return executeAsyncVoid([this, seconds]() -> core::VoidResult {
        try {
            te::TimePosition tePosition = TETypeConverter::secondsToTime(seconds);
            transportControl_.setPosition(tePosition, true);
            
            if (isPaused_.load()) {
                pausedPosition_ = tePosition;
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to locate to time: " + std::string(e.what())
            );
        }
    }, "Locate to time");
}

core::AsyncResult<core::VoidResult> TETransport::locateMusical(const std::string& musicalTime) {
    return executeAsyncVoid([this, musicalTime]() -> core::VoidResult {
        try {
            te::TimePosition tePosition = parseMusicalTime(musicalTime);
            transportControl_.setPosition(tePosition, true);
            
            if (isPaused_.load()) {
                pausedPosition_ = tePosition;
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::InvalidParameter,
                "Failed to parse musical time '" + musicalTime + "': " + std::string(e.what())
            );
        }
    }, "Locate to musical time");
}

core::AsyncResult<core::VoidResult> TETransport::gotoStart() {
    return locateSeconds(0.0);
}

core::AsyncResult<core::VoidResult> TETransport::gotoEnd() {
    return executeAsyncVoid([this]() -> core::VoidResult {
        try {
            te::TimePosition endPosition = edit_.getLength();
            transportControl_.setPosition(endPosition, true);
            
            if (isPaused_.load()) {
                pausedPosition_ = endPosition;
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to go to end: " + std::string(e.what())
            );
        }
    }, "Go to end");
}

core::AsyncResult<core::VoidResult> TETransport::rewind(core::TimestampSamples samples) {
    return executeAsyncVoid([this, samples]() -> core::VoidResult {
        try {
            te::TimePosition currentPos = transportControl_.getCurrentPosition();
            double sampleRate = edit_.engine.getDeviceManager().getSampleRate();
            double deltaSeconds = TETypeConverter::samplesToSeconds(samples, static_cast<core::SampleRate>(sampleRate));
            
            te::TimePosition newPos = te::TimePosition::fromSeconds(
                std::max(0.0, currentPos.inSeconds() - deltaSeconds)
            );
            
            transportControl_.setPosition(newPos, true);
            
            if (isPaused_.load()) {
                pausedPosition_ = newPos;
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to rewind: " + std::string(e.what())
            );
        }
    }, "Rewind");
}

core::AsyncResult<core::VoidResult> TETransport::fastForward(core::TimestampSamples samples) {
    return executeAsyncVoid([this, samples]() -> core::VoidResult {
        try {
            te::TimePosition currentPos = transportControl_.getCurrentPosition();
            double sampleRate = edit_.engine.getDeviceManager().getSampleRate();
            double deltaSeconds = TETypeConverter::samplesToSeconds(samples, static_cast<core::SampleRate>(sampleRate));
            
            te::TimePosition newPos = te::TimePosition::fromSeconds(currentPos.inSeconds() + deltaSeconds);
            
            transportControl_.setPosition(newPos, true);
            
            if (isPaused_.load()) {
                pausedPosition_ = newPos;
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::error(
                core::ErrorCode::TransportError,
                "Failed to fast forward: " + std::string(e.what())
            );
        }
    }, "Fast forward");
}

// ========================================================================
// Current State
// ========================================================================

core::TransportState TETransport::getState() const {
    std::lock_guard<std::recursive_mutex> lock(transportMutex_);
    
    if (isPaused_.load()) {
        return core::TransportState::Paused;
    }
    
    return TETypeConverter::convertTransportState(transportControl_.getPlayState());
}

core::TimestampSamples TETransport::getPosition() const {
    te::TimePosition tePosition = transportControl_.getCurrentPosition();
    double sampleRate = edit_.engine.getDeviceManager().getSampleRate();
    
    return TETypeConverter::secondsToSamples(
        tePosition.inSeconds(), 
        static_cast<core::SampleRate>(sampleRate)
    );
}

core::TimestampSeconds TETransport::getPositionSeconds() const {
    return transportControl_.getCurrentPosition().inSeconds();
}

std::string TETransport::getPositionMusical() const {
    te::TimePosition position = transportControl_.getCurrentPosition();
    return formatMusicalTime(position);
}

bool TETransport::isPlaying() const {
    return transportControl_.isPlaying();
}

bool TETransport::isRecording() const {
    return transportControl_.isRecording();
}

bool TETransport::isPaused() const {
    return isPaused_.load();
}

bool TETransport::isStopped() const {
    return !isPlaying() && !isRecording() && !isPaused();
}

// ========================================================================
// Loop Control
// ========================================================================

core::VoidResult TETransport::setLoopEnabled(bool enabled) {
    try {
        std::lock_guard<std::recursive_mutex> lock(transportMutex_);
        loopEnabled_ = enabled;
        
        auto& loopInfo = edit_.getLoopRange();
        loopInfo.setLoopRange({te::TimePosition::fromSeconds(0), te::TimePosition::fromSeconds(10)}, nullptr);
        
        notifyTransportEvent(core::TransportEvent::LoopChanged, 
                           enabled ? "Loop enabled" : "Loop disabled");
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::TransportError,
            "Failed to set loop enabled: " + std::string(e.what())
        );
    }
}

bool TETransport::isLoopEnabled() const {
    return loopEnabled_.load();
}

core::VoidResult TETransport::setLoopRegion(core::TimestampSamples start, core::TimestampSamples end) {
    try {
        std::lock_guard<std::recursive_mutex> lock(transportMutex_);
        
        double sampleRate = edit_.engine.getDeviceManager().getSampleRate();
        te::TimePosition startPos = te::TimePosition::fromSeconds(
            TETypeConverter::samplesToSeconds(start, static_cast<core::SampleRate>(sampleRate))
        );
        te::TimePosition endPos = te::TimePosition::fromSeconds(
            TETypeConverter::samplesToSeconds(end, static_cast<core::SampleRate>(sampleRate))
        );
        
        auto& loopInfo = edit_.getLoopRange();
        loopInfo.setLoopRange({startPos, endPos}, nullptr);
        
        loopStart_ = start;
        loopEnd_ = end;
        
        notifyTransportEvent(core::TransportEvent::LoopChanged, "Loop region changed");
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::TransportError,
            "Failed to set loop region: " + std::string(e.what())
        );
    }
}

core::TimestampSamples TETransport::getLoopStart() const {
    return loopStart_;
}

core::TimestampSamples TETransport::getLoopEnd() const {
    return loopEnd_;
}

core::VoidResult TETransport::setLoopMode(core::LoopMode mode) {
    // TE doesn't have different loop modes like ping-pong
    // This would be implemented as custom behavior
    return core::VoidResult::success();
}

core::LoopMode TETransport::getLoopMode() const {
    return core::LoopMode::Normal; // Default implementation
}

// ========================================================================
// Metronome Control
// ========================================================================

core::VoidResult TETransport::setMetronomeEnabled(bool enabled) {
    try {
        std::lock_guard<std::recursive_mutex> lock(transportMutex_);
        metronomeEnabled_ = enabled;
        
        // Enable/disable TE click track
        if (auto clickTrack = edit_.getClickTrack()) {
            clickTrack->setMute(!enabled);
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::TransportError,
            "Failed to set metronome enabled: " + std::string(e.what())
        );
    }
}

bool TETransport::isMetronomeEnabled() const {
    return metronomeEnabled_.load();
}

core::VoidResult TETransport::setMetronomeVolume(float volume) {
    try {
        std::lock_guard<std::recursive_mutex> lock(transportMutex_);
        metronomeVolume_ = juce::jlimit(0.0f, 1.0f, volume);
        
        if (auto clickTrack = edit_.getClickTrack()) {
            clickTrack->setVolumeDb(juce::Decibels::gainToDecibels(metronomeVolume_.load()));
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::TransportError,
            "Failed to set metronome volume: " + std::string(e.what())
        );
    }
}

float TETransport::getMetronomeVolume() const {
    return metronomeVolume_.load();
}

core::VoidResult TETransport::setMetronomeRecordOnly(bool recordOnly) {
    metronomeRecordOnly_ = recordOnly;
    return core::VoidResult::success();
}

bool TETransport::isMetronomeRecordOnly() const {
    return metronomeRecordOnly_.load();
}

core::VoidResult TETransport::setMetronomeSound(MetronomeSound sound, const std::string& customSoundPath) {
    std::lock_guard<std::recursive_mutex> lock(transportMutex_);
    metronomeSound_ = sound;
    customMetronomePath_ = customSoundPath;
    
    // Configure TE click track sound
    // Implementation depends on TE click track capabilities
    
    return core::VoidResult::success();
}

core::ITransport::MetronomeSound TETransport::getMetronomeSound() const {
    return metronomeSound_;
}

// ========================================================================
// Transport Info and Events
// ========================================================================

core::TransportInfo TETransport::getTransportInfo() const {
    std::lock_guard<std::recursive_mutex> lock(transportMutex_);
    
    auto now = std::chrono::steady_clock::now();
    if (now - lastInfoUpdate_ < INFO_CACHE_DURATION) {
        return cachedTransportInfo_;
    }
    
    // Update cached info
    const_cast<TETransport*>(this)->updateTransportInfo();
    lastInfoUpdate_ = now;
    
    return cachedTransportInfo_;
}

void TETransport::updateTransportInfo() const {
    cachedTransportInfo_.state = getState();
    cachedTransportInfo_.position = getPosition();
    cachedTransportInfo_.positionSeconds = getPositionSeconds();
    cachedTransportInfo_.isLoopEnabled = isLoopEnabled();
    cachedTransportInfo_.loopStart = getLoopStart();
    cachedTransportInfo_.loopEnd = getLoopEnd();
    cachedTransportInfo_.tempo = edit_.tempoSequence.getTempoAt(transportControl_.getCurrentPosition()).getBpm();
    
    auto timeSig = edit_.tempoSequence.getTimeSignatureAt(transportControl_.getCurrentPosition());
    cachedTransportInfo_.timeSignature = {timeSig.numerator, timeSig.denominator};
    
    cachedTransportInfo_.isMetronomeEnabled = isMetronomeEnabled();
    cachedTransportInfo_.playbackSpeed = getPlaybackSpeed();
    cachedTransportInfo_.syncSource = getSyncSource();
}

void TETransport::addEventListener(core::TransportEventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    eventCallbacks_.push_back(std::move(callback));
}

void TETransport::removeEventListener(core::TransportEventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    // Simplified implementation - real version would need proper callback matching
}

void TETransport::notifyTransportEvent(core::TransportEvent event, const std::string& details) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    
    core::TransportInfo info = getTransportInfo();
    for (const auto& callback : eventCallbacks_) {
        if (callback) {
            callback(event, info);
        }
    }
}

// ========================================================================
// Internal Implementation
// ========================================================================

te::TimePosition TETransport::parseMusicalTime(const std::string& musicalTime) const {
    // Parse formats like "1:2:480" (bar:beat:ticks)
    std::regex timeRegex(R"((\d+):(\d+):(\d+))");
    std::smatch matches;
    
    if (std::regex_match(musicalTime, matches, timeRegex)) {
        int bars = std::stoi(matches[1].str());
        int beats = std::stoi(matches[2].str());
        int ticks = std::stoi(matches[3].str());
        
        // Convert to TE TimePosition using tempo sequence
        auto& tempoSequence = edit_.tempoSequence;
        
        // This is simplified - real implementation would need proper beat/tick conversion
        double totalBeats = (bars - 1) * 4 + (beats - 1) + (ticks / 480.0);
        double bpm = tempoSequence.getTempoAt(te::TimePosition()).getBpm();
        double seconds = (totalBeats * 60.0) / bpm;
        
        return te::TimePosition::fromSeconds(seconds);
    }
    
    throw std::invalid_argument("Invalid musical time format: " + musicalTime);
}

std::string TETransport::formatMusicalTime(te::TimePosition position) const {
    // Convert TE position to musical time string
    double seconds = position.inSeconds();
    double bpm = edit_.tempoSequence.getTempoAt(position).getBpm();
    double totalBeats = (seconds * bpm) / 60.0;
    
    int bars = static_cast<int>(totalBeats / 4) + 1;
    int beats = static_cast<int>(totalBeats) % 4 + 1;
    int ticks = static_cast<int>((totalBeats - std::floor(totalBeats)) * 480);
    
    std::ostringstream oss;
    oss << bars << ":" << beats << ":" << ticks;
    return oss.str();
}

void TETransport::setupTransportCallbacks() {
    teTransportListener_ = std::make_unique<TETransportListener>(*this);
    transportControl_.addListener(teTransportListener_.get());
}

void TETransport::cleanupTransportCallbacks() {
    if (teTransportListener_) {
        transportControl_.removeListener(teTransportListener_.get());
        teTransportListener_.reset();
    }
}

// ========================================================================
// Placeholder implementations for remaining methods
// ========================================================================

core::VoidResult TETransport::setPlaybackSpeed(float speed) {
    playbackSpeed_ = juce::jlimit(0.1f, 4.0f, speed);
    // TE implementation would configure playback speed
    return core::VoidResult::success();
}

float TETransport::getPlaybackSpeed() const {
    return playbackSpeed_.load();
}

core::VoidResult TETransport::resetPlaybackSpeed() {
    return setPlaybackSpeed(1.0f);
}

core::VoidResult TETransport::setPitchCorrectionEnabled(bool enabled) {
    pitchCorrectionEnabled_ = enabled;
    return core::VoidResult::success();
}

bool TETransport::isPitchCorrectionEnabled() const {
    return pitchCorrectionEnabled_.load();
}

// Additional placeholder implementations for sync, MIDI control, quantization, etc.
// Following the same pattern as above...

core::VoidResult TETransport::setSyncSource(SyncSource source) {
    syncSource_ = source;
    return core::VoidResult::success();
}

TETransport::SyncSource TETransport::getSyncSource() const {
    return syncSource_;
}

bool TETransport::isExternallysynced() const {
    return syncSource_ != SyncSource::Internal;
}

std::string TETransport::getSyncStatus() const {
    return isExternallysynced() ? "Synced" : "Internal";
}

// Continue with remaining method implementations...
// Many would follow similar patterns to those shown above

} // namespace mixmind::adapters::tracktion