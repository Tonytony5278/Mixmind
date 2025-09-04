#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/audio/rapid/RapidAudioEngine.h"
#include "../src/ai/rapid/RapidCommandProcessor.h"

using namespace mixmind::rapid;

TEST_CASE("Rapid Audio Engine", "[rapid][audio]") {
    SECTION("Audio buffer operations") {
        AudioBuffer buffer(1024, 2);
        
        REQUIRE(buffer.getNumSamples() == 1024);
        REQUIRE(buffer.getNumChannels() == 2);
        
        // Test buffer writing
        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getWritePointer(1);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            leftChannel[i] = 0.5f;
            rightChannel[i] = -0.5f;
        }
        
        REQUIRE(buffer.getPeakLevel() == Catch::Approx(0.5f));
        REQUIRE(buffer.getRMSLevel() == Catch::Approx(0.5f).margin(0.01f));
    }
    
    SECTION("Audio engine initialization") {
        RapidAudioEngine engine;
        
        REQUIRE(engine.initialize(44100, 512));
        REQUIRE(engine.start());
        REQUIRE(engine.stop());
    }
    
    SECTION("Audio effect processing") {
        AudioBuffer buffer(512, 2);
        generateTestTone(buffer, 440.0f, 1.0f);
        
        float originalPeak = buffer.getPeakLevel();
        REQUIRE(originalPeak == Catch::Approx(1.0f).margin(0.1f));
        
        GainEffect gainEffect;
        gainEffect.setParameter("gain", 0.5f);
        gainEffect.process(buffer);
        
        float processedPeak = buffer.getPeakLevel();
        REQUIRE(processedPeak == Catch::Approx(0.5f).margin(0.1f));
    }
}

TEST_CASE("Rapid Natural Language Processing", "[rapid][ai][nlp]") {
    RapidNLP nlp;
    
    SECTION("Basic command parsing") {
        Command cmd = nlp.parseCommand("add reverb to track 1");
        
        REQUIRE(cmd.isValid());
        REQUIRE(cmd.action == "add");
        REQUIRE(cmd.target == "track");
        REQUIRE(cmd.object == "reverb");
        REQUIRE(cmd.parameters["track_id"] == "1");
    }
    
    SECTION("Parameter setting commands") {
        Command cmd = nlp.parseCommand("set volume to 50%");
        
        REQUIRE(cmd.isValid());
        REQUIRE(cmd.action == "set");
        REQUIRE(cmd.target == "parameter");
        REQUIRE(cmd.object == "volume");
        REQUIRE(cmd.parameters["value"] == "0.500000"); // Converted from percentage (float precision)
    }
    
    SECTION("Transport commands") {
        Command playCmd = nlp.parseCommand("play");
        REQUIRE(playCmd.isValid());
        REQUIRE(playCmd.action == "play");
        REQUIRE(playCmd.target == "transport");
        
        Command stopCmd = nlp.parseCommand("stop");
        REQUIRE(stopCmd.isValid());
        REQUIRE(stopCmd.action == "stop");
    }
    
    SECTION("Adjustment commands") {
        Command cmd = nlp.parseCommand("make track 1 louder");
        
        REQUIRE(cmd.isValid());
        REQUIRE(cmd.action == "adjust");
        REQUIRE(cmd.target == "track");
        REQUIRE(cmd.parameters["track_id"] == "1");
        REQUIRE(cmd.parameters["adjustment"] == "louder");
    }
    
    SECTION("Invalid commands") {
        Command cmd = nlp.parseCommand("do something impossible");
        REQUIRE_FALSE(cmd.isValid());
    }
}

TEST_CASE("Rapid Track Management", "[rapid][audio][track]") {
    SECTION("Track creation and basic operations") {
        RapidTrack track("Test Track");
        
        REQUIRE(track.getName() == "Test Track");
        REQUIRE(track.getVolume() == 1.0f);
        REQUIRE_FALSE(track.isMuted());
        
        track.setVolume(0.8f);
        REQUIRE(track.getVolume() == Catch::Approx(0.8f));
        
        track.setMuted(true);
        REQUIRE(track.isMuted());
    }
    
    SECTION("Effect management") {
        RapidTrack track("Effect Test Track");
        
        auto gainEffect = std::make_shared<GainEffect>();
        gainEffect->setParameter("gain", 0.5f);
        
        track.addEffect(gainEffect);
        REQUIRE(track.getEffectCount() == 1);
        
        // Test audio processing with effects
        AudioBuffer buffer(256, 2);
        generateTestTone(buffer, 1000.0f, 1.0f);
        
        float originalPeak = buffer.getPeakLevel();
        track.processAudio(buffer);
        float processedPeak = buffer.getPeakLevel();
        
        // Should be affected by both track volume (1.0) and effect gain (0.5)
        REQUIRE(processedPeak == Catch::Approx(originalPeak * 0.5f).margin(0.1f));
    }
    
    SECTION("Mute functionality") {
        RapidTrack track("Mute Test Track");
        AudioBuffer buffer(128, 2);
        generateTestTone(buffer, 440.0f, 0.8f);
        
        // Process without mute
        track.processAudio(buffer);
        REQUIRE(buffer.getPeakLevel() > 0.0f);
        
        // Process with mute
        generateTestTone(buffer, 440.0f, 0.8f); // Regenerate signal
        track.setMuted(true);
        track.processAudio(buffer);
        REQUIRE(buffer.getPeakLevel() == 0.0f); // Should be silent
    }
}

