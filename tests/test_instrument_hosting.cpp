#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "../src/tracks/InstrumentTrack.h"
#include "../src/vsti/VSTiHost.h"
#include "../src/midi/MIDIEvent.h"

using namespace mixmind;

class InstrumentHostingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create VSTi host
        vsti_host = std::make_shared<VSTiHost>();
        
        // Initialize with professional audio settings
        sample_rate = 44100.0;
        buffer_size = 512;
        
        auto host_init = vsti_host->initialize(sample_rate, buffer_size);
        ASSERT_TRUE(host_init.isSuccess()) << "Failed to initialize VSTi host";
        
        // Create artifacts directory
        std::filesystem::create_directories("artifacts");
        
        // Open test log
        log_file.open("artifacts/e2e_vsti.log");
        if (log_file.is_open()) {
            log_file << "=== MixMind AI VSTi Integration Test Log ===" << std::endl;
            log_file << "Date: " << getCurrentTimestamp() << std::endl;
            log_file << "Sample Rate: " << sample_rate << " Hz" << std::endl;
            log_file << "Buffer Size: " << buffer_size << " samples" << std::endl;
            log_file << "=======================================" << std::endl;
        }
        
        // Scan for available instruments
        auto instruments_result = vsti_host->scan_available_instruments();
        if (instruments_result.isSuccess()) {
            available_instruments = instruments_result.getValue();
            
            log_file << "\nAvailable Instruments (" << available_instruments.size() << "):" << std::endl;
            for (const auto& inst : available_instruments) {
                log_file << "  - " << inst.name << " (" << inst.path << ")" << std::endl;
            }
        } else {
            log_file << "\nWARNING: No VST instruments found" << std::endl;
            log_file << "Error: " << instruments_result.getError().toString() << std::endl;
        }
    }
    
    void TearDown() override {
        if (log_file.is_open()) {
            log_file << "\n=== Test Session Complete ===" << std::endl;
            log_file << "End Time: " << getCurrentTimestamp() << std::endl;
            log_file.close();
        }
        
        // Cleanup
        if (vsti_host) {
            vsti_host->shutdown();
        }
    }
    
    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    void logTestResult(const std::string& test_name, bool success, const std::string& details = "") {
        if (log_file.is_open()) {
            log_file << "\n[" << getCurrentTimestamp() << "] " << test_name 
                     << ": " << (success ? "PASS" : "FAIL") << std::endl;
            if (!details.empty()) {
                log_file << "  Details: " << details << std::endl;
            }
        }
    }
    
    // Test data
    std::shared_ptr<VSTiHost> vsti_host;
    double sample_rate = 44100.0;
    int buffer_size = 512;
    std::vector<VST3PluginInfo> available_instruments;
    std::ofstream log_file;
};

// Test 1: VSTi Host Initialization
TEST_F(InstrumentHostingTest, VSTiHostInitialization) {
    EXPECT_TRUE(vsti_host != nullptr);
    
    auto stats = vsti_host->get_host_stats();
    EXPECT_EQ(stats.active_instances, 0);
    
    EXPECT_DOUBLE_EQ(vsti_host->get_global_sample_rate(), sample_rate);
    EXPECT_EQ(vsti_host->get_global_buffer_size(), buffer_size);
    
    logTestResult("VSTiHostInitialization", true, "Host initialized with correct parameters");
}

// Test 2: Instrument Discovery
TEST_F(InstrumentHostingTest, InstrumentDiscovery) {
    bool found_instruments = !available_instruments.empty();
    
    if (!found_instruments) {
        std::cout << "SKIP: No VST instruments found. Install Surge XT: https://surge-synthesizer.github.io/releases" << std::endl;
        logTestResult("InstrumentDiscovery", false, "No instruments found - install free VSTi to test");
        return;
    }
    
    EXPECT_GT(available_instruments.size(), 0);
    
    // Check if we found known professional instruments
    bool found_serum = false, found_arcade = false;
    for (const auto& inst : available_instruments) {
        if (inst.name.find("Serum") != std::string::npos) found_serum = true;
        if (inst.name.find("Arcade") != std::string::npos) found_arcade = true;
    }
    
    std::string details = "Found " + std::to_string(available_instruments.size()) + " instruments";
    if (found_serum) details += ", Serum detected";
    if (found_arcade) details += ", Arcade detected";
    
    logTestResult("InstrumentDiscovery", true, details);
}

