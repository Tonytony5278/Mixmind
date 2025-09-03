#include <iostream>
#include <string>
#include "../src/ai/ActionReducer.h"

using namespace mixmind::ai;

// Demonstration of the deterministic AI action pipeline
void demonstrateBasicActions() {
    std::cout << "=== MixMind AI Action Pipeline Demo ===\n\n";
    
    // Create an action pipeline
    ActionPipeline pipeline;
    
    std::cout << "Initial project state:\n";
    std::cout << "- Tracks: " << pipeline.getCurrentState().tracks.size() << "\n";
    std::cout << "- Tempo: " << pipeline.getCurrentState().tempo << " BPM\n";
    std::cout << "- Time Signature: " << pipeline.getCurrentState().timeSignature.first 
              << "/" << pipeline.getCurrentState().timeSignature.second << "\n\n";
    
    // Demonstrate adding tracks
    std::cout << "Step 1: Adding tracks...\n";
    
    std::vector<std::string> trackNames = {"Lead Guitar", "Bass", "Drums", "Piano"};
    
    for (const auto& name : trackNames) {
        ActionCommand addTrack;
        addTrack.type = ActionType::ADD_TRACK;
        addTrack.params.push_back(name);
        
        auto result = pipeline.executeAction(addTrack);
        if (result.has_value() && result.value().success) {
            std::cout << "✓ Added track: " << name << "\n";
        } else {
            std::cout << "✗ Failed to add track: " << name << "\n";
        }
    }
    
    std::cout << "Current tracks: " << pipeline.getCurrentState().tracks.size() << "\n\n";
    
    // Demonstrate setting track properties
    std::cout << "Step 2: Setting track properties...\n";
    
    const auto& tracks = pipeline.getCurrentState().tracks;
    if (!tracks.empty()) {
        // Set volume for first track
        ActionCommand setVolume;
        setVolume.type = ActionType::SET_TRACK_VOLUME;
        setVolume.trackId = tracks[0].id;
        setVolume.params.push_back(0.8f);
        
        auto result = pipeline.executeAction(setVolume);
        if (result.has_value() && result.value().success) {
            std::cout << "✓ Set volume for " << tracks[0].name << " to 0.8\n";
        }
        
        // Set pan for second track
        if (tracks.size() > 1) {
            ActionCommand setPan;
            setPan.type = ActionType::SET_TRACK_PAN;
            setPan.trackId = tracks[1].id;
            setPan.params.push_back(-0.3f); // Pan left
            
            result = pipeline.executeAction(setPan);
            if (result.has_value() && result.value().success) {
                std::cout << "✓ Set pan for " << tracks[1].name << " to -0.3 (left)\n";
            }
        }
        
        // Mute third track
        if (tracks.size() > 2) {
            ActionCommand muteTrack;
            muteTrack.type = ActionType::MUTE_TRACK;
            muteTrack.trackId = tracks[2].id;
            
            result = pipeline.executeAction(muteTrack);
            if (result.has_value() && result.value().success) {
                std::cout << "✓ Muted track: " << tracks[2].name << "\n";
            }
        }
    }
    
    std::cout << "\nStep 3: Adding MIDI notes...\n";
    
    // Add MIDI notes to first track
    if (!tracks.empty()) {
        std::vector<std::pair<int, std::string>> notes = {
            {60, "C4"}, {64, "E4"}, {67, "G4"}, {72, "C5"}
        };
        
        uint64_t currentTime = 0;
        for (const auto& [pitch, noteName] : notes) {
            ActionCommand addNote;
            addNote.type = ActionType::ADD_MIDI_NOTE;
            addNote.trackId = tracks[0].id;
            addNote.params.push_back(int32_t(pitch));
            addNote.params.push_back(0.8f);                    // velocity
            addNote.params.push_back(currentTime);             // start time
            addNote.params.push_back(uint64_t(500));           // duration (500ms)
            
            auto result = pipeline.executeAction(addNote);
            if (result.has_value() && result.value().success) {
                std::cout << "♪ Added note " << noteName << " (pitch " << pitch << ") at " 
                         << currentTime << "ms\n";
            }
            
            currentTime += 600; // 600ms between notes
        }
    }
    
    std::cout << "Current MIDI notes: " << pipeline.getCurrentState().midiNotes.size() << "\n\n";
    
    // Demonstrate project-level changes
    std::cout << "Step 4: Setting project properties...\n";
    
    ActionCommand setTempo;
    setTempo.type = ActionType::SET_TEMPO;
    setTempo.params.push_back(140.0);
    
    auto result = pipeline.executeAction(setTempo);
    if (result.has_value() && result.value().success) {
        std::cout << "♫ Set tempo to 140 BPM\n";
    }
    
    ActionCommand setTimeSignature;
    setTimeSignature.type = ActionType::SET_TIME_SIGNATURE;
    setTimeSignature.params.push_back(6);
    setTimeSignature.params.push_back(8);
    
    result = pipeline.executeAction(setTimeSignature);
    if (result.has_value() && result.value().success) {
        std::cout << "♫ Set time signature to 6/8\n";
    }
    
    std::cout << "\nStep 5: Demonstrating undo/redo...\n";
    
    std::cout << "Current state: " << pipeline.getCurrentState().tracks.size() << " tracks, " 
              << pipeline.getCurrentState().tempo << " BPM\n";
    
    if (pipeline.canUndo()) {
        std::cout << "Performing undo...\n";
        auto undoResult = pipeline.undo();
        if (undoResult.has_value()) {
            std::cout << "After undo: " << undoResult.value().tracks.size() << " tracks, " 
                     << undoResult.value().tempo << " BPM\n";
        }
    }
    
    if (pipeline.canRedo()) {
        std::cout << "Performing redo...\n";
        auto redoResult = pipeline.redo();
        if (redoResult.has_value()) {
            std::cout << "After redo: " << redoResult.value().tracks.size() << " tracks, " 
                     << redoResult.value().tempo << " BPM\n";
        }
    }
    
    // Show final statistics
    std::cout << "\n=== Pipeline Statistics ===\n";
    auto stats = pipeline.getStats();
    std::cout << "Total actions executed: " << stats.totalActionsExecuted << "\n";
    std::cout << "Successful actions: " << stats.successfulActions << "\n";
    std::cout << "Failed actions: " << stats.failedActions << "\n";
    std::cout << "Undo operations: " << stats.undoOperations << "\n";
    std::cout << "Redo operations: " << stats.redoOperations << "\n";
    std::cout << "Average execution time: " << stats.averageExecutionTime_ms << " ms\n";
    
    // Show final project state
    std::cout << "\n=== Final Project State ===\n";
    const auto& finalState = pipeline.getCurrentState();
    std::cout << "Project version: " << finalState.version << "\n";
    std::cout << "Tempo: " << finalState.tempo << " BPM\n";
    std::cout << "Time signature: " << finalState.timeSignature.first << "/" 
              << finalState.timeSignature.second << "\n";
    std::cout << "Key signature: " << finalState.keySignature << "\n";
    std::cout << "Tracks: " << finalState.tracks.size() << "\n";
    
    for (size_t i = 0; i < finalState.tracks.size(); ++i) {
        const auto& track = finalState.tracks[i];
        std::cout << "  Track " << (i+1) << ": " << track.name 
                 << " (vol=" << track.volume 
                 << ", pan=" << track.pan
                 << ", muted=" << (track.muted ? "yes" : "no") 
                 << ", soloed=" << (track.soloed ? "yes" : "no") << ")\n";
    }
    
    std::cout << "MIDI notes: " << finalState.midiNotes.size() << "\n";
    for (size_t i = 0; i < finalState.midiNotes.size(); ++i) {
        const auto& note = finalState.midiNotes[i];
        std::cout << "  Note " << (i+1) << ": pitch=" << note.pitch 
                 << ", vel=" << note.velocity 
                 << ", start=" << note.startTime_ms << "ms"
                 << ", dur=" << note.duration_ms << "ms\n";
    }
    
    std::cout << "\n=== Serialization Demo ===\n";
    std::string serialized = pipeline.serialize();
    std::cout << "Serialized project size: " << serialized.length() << " characters\n";
    std::cout << "First 200 characters:\n" << serialized.substr(0, 200) << "...\n";
}

