#include <gtest/gtest.h>
#include "../src/midi/MIDIClip.h"
#include "../src/ui/PianoRollEditor.h"
#include "../src/ui/CCLaneEditor.h"
#include "../src/ui/StepSequencer.h"
#include <memory>
#include <chrono>

using namespace mixmind;

class PianoRollTest : public ::testing::Test {
protected:
    void SetUp() override {
        clip = std::make_shared<MIDIClip>("Test Clip");
        editor = PianoRollFactory::create_standard_editor(clip);
    }

    std::shared_ptr<MIDIClip> clip;
    std::unique_ptr<PianoRollEditor> editor;
};

class CCLaneTest : public ::testing::Test {
protected:
    void SetUp() override {
        clip = std::make_shared<MIDIClip>("Test Clip");
        manager = std::make_unique<CCLaneManager>(clip);
    }

    std::shared_ptr<MIDIClip> clip;
    std::unique_ptr<CCLaneManager> manager;
};

class StepSequencerTest : public ::testing::Test {
protected:
    void SetUp() override {
        clip = std::make_shared<MIDIClip>("Test Clip");
        sequencer = StepSequencerFactory::create_drum_sequencer(clip);
    }

    std::shared_ptr<MIDIClip> clip;
    std::unique_ptr<StepSequencer> sequencer;
};

// Test 1: MIDIClip Core Functionality
TEST_F(PianoRollTest, MIDIClipBasicOperations) {
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(clip->get_note_count(), 0);
    
    // Test adding notes
    MIDINote note1(60, 100, 0, 1000); // C4, velocity 100, start 0, length 1000 samples
    auto result = clip->add_note(note1);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 1);
    
    MIDINote note2(64, 80, 2000, 1500); // E4, velocity 80, start 2000, length 1500 samples
    result = clip->add_note(note2);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 2);
    
    // Test note access
    const auto& notes = clip->get_notes();
    EXPECT_EQ(notes.size(), 2);
    EXPECT_EQ(notes[0].note_number, 60);
    EXPECT_EQ(notes[1].note_number, 64);
    
    // Test removing notes
    result = clip->remove_note(0);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 1);
    EXPECT_EQ(clip->get_notes()[0].note_number, 64);
}

// Test 2: Piano Roll Note Drawing
TEST_F(PianoRollTest, NoteDrawingOperations) {
    ASSERT_NE(editor, nullptr);
    
    // Test drawing a note
    auto result = editor->draw_note_at_position(1.0, 60, 1.0, 100); // C4 at beat 1
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 1);
    
    // Test drawing multiple notes
    result = editor->draw_note_at_position(2.0, 64, 0.5, 90); // E4 at beat 2
    EXPECT_TRUE(result.is_success());
    result = editor->draw_note_at_position(3.0, 67, 0.25, 80); // G4 at beat 3
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 3);
    
    // Test note finding
    auto found_note = editor->find_note_at_position(1.0, 60);
    EXPECT_NE(found_note, nullptr);
    EXPECT_EQ(found_note->note_number, 60);
    EXPECT_EQ(found_note->velocity, 100);
    
    // Test erasing notes
    result = editor->erase_note_at_position(2.0, 64);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 2);
}

// Test 3: Note Selection and Editing
TEST_F(PianoRollTest, NoteSelectionAndEditing) {
    // Add some test notes
    editor->draw_note_at_position(1.0, 60, 1.0, 100);
    editor->draw_note_at_position(2.0, 62, 1.0, 90);
    editor->draw_note_at_position(3.0, 64, 1.0, 80);
    EXPECT_EQ(clip->get_note_count(), 3);
    
    // Test single note selection
    auto result = editor->select_note_at_position(1.0, 60);
    EXPECT_TRUE(result.is_success());
    
    auto selected = editor->get_clip()->get_selected_notes();
    EXPECT_EQ(selected.size(), 1);
    EXPECT_EQ(selected[0]->note_number, 60);
    
    // Test region selection
    result = editor->select_notes_in_region(1.5, 3.5, 60, 70, false);
    EXPECT_TRUE(result.is_success());
    
    selected = clip->get_selected_notes();
    EXPECT_EQ(selected.size(), 2); // Notes at beats 2 and 3
    
    // Test moving selected notes
    result = editor->transpose_selected_notes(2); // Move up 2 semitones
    EXPECT_TRUE(result.is_success());
    
    // Check that notes were transposed
    const auto& notes = clip->get_notes();
    bool found_transposed = false;
    for (const auto& note : notes) {
        if (note.selected && (note.note_number == 64 || note.note_number == 66)) {
            found_transposed = true;
        }
    }
    EXPECT_TRUE(found_transposed);
}