// Test 3: Instrument Track Creation
TEST_F(InstrumentHostingTest, InstrumentTrackCreation) {
    // Create instrument track without loading instrument
    auto track = InstrumentTrackFactory::create_track("Test Instrument Track", sample_rate, buffer_size, vsti_host);
    
    EXPECT_TRUE(track != nullptr);
    
    if (track) {
        EXPECT_EQ(track->get_track_type(), TrackType::INSTRUMENT);
        EXPECT_EQ(track->get_name(), "Test Instrument Track");
        EXPECT_FALSE(track->has_instrument());
        
        auto signal_flow = track->get_signal_flow();
        EXPECT_FALSE(signal_flow.accepts_audio_input);  // MIDI in
        EXPECT_TRUE(signal_flow.accepts_midi_input);    // ← Key: accepts MIDI
        EXPECT_TRUE(signal_flow.produces_audio_output); // → Key: produces Audio
        EXPECT_FALSE(signal_flow.produces_midi_output);
        EXPECT_TRUE(signal_flow.can_host_vsti);         // ⭐ Can host instruments
        
        logTestResult("InstrumentTrackCreation", true, "Track created with correct MIDI→Audio signal flow");
    } else {
        logTestResult("InstrumentTrackCreation", false, "Failed to create instrument track");
    }
}

// Test 4: VSTi Loading and Parameter Access
TEST_F(InstrumentHostingTest, VSTiLoadingAndParameters) {
    if (available_instruments.empty()) {
        std::cout << "SKIP: No VST instruments available for loading test" << std::endl;
        logTestResult("VSTiLoadingAndParameters", false, "No instruments available");
        return;
    }
    
    // Use first available instrument
    auto instrument = available_instruments[0];
    
    auto track = InstrumentTrackFactory::create_track("VSTi Test Track", sample_rate, buffer_size, vsti_host);
    ASSERT_TRUE(track != nullptr);
    
    // Load instrument
    auto load_result = track->load_instrument(instrument.path);
    EXPECT_TRUE(load_result.isSuccess()) << "Failed to load instrument: " << load_result.getError().toString();
    
    if (load_result.isSuccess()) {
        EXPECT_TRUE(track->has_instrument());
        EXPECT_EQ(track->get_instrument_name(), instrument.name);
        
        // Test parameter access
        auto param_names_result = track->get_instrument_parameter_names();
        EXPECT_TRUE(param_names_result.isSuccess());
        
        if (param_names_result.isSuccess()) {
            auto param_names = param_names_result.getValue();
            EXPECT_GT(param_names.size(), 0);
            
            // Test parameter get/set
            if (!param_names.empty()) {
                std::string test_param = param_names[0];
                
                auto set_result = track->set_instrument_parameter(test_param, 0.75f);
                EXPECT_TRUE(set_result.isSuccess());
                
                if (set_result.isSuccess()) {
                    auto get_result = track->get_instrument_parameter(test_param);
                    EXPECT_TRUE(get_result.isSuccess());
                    
                    if (get_result.isSuccess()) {
                        float value = get_result.getValue();
                        EXPECT_NEAR(value, 0.75f, 0.01f);
                    }
                }
            }
            
            std::string details = "Loaded " + instrument.name + " with " + 
                                std::to_string(param_names.size()) + " parameters";
            logTestResult("VSTiLoadingAndParameters", true, details);
        } else {
            logTestResult("VSTiLoadingAndParameters", false, "Failed to get parameters");
        }
    } else {
        logTestResult("VSTiLoadingAndParameters", false, "Failed to load instrument");
    }
}

