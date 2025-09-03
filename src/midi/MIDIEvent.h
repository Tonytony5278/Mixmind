#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace mixmind {

// MIDI event types for comprehensive MIDI handling
enum class MIDIEventType : uint8_t {
    NOTE_ON = 0x90,
    NOTE_OFF = 0x80,
    CONTROL_CHANGE = 0xB0,     // CC messages (mod wheel, etc.)
    PITCH_BEND = 0xE0,         // Pitch bend wheel
    AFTERTOUCH = 0xA0,         // Polyphonic aftertouch
    CHANNEL_PRESSURE = 0xD0,   // Channel aftertouch
    PROGRAM_CHANGE = 0xC0,     // Patch/preset changes
    SYSTEM_EXCLUSIVE = 0xF0    // SysEx data
};

// Standard MIDI CC numbers for common controllers
enum class MIDIController : uint8_t {
    MOD_WHEEL = 1,           // Modulation wheel
    BREATH = 2,              // Breath controller
    FOOT = 4,                // Foot controller  
    PORTAMENTO_TIME = 5,     // Portamento time
    DATA_ENTRY_MSB = 6,      // Data entry MSB
    VOLUME = 7,              // Channel volume
    BALANCE = 8,             // Balance
    PAN = 10,                // Pan position
    EXPRESSION = 11,         // Expression controller
    EFFECT_1 = 12,           // Effect control 1
    EFFECT_2 = 13,           // Effect control 2
    SUSTAIN = 64,            // Sustain pedal
    PORTAMENTO = 65,         // Portamento on/off
    SOSTENUTO = 66,          // Sostenuto pedal
    SOFT = 67,               // Soft pedal
    FILTER_RESONANCE = 71,   // Filter resonance
    RELEASE_TIME = 72,       // Release time
    ATTACK_TIME = 73,        // Attack time
    BRIGHTNESS = 74,         // Brightness/cutoff
    REVERB = 91,             // Reverb level
    TREMOLO = 92,            // Tremolo level
    CHORUS = 93,             // Chorus level
    DETUNE = 94,             // Detune level
    PHASER = 95,             // Phaser level
    ALL_SOUND_OFF = 120,     // All sound off
    RESET_ALL = 121,         // Reset all controllers
    LOCAL_CONTROL = 122,     // Local control on/off
    ALL_NOTES_OFF = 123      // All notes off
};

// Comprehensive MIDI event structure
struct MIDIEvent {
    MIDIEventType type;
    uint8_t channel;         // 0-15 (MIDI channels 1-16)
    uint8_t data1;           // Note number, CC number, etc.
    uint8_t data2;           // Velocity, CC value, etc.
    uint64_t timestamp;      // Sample-accurate timing
    
    // Helper constructors for common MIDI events
    static MIDIEvent note_on(uint8_t channel, uint8_t note, uint8_t velocity, uint64_t timestamp = 0) {
        return {MIDIEventType::NOTE_ON, channel, note, velocity, timestamp};
    }
    
    static MIDIEvent note_off(uint8_t channel, uint8_t note, uint8_t velocity = 64, uint64_t timestamp = 0) {
        return {MIDIEventType::NOTE_OFF, channel, note, velocity, timestamp};
    }
    
    static MIDIEvent control_change(uint8_t channel, MIDIController controller, uint8_t value, uint64_t timestamp = 0) {
        return {MIDIEventType::CONTROL_CHANGE, channel, static_cast<uint8_t>(controller), value, timestamp};
    }
    
    static MIDIEvent pitch_bend(uint8_t channel, uint16_t bend_value, uint64_t timestamp = 0) {
        // 14-bit pitch bend value (0-16383, center = 8192)
        uint8_t lsb = bend_value & 0x7F;
        uint8_t msb = (bend_value >> 7) & 0x7F;
        return {MIDIEventType::PITCH_BEND, channel, lsb, msb, timestamp};
    }
    
    // Get 14-bit pitch bend value (0-16383, center = 8192)
    uint16_t get_pitch_bend_value() const {
        if (type == MIDIEventType::PITCH_BEND) {
            return data1 | (data2 << 7);
        }
        return 8192; // Center value
    }
    
    // Check if this is a note event (on or off)
    bool is_note_event() const {
        return type == MIDIEventType::NOTE_ON || type == MIDIEventType::NOTE_OFF;
    }
    
    // Get note name for note events
    std::string get_note_name() const {
        if (!is_note_event()) return "";
        
        static const char* note_names[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        
        int octave = (data1 / 12) - 1;
        int note_class = data1 % 12;
        
        return std::string(note_names[note_class]) + std::to_string(octave);
    }
    
    // Convert to readable string for debugging
    std::string to_string() const {
        switch (type) {
            case MIDIEventType::NOTE_ON:
                return "Note On: Ch" + std::to_string(channel + 1) + 
                       " " + get_note_name() + " Vel=" + std::to_string(data2);
                       
            case MIDIEventType::NOTE_OFF:
                return "Note Off: Ch" + std::to_string(channel + 1) + 
                       " " + get_note_name() + " Vel=" + std::to_string(data2);
                       
            case MIDIEventType::CONTROL_CHANGE:
                return "CC: Ch" + std::to_string(channel + 1) + 
                       " CC" + std::to_string(data1) + "=" + std::to_string(data2);
                       
            case MIDIEventType::PITCH_BEND:
                return "Pitch Bend: Ch" + std::to_string(channel + 1) + 
                       " Value=" + std::to_string(get_pitch_bend_value());
                       
            default:
                return "MIDI Event: Type=0x" + std::to_string(static_cast<int>(type));
        }
    }
};

// MIDI event buffer for real-time processing
using MIDIEventBuffer = std::vector<MIDIEvent>;

// Sort MIDI events by timestamp for correct playback order
inline void sort_midi_events(MIDIEventBuffer& events) {
    std::sort(events.begin(), events.end(), 
              [](const MIDIEvent& a, const MIDIEvent& b) {
                  return a.timestamp < b.timestamp;
              });
}

} // namespace mixmind