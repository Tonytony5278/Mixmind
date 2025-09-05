#pragma once

#include <string>
#include <variant>

namespace mixmind::ai {

// Core transport actions
struct SetTempo {
    double bpm;
    
    bool isValid() const { 
        return bpm > 0.0 && bpm <= 300.0; 
    }
};

struct SetLoop {
    double startBeats;
    double endBeats;
    
    bool isValid() const {
        return startBeats >= 0.0 && endBeats > startBeats && (endBeats - startBeats) <= 1000.0;
    }
};

struct SetCursor {
    double posBeats;
    
    bool isValid() const {
        return posBeats >= 0.0 && posBeats <= 10000.0; // Reasonable session length limit
    }
};

// Track management actions
struct AddAudioTrack {
    std::string name;
    
    bool isValid() const {
        return !name.empty() && name.length() <= 64;
    }
};

struct AddMidiTrack {
    std::string name;
    
    bool isValid() const {
        return !name.empty() && name.length() <= 64;
    }
};

// Audio processing actions
struct AdjustGain {
    int trackIndex;
    double dB;
    
    bool isValid() const {
        return trackIndex >= 0 && trackIndex < 128 && dB >= -60.0 && dB <= 12.0;
    }
};

struct Normalize {
    int trackIndex;
    double targetLUFS = -23.0;
    
    bool isValid() const {
        return trackIndex >= 0 && trackIndex < 128 && targetLUFS >= -60.0 && targetLUFS <= -6.0;
    }
};

// Clip processing actions
struct FadeIn {
    int clipId;
    int ms;
    
    bool isValid() const {
        return clipId >= 0 && ms > 0 && ms <= 30000; // Max 30 second fade
    }
};

struct FadeOut {
    int clipId;
    int ms;
    
    bool isValid() const {
        return clipId >= 0 && ms > 0 && ms <= 30000; // Max 30 second fade
    }
};

// Transport control actions
struct PlayTransport {
    bool fromStart = false;
    
    bool isValid() const { return true; }
};

struct StopTransport {
    bool returnToStart = false;
    
    bool isValid() const { return true; }
};

struct ToggleRecording {
    bool enable = true;
    
    bool isValid() const { return true; }
};

// Action variant type
using Action = std::variant<
    SetTempo,
    SetLoop, 
    SetCursor,
    AddAudioTrack,
    AddMidiTrack,
    AdjustGain,
    Normalize,
    FadeIn,
    FadeOut,
    PlayTransport,
    StopTransport,
    ToggleRecording
>;

// Result type for action processing
template<typename T>
struct Result {
    bool ok;
    std::string msg;
    T value;
    
    static Result<T> success(T val, const std::string& message = "") {
        return {true, message, std::move(val)};
    }
    
    static Result<T> error(const std::string& message) {
        return {false, message, {}};
    }
    
    explicit operator bool() const { return ok; }
};

// Specialized void result
template<>
struct Result<void> {
    bool ok;
    std::string msg;
    
    static Result<void> success(const std::string& message = "") {
        return {true, message};
    }
    
    static Result<void> error(const std::string& message) {
        return {false, message};
    }
    
    explicit operator bool() const { return ok; }
};

// Helper functions for action validation and string representation
std::string actionToString(const Action& action);
bool validateAction(const Action& action);
std::string getActionTypeName(const Action& action);

} // namespace mixmind::ai