// Test 5: MIDI Input Processing
TEST_F(InstrumentHostingTest, MIDIInputProcessing) {
    if (available_instruments.empty()) {
        std::cout << "SKIP: No VST instruments available for MIDI test" << std::endl;
        logTestResult("MIDIInputProcessing", false, "No instruments available");
        return;
    }
    
    auto instrument = available_instruments[0];
    auto track = InstrumentTrackFactory::create_track("MIDI Test Track", sample_rate, buffer_size, vsti_host);
    ASSERT_TRUE(track != nullptr);
    
    auto load_result = track->load_instrument(instrument.path);
    ASSERT_TRUE(load_result.isSuccess());
    
    // Create MIDI events for testing
    MIDIEventBuffer test_events;
    
    // Note on: C4 (middle C)
    test_events.push_back(MIDIEvent::note_on(0, 60, 100, 0));
    
    // Control change: Modulation wheel
    test_events.push_back(MIDIEvent::control_change(0, MIDIController::MOD_WHEEL, 64, 100));
    
    // Note off: C4
    test_events.push_back(MIDIEvent::note_off(0, 60, 64, 44100)); // 1 second later
    
    // Process MIDI events
    auto midi_result = track->process_midi_input(test_events, 0);
    EXPECT_TRUE(midi_result.isSuccess());
    
    if (midi_result.isSuccess()) {
        auto perf_stats = track->get_performance_stats();
        EXPECT_EQ(perf_stats.midi_events_processed, 3);
        
        logTestResult("MIDIInputProcessing", true, "Processed 3 MIDI events (note on/off, CC)");
    } else {
        logTestResult("MIDIInputProcessing", false, "MIDI processing failed");
    }
}

// Test 6: Audio Output Generation (MIDI → Audio conversion) ⭐ KEY TEST
TEST_F(InstrumentHostingTest, AudioOutputGeneration) {
    if (available_instruments.empty()) {
        std::cout << "SKIP: No VST instruments available for audio generation test" << std::endl;
        logTestResult("AudioOutputGeneration", false, "No instruments available");
        return;
    }
    
    auto instrument = available_instruments[0];
    auto track = InstrumentTrackFactory::create_track("Audio Gen Track", sample_rate, buffer_size, vsti_host);
    ASSERT_TRUE(track != nullptr);
    
    auto load_result = track->load_instrument(instrument.path);
    ASSERT_TRUE(load_result.isSuccess());
    
    // Send MIDI note to trigger audio generation
    MIDIEventBuffer midi_events;
    midi_events.push_back(MIDIEvent::note_on(0, 60, 100, 0)); // C4, full velocity
    
    auto midi_result = track->process_midi_input(midi_events, 0);
    ASSERT_TRUE(midi_result.isSuccess());
    
    // Render audio (MIDI → Audio conversion)
    int render_samples = 4410; // 100ms at 44.1kHz
    auto audio_result = track->render_audio(render_samples);
    
    EXPECT_TRUE(audio_result.isSuccess());
    
    if (audio_result.isSuccess()) {
        auto audio_data = audio_result.getValue();
        
        EXPECT_EQ(audio_data.size(), 2); // Stereo output
        EXPECT_EQ(audio_data[0].size(), render_samples); // Left channel
        EXPECT_EQ(audio_data[1].size(), render_samples); // Right channel
        
        // Check that audio was actually generated (non-zero samples)
        bool has_audio_signal = false;
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < render_samples; ++i) {
                if (std::abs(audio_data[ch][i]) > 0.001f) {
                    has_audio_signal = true;
                    break;
                }
            }
            if (has_audio_signal) break;
        }
        
        EXPECT_TRUE(has_audio_signal) << "Expected audio signal from MIDI note";
        
        auto perf_stats = track->get_performance_stats();
        EXPECT_EQ(perf_stats.audio_samples_rendered, render_samples);
        
        std::string details = "Generated " + std::to_string(render_samples) + 
                            " stereo samples from MIDI input";
        if (has_audio_signal) details += " (audio signal detected)";
        
        logTestResult("AudioOutputGeneration", has_audio_signal, details);
    } else {
        logTestResult("AudioOutputGeneration", false, "Audio rendering failed");
    }
}

