#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/ai/ActionReducer.h"

using namespace mixmind::ai;

TEST_CASE("ActionCommand validation and serialization", "[ai][action]") {
    SECTION("Valid action commands pass validation") {
        ActionCommand addTrack;
        addTrack.type = ActionType::ADD_TRACK;
        addTrack.params.push_back(std::string("Lead Guitar"));
        
        REQUIRE(addTrack.validate());
        REQUIRE(addTrack.toString().find("ADD_TRACK") != std::string::npos);
        REQUIRE(addTrack.toString().find("Lead Guitar") != std::string::npos);
    }
    
    SECTION("Invalid action commands fail validation") {
        ActionCommand invalidTrack;
        invalidTrack.type = ActionType::SET_TRACK_VOLUME;
        invalidTrack.trackId = "";  // Missing trackId
        
        REQUIRE_FALSE(invalidTrack.validate());
    }
    
    SECTION("Parameter access with type checking") {
        ActionCommand cmd;
        cmd.params.push_back(42);
        cmd.params.push_back(std::string("test"));
        cmd.params.push_back(3.14f);
        
        // Valid type access
        auto intResult = cmd.getParam<int32_t>(0);
        REQUIRE(intResult.has_value());
        REQUIRE(intResult.value() == 42);
        
        auto stringResult = cmd.getParam<std::string>(1);
        REQUIRE(stringResult.has_value());
        REQUIRE(stringResult.value() == "test");
        
        // Type mismatch
        auto wrongTypeResult = cmd.getParam<float>(0); // int vs float
        REQUIRE_FALSE(wrongTypeResult.has_value());
        
        // Out of bounds
        auto outOfBoundsResult = cmd.getParam<int32_t>(10);
        REQUIRE_FALSE(outOfBoundsResult.has_value());
    }
}

TEST_CASE("ProjectState management", "[ai][state]") {
    SECTION("Default project state is valid") {
        ProjectState state;
        state.tempo = 120.0;
        state.timeSignature = {4, 4};
        state.keySignature = "C";
        
        REQUIRE(state.validate());
    }
    
    SECTION("Invalid project states fail validation") {
        ProjectState invalidState;
        invalidState.tempo = -10.0;  // Invalid tempo
        
        REQUIRE_FALSE(invalidState.validate());
    }
    
    SECTION("Project state copying increments version") {
        ProjectState original;
        original.tempo = 120.0;
        original.version = 5;
        
        ProjectState copy = original.copy();
        REQUIRE(copy.version == 6);
        REQUIRE(copy.tempo == original.tempo);
        REQUIRE_FALSE(copy.lastModified.empty());
    }
    
    SECTION("Track validation") {
        ProjectState state;
        state.tempo = 120.0;
        
        // Valid track
        ProjectState::Track validTrack;
        validTrack.id = "track_1";
        validTrack.name = "Guitar";
        validTrack.volume = 0.8f;
        validTrack.pan = 0.2f;
        
        state.tracks.push_back(validTrack);
        REQUIRE(state.validate());
        
        // Invalid track - volume out of range
        ProjectState::Track invalidTrack;
        invalidTrack.id = "track_2";
        invalidTrack.name = "Bass";
        invalidTrack.volume = 3.0f;  // Too high
        
        state.tracks.push_back(invalidTrack);
        REQUIRE_FALSE(state.validate());
    }
    
    SECTION("MIDI note validation") {
        ProjectState state;
        state.tempo = 120.0;
        
        // Add a track first
        ProjectState::Track track;
        track.id = "track_1";
        track.name = "Piano";
        state.tracks.push_back(track);
        
        // Valid MIDI note
        ProjectState::MIDINote validNote;
        validNote.pitch = 60;  // Middle C
        validNote.velocity = 0.8f;
        validNote.startTime_ms = 1000;
        validNote.duration_ms = 500;
        validNote.trackId = "track_1";
        
        state.midiNotes.push_back(validNote);
        REQUIRE(state.validate());
        
        // Invalid MIDI note - pitch out of range
        ProjectState::MIDINote invalidNote;
        invalidNote.pitch = 128;  // Out of MIDI range
        invalidNote.velocity = 0.8f;
        invalidNote.startTime_ms = 2000;
        invalidNote.duration_ms = 500;
        invalidNote.trackId = "track_1";
        
        state.midiNotes.push_back(invalidNote);
        REQUIRE_FALSE(state.validate());
    }
}

