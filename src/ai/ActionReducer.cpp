#include "ActionReducer.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace mixmind::ai {

    // ActionCommand implementation
    bool ActionCommand::validate() const {
        // Basic validation - each action type has specific parameter requirements
        switch (type) {
            case ActionType::ADD_TRACK:
                return params.size() >= 1; // At least track name
                
            case ActionType::RENAME_TRACK:
                return !trackId.empty() && params.size() >= 1; // trackId + new name
                
            case ActionType::SET_TRACK_VOLUME:
                return !trackId.empty() && params.size() >= 1; // trackId + volume
                
            case ActionType::SET_TRACK_PAN:
                return !trackId.empty() && params.size() >= 1; // trackId + pan value
                
            case ActionType::MUTE_TRACK:
            case ActionType::SOLO_TRACK:
                return !trackId.empty(); // Just trackId needed
                
            case ActionType::ADD_MIDI_NOTE:
                return !trackId.empty() && params.size() >= 4; // pitch, velocity, start, duration
                
            case ActionType::REMOVE_MIDI_NOTE:
                return !trackId.empty() && params.size() >= 2; // pitch, start time for identification
                
            case ActionType::QUANTIZE_MIDI:
                return !trackId.empty() && params.size() >= 1; // quantize grid
                
            case ActionType::TRANSPOSE_MIDI:
                return !trackId.empty() && params.size() >= 1; // semitones
                
            case ActionType::SET_TEMPO:
                return params.size() >= 1; // tempo value
                
            case ActionType::SET_TIME_SIGNATURE:
                return params.size() >= 2; // numerator, denominator
                
            default:
                return true; // Allow unknown actions for extensibility
        }
    }

    std::string ActionCommand::toString() const {
        std::ostringstream ss;
        ss << "Action{type=" << static_cast<int>(type);
        
        if (!trackId.empty()) {
            ss << ", trackId=" << trackId;
        }
        
        if (!regionId.empty()) {
            ss << ", regionId=" << regionId;
        }
        
        ss << ", params=[";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) ss << ", ";
            
            std::visit([&ss](const auto& value) {
                ss << value;
            }, params[i]);
        }
        ss << "]";
        
        if (timestamp_ms > 0) {
            ss << ", timestamp=" << timestamp_ms;
        }
        
        ss << "}";
        return ss.str();
    }

    // ProjectState implementation
    ProjectState ProjectState::copy() const {
        ProjectState newState = *this;
        newState.version = version + 1;
        
        // Update last modified timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        newState.lastModified = ss.str();
        
        return newState;
    }

    bool ProjectState::validate() const {
        // Check basic project constraints
        if (tempo <= 0 || tempo > 300) return false;
        if (timeSignature.first <= 0 || timeSignature.second <= 0) return false;
        
        // Validate track constraints
        for (const auto& track : tracks) {
            if (track.id.empty() || track.name.empty()) return false;
            if (track.volume < 0.0f || track.volume > 2.0f) return false; // Allow boost up to 2x
            if (track.pan < -1.0f || track.pan > 1.0f) return false;
        }
        
        // Validate MIDI notes
        for (const auto& note : midiNotes) {
            if (note.pitch < 0 || note.pitch > 127) return false;
            if (note.velocity < 0.0f || note.velocity > 1.0f) return false;
            if (note.duration_ms == 0) return false;
            
            // Check that trackId exists
            bool trackExists = std::any_of(tracks.begin(), tracks.end(),
                [&note](const Track& track) { return track.id == note.trackId; });
            if (!trackExists && !note.trackId.empty()) return false;
        }
        
        // Validate audio regions
        for (const auto& region : audioRegions) {
            if (region.id.empty() || region.duration_ms == 0) return false;
            
            // Check that trackId exists
            bool trackExists = std::any_of(tracks.begin(), tracks.end(),
                [&region](const Track& track) { return track.id == region.trackId; });
            if (!trackExists && !region.trackId.empty()) return false;
        }
        
        return true;
    }

    std::string ProjectState::toJSON() const {
        std::ostringstream json;
        json << std::fixed << std::setprecision(3);
        
        json << "{\\n";
        json << "  \\"version\\": " << version << ",\\n";
        json << "  \\"lastModified\\": \\"" << lastModified << "\\",\\n";
        json << "  \\"tempo\\": " << tempo << ",\\n";
        json << "  \\"timeSignature\\": [" << timeSignature.first << ", " << timeSignature.second << "],\\n";
        json << "  \\"keySignature\\": \\"" << keySignature << "\\",\\n";
        
        json << "  \\"tracks\\": [\\n";
        for (size_t i = 0; i < tracks.size(); ++i) {
            const auto& track = tracks[i];
            json << "    {\\n";
            json << "      \\"id\\": \\"" << track.id << "\\",\\n";
            json << "      \\"name\\": \\"" << track.name << "\\",\\n";
            json << "      \\"volume\\": " << track.volume << ",\\n";
            json << "      \\"pan\\": " << track.pan << ",\\n";
            json << "      \\"muted\\": " << (track.muted ? "true" : "false") << ",\\n";
            json << "      \\"soloed\\": " << (track.soloed ? "true" : "false") << "\\n";
            json << "    }";
            if (i < tracks.size() - 1) json << ",";
            json << "\\n";
        }
        json << "  ],\\n";
        
        json << "  \\"midiNotes\\": [\\n";
        for (size_t i = 0; i < midiNotes.size(); ++i) {
            const auto& note = midiNotes[i];
            json << "    {\\n";
            json << "      \\"pitch\\": " << note.pitch << ",\\n";
            json << "      \\"velocity\\": " << note.velocity << ",\\n";
            json << "      \\"startTime_ms\\": " << note.startTime_ms << ",\\n";
            json << "      \\"duration_ms\\": " << note.duration_ms << ",\\n";
            json << "      \\"trackId\\": \\"" << note.trackId << "\\"\\n";
            json << "    }";
            if (i < midiNotes.size() - 1) json << ",";
            json << "\\n";
        }
        json << "  ]\\n";
        
        json << "}";
        return json.str();
    }

    // ActionReducer implementation
    ActionResult ActionReducer::reduce(const ProjectState& currentState, const ActionCommand& action) {
        // Validate action first
        auto validation = validateAction(currentState, action);
        if (!validation.has_value()) {
            return {false, validation.error().message, currentState, {}, {}};
        }
        
        // Route to specific handler based on action type
        switch (action.type) {
            case ActionType::ADD_TRACK:
                return handleAddTrack(currentState, action);
                
            case ActionType::REMOVE_TRACK:
                return handleRemoveTrack(currentState, action);
                
            case ActionType::RENAME_TRACK:
                return handleRenameTrack(currentState, action);
                
            case ActionType::SET_TRACK_VOLUME:
                return handleSetTrackVolume(currentState, action);
                
            case ActionType::SET_TRACK_PAN:
                return handleSetTrackPan(currentState, action);
                
            case ActionType::MUTE_TRACK:
                return handleMuteTrack(currentState, action);
                
            case ActionType::SOLO_TRACK:
                return handleSoloTrack(currentState, action);
                
            case ActionType::ADD_MIDI_NOTE:
                return handleAddMIDINote(currentState, action);
                
            case ActionType::REMOVE_MIDI_NOTE:
                return handleRemoveMIDINote(currentState, action);
                
            case ActionType::QUANTIZE_MIDI:
                return handleQuantizeMIDI(currentState, action);
                
            case ActionType::TRANSPOSE_MIDI:
                return handleTransposeMIDI(currentState, action);
                
            case ActionType::SET_TEMPO:
                return handleSetTempo(currentState, action);
                
            case ActionType::SET_TIME_SIGNATURE:
                return handleSetTimeSignature(currentState, action);
                
            case ActionType::SET_KEY_SIGNATURE:
                return handleSetKeySignature(currentState, action);
                
            default:
                return {false, "Unknown action type", currentState, {}, {}};
        }
    }

    ActionResult ActionReducer::reduceBatch(const ProjectState& currentState, 
                                          const std::vector<ActionCommand>& actions) {
        ProjectState state = currentState;
        std::vector<std::string> allWarnings;
        std::vector<ActionCommand> reverseCommands;
        
        for (const auto& action : actions) {
            ActionResult result = reduce(state, action);
            
            if (!result.success) {
                // Batch failed - return error with original state
                return {false, "Batch action failed: " + result.errorMessage, currentState, allWarnings, {}};
            }
            
            state = result.newState;
            allWarnings.insert(allWarnings.end(), result.warnings.begin(), result.warnings.end());
            reverseCommands.insert(reverseCommands.begin(), result.reverseCommand); // Reverse order for undo
        }
        
        // Create compound reverse command (simplified - in practice might need more sophistication)
        ActionCommand batchReverseCmd;
        batchReverseCmd.type = ActionType::UNDO; // Special batch undo marker
        
        return {true, "", state, allWarnings, batchReverseCmd};
    }

    core::Result<void> ActionReducer::validateAction(const ProjectState& currentState, 
                                                   const ActionCommand& action) {
        if (!action.validate()) {
            return core::Error{"Action failed basic validation"};
        }
        
        // Context-specific validation
        switch (action.type) {
            case ActionType::RENAME_TRACK:
            case ActionType::SET_TRACK_VOLUME:
            case ActionType::SET_TRACK_PAN:
            case ActionType::MUTE_TRACK:
            case ActionType::SOLO_TRACK: {
                const auto* track = findTrack(currentState, action.trackId);
                if (!track) {
                    return core::Error{"Track not found: " + action.trackId};
                }
                break;
            }
            
            case ActionType::SET_TRACK_VOLUME: {
                auto volumeResult = action.getParam<float>(0);
                if (!volumeResult.has_value() || volumeResult.value() < 0.0f || volumeResult.value() > 2.0f) {
                    return core::Error{"Invalid volume value"};
                }
                break;
            }
            
            case ActionType::SET_TRACK_PAN: {
                auto panResult = action.getParam<float>(0);
                if (!panResult.has_value() || panResult.value() < -1.0f || panResult.value() > 1.0f) {
                    return core::Error{"Invalid pan value"};
                }
                break;
            }
            
            case ActionType::SET_TEMPO: {
                auto tempoResult = action.getParam<double>(0);
                if (!tempoResult.has_value() || tempoResult.value() <= 0 || tempoResult.value() > 300) {
                    return core::Error{"Invalid tempo value"};
                }
                break;
            }
        }
        
        return {};
    }

    // Individual action handlers
    ActionResult ActionReducer::handleAddTrack(const ProjectState& state, const ActionCommand& cmd) {
        auto nameResult = cmd.getParam<std::string>(0);
        if (!nameResult.has_value()) {
            return {false, "Track name parameter missing", state, {}, {}};
        }
        
        ProjectState newState = state.copy();
        ProjectState::Track newTrack;
        newTrack.id = generateId("track");
        newTrack.name = nameResult.value();
        
        newState.tracks.push_back(newTrack);
        
        // Generate reverse command
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::REMOVE_TRACK;
        reverseCmd.trackId = newTrack.id;
        
        return {true, "", newState, {}, reverseCmd};
    }

    ActionResult ActionReducer::handleRemoveTrack(const ProjectState& state, const ActionCommand& cmd) {
        ProjectState newState = state.copy();
        
        auto it = std::find_if(newState.tracks.begin(), newState.tracks.end(),
            [&cmd](const ProjectState::Track& track) { return track.id == cmd.trackId; });
            
        if (it == newState.tracks.end()) {
            return {false, "Track not found for removal", state, {}, {}};
        }
        
        // Store track info for reverse command
        ProjectState::Track removedTrack = *it;
        newState.tracks.erase(it);
        
        // Remove associated MIDI notes and audio regions
        newState.midiNotes.erase(
            std::remove_if(newState.midiNotes.begin(), newState.midiNotes.end(),
                [&cmd](const ProjectState::MIDINote& note) { return note.trackId == cmd.trackId; }),
            newState.midiNotes.end());
            
        newState.audioRegions.erase(
            std::remove_if(newState.audioRegions.begin(), newState.audioRegions.end(),
                [&cmd](const ProjectState::AudioRegion& region) { return region.trackId == cmd.trackId; }),
            newState.audioRegions.end());
        
        // Generate reverse command
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::ADD_TRACK;
        reverseCmd.params.push_back(removedTrack.name);
        
        std::vector<std::string> warnings;
        if (!newState.midiNotes.empty() || !newState.audioRegions.empty()) {
            warnings.push_back("Removed track contained MIDI notes or audio regions");
        }
        
        return {true, "", newState, warnings, reverseCmd};
    }

    ActionResult ActionReducer::handleSetTrackVolume(const ProjectState& state, const ActionCommand& cmd) {
        auto volumeResult = cmd.getParam<float>(0);
        if (!volumeResult.has_value()) {
            return {false, "Volume parameter missing", state, {}, {}};
        }
        
        ProjectState newState = state.copy();
        auto* track = findTrack(newState, cmd.trackId);
        if (!track) {
            return {false, "Track not found", state, {}, {}};
        }
        
        float oldVolume = track->volume;
        track->volume = volumeResult.value();
        
        // Generate reverse command
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::SET_TRACK_VOLUME;
        reverseCmd.trackId = cmd.trackId;
        reverseCmd.params.push_back(oldVolume);
        
        return {true, "", newState, {}, reverseCmd};
    }

    ActionResult ActionReducer::handleAddMIDINote(const ProjectState& state, const ActionCommand& cmd) {
        auto pitchResult = cmd.getParam<int32_t>(0);
        auto velocityResult = cmd.getParam<float>(1);
        auto startTimeResult = cmd.getParam<uint64_t>(2);
        auto durationResult = cmd.getParam<uint64_t>(3);
        
        if (!pitchResult.has_value() || !velocityResult.has_value() || 
            !startTimeResult.has_value() || !durationResult.has_value()) {
            return {false, "MIDI note parameters incomplete", state, {}, {}};
        }
        
        ProjectState newState = state.copy();
        ProjectState::MIDINote newNote;
        newNote.pitch = pitchResult.value();
        newNote.velocity = velocityResult.value();
        newNote.startTime_ms = startTimeResult.value();
        newNote.duration_ms = durationResult.value();
        newNote.trackId = cmd.trackId;
        
        newState.midiNotes.push_back(newNote);
        
        // Generate reverse command
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::REMOVE_MIDI_NOTE;
        reverseCmd.trackId = cmd.trackId;
        reverseCmd.params.push_back(newNote.pitch);
        reverseCmd.params.push_back(newNote.startTime_ms);
        
        return {true, "", newState, {}, reverseCmd};
    }

    ActionResult ActionReducer::handleSetTempo(const ProjectState& state, const ActionCommand& cmd) {
        auto tempoResult = cmd.getParam<double>(0);
        if (!tempoResult.has_value()) {
            return {false, "Tempo parameter missing", state, {}, {}};
        }
        
        ProjectState newState = state.copy();
        double oldTempo = newState.tempo;
        newState.tempo = tempoResult.value();
        
        // Generate reverse command
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::SET_TEMPO;
        reverseCmd.params.push_back(oldTempo);
        
        return {true, "", newState, {}, reverseCmd};
    }

    // Helper function implementations
    std::string ActionReducer::generateId(const std::string& prefix) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint32_t> dis(100000, 999999);
        
        return prefix + "_" + std::to_string(getCurrentTimestamp()) + "_" + std::to_string(dis(gen));
    }

    uint64_t ActionReducer::getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    ProjectState::Track* ActionReducer::findTrack(ProjectState& state, const std::string& trackId) {
        auto it = std::find_if(state.tracks.begin(), state.tracks.end(),
            [&trackId](const ProjectState::Track& track) { return track.id == trackId; });
        return it != state.tracks.end() ? &(*it) : nullptr;
    }

    const ProjectState::Track* ActionReducer::findTrack(const ProjectState& state, const std::string& trackId) {
        auto it = std::find_if(state.tracks.begin(), state.tracks.end(),
            [&trackId](const ProjectState::Track& track) { return track.id == trackId; });
        return it != state.tracks.end() ? &(*it) : nullptr;
    }

    // Stub implementations for remaining handlers (simplified for demo)
    ActionResult ActionReducer::handleRenameTrack(const ProjectState& state, const ActionCommand& cmd) {
        ProjectState newState = state.copy();
        auto* track = findTrack(newState, cmd.trackId);
        if (!track) return {false, "Track not found", state, {}, {}};
        
        auto newNameResult = cmd.getParam<std::string>(0);
        if (!newNameResult.has_value()) return {false, "Name parameter missing", state, {}, {}};
        
        std::string oldName = track->name;
        track->name = newNameResult.value();
        
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::RENAME_TRACK;
        reverseCmd.trackId = cmd.trackId;
        reverseCmd.params.push_back(oldName);
        
        return {true, "", newState, {}, reverseCmd};
    }

    ActionResult ActionReducer::handleSetTrackPan(const ProjectState& state, const ActionCommand& cmd) {
        ProjectState newState = state.copy();
        auto* track = findTrack(newState, cmd.trackId);
        if (!track) return {false, "Track not found", state, {}, {}};
        
        auto panResult = cmd.getParam<float>(0);
        if (!panResult.has_value()) return {false, "Pan parameter missing", state, {}, {}};
        
        float oldPan = track->pan;
        track->pan = panResult.value();
        
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::SET_TRACK_PAN;
        reverseCmd.trackId = cmd.trackId;
        reverseCmd.params.push_back(oldPan);
        
        return {true, "", newState, {}, reverseCmd};
    }

    ActionResult ActionReducer::handleMuteTrack(const ProjectState& state, const ActionCommand& cmd) {
        ProjectState newState = state.copy();
        auto* track = findTrack(newState, cmd.trackId);
        if (!track) return {false, "Track not found", state, {}, {}};
        
        track->muted = !track->muted; // Toggle mute state
        
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::MUTE_TRACK;
        reverseCmd.trackId = cmd.trackId;
        
        return {true, "", newState, {}, reverseCmd};
    }

    ActionResult ActionReducer::handleSoloTrack(const ProjectState& state, const ActionCommand& cmd) {
        ProjectState newState = state.copy();
        auto* track = findTrack(newState, cmd.trackId);
        if (!track) return {false, "Track not found", state, {}, {}};
        
        track->soloed = !track->soloed; // Toggle solo state
        
        ActionCommand reverseCmd;
        reverseCmd.type = ActionType::SOLO_TRACK;
        reverseCmd.trackId = cmd.trackId;
        
        return {true, "", newState, {}, reverseCmd};
    }

    // Placeholder implementations for remaining handlers
    ActionResult ActionReducer::handleRemoveMIDINote(const ProjectState& state, const ActionCommand& cmd) {
        return {true, "", state.copy(), {}, {}}; // Simplified
    }

    ActionResult ActionReducer::handleModifyMIDINote(const ProjectState& state, const ActionCommand& cmd) {
        return {true, "", state.copy(), {}, {}}; // Simplified
    }

    ActionResult ActionReducer::handleQuantizeMIDI(const ProjectState& state, const ActionCommand& cmd) {
        return {true, "", state.copy(), {}, {}}; // Simplified
    }

    ActionResult ActionReducer::handleTransposeMIDI(const ProjectState& state, const ActionCommand& cmd) {
        return {true, "", state.copy(), {}, {}}; // Simplified
    }

    ActionResult ActionReducer::handleSetTimeSignature(const ProjectState& state, const ActionCommand& cmd) {
        return {true, "", state.copy(), {}, {}}; // Simplified
    }

    ActionResult ActionReducer::handleSetKeySignature(const ProjectState& state, const ActionCommand& cmd) {
        return {true, "", state.copy(), {}, {}}; // Simplified
    }

} // namespace mixmind::ai