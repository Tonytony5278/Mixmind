#include "PianoRollEditor.h"
#include <algorithm>
#include <cmath>

namespace mixmind {

PianoRollEditor::PianoRollEditor(std::shared_ptr<MIDIClip> clip) 
    : m_clip(clip) {
    // Set default view for 4 bars at 120 BPM
    m_view.end_time = MIDIClip::bars_to_samples(4.0, 120.0, 4, 44100.0);
    m_view.min_note = 36;  // C2 (typical bottom of piano roll)
    m_view.max_note = 96;  // C7 (typical top of piano roll)
}

Result<bool> PianoRollEditor::draw_note_at_position(double time_beats, uint8_t note_number, double length_beats, uint8_t velocity) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    save_state_snapshot();
    
    // Apply grid snapping
    double snapped_time = snap_beats_to_grid(time_beats);
    double snapped_length = snap_beats_to_grid(length_beats);
    if (snapped_length < 0.25) snapped_length = 0.25; // Minimum 1/16 note
    
    uint64_t start_time_samples = beats_to_samples(snapped_time);
    uint64_t length_samples = beats_to_samples(snapped_length);
    
    // Check if note already exists at this position
    auto existing_note = find_note_at_position(snapped_time, note_number);
    if (existing_note) {
        // Update existing note
        existing_note->velocity = velocity;
        existing_note->length = length_samples;
    } else {
        // Create new note
        MIDINote note(note_number, velocity, start_time_samples, length_samples, m_default_props.channel);
        auto result = m_clip->add_note(note);
        if (!result.is_success()) {
            return result;
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::erase_note_at_position(double time_beats, uint8_t note_number) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    auto note = find_note_at_position(time_beats, note_number);
    if (!note) {
        return Result<bool>::error("No note found at position");
    }
    
    save_state_snapshot();
    
    // Find the note index and remove it
    auto& notes = m_clip->get_notes_mutable();
    auto it = std::find_if(notes.begin(), notes.end(),
        [note](const MIDINote& n) { return &n == note; });
    
    if (it != notes.end()) {
        notes.erase(it);
        notify_edit_changed();
        return Result<bool>::success(true);
    }
    
    return Result<bool>::error("Failed to erase note");
}

Result<bool> PianoRollEditor::erase_notes_in_region(double start_time_beats, double end_time_beats, uint8_t min_note, uint8_t max_note) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    save_state_snapshot();
    
    uint64_t start_samples = beats_to_samples(start_time_beats);
    uint64_t end_samples = beats_to_samples(end_time_beats);
    
    auto& notes = m_clip->get_notes_mutable();
    size_t removed_count = 0;
    
    // Remove notes that overlap with the region
    auto it = std::remove_if(notes.begin(), notes.end(),
        [start_samples, end_samples, min_note, max_note, &removed_count](const MIDINote& note) {
            bool overlaps = note.overlaps(start_samples, end_samples);
            bool in_pitch_range = note.note_number >= min_note && note.note_number <= max_note;
            if (overlaps && in_pitch_range) {
                removed_count++;
                return true;
            }
            return false;
        });
    
    notes.erase(it, notes.end());
    
    if (removed_count > 0) {
        notify_edit_changed();
    }
    
    return Result<bool>::success(removed_count > 0);
}

Result<bool> PianoRollEditor::trim_note_start(MIDINote* note, double new_start_time_beats) {
    if (!note || !m_clip) {
        return Result<bool>::error("Invalid note or no clip loaded");
    }
    
    save_state_snapshot();
    
    double snapped_time = snap_beats_to_grid(new_start_time_beats);
    uint64_t new_start_samples = beats_to_samples(snapped_time);
    uint64_t original_end = note->get_end_time();
    
    // Ensure the new start time doesn't go past the original end
    if (new_start_samples >= original_end) {
        return Result<bool>::error("New start time would eliminate note");
    }
    
    note->start_time = new_start_samples;
    note->length = original_end - new_start_samples;
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::trim_note_end(MIDINote* note, double new_end_time_beats) {
    if (!note || !m_clip) {
        return Result<bool>::error("Invalid note or no clip loaded");
    }
    
    save_state_snapshot();
    
    double snapped_time = snap_beats_to_grid(new_end_time_beats);
    uint64_t new_end_samples = beats_to_samples(snapped_time);
    
    // Ensure the new end time doesn't go before the start
    if (new_end_samples <= note->start_time) {
        return Result<bool>::error("New end time would eliminate note");
    }
    
    note->length = new_end_samples - note->start_time;
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::split_note_at_time(MIDINote* note, double split_time_beats) {
    if (!note || !m_clip) {
        return Result<bool>::error("Invalid note or no clip loaded");
    }
    
    save_state_snapshot();
    
    double snapped_time = snap_beats_to_grid(split_time_beats);
    uint64_t split_samples = beats_to_samples(snapped_time);
    
    // Check if split point is within the note
    if (split_samples <= note->start_time || split_samples >= note->get_end_time()) {
        return Result<bool>::error("Split point is outside note boundaries");
    }
    
    // Create the second half of the note
    uint64_t original_end = note->get_end_time();
    MIDINote second_half = *note;
    second_half.start_time = split_samples;
    second_half.length = original_end - split_samples;
    
    // Trim the original note to the split point
    note->length = split_samples - note->start_time;
    
    // Add the second half
    auto result = m_clip->add_note(second_half);
    if (!result.is_success()) {
        return result;
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::split_selected_notes_at_time(double split_time_beats) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    save_state_snapshot();
    
    auto selected_notes = m_clip->get_selected_notes();
    if (selected_notes.empty()) {
        return Result<bool>::error("No notes selected");
    }
    
    // Split each selected note that contains the split point
    for (auto note : selected_notes) {
        uint64_t split_samples = beats_to_samples(split_time_beats);
        if (note->contains_time(split_samples)) {
            auto result = split_note_at_time(note, split_time_beats);
            if (!result.is_success()) {
                // Continue with other notes even if one fails
                continue;
            }
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::select_note_at_position(double time_beats, uint8_t note_number, bool add_to_selection) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    if (!add_to_selection) {
        m_clip->deselect_all_notes();
    }
    
    auto note = find_note_at_position(time_beats, note_number);
    if (note) {
        note->selected = true;
        return Result<bool>::success(true);
    }
    
    return Result<bool>::error("No note found at position");
}

Result<bool> PianoRollEditor::select_notes_in_region(double start_time_beats, double end_time_beats, uint8_t min_note, uint8_t max_note, bool add_to_selection) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    if (!add_to_selection) {
        m_clip->deselect_all_notes();
    }
    
    uint64_t start_samples = beats_to_samples(start_time_beats);
    uint64_t end_samples = beats_to_samples(end_time_beats);
    
    m_clip->select_notes_in_range(start_samples, end_samples, min_note, max_note);
    
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::set_note_velocity(MIDINote* note, uint8_t velocity) {
    if (!note) {
        return Result<bool>::error("Invalid note");
    }
    
    if (velocity == 0 || velocity > 127) {
        return Result<bool>::error("Invalid velocity value");
    }
    
    save_state_snapshot();
    note->velocity = velocity;
    notify_edit_changed();
    
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::adjust_selected_velocity(int velocity_delta) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    save_state_snapshot();
    
    auto selected_notes = m_clip->get_selected_notes();
    if (selected_notes.empty()) {
        return Result<bool>::error("No notes selected");
    }
    
    for (auto note : selected_notes) {
        int new_velocity = static_cast<int>(note->velocity) + velocity_delta;
        new_velocity = std::clamp(new_velocity, 1, 127);
        note->velocity = static_cast<uint8_t>(new_velocity);
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::scale_selected_velocity(float scale_factor) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    return m_clip->scale_selected_velocity(scale_factor);
}

Result<bool> PianoRollEditor::duplicate_selected_notes(double time_offset_beats) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    save_state_snapshot();
    
    auto selected_notes = m_clip->get_selected_notes();
    if (selected_notes.empty()) {
        return Result<bool>::error("No notes selected");
    }
    
    uint64_t offset_samples = beats_to_samples(time_offset_beats);
    
    // Create duplicated notes
    for (const auto note : selected_notes) {
        MIDINote duplicated = *note;
        duplicated.start_time += offset_samples;
        duplicated.selected = false; // Deselect duplicates
        
        auto result = m_clip->add_note(duplicated);
        if (!result.is_success()) {
            return result;
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::transpose_selected_notes(int8_t semitones) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    save_state_snapshot();
    auto result = m_clip->transpose_selected_notes(semitones);
    if (result.is_success()) {
        notify_edit_changed();
    }
    return result;
}

Result<bool> PianoRollEditor::quantize_selected_notes(MIDIClip::QuantizeResolution resolution, float strength) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    save_state_snapshot();
    auto result = m_clip->quantize_selected_notes(resolution, strength);
    if (result.is_success()) {
        notify_edit_changed();
    }
    return result;
}

// Time conversion helpers
uint64_t PianoRollEditor::beats_to_samples(double beats) const {
    return MIDIClip::beats_to_samples(beats, m_view.bpm, 44100.0);
}

double PianoRollEditor::samples_to_beats(uint64_t samples) const {
    return MIDIClip::samples_to_beats(samples, m_view.bpm, 44100.0);
}

uint64_t PianoRollEditor::snap_time_to_grid(uint64_t time) const {
    if (m_grid_snap == GridSnap::OFF) {
        return time;
    }
    
    double beats = samples_to_beats(time);
    double snapped_beats = snap_beats_to_grid(beats);
    return beats_to_samples(snapped_beats);
}

double PianoRollEditor::snap_beats_to_grid(double beats) const {
    if (m_grid_snap == GridSnap::OFF) {
        return beats;
    }
    
    double grid_size = 0.0;
    switch (m_grid_snap) {
        case GridSnap::QUARTER:        grid_size = 1.0; break;
        case GridSnap::EIGHTH:         grid_size = 0.5; break;
        case GridSnap::SIXTEENTH:      grid_size = 0.25; break;
        case GridSnap::THIRTY_SECOND:  grid_size = 0.125; break;
        case GridSnap::TRIPLET_EIGHTH: grid_size = 1.0 / 3.0; break;
        case GridSnap::TRIPLET_SIXTEENTH: grid_size = 1.0 / 6.0; break;
        default: return beats;
    }
    
    return std::round(beats / grid_size) * grid_size;
}

MIDINote* PianoRollEditor::find_note_at_position(double time_beats, uint8_t note_number, double tolerance_beats) {
    if (!m_clip) {
        return nullptr;
    }
    
    uint64_t time_samples = beats_to_samples(time_beats);
    uint64_t tolerance_samples = beats_to_samples(tolerance_beats);
    
    auto& notes = m_clip->get_notes_mutable();
    for (auto& note : notes) {
        if (note.note_number == note_number) {
            // Check if the time is within the note boundaries + tolerance
            uint64_t start_with_tolerance = (note.start_time > tolerance_samples) ? 
                note.start_time - tolerance_samples : 0;
            uint64_t end_with_tolerance = note.get_end_time() + tolerance_samples;
            
            if (time_samples >= start_with_tolerance && time_samples <= end_with_tolerance) {
                return &note;
            }
        }
    }
    
    return nullptr;
}

std::vector<MIDINote*> PianoRollEditor::find_notes_in_region(double start_time_beats, double end_time_beats, uint8_t min_note, uint8_t max_note) {
    if (!m_clip) {
        return {};
    }
    
    uint64_t start_samples = beats_to_samples(start_time_beats);
    uint64_t end_samples = beats_to_samples(end_time_beats);
    
    return m_clip->get_notes_in_range(start_samples, end_samples);
}

Result<bool> PianoRollEditor::copy_selected_notes() {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    auto selected_notes = m_clip->get_selected_notes();
    if (selected_notes.empty()) {
        return Result<bool>::error("No notes selected");
    }
    
    m_clipboard.clear();
    for (const auto note : selected_notes) {
        m_clipboard.push_back(*note);
    }
    
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::cut_selected_notes() {
    auto copy_result = copy_selected_notes();
    if (!copy_result.is_success()) {
        return copy_result;
    }
    
    save_state_snapshot();
    auto result = m_clip->remove_selected_notes();
    if (result.is_success()) {
        notify_edit_changed();
    }
    return result;
}

Result<bool> PianoRollEditor::paste_notes_at_time(double time_beats) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    if (m_clipboard.empty()) {
        return Result<bool>::error("No notes in clipboard");
    }
    
    save_state_snapshot();
    
    // Calculate the offset needed to paste at the target time
    uint64_t paste_time_samples = beats_to_samples(time_beats);
    uint64_t earliest_start = m_clipboard[0].start_time;
    for (const auto& note : m_clipboard) {
        if (note.start_time < earliest_start) {
            earliest_start = note.start_time;
        }
    }
    
    int64_t time_offset = static_cast<int64_t>(paste_time_samples) - static_cast<int64_t>(earliest_start);
    
    // Paste notes with the calculated offset
    for (auto note : m_clipboard) {
        note.start_time = static_cast<uint64_t>(static_cast<int64_t>(note.start_time) + time_offset);
        note.selected = true; // Select pasted notes
        
        auto result = m_clip->add_note(note);
        if (!result.is_success()) {
            return result;
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

void PianoRollEditor::save_state_snapshot() {
    if (!m_clip) return;
    
    StateSnapshot snapshot = create_state_snapshot();
    
    // Add to undo stack
    m_undo_stack.push_back(snapshot);
    
    // Limit undo stack size
    if (m_undo_stack.size() > MAX_UNDO_STATES) {
        m_undo_stack.erase(m_undo_stack.begin());
    }
    
    // Clear redo stack when new changes are made
    m_redo_stack.clear();
}

Result<bool> PianoRollEditor::undo_last_operation() {
    if (!m_clip || m_undo_stack.empty()) {
        return Result<bool>::error("Nothing to undo");
    }
    
    // Save current state to redo stack
    m_redo_stack.push_back(create_state_snapshot());
    
    // Restore previous state
    StateSnapshot state = m_undo_stack.back();
    m_undo_stack.pop_back();
    restore_state_snapshot(state);
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> PianoRollEditor::redo_last_operation() {
    if (!m_clip || m_redo_stack.empty()) {
        return Result<bool>::error("Nothing to redo");
    }
    
    // Save current state to undo stack
    m_undo_stack.push_back(create_state_snapshot());
    
    // Restore next state
    StateSnapshot state = m_redo_stack.back();
    m_redo_stack.pop_back();
    restore_state_snapshot(state);
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

void PianoRollEditor::clear_undo_history() {
    m_undo_stack.clear();
    m_redo_stack.clear();
}

void PianoRollEditor::notify_edit_changed() {
    if (m_edit_callback) {
        m_edit_callback();
    }
}

PianoRollEditor::StateSnapshot PianoRollEditor::create_state_snapshot() const {
    StateSnapshot snapshot;
    if (m_clip) {
        snapshot.notes = m_clip->get_notes();
        snapshot.cc_events = m_clip->get_cc_events();
    }
    return snapshot;
}

void PianoRollEditor::restore_state_snapshot(const StateSnapshot& snapshot) {
    if (!m_clip) return;
    
    m_clip->clear_all_notes();
    m_clip->clear_all_cc();
    
    for (const auto& note : snapshot.notes) {
        m_clip->add_note(note);
    }
    
    for (const auto& cc : snapshot.cc_events) {
        m_clip->add_cc_event(cc);
    }
}

// Factory implementations
std::unique_ptr<PianoRollEditor> PianoRollFactory::create_standard_editor(std::shared_ptr<MIDIClip> clip) {
    auto editor = std::make_unique<PianoRollEditor>(clip);
    editor->set_grid_snap(GridSnap::SIXTEENTH);
    editor->set_mode(PianoRollMode::DRAW);
    
    PianoRollEditor::DefaultNoteProperties props;
    props.velocity = 100;
    props.length_beats = 1.0;
    props.channel = 0;
    editor->set_default_note_properties(props);
    
    return editor;
}

std::unique_ptr<PianoRollEditor> PianoRollFactory::create_drum_editor(std::shared_ptr<MIDIClip> clip) {
    auto editor = std::make_unique<PianoRollEditor>(clip);
    editor->set_grid_snap(GridSnap::SIXTEENTH);
    editor->set_mode(PianoRollMode::DRAW);
    editor->set_step_input_enabled(true);
    editor->set_drum_grid_enabled(true);
    
    PianoRollEditor::DefaultNoteProperties props;
    props.velocity = 127;  // Higher default velocity for drums
    props.length_beats = 0.25;  // Shorter notes for drums
    props.channel = 9;  // Standard MIDI drum channel
    editor->set_default_note_properties(props);
    
    // Set view to show typical drum range
    PianoRollView view = editor->get_view();
    view.min_note = 35;  // B1 (kick drum range)
    view.max_note = 81;  // A5 (cymbal range)
    editor->set_view(view);
    
    return editor;
}

std::unique_ptr<PianoRollEditor> PianoRollFactory::create_melody_editor(std::shared_ptr<MIDIClip> clip) {
    auto editor = std::make_unique<PianoRollEditor>(clip);
    editor->set_grid_snap(GridSnap::EIGHTH);
    editor->set_mode(PianoRollMode::DRAW);
    
    PianoRollEditor::DefaultNoteProperties props;
    props.velocity = 80;  // Moderate velocity for melody
    props.length_beats = 0.5;  // Eighth note default
    props.channel = 0;
    editor->set_default_note_properties(props);
    
    // Set view to show melodic range
    PianoRollView view = editor->get_view();
    view.min_note = 48;  // C3
    view.max_note = 84;  // C6
    editor->set_view(view);
    
    return editor;
}

} // namespace mixmind