TEST_CASE("ActionReducer basic operations", "[ai][reducer]") {
    SECTION("Add track action") {
        ProjectState initialState;
        initialState.tempo = 120.0;
        
        ActionCommand addTrack;
        addTrack.type = ActionType::ADD_TRACK;
        addTrack.params.push_back(std::string("New Track"));
        
        ActionResult result = ActionReducer::reduce(initialState, addTrack);
        
        REQUIRE(result.success);
        REQUIRE(result.newState.tracks.size() == 1);
        REQUIRE(result.newState.tracks[0].name == "New Track");
        REQUIRE_FALSE(result.newState.tracks[0].id.empty());
        
        // Check reverse command
        REQUIRE(result.reverseCommand.type == ActionType::REMOVE_TRACK);
        REQUIRE(result.reverseCommand.trackId == result.newState.tracks[0].id);
    }
    
    SECTION("Set track volume action") {
        ProjectState state;
        state.tempo = 120.0;
        
        // Add a track first
        ProjectState::Track track;
        track.id = "test_track";
        track.name = "Test Track";
        track.volume = 1.0f;
        state.tracks.push_back(track);
        
        ActionCommand setVolume;
        setVolume.type = ActionType::SET_TRACK_VOLUME;
        setVolume.trackId = "test_track";
        setVolume.params.push_back(0.5f);
        
        ActionResult result = ActionReducer::reduce(state, setVolume);
        
        REQUIRE(result.success);
        REQUIRE(result.newState.tracks[0].volume == Catch::Approx(0.5f));
        
        // Check reverse command
        REQUIRE(result.reverseCommand.type == ActionType::SET_TRACK_VOLUME);
        REQUIRE(result.reverseCommand.trackId == "test_track");
        auto oldVolumeResult = result.reverseCommand.getParam<float>(0);
        REQUIRE(oldVolumeResult.has_value());
        REQUIRE(oldVolumeResult.value() == Catch::Approx(1.0f));
    }
    
    SECTION("Set tempo action") {
        ProjectState state;
        state.tempo = 120.0;
        
        ActionCommand setTempo;
        setTempo.type = ActionType::SET_TEMPO;
        setTempo.params.push_back(140.0);
        
        ActionResult result = ActionReducer::reduce(state, setTempo);
        
        REQUIRE(result.success);
        REQUIRE(result.newState.tempo == Catch::Approx(140.0));
        
        // Check reverse command
        REQUIRE(result.reverseCommand.type == ActionType::SET_TEMPO);
        auto oldTempoResult = result.reverseCommand.getParam<double>(0);
        REQUIRE(oldTempoResult.has_value());
        REQUIRE(oldTempoResult.value() == Catch::Approx(120.0));
    }
    
    SECTION("Add MIDI note action") {
        ProjectState state;
        state.tempo = 120.0;
        
        // Add a track first
        ProjectState::Track track;
        track.id = "midi_track";
        track.name = "MIDI Track";
        state.tracks.push_back(track);
        
        ActionCommand addNote;
        addNote.type = ActionType::ADD_MIDI_NOTE;
        addNote.trackId = "midi_track";
        addNote.params.push_back(int32_t(60));      // pitch
        addNote.params.push_back(0.8f);             // velocity
        addNote.params.push_back(uint64_t(1000));   // start time
        addNote.params.push_back(uint64_t(500));    // duration
        
        ActionResult result = ActionReducer::reduce(state, addNote);
        
        REQUIRE(result.success);
        REQUIRE(result.newState.midiNotes.size() == 1);
        REQUIRE(result.newState.midiNotes[0].pitch == 60);
        REQUIRE(result.newState.midiNotes[0].velocity == Catch::Approx(0.8f));
        REQUIRE(result.newState.midiNotes[0].trackId == "midi_track");
    }
}