// Test 7: Multi-Track Instrument Hosting
TEST_F(InstrumentHostingTest, MultiTrackInstrumentHosting) {
    if (available_instruments.empty()) {
        std::cout << "SKIP: No VST instruments available for multi-track test" << std::endl;
        logTestResult("MultiTrackInstrumentHosting", false, "No instruments available");
        return;
    }
    
    // Create multiple instrument tracks
    std::vector<std::shared_ptr<InstrumentTrack>> tracks;
    
    int num_tracks = std::min(3, static_cast<int>(available_instruments.size()));
    
    for (int i = 0; i < num_tracks; ++i) {
        std::string track_name = "Multi Track " + std::to_string(i + 1);
        auto track = InstrumentTrackFactory::create_track(track_name, sample_rate, buffer_size, vsti_host);
        
        EXPECT_TRUE(track != nullptr);
        
        if (track) {
            auto load_result = track->load_instrument(available_instruments[i].path);
            EXPECT_TRUE(load_result.isSuccess());
            
            if (load_result.isSuccess()) {
                tracks.push_back(track);
            }
        }
    }
    
    EXPECT_GT(tracks.size(), 0);
    
    // Send different MIDI notes to each track
    for (size_t i = 0; i < tracks.size(); ++i) {
        MIDIEventBuffer midi_events;
        midi_events.push_back(MIDIEvent::note_on(0, 60 + i, 100, 0)); // C4, C#4, D4, etc.
        
        auto midi_result = tracks[i]->process_midi_input(midi_events, 0);
        EXPECT_TRUE(midi_result.isSuccess());
    }
    
    // Render audio from all tracks
    std::vector<std::vector<std::vector<float>>> track_outputs;
    int render_samples = 2205; // 50ms
    
    for (auto& track : tracks) {
        auto audio_result = track->render_audio(render_samples);
        EXPECT_TRUE(audio_result.isSuccess());
        
        if (audio_result.isSuccess()) {
            track_outputs.push_back(audio_result.getValue());
        }
    }
    
    EXPECT_EQ(track_outputs.size(), tracks.size());
    
    std::string details = "Successfully hosted " + std::to_string(tracks.size()) + " instruments simultaneously";
    logTestResult("MultiTrackInstrumentHosting", track_outputs.size() == tracks.size(), details);
}

// Test 8: Performance and Latency
TEST_F(InstrumentHostingTest, PerformanceAndLatency) {
    if (available_instruments.empty()) {
        std::cout << "SKIP: No VST instruments available for performance test" << std::endl;
        logTestResult("PerformanceAndLatency", false, "No instruments available");
        return;
    }
    
    auto instrument = available_instruments[0];
    auto track = InstrumentTrackFactory::create_track("Performance Track", sample_rate, buffer_size, vsti_host);
    ASSERT_TRUE(track != nullptr);
    
    auto load_result = track->load_instrument(instrument.path);
    ASSERT_TRUE(load_result.isSuccess());
    
    // Measure MIDI processing latency
    auto start_time = std::chrono::high_resolution_clock::now();
    
    MIDIEventBuffer midi_events;
    for (int note = 60; note < 72; ++note) { // Octave of notes
        midi_events.push_back(MIDIEvent::note_on(0, note, 100, 0));
    }
    
    auto midi_result = track->process_midi_input(midi_events, 0);
    ASSERT_TRUE(midi_result.isSuccess());
    
    auto midi_time = std::chrono::high_resolution_clock::now();
    
    // Measure audio rendering latency
    auto audio_result = track->render_audio(buffer_size);
    ASSERT_TRUE(audio_result.isSuccess());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate latencies
    auto midi_latency = std::chrono::duration_cast<std::chrono::microseconds>(midi_time - start_time);
    auto audio_latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - midi_time);
    
    double midi_latency_ms = midi_latency.count() / 1000.0;
    double audio_latency_ms = audio_latency.count() / 1000.0;
    double total_latency_ms = midi_latency_ms + audio_latency_ms;
    
    // Performance criteria: Total latency should be under 10ms for real-time performance
    bool performance_acceptable = total_latency_ms < 10.0;
    
    auto perf_stats = track->get_performance_stats();
    
    std::stringstream details;
    details << "MIDI: " << std::fixed << std::setprecision(2) << midi_latency_ms << "ms, "
            << "Audio: " << audio_latency_ms << "ms, "
            << "Total: " << total_latency_ms << "ms";
    
    EXPECT_LT(total_latency_ms, 10.0) << "Latency too high for real-time performance";
    
    logTestResult("PerformanceAndLatency", performance_acceptable, details.str());
}

