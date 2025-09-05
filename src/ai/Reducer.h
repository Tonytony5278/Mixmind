#pragma once

#include "Actions.h"
#include <memory>
#include <functional>
#include <vector>

// Forward declarations for Tracktion Engine types
namespace te = tracktion_engine;

namespace mixmind::ai {

// Application state that the reducer operates on
struct AppState {
    // Core session state
    double currentTempo = 120.0;
    double currentPosition = 0.0;
    bool isLooping = false;
    double loopStart = 0.0;
    double loopEnd = 8.0;
    
    // Transport state
    bool isPlaying = false;
    bool isRecording = false;
    
    // Track state
    struct TrackInfo {
        int index;
        std::string name;
        bool isMuted = false;
        bool isSolo = false;
        double gain = 0.0; // dB
    };
    std::vector<TrackInfo> tracks;
    
    // Validation helpers
    bool hasTrack(int index) const {
        return index >= 0 && index < static_cast<int>(tracks.size());
    }
    
    TrackInfo* getTrack(int index) {
        return hasTrack(index) ? &tracks[index] : nullptr;
    }
    
    const TrackInfo* getTrack(int index) const {
        return hasTrack(index) ? &tracks[index] : nullptr;
    }
};

// Event types for undo/redo system
enum class EventType {
    TempoChanged,
    LoopChanged,
    CursorMoved,
    TrackAdded,
    TrackGainChanged,
    TransportStateChanged,
    AudioProcessed
};

struct DomainEvent {
    EventType type;
    std::string description;
    Action originalAction;
    
    // Serialization data for undo/redo
    std::string undoData;
    
    DomainEvent(EventType t, const std::string& desc, const Action& action)
        : type(t), description(desc), originalAction(action) {}
};

// Forward declaration of Tracktion interface
class ITracktionInterface {
public:
    virtual ~ITracktionInterface() = default;
    
    // Transport operations
    virtual Result<void> setTempo(double bpm) = 0;
    virtual Result<void> setLoopRange(double startBeats, double endBeats) = 0;
    virtual Result<void> setCursorPosition(double posBeats) = 0;
    virtual Result<void> playTransport(bool fromStart) = 0;
    virtual Result<void> stopTransport(bool returnToStart) = 0;
    virtual Result<void> toggleRecording(bool enable) = 0;
    
    // Track operations  
    virtual Result<int> addAudioTrack(const std::string& name) = 0;
    virtual Result<int> addMidiTrack(const std::string& name) = 0;
    virtual Result<void> setTrackGain(int trackIndex, double dB) = 0;
    
    // Audio processing
    virtual Result<void> normalizeTrack(int trackIndex, double targetLUFS) = 0;
    virtual Result<void> fadeClipIn(int clipId, int durationMs) = 0;
    virtual Result<void> fadeClipOut(int clipId, int durationMs) = 0;
};

// Main reducer class
class Reducer {
public:
    using EventCallback = std::function<void(const DomainEvent&)>;
    
    explicit Reducer(std::shared_ptr<ITracktionInterface> tracktionInterface);
    ~Reducer();
    
    // Main reduction function
    Result<void> reduce(const Action& action, AppState& state);
    
    // Event handling
    void setEventCallback(EventCallback callback);
    void clearEventCallback();
    
    // State validation
    static bool validateState(const AppState& state);
    
    // Action processing helpers
    static std::string getActionValidationError(const Action& action);
    
private:
    std::shared_ptr<ITracktionInterface> tracktion_;
    EventCallback eventCallback_;
    
    // Individual action handlers
    Result<void> handleSetTempo(const SetTempo& action, AppState& state);
    Result<void> handleSetLoop(const SetLoop& action, AppState& state);
    Result<void> handleSetCursor(const SetCursor& action, AppState& state);
    Result<void> handleAddAudioTrack(const AddAudioTrack& action, AppState& state);
    Result<void> handleAddMidiTrack(const AddMidiTrack& action, AppState& state);
    Result<void> handleAdjustGain(const AdjustGain& action, AppState& state);
    Result<void> handleNormalize(const Normalize& action, AppState& state);
    Result<void> handleFadeIn(const FadeIn& action, AppState& state);
    Result<void> handleFadeOut(const FadeOut& action, AppState& state);
    Result<void> handlePlayTransport(const PlayTransport& action, AppState& state);
    Result<void> handleStopTransport(const StopTransport& action, AppState& state);
    Result<void> handleToggleRecording(const ToggleRecording& action, AppState& state);
    
    // Event emission
    void emitEvent(EventType type, const std::string& description, const Action& action);
    
    // Validation helpers
    bool validatePreconditions(const Action& action, const AppState& state);
    std::string getPreconditionError(const Action& action, const AppState& state);
};

} // namespace mixmind::ai