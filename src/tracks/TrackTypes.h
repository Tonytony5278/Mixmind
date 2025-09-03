#pragma once

namespace mixmind {

// Track types with proper signal flow definitions
enum class TrackType {
    AUDIO,        // Audio in → Audio out (audio recordings, loops)
    MIDI,         // MIDI in → MIDI out (MIDI data routing)  
    INSTRUMENT,   // MIDI in → Audio out ⭐ (VSTi hosting)
    AUX_SEND,     // Audio routing (sends/returns)
    MASTER        // Final mix output (stereo sum)
};

// Signal flow capabilities for each track type
struct TrackSignalFlow {
    bool accepts_audio_input;
    bool accepts_midi_input;
    bool produces_audio_output;
    bool produces_midi_output;
    bool can_host_vsti;
    bool can_host_audio_fx;
    
    static TrackSignalFlow for_track_type(TrackType type) {
        switch (type) {
            case TrackType::AUDIO:
                return {true, false, true, false, false, true};
                
            case TrackType::MIDI:  
                return {false, true, false, true, false, false};
                
            case TrackType::INSTRUMENT:  // ⭐ Key track type
                return {false, true, true, false, true, true};
                
            case TrackType::AUX_SEND:
                return {true, false, true, false, false, true};
                
            case TrackType::MASTER:
                return {true, false, true, false, false, true};
        }
        
        return {false, false, false, false, false, false};
    }
};

} // namespace mixmind