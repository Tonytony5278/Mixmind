#pragma once

#include "MIDIEvent.h"
#include "../core/result.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <memory>
#include <algorithm>

namespace mixmind {

// MIDI Note with editing properties
struct MIDINote {
    uint8_t note_number;        // 0-127 (C-2 to G8)
    uint8_t velocity;           // 1-127 (note velocity)
    uint64_t start_time;        // Start time in samples
    uint64_t length;            // Note length in samples
    uint8_t channel;            // MIDI channel (0-15)
    
    // Note editing properties
    bool selected = false;      // Selected for editing
    bool muted = false;         // Muted note
    float probability = 1.0f;   // Probability for humanization (0.0-1.0)
    int8_t micro_timing = 0;    // Micro-timing offset in samples (-127 to +127)
    
    // Constructors
    MIDINote() = default;
    
    MIDINote(uint8_t note, uint8_t vel, uint64_t start, uint64_t len, uint8_t ch = 0)
        : note_number(note), velocity(vel), start_time(start), length(len), channel(ch) {}
    
    // Get end time
    uint64_t get_end_time() const { return start_time + length; }
    
    // Check if note overlaps with time range
    bool overlaps(uint64_t range_start, uint64_t range_end) const {
        return start_time < range_end && get_end_time() > range_start;
    }
    
    // Check if note contains time point
    bool contains_time(uint64_t time) const {
        return time >= start_time && time < get_end_time();
    }
    
    // Get note name (C4, D#5, etc.)
    std::string get_note_name() const {
        static const char* note_names[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        
        int octave = (note_number / 12) - 1;
        int note_class = note_number % 12;
        
        return std::string(note_names[note_class]) + std::to_string(octave);
    }
    
    // Convert to MIDI events (note on/off pair)
    std::pair<MIDIEvent, MIDIEvent> to_midi_events() const {
        MIDIEvent note_on = MIDIEvent::note_on(channel, note_number, velocity, start_time + micro_timing);
        MIDIEvent note_off = MIDIEvent::note_off(channel, note_number, 64, get_end_time() + micro_timing);
        return {note_on, note_off};
    }
};

// MIDI Control Change event with timing
struct MIDIControlChange {
    uint8_t controller;         // CC number (0-127)
    uint8_t value;              // CC value (0-127)
    uint64_t time;              // Time in samples
    uint8_t channel;            // MIDI channel (0-15)
    
    // Editing properties
    bool selected = false;
    
    MIDIControlChange() = default;
    
    MIDIControlChange(uint8_t cc, uint8_t val, uint64_t t, uint8_t ch = 0)
        : controller(cc), value(val), time(t), channel(ch) {}
    
    // Convert to MIDI event
    MIDIEvent to_midi_event() const {
        return MIDIEvent::control_change(channel, static_cast<MIDIController>(controller), value, time);
    }
};

// Piano Roll MIDI clip - the core data structure
class MIDIClip {
public:
    MIDIClip(const std::string& name = "MIDI Clip");
    ~MIDIClip() = default;
    
    // Clip properties
    const std::string& get_name() const { return m_name; }
    void set_name(const std::string& name) { m_name = name; }
    
    uint64_t get_length() const { return m_length; }
    void set_length(uint64_t length) { m_length = length; }
    
    uint64_t get_start_offset() const { return m_start_offset; }
    void set_start_offset(uint64_t offset) { m_start_offset = offset; }
    
    bool is_looped() const { return m_looped; }
    void set_looped(bool looped) { m_looped = looped; }
    
    // Note editing operations
    Result<bool> add_note(const MIDINote& note);
    Result<bool> remove_note(size_t note_index);
    Result<bool> remove_selected_notes();
    
    // Note access
    const std::vector<MIDINote>& get_notes() const { return m_notes; }
    std::vector<MIDINote>& get_notes_mutable() { return m_notes; }
    
    Result<MIDINote*> get_note_at_time(uint64_t time, uint8_t note_number);
    std::vector<MIDINote*> get_notes_in_range(uint64_t start_time, uint64_t end_time);
    std::vector<MIDINote*> get_selected_notes();
    
    // Note selection
    void select_all_notes();
    void deselect_all_notes();
    void select_notes_in_range(uint64_t start_time, uint64_t end_time, uint8_t min_note, uint8_t max_note);
    
    // Note manipulation
    Result<bool> move_selected_notes(int64_t time_delta, int8_t pitch_delta);
    Result<bool> resize_selected_notes(int64_t length_delta);
    Result<bool> set_selected_velocity(uint8_t velocity);
    Result<bool> scale_selected_velocity(float scale);
    