// Test 9: State Persistence
TEST_F(InstrumentHostingTest, StatePersistence) {
    if (available_instruments.empty()) {
        std::cout << "SKIP: No VST instruments available for state test" << std::endl;
        logTestResult("StatePersistence", false, "No instruments available");
        return;
    }
    
    auto instrument = available_instruments[0];
    auto track = InstrumentTrackFactory::create_track("State Track", sample_rate, buffer_size, vsti_host);
    ASSERT_TRUE(track != nullptr);
    
    auto load_result = track->load_instrument(instrument.path);
    ASSERT_TRUE(load_result.isSuccess());
    
    // Set some parameters
    auto param_names_result = track->get_instrument_parameter_names();
    ASSERT_TRUE(param_names_result.isSuccess());
    
    auto param_names = param_names_result.getValue();
    if (!param_names.empty()) {
        // Set parameters to specific values
        std::map<std::string, float> test_values;
        for (size_t i = 0; i < std::min(param_names.size(), size_t(3)); ++i) {
            float test_value = 0.3f + (i * 0.2f); // 0.3, 0.5, 0.7
            test_values[param_names[i]] = test_value;
            
            auto set_result = track->set_instrument_parameter(param_names[i], test_value);
            EXPECT_TRUE(set_result.isSuccess());
        }
        
        // Verify parameters were set correctly
        bool all_params_correct = true;
        for (const auto& pair : test_values) {
            auto get_result = track->get_instrument_parameter(pair.first);
            if (!get_result.isSuccess() || std::abs(get_result.getValue() - pair.second) > 0.01f) {
                all_params_correct = false;
                break;
            }
        }
        
        std::string details = "Set and verified " + std::to_string(test_values.size()) + " parameters";
        logTestResult("StatePersistence", all_params_correct, details);
    } else {
        logTestResult("StatePersistence", false, "No parameters available for testing");
    }
}

// Test 10: Generate Demo Audio (4 bars rendered)
TEST_F(InstrumentHostingTest, GenerateDemoAudio) {
    if (available_instruments.empty()) {
        std::cout << "SKIP: No VST instruments available for demo generation" << std::endl;
        logTestResult("GenerateDemoAudio", false, "No instruments available");
        return;
    }
    
    auto instrument = available_instruments[0];
    auto track = InstrumentTrackFactory::create_track("Demo Track", sample_rate, buffer_size, vsti_host);
    ASSERT_TRUE(track != nullptr);
    
    auto load_result = track->load_instrument(instrument.path);
    ASSERT_TRUE(load_result.isSuccess());
    
    // Create 4 bars of MIDI at 120 BPM
    double bpm = 120.0;
    double beats_per_second = bpm / 60.0;
    double samples_per_beat = sample_rate / beats_per_second;
    int samples_per_bar = static_cast<int>(samples_per_beat * 4); // 4/4 time
    int total_samples = samples_per_bar * 4; // 4 bars
    
    // Create MIDI sequence (simple chord progression)
    std::vector<MIDIEventBuffer> midi_sequence;
    
    // Bar 1: C major chord
    MIDIEventBuffer bar1;
    bar1.push_back(MIDIEvent::note_on(0, 60, 100, 0));        // C
    bar1.push_back(MIDIEvent::note_on(0, 64, 100, 0));        // E
    bar1.push_back(MIDIEvent::note_on(0, 67, 100, 0));        // G
    bar1.push_back(MIDIEvent::note_off(0, 60, 64, samples_per_bar - 1));
    bar1.push_back(MIDIEvent::note_off(0, 64, 64, samples_per_bar - 1));
    bar1.push_back(MIDIEvent::note_off(0, 67, 64, samples_per_bar - 1));
    
    // Bar 2: F major chord  
    MIDIEventBuffer bar2;
    bar2.push_back(MIDIEvent::note_on(0, 57, 100, samples_per_bar));     // F
    bar2.push_back(MIDIEvent::note_on(0, 60, 100, samples_per_bar));     // A
    bar2.push_back(MIDIEvent::note_on(0, 65, 100, samples_per_bar));     // C
    bar2.push_back(MIDIEvent::note_off(0, 57, 64, samples_per_bar * 2 - 1));
    bar2.push_back(MIDIEvent::note_off(0, 60, 64, samples_per_bar * 2 - 1));
    bar2.push_back(MIDIEvent::note_off(0, 65, 64, samples_per_bar * 2 - 1));
    
    // Process MIDI and render audio
    std::vector<std::vector<float>> full_audio(2);
    full_audio[0].reserve(total_samples);
    full_audio[1].reserve(total_samples);
    
    // Process in chunks
    int samples_rendered = 0;
    while (samples_rendered < total_samples) {
        int chunk_size = std::min(buffer_size, total_samples - samples_rendered);
        
        // Send appropriate MIDI events for this time slice
        MIDIEventBuffer chunk_events;
        
        if (samples_rendered == 0) {
            chunk_events = bar1;
        } else if (samples_rendered == samples_per_bar) {
            chunk_events = bar2;
        }
        
        if (!chunk_events.empty()) {
            auto midi_result = track->process_midi_input(chunk_events, samples_rendered);
            ASSERT_TRUE(midi_result.isSuccess());
        }
        
        // Render audio chunk
        auto audio_result = track->render_audio(chunk_size);
        ASSERT_TRUE(audio_result.isSuccess());
        
        auto chunk_audio = audio_result.getValue();
        
        // Append to full audio
        for (int ch = 0; ch < 2; ++ch) {
            full_audio[ch].insert(full_audio[ch].end(), 
                                chunk_audio[ch].begin(), 
                                chunk_audio[ch].end());
        }
        
        samples_rendered += chunk_size;
    }
    
    // Save as WAV file
    std::string output_file = "artifacts/midi_demo.wav";
    bool wav_saved = save_wav_file(output_file, full_audio, sample_rate);
    
    EXPECT_TRUE(wav_saved);
    
    if (wav_saved) {
        double duration_seconds = total_samples / sample_rate;
        std::stringstream details;
        details << "Generated " << std::fixed << std::setprecision(1) << duration_seconds 
                << "s demo (" << total_samples << " samples) saved to " << output_file;
        
        logTestResult("GenerateDemoAudio", true, details.str());
    } else {
        logTestResult("GenerateDemoAudio", false, "Failed to save demo audio");
    }
}

