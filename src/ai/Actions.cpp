#include "Actions.h"
#include <sstream>

namespace mixmind::ai {

std::string actionToString(const Action& action) {
    return std::visit([](const auto& a) -> std::string {
        using T = std::decay_t<decltype(a)>;
        std::ostringstream oss;
        
        if constexpr (std::is_same_v<T, SetTempo>) {
            oss << "SetTempo{bpm=" << a.bpm << "}";
        }
        else if constexpr (std::is_same_v<T, SetLoop>) {
            oss << "SetLoop{start=" << a.startBeats << ", end=" << a.endBeats << "}";
        }
        else if constexpr (std::is_same_v<T, SetCursor>) {
            oss << "SetCursor{pos=" << a.posBeats << "}";
        }
        else if constexpr (std::is_same_v<T, AddAudioTrack>) {
            oss << "AddAudioTrack{name=\"" << a.name << "\"}";
        }
        else if constexpr (std::is_same_v<T, AddMidiTrack>) {
            oss << "AddMidiTrack{name=\"" << a.name << "\"}";
        }
        else if constexpr (std::is_same_v<T, AdjustGain>) {
            oss << "AdjustGain{track=" << a.trackIndex << ", dB=" << a.dB << "}";
        }
        else if constexpr (std::is_same_v<T, Normalize>) {
            oss << "Normalize{track=" << a.trackIndex << ", target=" << a.targetLUFS << " LUFS}";
        }
        else if constexpr (std::is_same_v<T, FadeIn>) {
            oss << "FadeIn{clip=" << a.clipId << ", duration=" << a.ms << "ms}";
        }
        else if constexpr (std::is_same_v<T, FadeOut>) {
            oss << "FadeOut{clip=" << a.clipId << ", duration=" << a.ms << "ms}";
        }
        else if constexpr (std::is_same_v<T, PlayTransport>) {
            oss << "PlayTransport{fromStart=" << (a.fromStart ? "true" : "false") << "}";
        }
        else if constexpr (std::is_same_v<T, StopTransport>) {
            oss << "StopTransport{returnToStart=" << (a.returnToStart ? "true" : "false") << "}";
        }
        else if constexpr (std::is_same_v<T, ToggleRecording>) {
            oss << "ToggleRecording{enable=" << (a.enable ? "true" : "false") << "}";
        }
        else {
            oss << "UnknownAction";
        }
        
        return oss.str();
    }, action);
}

bool validateAction(const Action& action) {
    return std::visit([](const auto& a) -> bool {
        return a.isValid();
    }, action);
}

std::string getActionTypeName(const Action& action) {
    return std::visit([](const auto& a) -> std::string {
        using T = std::decay_t<decltype(a)>;
        
        if constexpr (std::is_same_v<T, SetTempo>) return "SetTempo";
        else if constexpr (std::is_same_v<T, SetLoop>) return "SetLoop";
        else if constexpr (std::is_same_v<T, SetCursor>) return "SetCursor";
        else if constexpr (std::is_same_v<T, AddAudioTrack>) return "AddAudioTrack";
        else if constexpr (std::is_same_v<T, AddMidiTrack>) return "AddMidiTrack";
        else if constexpr (std::is_same_v<T, AdjustGain>) return "AdjustGain";
        else if constexpr (std::is_same_v<T, Normalize>) return "Normalize";
        else if constexpr (std::is_same_v<T, FadeIn>) return "FadeIn";
        else if constexpr (std::is_same_v<T, FadeOut>) return "FadeOut";
        else if constexpr (std::is_same_v<T, PlayTransport>) return "PlayTransport";
        else if constexpr (std::is_same_v<T, StopTransport>) return "StopTransport";
        else if constexpr (std::is_same_v<T, ToggleRecording>) return "ToggleRecording";
        else return "Unknown";
    }, action);
}

} // namespace mixmind::ai