TEST_CASE("Rapid DAW Integration", "[rapid][daw][integration]") {
    SECTION("DAW initialization and basic setup") {
        RapidDAW daw;
        
        REQUIRE(daw.initialize(44100, 256));
        REQUIRE(daw.getTrackCount() == 0);
        
        daw.addTrack("Test Track 1");
        daw.addTrack("Test Track 2");
        
        REQUIRE(daw.getTrackCount() == 2);
        
        RapidTrack* track1 = daw.getTrack(0);
        RapidTrack* track2 = daw.getTrack(1);
        
        REQUIRE(track1 != nullptr);
        REQUIRE(track2 != nullptr);
        REQUIRE(track1->getName() == "Test Track 1");
        REQUIRE(track2->getName() == "Test Track 2");
    }
    
    SECTION("Natural language command execution") {
        RapidDAW daw;
        daw.initialize();
        daw.addTrack("Vocal Track");
        daw.addTrack("Drum Track");
        
        // Test effect addition
        std::string result = daw.executeCommand("add reverb to track 1");
        REQUIRE(result.find("Added reverb") != std::string::npos);
        
        // Test parameter setting
        result = daw.executeCommand("set volume to 75%");
        REQUIRE(result.find("Set volume to 0.75") != std::string::npos);
        
        // Test transport commands
        result = daw.executeCommand("play");
        REQUIRE(result == "Playback started");
        
        result = daw.executeCommand("stop");
        REQUIRE(result == "Playback stopped");
        
        // Test track adjustment
        result = daw.executeCommand("make track 1 louder");
        REQUIRE(result.find("louder") != std::string::npos);
    }
    
    SECTION("Audio processing pipeline") {
        RapidDAW daw;
        daw.initialize(44100, 128);
        daw.addTrack("Test Audio Track");
        
        // Start audio processing
        REQUIRE(daw.start());
        
        // Process a test block
        daw.processTestBlock(); // Should not crash
        
        REQUIRE(daw.stop());
    }
    
    SECTION("Error handling") {
        RapidDAW daw;
        daw.initialize();
        
        // Test invalid commands
        std::string result = daw.executeCommand("invalid command syntax");
        REQUIRE(result.find("Error") != std::string::npos);
        
        // Test operations on non-existent tracks
        result = daw.executeCommand("add reverb to track 99");
        REQUIRE(result.find("not found") != std::string::npos);
    }
}

TEST_CASE("Rapid Development Performance", "[rapid][performance]") {
    SECTION("Audio processing performance") {
        RapidAudioEngine engine;
        engine.initialize(44100, 512);
        
        AudioBuffer testBuffer(512, 2);
        generateTestTone(testBuffer, 440.0f, 0.5f);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Process 1000 blocks (simulating ~11 seconds of audio)
        for (int i = 0; i < 1000; ++i) {
            GainEffect effect;
            effect.setParameter("gain", 0.8f);
            effect.process(testBuffer);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Should process much faster than real-time
        REQUIRE(duration.count() < 1000); // Should take less than 1 second
    }
    
    SECTION("Command parsing performance") {
        RapidNLP nlp;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Parse 1000 commands
        for (int i = 0; i < 1000; ++i) {
            Command cmd = nlp.parseCommand("add reverb to track 1");
            REQUIRE(cmd.isValid());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Should parse very quickly
        REQUIRE(duration.count() < 100); // Should take less than 100ms
    }
}

TEST_CASE("Rapid Development Demo Scenario", "[rapid][demo]") {
    SECTION("Complete workflow demonstration") {
        RapidDAW daw;
        REQUIRE(daw.initialize(44100, 256));
        
        // Create a basic project setup
        daw.addTrack("Drums");
        daw.addTrack("Bass");
        daw.addTrack("Guitar");
        daw.addTrack("Vocals");
        
        REQUIRE(daw.getTrackCount() == 4);
        
        // Demonstrate AI commands
        std::vector<std::string> commands = {
            "add reverb to track 4",        // Add reverb to vocals
            "add gain to track 1",          // Add gain to drums  
            "set volume to 80%",            // Set overall volume
            "make track 1 louder",          // Make drums louder
            "make track 4 quieter",         // Make vocals quieter
            "play",                         // Start playback
            "stop"                          // Stop playback
        };
        
        for (const auto& command : commands) {
            std::string result = daw.executeCommand(command);
            REQUIRE_FALSE(result.find("Error") == 0); // No errors should occur
        }
        
        // Verify some effects were added
        REQUIRE(daw.getTrack(0)->getEffectCount() >= 1); // Drums should have gain
        REQUIRE(daw.getTrack(3)->getEffectCount() >= 1); // Vocals should have reverb
        
        // Process some audio to ensure everything works
        REQUIRE(daw.start());
        daw.processTestBlock();
        REQUIRE(daw.stop());
    }
}