TEST_CASE("ActionReducer validation", "[ai][reducer][validation]") {
    SECTION("Action validation catches invalid parameters") {
        ProjectState state;
        state.tempo = 120.0;
        
        ActionCommand invalidTempo;
        invalidTempo.type = ActionType::SET_TEMPO;
        invalidTempo.params.push_back(-50.0);  // Invalid negative tempo
        
        auto validation = ActionReducer::validateAction(state, invalidTempo);
        REQUIRE_FALSE(validation.has_value());
    }
    
    SECTION("Action validation catches missing tracks") {
        ProjectState state;
        state.tempo = 120.0;
        
        ActionCommand volumeForNonexistentTrack;
        volumeForNonexistentTrack.type = ActionType::SET_TRACK_VOLUME;
        volumeForNonexistentTrack.trackId = "nonexistent";
        volumeForNonexistentTrack.params.push_back(0.8f);
        
        auto validation = ActionReducer::validateAction(state, volumeForNonexistentTrack);
        REQUIRE_FALSE(validation.has_value());
    }
    
    SECTION("Validation allows valid range values") {
        ProjectState state;
        state.tempo = 120.0;
        
        ProjectState::Track track;
        track.id = "test_track";
        track.name = "Test";
        state.tracks.push_back(track);
        
        // Test volume range
        ActionCommand validVolume;
        validVolume.type = ActionType::SET_TRACK_VOLUME;
        validVolume.trackId = "test_track";
        validVolume.params.push_back(1.5f);  // Valid boost
        
        auto validation = ActionReducer::validateAction(state, validVolume);
        REQUIRE(validation.has_value());
        
        // Test pan range
        ActionCommand validPan;
        validPan.type = ActionType::SET_TRACK_PAN;
        validPan.trackId = "test_track";
        validPan.params.push_back(-0.5f);  // Valid left pan
        
        validation = ActionReducer::validateAction(state, validPan);
        REQUIRE(validation.has_value());
    }
}

TEST_CASE("ActionReducer batch operations", "[ai][reducer][batch]") {
    SECTION("Successful batch execution") {
        ProjectState state;
        state.tempo = 120.0;
        
        std::vector<ActionCommand> batch;
        
        // Add multiple tracks
        for (int i = 0; i < 3; ++i) {
            ActionCommand addTrack;
            addTrack.type = ActionType::ADD_TRACK;
            addTrack.params.push_back(std::string("Track ") + std::to_string(i + 1));
            batch.push_back(addTrack);
        }
        
        // Set tempo
        ActionCommand setTempo;
        setTempo.type = ActionType::SET_TEMPO;
        setTempo.params.push_back(130.0);
        batch.push_back(setTempo);
        
        ActionResult result = ActionReducer::reduceBatch(state, batch);
        
        REQUIRE(result.success);
        REQUIRE(result.newState.tracks.size() == 3);
        REQUIRE(result.newState.tempo == Catch::Approx(130.0));
    }
    
    SECTION("Batch fails if any action fails") {
        ProjectState state;
        state.tempo = 120.0;
        
        std::vector<ActionCommand> batch;
        
        // Valid action
        ActionCommand validAction;
        validAction.type = ActionType::SET_TEMPO;
        validAction.params.push_back(140.0);
        batch.push_back(validAction);
        
        // Invalid action
        ActionCommand invalidAction;
        invalidAction.type = ActionType::SET_TRACK_VOLUME;
        invalidAction.trackId = "nonexistent";
        invalidAction.params.push_back(0.8f);
        batch.push_back(invalidAction);
        
        ActionResult result = ActionReducer::reduceBatch(state, batch);
        
        REQUIRE_FALSE(result.success);
        REQUIRE(result.newState.tempo == Catch::Approx(120.0)); // Original state unchanged
    }
}

TEST_CASE("ActionHistory functionality", "[ai][history]") {
    SECTION("Recording and retrieving history") {
        ActionHistory history;
        
        ProjectState state1;
        state1.tempo = 120.0;
        state1.version = 1;
        
        ProjectState state2;
        state2.tempo = 140.0;
        state2.version = 2;
        
        ActionCommand action;
        action.type = ActionType::SET_TEMPO;
        action.params.push_back(140.0);
        
        history.recordAction(action, state2);
        
        REQUIRE(history.getHistorySize() == 1);
        REQUIRE(history.canUndo());
        REQUIRE_FALSE(history.canRedo());
    }
    
    SECTION("Undo and redo operations") {
        ActionHistory history;
        
        ProjectState initialState;
        initialState.tempo = 120.0;
        initialState.version = 1;
        
        ProjectState modifiedState;
        modifiedState.tempo = 140.0;
        modifiedState.version = 2;
        
        ActionCommand action;
        action.type = ActionType::SET_TEMPO;
        action.params.push_back(140.0);
        
        // Record initial state (simulating pipeline behavior)
        ActionCommand initialAction;
        history.recordAction(initialAction, initialState);
        
        // Record the actual action
        history.recordAction(action, modifiedState);
        
        REQUIRE(history.canUndo());
        
        // Undo should give us the initial state
        auto undoResult = history.undo();
        REQUIRE(undoResult.has_value());
        REQUIRE(undoResult.value().tempo == Catch::Approx(120.0));
        
        REQUIRE(history.canRedo());
        
        // Redo should give us the modified state
        auto redoResult = history.redo();
        REQUIRE(redoResult.has_value());
        REQUIRE(redoResult.value().tempo == Catch::Approx(140.0));
    }
    
    SECTION("History descriptions") {
        ActionHistory history;
        
        ProjectState state;
        state.version = 1;
        
        ActionCommand addTrackAction;
        addTrackAction.type = ActionType::ADD_TRACK;
        addTrackAction.params.push_back(std::string("New Track"));
        
        // Record initial state first
        ActionCommand initialAction;
        history.recordAction(initialAction, state);
        
        // Record the actual action
        history.recordAction(addTrackAction, state);
        
        std::string undoDesc = history.getUndoDescription();
        REQUIRE(undoDesc.find("Add Track") != std::string::npos);
    }
}