// Test 4: Note Trimming and Splitting
TEST_F(PianoRollTest, NoteTrimming) {
    // Add a test note
    editor->draw_note_at_position(1.0, 60, 2.0, 100); // 2-beat note
    EXPECT_EQ(clip->get_note_count(), 1);
    
    auto note = editor->find_note_at_position(1.0, 60);
    ASSERT_NE(note, nullptr);
    
    // Test trimming note start
    auto result = editor->trim_note_start(note, 1.5);
    EXPECT_TRUE(result.is_success());
    
    // Test trimming note end
    result = editor->trim_note_end(note, 2.5);
    EXPECT_TRUE(result.is_success());
    
    // Test splitting note
    result = editor->split_note_at_time(note, 2.0);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 2); // Should have 2 notes after split
}

// Test 5: Quantization
TEST_F(PianoRollTest, Quantization) {
    // Add slightly off-time notes
    auto& notes = clip->get_notes_mutable();
    
    // Add note slightly before beat 1
    MIDINote note1(60, 100, MIDIClip::beats_to_samples(0.9), MIDIClip::beats_to_samples(0.5));
    clip->add_note(note1);
    
    // Add note slightly after beat 2
    MIDINote note2(64, 100, MIDIClip::beats_to_samples(2.1), MIDIClip::beats_to_samples(0.5));
    clip->add_note(note2);
    
    EXPECT_EQ(clip->get_note_count(), 2);
    
    // Select all notes and quantize to 16th notes
    clip->select_all_notes();
    auto result = editor->quantize_selected_notes(MIDIClip::QuantizeResolution::SIXTEENTH, 1.0f);
    EXPECT_TRUE(result.is_success());
    
    // Check that notes are now quantized
    const auto& quantized_notes = clip->get_notes();
    for (const auto& note : quantized_notes) {
        double note_time_beats = MIDIClip::samples_to_beats(note.start_time);
        // Should be close to exact beat boundaries (within 0.1 beats)
        double fractional_part = std::fmod(note_time_beats * 4, 1.0); // Check 16th note alignment
        EXPECT_TRUE(fractional_part < 0.1 || fractional_part > 0.9);
    }
}

// Test 6: Velocity Editing
TEST_F(PianoRollTest, VelocityEditing) {
    // Add test notes
    editor->draw_note_at_position(1.0, 60, 1.0, 100);
    editor->draw_note_at_position(2.0, 62, 1.0, 80);
    editor->draw_note_at_position(3.0, 64, 1.0, 60);
    
    // Select all notes
    clip->select_all_notes();
    
    // Test setting absolute velocity
    auto result = editor->set_selected_velocity(90);
    EXPECT_TRUE(result.is_success());
    
    const auto& notes = clip->get_notes();
    for (const auto& note : notes) {
        EXPECT_EQ(note.velocity, 90);
    }
    
    // Test velocity scaling
    result = editor->scale_selected_velocity(1.2f);
    EXPECT_TRUE(result.is_success());
    
    // Test velocity adjustment
    result = editor->adjust_selected_velocity(10);
    EXPECT_TRUE(result.is_success());
}

// Test 7: Copy/Paste Operations
TEST_F(PianoRollTest, CopyPasteOperations) {
    // Add test notes
    editor->draw_note_at_position(1.0, 60, 0.5, 100);
    editor->draw_note_at_position(1.5, 64, 0.5, 90);
    EXPECT_EQ(clip->get_note_count(), 2);
    
    // Select notes
    clip->select_all_notes();
    
    // Copy notes
    auto result = editor->copy_selected_notes();
    EXPECT_TRUE(result.is_success());
    
    // Paste at different time
    result = editor->paste_notes_at_time(3.0);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 4); // Original 2 + pasted 2
    
    // Check that pasted notes are at correct positions
    auto notes_at_3 = editor->find_notes_in_region(3.0, 4.0, 0, 127);
    EXPECT_EQ(notes_at_3.size(), 2);
}