    // Quantization
    enum class QuantizeResolution {
        QUARTER = 4,      // 1/4 notes
        EIGHTH = 8,       // 1/8 notes  
        SIXTEENTH = 16,   // 1/16 notes
        THIRTY_SECOND = 32, // 1/32 notes
        TRIPLET_EIGHTH = -8,   // 1/8 triplets
        TRIPLET_SIXTEENTH = -16 // 1/16 triplets
    };
    
    Result<bool> quantize_selected_notes(QuantizeResolution resolution, float strength = 1.0f);
    Result<bool> quantize_all_notes(QuantizeResolution resolution, float strength = 1.0f);
    
    // CC (Control Change) operations
    Result<bool> add_cc_event(const MIDIControlChange& cc);
    Result<bool> remove_cc_event(size_t cc_index);
    const std::vector<MIDIControlChange>& get_cc_events() const { return m_cc_events; }
    std::vector<MIDIControlChange>& get_cc_events_mutable() { return m_cc_events; }
    
    // Get CC events for specific controller
    std::vector<MIDIControlChange*> get_cc_events_for_controller(uint8_t controller);
    
    // MIDI event generation
    MIDIEventBuffer generate_midi_events(uint64_t start_time, uint64_t end_time) const;
    MIDIEventBuffer generate_all_midi_events() const;
    
    // Clip operations
    Result<bool> cut_notes(uint64_t start_time, uint64_t end_time);
    Result<bool> copy_notes(uint64_t start_time, uint64_t end_time, std::vector<MIDINote>& out_notes) const;
    Result<bool> paste_notes(const std::vector<MIDINote>& notes, uint64_t paste_time);
    
    // Musical operations
    Result<bool> transpose_selected_notes(int8_t semitones);
    Result<bool> reverse_selected_notes();
    Result<bool> duplicate_selected_notes(uint64_t time_offset);
    
    // Humanization
    Result<bool> humanize_selected_notes(float timing_variance, float velocity_variance);
    
    // Utility functions
    void clear_all_notes() { m_notes.clear(); }
    void clear_all_cc() { m_cc_events.clear(); }
    size_t get_note_count() const { return m_notes.size(); }
    size_t get_cc_count() const { return m_cc_events.size(); }
    
    // Time conversion helpers (assumes 44.1kHz sample rate)
    static uint64_t beats_to_samples(double beats, double bpm = 120.0, double sample_rate = 44100.0);
    static double samples_to_beats(uint64_t samples, double bpm = 120.0, double sample_rate = 44100.0);
    static uint64_t bars_to_samples(double bars, double bpm = 120.0, int time_sig_num = 4, double sample_rate = 44100.0);
    
private:
    std::string m_name;
    uint64_t m_length = 0;          // Clip length in samples
    uint64_t m_start_offset = 0;    // Start offset in samples
    bool m_looped = false;          // Loop the clip
    
    std::vector<MIDINote> m_notes;
    std::vector<MIDIControlChange> m_cc_events;
    
    // Helper functions
    uint64_t quantize_time(uint64_t time, QuantizeResolution resolution, double bpm = 120.0, double sample_rate = 44100.0);
    bool validate_note(const MIDINote& note) const;
    void sort_notes_by_time();
    void sort_cc_by_time();
};

// MIDI Clip Factory for common patterns
class MIDIClipFactory {
public:
    // Create empty clip with specified length (in bars)
    static std::shared_ptr<MIDIClip> create_empty_clip(const std::string& name, double length_bars = 4.0, double bpm = 120.0);
    
    // Create clip with chord progression
    static std::shared_ptr<MIDIClip> create_chord_progression(
        const std::string& name,
        const std::vector<std::vector<uint8_t>>& chords,
        double chord_length_beats = 4.0,
        uint8_t velocity = 100,
        double bpm = 120.0
    );
    
    // Create drum pattern clip
    static std::shared_ptr<MIDIClip> create_drum_pattern(
        const std::string& name,
        const std::map<uint8_t, std::vector<double>>& drum_hits, // note -> beat positions
        double pattern_length_bars = 1.0,
        uint8_t velocity = 100,
        double bpm = 120.0
    );
    
    // Create scale pattern (for testing instruments)
    static std::shared_ptr<MIDIClip> create_scale_pattern(
        const std::string& name,
        uint8_t root_note = 60, // C4
        const std::vector<int>& scale_intervals = {0, 2, 4, 5, 7, 9, 11, 12}, // Major scale
        double note_length_beats = 0.5,
        uint8_t velocity = 100,
        double bpm = 120.0
    );
};

} // namespace mixmind