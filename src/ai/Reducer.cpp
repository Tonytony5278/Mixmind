#include "Reducer.h"
#include <sstream>

namespace mixmind::ai {

Reducer::Reducer(std::shared_ptr<ITracktionInterface> tracktionInterface)
    : tracktion_(tracktionInterface) {
}

Reducer::~Reducer() = default;

Result<void> Reducer::reduce(const Action& action, AppState& state) {
    // 1. Validate action structure
    if (!validateAction(action)) {
        return Result<void>::error("Invalid action: " + getActionValidationError(action));
    }
    
    // 2. Validate preconditions
    if (!validatePreconditions(action, state)) {
        return Result<void>::error("Precondition failed: " + getPreconditionError(action, state));
    }
    
    // 3. Dispatch to specific handler
    Result<void> result = std::visit([this, &state](const auto& a) -> Result<void> {
        using T = std::decay_t<decltype(a)>;
        
        if constexpr (std::is_same_v<T, SetTempo>) {
            return handleSetTempo(a, state);
        }
        else if constexpr (std::is_same_v<T, SetLoop>) {
            return handleSetLoop(a, state);
        }
        else if constexpr (std::is_same_v<T, SetCursor>) {
            return handleSetCursor(a, state);
        }
        else if constexpr (std::is_same_v<T, AddAudioTrack>) {
            return handleAddAudioTrack(a, state);
        }
        else if constexpr (std::is_same_v<T, AddMidiTrack>) {
            return handleAddMidiTrack(a, state);
        }
        else if constexpr (std::is_same_v<T, AdjustGain>) {
            return handleAdjustGain(a, state);
        }
        else if constexpr (std::is_same_v<T, Normalize>) {
            return handleNormalize(a, state);
        }
        else if constexpr (std::is_same_v<T, FadeIn>) {
            return handleFadeIn(a, state);
        }
        else if constexpr (std::is_same_v<T, FadeOut>) {
            return handleFadeOut(a, state);
        }
        else if constexpr (std::is_same_v<T, PlayTransport>) {
            return handlePlayTransport(a, state);
        }
        else if constexpr (std::is_same_v<T, StopTransport>) {
            return handleStopTransport(a, state);
        }
        else if constexpr (std::is_same_v<T, ToggleRecording>) {
            return handleToggleRecording(a, state);
        }
        else {
            return Result<void>::error("Unsupported action type");
        }
    }, action);
    
    // 4. Validate final state
    if (result && !validateState(state)) {
        return Result<void>::error("Action resulted in invalid state");
    }
    
    return result;
}

void Reducer::setEventCallback(EventCallback callback) {
    eventCallback_ = callback;
}

void Reducer::clearEventCallback() {
    eventCallback_ = nullptr;
}

bool Reducer::validateState(const AppState& state) {
    // Basic state validation
    if (state.currentTempo <= 0.0 || state.currentTempo > 300.0) return false;
    if (state.currentPosition < 0.0) return false;
    if (state.loopStart < 0.0 || state.loopEnd <= state.loopStart) return false;
    
    // Track validation
    for (size_t i = 0; i < state.tracks.size(); ++i) {
        const auto& track = state.tracks[i];
        if (track.index != static_cast<int>(i)) return false;
        if (track.name.empty()) return false;
        if (track.gain < -60.0 || track.gain > 12.0) return false;
    }
    
    return true;
}

std::string Reducer::getActionValidationError(const Action& action) {
    return std::visit([](const auto& a) -> std::string {
        using T = std::decay_t<decltype(a)>;
        
        if constexpr (std::is_same_v<T, SetTempo>) {
            if (a.bpm <= 0.0) return "BPM must be positive";
            if (a.bpm > 300.0) return "BPM too high (max 300)";
        }
        else if constexpr (std::is_same_v<T, SetLoop>) {
            if (a.startBeats < 0.0) return "Loop start must be non-negative";
            if (a.endBeats <= a.startBeats) return "Loop end must be after start";
            if ((a.endBeats - a.startBeats) > 1000.0) return "Loop too long (max 1000 beats)";
        }
        else if constexpr (std::is_same_v<T, SetCursor>) {
            if (a.posBeats < 0.0) return "Cursor position must be non-negative";
            if (a.posBeats > 10000.0) return "Cursor position too large";
        }
        else if constexpr (std::is_same_v<T, AddAudioTrack> || std::is_same_v<T, AddMidiTrack>) {
            if (a.name.empty()) return "Track name cannot be empty";
            if (a.name.length() > 64) return "Track name too long (max 64 chars)";
        }
        else if constexpr (std::is_same_v<T, AdjustGain>) {
            if (a.trackIndex < 0) return "Track index must be non-negative";
            if (a.trackIndex >= 128) return "Track index too high (max 127)";
            if (a.dB < -60.0) return "Gain too low (min -60dB)";
            if (a.dB > 12.0) return "Gain too high (max 12dB)";
        }
        else if constexpr (std::is_same_v<T, Normalize>) {
            if (a.trackIndex < 0) return "Track index must be non-negative";
            if (a.targetLUFS < -60.0 || a.targetLUFS > -6.0) return "Target LUFS out of range (-60 to -6)";
        }
        else if constexpr (std::is_same_v<T, FadeIn> || std::is_same_v<T, FadeOut>) {
            if (a.clipId < 0) return "Clip ID must be non-negative";
            if (a.ms <= 0) return "Fade duration must be positive";
            if (a.ms > 30000) return "Fade duration too long (max 30s)";
        }
        
        return ""; // Valid
    }, action);
}

// Individual action handlers
Result<void> Reducer::handleSetTempo(const SetTempo& action, AppState& state) {
    auto result = tracktion_->setTempo(action.bpm);
    if (!result) return result;
    
    state.currentTempo = action.bpm;
    emitEvent(EventType::TempoChanged, "Tempo set to " + std::to_string(action.bpm) + " BPM", action);
    return Result<void>::success();
}

Result<void> Reducer::handleSetLoop(const SetLoop& action, AppState& state) {
    auto result = tracktion_->setLoopRange(action.startBeats, action.endBeats);
    if (!result) return result;
    
    state.loopStart = action.startBeats;
    state.loopEnd = action.endBeats;
    state.isLooping = true;
    emitEvent(EventType::LoopChanged, "Loop set from " + std::to_string(action.startBeats) + 
              " to " + std::to_string(action.endBeats) + " beats", action);
    return Result<void>::success();
}

Result<void> Reducer::handleSetCursor(const SetCursor& action, AppState& state) {
    auto result = tracktion_->setCursorPosition(action.posBeats);
    if (!result) return result;
    
    state.currentPosition = action.posBeats;
    emitEvent(EventType::CursorMoved, "Cursor moved to " + std::to_string(action.posBeats) + " beats", action);
    return Result<void>::success();
}

Result<void> Reducer::handleAddAudioTrack(const AddAudioTrack& action, AppState& state) {
    auto result = tracktion_->addAudioTrack(action.name);
    if (!result) return result;
    
    AppState::TrackInfo track;
    track.index = result.value;
    track.name = action.name;
    state.tracks.push_back(track);
    
    emitEvent(EventType::TrackAdded, "Audio track \"" + action.name + "\" added", action);
    return Result<void>::success();
}

Result<void> Reducer::handleAddMidiTrack(const AddMidiTrack& action, AppState& state) {
    auto result = tracktion_->addMidiTrack(action.name);
    if (!result) return result;
    
    AppState::TrackInfo track;
    track.index = result.value;
    track.name = action.name;
    state.tracks.push_back(track);
    
    emitEvent(EventType::TrackAdded, "MIDI track \"" + action.name + "\" added", action);
    return Result<void>::success();
}

Result<void> Reducer::handleAdjustGain(const AdjustGain& action, AppState& state) {
    auto result = tracktion_->setTrackGain(action.trackIndex, action.dB);
    if (!result) return result;
    
    if (auto* track = state.getTrack(action.trackIndex)) {
        track->gain = action.dB;
        emitEvent(EventType::TrackGainChanged, "Track " + std::to_string(action.trackIndex) + 
                  " gain set to " + std::to_string(action.dB) + " dB", action);
    }
    
    return Result<void>::success();
}

Result<void> Reducer::handleNormalize(const Normalize& action, AppState& state) {
    auto result = tracktion_->normalizeTrack(action.trackIndex, action.targetLUFS);
    if (!result) return result;
    
    emitEvent(EventType::AudioProcessed, "Track " + std::to_string(action.trackIndex) + 
              " normalized to " + std::to_string(action.targetLUFS) + " LUFS", action);
    return Result<void>::success();
}

Result<void> Reducer::handleFadeIn(const FadeIn& action, AppState& state) {
    auto result = tracktion_->fadeClipIn(action.clipId, action.ms);
    if (!result) return result;
    
    emitEvent(EventType::AudioProcessed, "Clip " + std::to_string(action.clipId) + 
              " fade in: " + std::to_string(action.ms) + "ms", action);
    return Result<void>::success();
}

Result<void> Reducer::handleFadeOut(const FadeOut& action, AppState& state) {
    auto result = tracktion_->fadeClipOut(action.clipId, action.ms);
    if (!result) return result;
    
    emitEvent(EventType::AudioProcessed, "Clip " + std::to_string(action.clipId) + 
              " fade out: " + std::to_string(action.ms) + "ms", action);
    return Result<void>::success();
}

Result<void> Reducer::handlePlayTransport(const PlayTransport& action, AppState& state) {
    auto result = tracktion_->playTransport(action.fromStart);
    if (!result) return result;
    
    state.isPlaying = true;
    if (action.fromStart) {
        state.currentPosition = 0.0;
    }
    
    emitEvent(EventType::TransportStateChanged, action.fromStart ? "Playing from start" : "Playing", action);
    return Result<void>::success();
}

Result<void> Reducer::handleStopTransport(const StopTransport& action, AppState& state) {
    auto result = tracktion_->stopTransport(action.returnToStart);
    if (!result) return result;
    
    state.isPlaying = false;
    state.isRecording = false;
    if (action.returnToStart) {
        state.currentPosition = 0.0;
    }
    
    emitEvent(EventType::TransportStateChanged, action.returnToStart ? "Stopped, returned to start" : "Stopped", action);
    return Result<void>::success();
}

Result<void> Reducer::handleToggleRecording(const ToggleRecording& action, AppState& state) {
    auto result = tracktion_->toggleRecording(action.enable);
    if (!result) return result;
    
    state.isRecording = action.enable;
    emitEvent(EventType::TransportStateChanged, action.enable ? "Recording started" : "Recording stopped", action);
    return Result<void>::success();
}

void Reducer::emitEvent(EventType type, const std::string& description, const Action& action) {
    if (eventCallback_) {
        DomainEvent event(type, description, action);
        eventCallback_(event);
    }
}

bool Reducer::validatePreconditions(const Action& action, const AppState& state) {
    return std::visit([&state](const auto& a) -> bool {
        using T = std::decay_t<decltype(a)>;
        
        if constexpr (std::is_same_v<T, AdjustGain> || std::is_same_v<T, Normalize>) {
            return state.hasTrack(a.trackIndex);
        }
        // Other actions don't have preconditions that depend on state
        return true;
    }, action);
}

std::string Reducer::getPreconditionError(const Action& action, const AppState& state) {
    return std::visit([&state](const auto& a) -> std::string {
        using T = std::decay_t<decltype(a)>;
        
        if constexpr (std::is_same_v<T, AdjustGain>) {
            if (!state.hasTrack(a.trackIndex)) {
                return "Track " + std::to_string(a.trackIndex) + " does not exist";
            }
        }
        else if constexpr (std::is_same_v<T, Normalize>) {
            if (!state.hasTrack(a.trackIndex)) {
                return "Track " + std::to_string(a.trackIndex) + " does not exist";
            }
        }
        
        return ""; // No error
    }, action);
}

} // namespace mixmind::ai