// Test 8: Undo/Redo Functionality
TEST_F(PianoRollTest, UndoRedoOperations) {
    // Initial state: no notes
    EXPECT_EQ(clip->get_note_count(), 0);
    
    // Add a note
    editor->draw_note_at_position(1.0, 60, 1.0, 100);
    EXPECT_EQ(clip->get_note_count(), 1);
    
    // Undo
    auto result = editor->undo_last_operation();
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 0);
    
    // Redo
    result = editor->redo_last_operation();
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 1);
}

// Test 9: CC Lane Basic Operations
TEST_F(CCLaneTest, CCLaneBasicOperations) {
    ASSERT_NE(manager, nullptr);
    
    // Add CC lane
    auto lane_result = manager->add_cc_lane(1, "Mod Wheel");
    EXPECT_TRUE(lane_result.is_success());
    
    auto lane = lane_result.get_value();
    ASSERT_NE(lane, nullptr);
    EXPECT_EQ(lane->get_controller(), 1);
    
    // Draw CC points
    auto result = lane->draw_cc_point(1.0, 64); // Middle value at beat 1
    EXPECT_TRUE(result.is_success());
    
    result = lane->draw_cc_point(2.0, 127); // Max value at beat 2
    EXPECT_TRUE(result.is_success());
    
    EXPECT_EQ(lane->get_cc_event_count(), 2);
    
    // Test CC interpolation
    uint8_t interpolated = lane->get_cc_value_at_time(1.5); // Halfway between
    EXPECT_GT(interpolated, 64);
    EXPECT_LT(interpolated, 127);
}

// Test 10: CC Lane Automation
TEST_F(CCLaneTest, CCLaneAutomation) {
    auto lane_result = manager->add_cc_lane(74, "Cutoff");
    EXPECT_TRUE(lane_result.is_success());
    
    auto lane = lane_result.get_value();
    
    // Create automation ramp
    auto result = lane->draw_cc_ramp(1.0, 4.0, 0, 127);
    EXPECT_TRUE(result.is_success());
    
    EXPECT_GT(lane->get_cc_event_count(), 2); // Should create multiple points
    
    // Create LFO automation
    result = lane->create_lfo_automation(5.0, 8.0, 2.0, 40, 64); // 2Hz LFO
    EXPECT_TRUE(result.is_success());
    
    // Test shape creation
    result = lane->create_automation_shape(9.0, 12.0, "sine");
    EXPECT_TRUE(result.is_success());
}

// Test 11: Step Sequencer Basic Operations
TEST_F(StepSequencerTest, StepSequencerBasicOperations) {
    ASSERT_NE(sequencer, nullptr);
    
    // Test pattern configuration
    sequencer->set_pattern_length(PatternLength::BARS_1);
    sequencer->set_step_resolution(StepResolution::SIXTEENTH);
    
    EXPECT_EQ(sequencer->get_total_steps(), 16); // 16 steps per bar
    EXPECT_EQ(sequencer->get_steps_per_bar(), 16);
    
    // Add drum lane
    auto result = sequencer->add_drum_lane(36, "Kick"); // C2 kick
    EXPECT_TRUE(result.is_success());
    
    // Toggle some steps
    result = sequencer->toggle_step(36, 0); // Step 1 (beat 1)
    EXPECT_TRUE(result.is_success());
    EXPECT_TRUE(sequencer->is_step_active(36, 0));
    
    result = sequencer->toggle_step(36, 4); // Step 5 (beat 2)
    EXPECT_TRUE(result.is_success());
    
    result = sequencer->toggle_step(36, 8); // Step 9 (beat 3)
    EXPECT_TRUE(result.is_success());
    
    result = sequencer->toggle_step(36, 12); // Step 13 (beat 4)
    EXPECT_TRUE(result.is_success());
    
    // Generate MIDI from pattern
    result = sequencer->generate_midi_from_pattern();
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(clip->get_note_count(), 4); // 4 kick hits
}