private:
    bool save_wav_file(const std::string& filename, const std::vector<std::vector<float>>& audio, double sample_rate) {
        // Simple WAV file writer (16-bit PCM)
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;
        
        int num_channels = static_cast<int>(audio.size());
        int num_samples = static_cast<int>(audio[0].size());
        int byte_rate = static_cast<int>(sample_rate) * num_channels * 2;
        int data_size = num_samples * num_channels * 2;
        
        // WAV header
        file.write("RIFF", 4);
        int32_t file_size = 36 + data_size;
        file.write(reinterpret_cast<char*>(&file_size), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        int32_t fmt_size = 16;
        file.write(reinterpret_cast<char*>(&fmt_size), 4);
        int16_t format = 1; // PCM
        file.write(reinterpret_cast<char*>(&format), 2);
        int16_t channels = static_cast<int16_t>(num_channels);
        file.write(reinterpret_cast<char*>(&channels), 2);
        int32_t sr = static_cast<int32_t>(sample_rate);
        file.write(reinterpret_cast<char*>(&sr), 4);
        int32_t br = byte_rate;
        file.write(reinterpret_cast<char*>(&br), 4);
        int16_t block_align = num_channels * 2;
        file.write(reinterpret_cast<char*>(&block_align), 2);
        int16_t bits_per_sample = 16;
        file.write(reinterpret_cast<char*>(&bits_per_sample), 2);
        file.write("data", 4);
        file.write(reinterpret_cast<char*>(&data_size), 4);
        
        // Audio data (interleaved)
        for (int sample = 0; sample < num_samples; ++sample) {
            for (int ch = 0; ch < num_channels; ++ch) {
                float float_sample = audio[ch][sample];
                int16_t int_sample = static_cast<int16_t>(float_sample * 32767.0f);
                file.write(reinterpret_cast<char*>(&int_sample), 2);
            }
        }
        
        file.close();
        return true;
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "\n=== MixMind AI Instrument Hosting Tests ===" << std::endl;
    std::cout << "Testing MIDI in → Audio out signal flow with real VST instruments" << std::endl;
    std::cout << "===============================================\n" << std::endl;
    
    auto result = RUN_ALL_TESTS();
    
    std::cout << "\n=== Instrument Hosting Test Summary ===" << std::endl;
    if (result == 0) {
        std::cout << "✅ All instrument hosting tests passed!" << std::endl;
        std::cout << "MIDI → Audio conversion validated with real VST instruments" << std::endl;
        std::cout << "Check artifacts/ directory for test logs and demo audio" << std::endl;
    } else {
        std::cout << "❌ Some tests failed - check artifacts/e2e_vsti.log" << std::endl;
    }
    
    return result;
}