void demonstrateBatchOperations() {
    std::cout << "\n=== Batch Operations Demo ===\n";
    
    ActionPipeline pipeline;
    
    // Create a batch of related actions
    std::vector<ActionCommand> batch;
    
    // Add multiple tracks at once
    std::vector<std::string> trackNames = {"Drums", "Bass", "Guitar", "Keys", "Vocals"};
    
    for (const auto& name : trackNames) {
        ActionCommand addTrack;
        addTrack.type = ActionType::ADD_TRACK;
        addTrack.params.push_back(name);
        batch.push_back(addTrack);
    }
    
    // Set initial project properties
    ActionCommand setTempo;
    setTempo.type = ActionType::SET_TEMPO;
    setTempo.params.push_back(128.0);
    batch.push_back(setTempo);
    
    std::cout << "Executing batch with " << batch.size() << " actions...\n";
    
    auto result = pipeline.executeBatch(batch);
    if (result.has_value() && result.value().success) {
        std::cout << "✓ Batch executed successfully!\n";
        std::cout << "Result: " << pipeline.getCurrentState().tracks.size() 
                 << " tracks, tempo = " << pipeline.getCurrentState().tempo << " BPM\n";
    } else {
        std::cout << "✗ Batch execution failed\n";
    }
    
    // Demonstrate batch failure with invalid action
    std::cout << "\nTesting batch with invalid action...\n";
    
    std::vector<ActionCommand> failingBatch;
    
    // Valid action
    ActionCommand validAction;
    validAction.type = ActionType::SET_TEMPO;
    validAction.params.push_back(130.0);
    failingBatch.push_back(validAction);
    
    // Invalid action (trying to modify non-existent track)
    ActionCommand invalidAction;
    invalidAction.type = ActionType::SET_TRACK_VOLUME;
    invalidAction.trackId = "nonexistent_track_id";
    invalidAction.params.push_back(0.5f);
    failingBatch.push_back(invalidAction);
    
    auto failResult = pipeline.executeBatch(failingBatch);
    if (!failResult.has_value() || !failResult.value().success) {
        std::cout << "✓ Batch correctly failed due to invalid action\n";
        std::cout << "Project state unchanged: tempo = " << pipeline.getCurrentState().tempo << " BPM\n";
    }
}