// Test 12: Step Sequencer Pattern Operations
TEST_F(StepSequencerTest, StepSequencerPatternOperations) {
    // Add kick lane and activate some steps
    sequencer->add_drum_lane(36, "Kick");
    sequencer->set_step(36, 0, true);
    sequencer->set_step(36, 4, true);
    sequencer->set_step(36, 8, true);
    
    // Test lane copying
    auto result = sequencer->copy_lane(36, 38); // Copy kick to snare
    EXPECT_TRUE(result.is_success());
    
    EXPECT_TRUE(sequencer->is_step_active(38, 0));
    EXPECT_TRUE(sequencer->is_step_active(38, 4));
    EXPECT_TRUE(sequencer->is_step_active(38, 8));
    
    // Test lane shifting
    result = sequencer->shift_lane(38, 2); // Shift snare by 2 steps
    EXPECT_TRUE(result.is_success());
    
    EXPECT_TRUE(sequencer->is_step_active(38, 2));
    EXPECT_TRUE(sequencer->is_step_active(38, 6));
    EXPECT_TRUE(sequencer->is_step_active(38, 10));
    
    // Test lane reversal
    result = sequencer->reverse_lane(36);
    EXPECT_TRUE(result.is_success());
    
    // Test clearing lane
    result = sequencer->clear_lane(38);
    EXPECT_TRUE(result.is_success());
    
    for (size_t i = 0; i < sequencer->get_total_steps(); ++i) {
        EXPECT_FALSE(sequencer->is_step_active(38, i));
    }
}

// Test 13: Step Sequencer Swing and Humanization
TEST_F(StepSequencerTest, StepSequencerGroove) {
    // Add kick pattern
    sequencer->add_drum_lane(36, "Kick");
    for (size_t i = 0; i < 16; i += 2) { // Every other step
        sequencer->set_step(36, i, true);
    }
    
    // Test swing
    sequencer->set_swing(0.3f);
    EXPECT_FLOAT_EQ(sequencer->get_swing(), 0.3f);
    
    // Test humanization
    sequencer->set_humanize_velocity(0.2f);
    sequencer->set_humanize_timing(0.1f);
    
    EXPECT_FLOAT_EQ(sequencer->get_humanize_velocity(), 0.2f);
    EXPECT_FLOAT_EQ(sequencer->get_humanize_timing(), 0.1f);
    
    // Generate MIDI with groove applied
    auto result = sequencer->generate_midi_from_pattern();
    EXPECT_TRUE(result.is_success());
    EXPECT_GT(clip->get_note_count(), 0);
}

// Test 14: Step Input Mode
TEST_F(StepSequencerTest, StepInputMode) {
    sequencer->set_step_input_active(true);
    EXPECT_TRUE(sequencer->is_step_input_active());
    
    // Test input at current step
    auto result = sequencer->input_note_at_current_step(36, 100);
    EXPECT_TRUE(result.is_success());
    EXPECT_TRUE(sequencer->is_step_active(36, 0)); // Should activate step 0
    
    // Advance step and input another note
    sequencer->advance_step();
    EXPECT_EQ(sequencer->get_current_step(), 1);
    
    result = sequencer->input_note_at_current_step(38, 90);
    EXPECT_TRUE(result.is_success());
    EXPECT_TRUE(sequencer->is_step_active(38, 1)); // Should activate step 1
}

// Test 15: Performance Test
TEST_F(PianoRollTest, PerformanceTest) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Add many notes quickly
    for (int i = 0; i < 1000; ++i) {
        double time_beats = i * 0.25; // 16th note intervals
        uint8_t note = 60 + (i % 12); // Cycle through octave
        uint8_t velocity = 80 + (i % 48); // Vary velocity
        
        editor->draw_note_at_position(time_beats, note, 0.2, velocity);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(clip->get_note_count(), 1000);
    EXPECT_LT(duration.count(), 1000); // Should complete in less than 1 second
    
    std::cout << "Added 1000 notes in " << duration.count() << "ms" << std::endl;
    
    // Test selection performance
    start = std::chrono::high_resolution_clock::now();
    
    clip->select_all_notes();
    auto selected = clip->get_selected_notes();
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    EXPECT_EQ(selected.size(), 1000);
    EXPECT_LT(duration.count(), 10000); // Should complete in less than 10ms
    
    std::cout << "Selected 1000 notes in " << duration.count() << "μs" << std::endl;
}