TEST_CASE("ActionPipeline integration", "[ai][pipeline]") {
    SECTION("Basic pipeline execution") {
        ActionPipeline pipeline;
        
        ActionCommand addTrack;
        addTrack.type = ActionType::ADD_TRACK;
        addTrack.params.push_back(std::string("Test Track"));
        
        auto result = pipeline.executeAction(addTrack);
        
        REQUIRE(result.has_value());
        REQUIRE(result.value().success);
        REQUIRE(pipeline.getCurrentState().tracks.size() == 1);
        REQUIRE(pipeline.canUndo());
    }
    
    SECTION("Pipeline undo/redo") {
        ActionPipeline pipeline;
        
        // Execute an action
        ActionCommand setTempo;
        setTempo.type = ActionType::SET_TEMPO;
        setTempo.params.push_back(150.0);
        
        auto result = pipeline.executeAction(setTempo);
        REQUIRE(result.has_value());
        REQUIRE(result.value().success);
        REQUIRE(pipeline.getCurrentState().tempo == Catch::Approx(150.0));
        
        // Undo
        auto undoResult = pipeline.undo();
        REQUIRE(undoResult.has_value());
        REQUIRE(undoResult.value().tempo == Catch::Approx(120.0)); // Default tempo
        
        // Redo
        auto redoResult = pipeline.redo();
        REQUIRE(redoResult.has_value());
        REQUIRE(redoResult.value().tempo == Catch::Approx(150.0));
    }
    
    SECTION("Pipeline validation can be disabled") {
        ActionPipeline pipeline;
        pipeline.enableValidation(false);
        
        // This would normally fail validation
        ActionCommand invalidAction;
        invalidAction.type = ActionType::SET_TRACK_VOLUME;
        invalidAction.trackId = "nonexistent";
        invalidAction.params.push_back(0.8f);
        
        auto result = pipeline.executeAction(invalidAction);
        // With validation disabled, the reducer might still catch this,
        // but the pipeline won't pre-validate
        REQUIRE(result.has_value());
    }
    
    SECTION("Pipeline statistics tracking") {
        ActionPipeline pipeline;
        
        ActionCommand validAction;
        validAction.type = ActionType::SET_TEMPO;
        validAction.params.push_back(130.0);
        
        ActionCommand invalidAction;
        invalidAction.type = ActionType::SET_TRACK_VOLUME;
        invalidAction.trackId = "nonexistent";
        invalidAction.params.push_back(0.8f);
        
        pipeline.executeAction(validAction);
        pipeline.executeAction(invalidAction);
        
        auto stats = pipeline.getStats();
        REQUIRE(stats.totalActionsExecuted == 2);
        REQUIRE(stats.successfulActions == 1);
        REQUIRE(stats.failedActions == 1);
        REQUIRE(stats.averageExecutionTime_ms >= 0.0);
    }
    
    SECTION("Pipeline serialization") {
        ActionPipeline pipeline;
        
        ActionCommand addTrack;
        addTrack.type = ActionType::ADD_TRACK;
        addTrack.params.push_back(std::string("Serialization Test"));
        
        pipeline.executeAction(addTrack);
        
        std::string serialized = pipeline.serialize();
        
        REQUIRE_FALSE(serialized.empty());
        REQUIRE(serialized.find("currentState") != std::string::npos);
        REQUIRE(serialized.find("stats") != std::string::npos);
        REQUIRE(serialized.find("totalActionsExecuted") != std::string::npos);
    }
}