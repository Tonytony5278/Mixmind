#pragma once

#include "../midi/MIDIClip.h"
#include "../core/result.h"
#include <memory>
#include <vector>
#include <functional>

namespace mixmind {

// Piano Roll editing modes
enum class PianoRollMode {
    DRAW,           // Draw new notes
    ERASE,          // Erase existing notes
    SELECT,         // Select notes for editing
    TRIM,           // Trim note start/end
    VELOCITY,       // Edit note velocity
    SPLIT           // Split notes
};

// Grid snap settings for musical timing
enum class GridSnap {
    OFF,            // No grid snapping
    QUARTER,        // 1/4 notes
    EIGHTH,         // 1/8 notes
    SIXTEENTH,      // 1/16 notes
    THIRTY_SECOND,  // 1/32 notes
    TRIPLET_EIGHTH, // 1/8 triplets
    TRIPLET_SIXTEENTH // 1/16 triplets
};

// Piano Roll view settings
struct PianoRollView {
    uint64_t start_time = 0;        // View start time in samples
    uint64_t end_time = 0;          // View end time in samples
    uint8_t min_note = 0;           // Lowest visible MIDI note
    uint8_t max_note = 127;         // Highest visible MIDI note
    float pixels_per_beat = 32.0f;  // Horizontal zoom
    float pixels_per_note = 12.0f;  // Vertical zoom
    double bpm = 120.0;             // Current BPM for timing
};

// Piano Roll editor - handles interactive MIDI editing
class PianoRollEditor {
public:
    PianoRollEditor(std::shared_ptr<MIDIClip> clip = nullptr);
    ~PianoRollEditor() = default;
    
    // Clip management
    void set_clip(std::shared_ptr<MIDIClip> clip) { m_clip = clip; }
    std::shared_ptr<MIDIClip> get_clip() const { return m_clip; }
    
    // Editing mode
    void set_mode(PianoRollMode mode) { m_mode = mode; }
    PianoRollMode get_mode() const { return m_mode; }
    
    // Grid and snap settings
    void set_grid_snap(GridSnap snap) { m_grid_snap = snap; }
    GridSnap get_grid_snap() const { return m_grid_snap; }
    
    void set_view(const PianoRollView& view) { m_view = view; }
    const PianoRollView& get_view() const { return m_view; }
    
    // Drawing operations (mouse/touch input)
    Result<bool> draw_note_at_position(double time_beats, uint8_t note_number, double length_beats = 1.0, uint8_t velocity = 100);
    Result<bool> erase_note_at_position(double time_beats, uint8_t note_number);
    Result<bool> erase_notes_in_region(double start_time_beats, double end_time_beats, uint8_t min_note, uint8_t max_note);
    
    // Note trimming operations
    Result<bool> trim_note_start(MIDINote* note, double new_start_time_beats);
    Result<bool> trim_note_end(MIDINote* note, double new_end_time_beats);
    
    // Note splitting
    Result<bool> split_note_at_time(MIDINote* note, double split_time_beats);
    Result<bool> split_selected_notes_at_time(double split_time_beats);
    
    // Selection operations
    Result<bool> select_note_at_position(double time_beats, uint8_t note_number, bool add_to_selection = false);
    Result<bool> select_notes_in_region(double start_time_beats, double end_time_beats, uint8_t min_note, uint8_t max_note, bool add_to_selection = false);
    Result<bool> select_all_notes() { if (m_clip) m_clip->select_all_notes(); return Result<bool>::success(true); }
    Result<bool> deselect_all_notes() { if (m_clip) m_clip->deselect_all_notes(); return Result<bool>::success(true); }
    
    // Velocity editing
    Result<bool> set_note_velocity(MIDINote* note, uint8_t velocity);
    Result<bool> adjust_selected_velocity(int velocity_delta);
    Result<bool> scale_selected_velocity(float scale_factor);
    