void demonstrateValidation() {
    std::cout << "\n=== Validation Demo ===\n";
    
    ActionPipeline pipeline;
    
    // Test various invalid actions
    std::cout << "Testing parameter validation...\n";
    
    // Invalid tempo
    ActionCommand invalidTempo;
    invalidTempo.type = ActionType::SET_TEMPO;
    invalidTempo.params.push_back(-50.0);
    
    auto result = pipeline.executeAction(invalidTempo);
    if (!result.has_value() || !result.value().success) {
        std::cout << "✓ Correctly rejected negative tempo\n";
    }
    
    // Invalid volume
    ActionCommand invalidVolume;
    invalidVolume.type = ActionType::SET_TRACK_VOLUME;
    invalidVolume.trackId = "some_track";
    invalidVolume.params.push_back(3.0f); // Too high
    
    result = pipeline.executeAction(invalidVolume);
    if (!result.has_value() || !result.value().success) {
        std::cout << "✓ Correctly rejected excessive volume\n";
    }
    
    // Test validation disable
    std::cout << "\nTesting with validation disabled...\n";
    pipeline.enableValidation(false);
    
    result = pipeline.executeAction(invalidTempo);
    if (result.has_value()) {
        std::cout << "Action processed with validation disabled (may still fail in reducer)\n";
    }
    
    pipeline.enableValidation(true);
    std::cout << "Validation re-enabled\n";
}

int main() {
    try {
        demonstrateBasicActions();
        demonstrateBatchOperations();
        demonstrateValidation();
        
        std::cout << "\n=== Demo Complete ===\n";
        std::cout << "The AI Action Pipeline provides:\n";
        std::cout << "• Deterministic, pure functional operations\n";
        std::cout << "• Complete undo/redo support\n";
        std::cout << "• Batch transaction semantics\n";
        std::cout << "• Comprehensive validation\n";
        std::cout << "• Performance monitoring\n";
        std::cout << "• State serialization\n";
        std::cout << "\nThis forms the foundation for AI-driven DAW automation.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Demo error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}