    // Musical operations
    Result<bool> duplicate_selected_notes(double time_offset_beats);
    Result<bool> transpose_selected_notes(int8_t semitones);
    Result<bool> quantize_selected_notes(MIDIClip::QuantizeResolution resolution, float strength = 1.0f);
    
    // Time/pitch conversion helpers
    uint64_t beats_to_samples(double beats) const;
    double samples_to_beats(uint64_t samples) const;
    uint64_t snap_time_to_grid(uint64_t time) const;
    double snap_beats_to_grid(double beats) const;
    
    // Note finding and hit testing
    MIDINote* find_note_at_position(double time_beats, uint8_t note_number, double tolerance_beats = 0.1);
    std::vector<MIDINote*> find_notes_in_region(double start_time_beats, double end_time_beats, uint8_t min_note, uint8_t max_note);
    
    // Clipboard operations
    Result<bool> copy_selected_notes();
    Result<bool> cut_selected_notes();
    Result<bool> paste_notes_at_time(double time_beats);
    
    // Undo/Redo support (state snapshots)
    void save_state_snapshot();
    Result<bool> undo_last_operation();
    Result<bool> redo_last_operation();
    void clear_undo_history();
    
    // Default note properties
    struct DefaultNoteProperties {
        uint8_t velocity = 100;
        double length_beats = 1.0;
        uint8_t channel = 0;
    };
    
    void set_default_note_properties(const DefaultNoteProperties& props) { m_default_props = props; }
    const DefaultNoteProperties& get_default_note_properties() const { return m_default_props; }
    
    // Step input mode (for drum programming)
    bool is_step_input_enabled() const { return m_step_input_enabled; }
    void set_step_input_enabled(bool enabled) { m_step_input_enabled = enabled; }
    
    // Drum grid mode (fixed velocities per lane)
    bool is_drum_grid_enabled() const { return m_drum_grid_enabled; }
    void set_drum_grid_enabled(bool enabled) { m_drum_grid_enabled = enabled; }
    
    // Event callback for UI updates
    using EditCallback = std::function<void()>;
    void set_edit_callback(EditCallback callback) { m_edit_callback = callback; }

private:
    std::shared_ptr<MIDIClip> m_clip;
    PianoRollMode m_mode = PianoRollMode::DRAW;
    GridSnap m_grid_snap = GridSnap::SIXTEENTH;
    PianoRollView m_view;
    DefaultNoteProperties m_default_props;
    
    bool m_step_input_enabled = false;
    bool m_drum_grid_enabled = false;
    
    // Clipboard for copy/paste
    std::vector<MIDINote> m_clipboard;
    
    // Undo/Redo state management
    struct StateSnapshot {
        std::vector<MIDINote> notes;
        std::vector<MIDIControlChange> cc_events;
    };
    std::vector<StateSnapshot> m_undo_stack;
    std::vector<StateSnapshot> m_redo_stack;
    static constexpr size_t MAX_UNDO_STATES = 50;
    
    EditCallback m_edit_callback;
    
    // Helper methods
    void notify_edit_changed();
    StateSnapshot create_state_snapshot() const;
    void restore_state_snapshot(const StateSnapshot& snapshot);
    GridSnap grid_snap_from_quantize_resolution(MIDIClip::QuantizeResolution resolution) const;
    MIDIClip::QuantizeResolution quantize_resolution_from_grid_snap(GridSnap snap) const;
};

// Factory for creating common piano roll setups
class PianoRollFactory {
public:
    // Create editor with standard settings
    static std::unique_ptr<PianoRollEditor> create_standard_editor(std::shared_ptr<MIDIClip> clip = nullptr);
    
    // Create editor configured for drum programming
    static std::unique_ptr<PianoRollEditor> create_drum_editor(std::shared_ptr<MIDIClip> clip = nullptr);
    
    // Create editor for melody/chord editing
    static std::unique_ptr<PianoRollEditor> create_melody_editor(std::shared_ptr<MIDIClip> clip = nullptr);
};

} // namespace mixmind