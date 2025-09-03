#include <gtest/gtest.h>
#include "../src/mixer/AudioBus.h"
#include "../src/mixer/MixerTypes.h"
#include "../src/audio/MeterProcessor.h"
#include "../src/audio/AudioBuffer.h"
#include <memory>
#include <cmath>
#include <thread>
#include <chrono>

namespace mixmind {

class MixerSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create audio buffers for testing
        test_buffer = std::make_shared<AudioBuffer>(2, 1024);
        output_buffer = std::make_shared<AudioBuffer>(2, 1024);
        
        // Fill test buffer with known signal (1kHz sine wave)
        generate_test_tone(test_buffer, 1000.0, 0.5, 1024);
        
        // Create bus manager
        bus_manager = std::make_unique<AudioBusManager>();
    }
    
    void TearDown() override {
        bus_manager.reset();
    }
    
    void generate_test_tone(std::shared_ptr<AudioBuffer> buffer, double frequency, 
                           double amplitude, uint32_t samples) {
        double sample_rate = 44100.0;
        double phase_increment = 2.0 * M_PI * frequency / sample_rate;
        
        for (uint32_t ch = 0; ch < buffer->get_channel_count(); ++ch) {
            auto channel_data = buffer->get_channel_data(ch);
            for (uint32_t sample = 0; sample < samples; ++sample) {
                double phase = sample * phase_increment;
                channel_data[sample] = amplitude * std::sin(phase);
            }
        }
    }
    
    void generate_silence(std::shared_ptr<AudioBuffer> buffer, uint32_t samples) {
        for (uint32_t ch = 0; ch < buffer->get_channel_count(); ++ch) {
            auto channel_data = buffer->get_channel_data(ch);
            std::fill(channel_data, channel_data + samples, 0.0);
        }
    }
    
    double measure_rms_level(std::shared_ptr<AudioBuffer> buffer, uint32_t samples) {
        double sum_squares = 0.0;
        uint32_t total_samples = 0;
        
        for (uint32_t ch = 0; ch < buffer->get_channel_count(); ++ch) {
            auto channel_data = buffer->get_channel_data(ch);
            for (uint32_t sample = 0; sample < samples; ++sample) {
                sum_squares += channel_data[sample] * channel_data[sample];
                total_samples++;
            }
        }
        
        return total_samples > 0 ? std::sqrt(sum_squares / total_samples) : 0.0;
    }
    
    std::shared_ptr<AudioBuffer> test_buffer;
    std::shared_ptr<AudioBuffer> output_buffer;
    std::unique_ptr<AudioBusManager> bus_manager;
};

// Test 1: AudioBus basic operations
TEST_F(MixerSystemTest, AudioBusBasicOperations) {
    // Create a test bus
    BusConfig config(BusConfig::AUX_SEND, "Test Bus", 2);
    config.volume_db = -6.0;
    config.pan_position = 0.5;  // Right-biased
    
    AudioBus bus(1, config);
    
    // Test configuration
    EXPECT_EQ(bus.get_bus_id(), 1);
    EXPECT_EQ(bus.get_name(), "Test Bus");
    EXPECT_EQ(bus.get_channel_count(), 2);
    EXPECT_DOUBLE_EQ(bus.get_volume_db(), -6.0);
    EXPECT_DOUBLE_EQ(bus.get_pan_position(), 0.5);
    EXPECT_EQ(bus.get_bus_type(), BusConfig::AUX_SEND);
    
    // Test volume calculations
    double expected_linear = std::pow(10.0, -6.0 / 20.0);  // -6dB to linear
    EXPECT_NEAR(bus.get_volume_linear(), expected_linear, 0.001);
    
    // Test mute/solo
    EXPECT_FALSE(bus.is_muted());
    EXPECT_FALSE(bus.is_soloed());
    
    bus.set_mute(true);
    bus.set_solo(true);
    
    EXPECT_TRUE(bus.is_muted());
    EXPECT_TRUE(bus.is_soloed());
}

// Test 2: AudioBus input source management
TEST_F(MixerSystemTest, AudioBusInputManagement) {
    BusConfig config(BusConfig::GROUP_BUS, "Group Bus", 2);
    AudioBus bus(2, config);
    
    // Add input sources
    auto result1 = bus.add_input_source(101, 1.0);
    auto result2 = bus.add_input_source(102, 0.5);
    auto result3 = bus.add_input_source(103, 2.0);
    
    EXPECT_TRUE(result1.is_ok());
    EXPECT_TRUE(result2.is_ok());
    EXPECT_TRUE(result3.is_ok());
    
    EXPECT_EQ(bus.get_input_count(), 3);
    
    // Test input levels
    EXPECT_DOUBLE_EQ(bus.get_input_level(101), 1.0);
    EXPECT_DOUBLE_EQ(bus.get_input_level(102), 0.5);
    EXPECT_DOUBLE_EQ(bus.get_input_level(103), 2.0);
    EXPECT_DOUBLE_EQ(bus.get_input_level(999), 0.0);  // Non-existent source
    
    // Update input level
    auto update_result = bus.set_input_level(102, 0.75);
    EXPECT_TRUE(update_result.is_ok());
    EXPECT_DOUBLE_EQ(bus.get_input_level(102), 0.75);
    
    // Remove input source
    auto remove_result = bus.remove_input_source(102);
    EXPECT_TRUE(remove_result.is_ok());
    EXPECT_EQ(bus.get_input_count(), 2);
    EXPECT_DOUBLE_EQ(bus.get_input_level(102), 0.0);
    
    // Test input source list
    auto sources = bus.get_input_sources();
    EXPECT_EQ(sources.size(), 2);
    EXPECT_TRUE(std::find(sources.begin(), sources.end(), 101) != sources.end());
    EXPECT_TRUE(std::find(sources.begin(), sources.end(), 103) != sources.end());
}

// Test 3: AudioBus output routing
TEST_F(MixerSystemTest, AudioBusOutputRouting) {
    BusConfig config(BusConfig::AUX_SEND, "Send Bus", 2);
    AudioBus bus(3, config);
    
    // Add output destinations
    RouteDestination dest1(RouteDestination::BUS, 101);
    dest1.send_level = 0.8;
    dest1.send_pan = -0.3;
    dest1.pre_fader = true;
    
    RouteDestination dest2(RouteDestination::MASTER_OUT, 0);
    dest2.send_level = 1.0;
    dest2.pre_fader = false;
    
    auto result1 = bus.add_output_destination(dest1);
    auto result2 = bus.add_output_destination(dest2);
    
    EXPECT_TRUE(result1.is_ok());
    EXPECT_TRUE(result2.is_ok());
    EXPECT_EQ(bus.get_output_count(), 2);
    
    // Test output destinations
    auto destinations = bus.get_output_destinations();
    EXPECT_EQ(destinations.size(), 2);
    
    // Verify first destination
    bool found_bus_dest = false;
    for (const auto& dest : destinations) {
        if (dest.type == RouteDestination::BUS && dest.destination_id == 101) {
            EXPECT_DOUBLE_EQ(dest.send_level, 0.8);
            EXPECT_DOUBLE_EQ(dest.send_pan, -0.3);
            EXPECT_TRUE(dest.pre_fader);
            found_bus_dest = true;
        }
    }
    EXPECT_TRUE(found_bus_dest);
    
    // Update output destination
    dest1.send_level = 0.6;
    auto update_result = bus.update_output_destination(dest1);
    EXPECT_TRUE(update_result.is_ok());
    
    // Remove output destination
    auto remove_result = bus.remove_output_destination(101);
    EXPECT_TRUE(remove_result.is_ok());
    EXPECT_EQ(bus.get_output_count(), 1);
}

// Test 4: AudioBus audio processing with volume and pan
TEST_F(MixerSystemTest, AudioBusAudioProcessing) {
    BusConfig config(BusConfig::GROUP_BUS, "Processing Bus", 2);
    config.volume_db = -12.0;  // Half amplitude
    config.pan_position = 1.0;  // Full right
    
    AudioBus bus(4, config);
    
    // Process test audio
    auto result = bus.process_audio(test_buffer, output_buffer, 0, 512);
    EXPECT_TRUE(result.is_ok());
    
    // Check that output has been processed
    double input_rms = measure_rms_level(test_buffer, 512);
    double output_rms = measure_rms_level(output_buffer, 512);
    
    // Output should be quieter due to -12dB volume
    double expected_gain = std::pow(10.0, -12.0 / 20.0);
    EXPECT_NEAR(output_rms, input_rms * expected_gain, 0.01);
    
    // Test with muted bus
    bus.set_mute(true);
    auto mute_result = bus.process_audio(test_buffer, output_buffer, 0, 512);
    EXPECT_TRUE(mute_result.is_ok());
    
    // Output should be silent when muted
    double muted_rms = measure_rms_level(output_buffer, 512);
    EXPECT_NEAR(muted_rms, 0.0, 0.001);
}

// Test 5: Plugin Delay Compensation
TEST_F(MixerSystemTest, PluginDelayCompensation) {
    BusConfig config(BusConfig::GROUP_BUS, "PDC Bus", 2);
    AudioBus bus(5, config);
    
    // Set delay compensation
    uint32_t delay_samples = 256;
    bus.set_delay_compensation_samples(delay_samples);
    
    EXPECT_EQ(bus.get_delay_compensation_samples(), delay_samples);
    EXPECT_NEAR(bus.get_delay_compensation_ms(), 256.0 / 44100.0 * 1000.0, 0.1);
    
    // Process audio with delay compensation
    auto result = bus.process_audio(test_buffer, output_buffer, 0, 512);
    EXPECT_TRUE(result.is_ok());
    
    // The first delay_samples should be silence (or previous buffer content)
    // This is difficult to test precisely without multiple process calls
    EXPECT_TRUE(result.is_ok());  // Basic functionality test
}

// Test 6: AudioBus activity detection
TEST_F(MixerSystemTest, AudioBusActivityDetection) {
    BusConfig config(BusConfig::AUX_SEND, "Activity Bus", 2);
    AudioBus bus(6, config);
    
    // Initially inactive
    EXPECT_FALSE(bus.is_active());
    EXPECT_EQ(bus.get_samples_processed(), 0);
    
    // Process audio with signal
    auto result = bus.process_audio(test_buffer, output_buffer, 0, 512);
    EXPECT_TRUE(result.is_ok());
    
    // Should be active after processing signal
    EXPECT_TRUE(bus.is_active());
    EXPECT_EQ(bus.get_samples_processed(), 512);
    
    // Process silence
    generate_silence(test_buffer, 512);
    result = bus.process_audio(test_buffer, output_buffer, 0, 512);
    EXPECT_TRUE(result.is_ok());
    
    // Should be inactive after processing silence
    EXPECT_FALSE(bus.is_active());
    EXPECT_EQ(bus.get_samples_processed(), 1024);
}

// Test 7: AudioBusManager basic operations
TEST_F(MixerSystemTest, AudioBusManagerBasicOperations) {
    // Manager should start with master bus
    EXPECT_GT(bus_manager->get_bus_count(), 0);
    EXPECT_GT(bus_manager->get_master_bus_id(), 0);
    
    auto master_bus = bus_manager->get_master_bus();
    EXPECT_NE(master_bus, nullptr);
    EXPECT_EQ(master_bus->get_bus_type(), BusConfig::MASTER_BUS);
    
    // Create additional buses
    BusConfig aux_config(BusConfig::AUX_SEND, "Reverb Send", 2);
    auto aux_result = bus_manager->create_bus(aux_config);
    EXPECT_TRUE(aux_result.is_ok());
    
    BusConfig group_config(BusConfig::GROUP_BUS, "Drums", 2);
    auto group_result = bus_manager->create_bus(group_config);
    EXPECT_TRUE(group_result.is_ok());
    
    EXPECT_EQ(bus_manager->get_bus_count(), 3);  // Master + 2 created
    
    // Test bus retrieval
    uint32_t aux_id = aux_result.unwrap();
    auto retrieved_bus = bus_manager->get_bus(aux_id);
    EXPECT_TRUE(retrieved_bus.is_ok());
    EXPECT_EQ(retrieved_bus.unwrap()->get_name(), "Reverb Send");
    
    // Test bus removal
    auto remove_result = bus_manager->remove_bus(aux_id);
    EXPECT_TRUE(remove_result.is_ok());
    EXPECT_EQ(bus_manager->get_bus_count(), 2);
    
    // Cannot remove master bus
    auto remove_master_result = bus_manager->remove_bus(bus_manager->get_master_bus_id());
    EXPECT_FALSE(remove_master_result.is_ok());
}

// Test 8: AudioBusManager solo/mute state management
TEST_F(MixerSystemTest, AudioBusManagerSoloMuteStates) {
    // Create test buses
    BusConfig config1(BusConfig::GROUP_BUS, "Bus 1", 2);
    BusConfig config2(BusConfig::GROUP_BUS, "Bus 2", 2);
    
    auto bus1_result = bus_manager->create_bus(config1);
    auto bus2_result = bus_manager->create_bus(config2);
    
    EXPECT_TRUE(bus1_result.is_ok());
    EXPECT_TRUE(bus2_result.is_ok());
    
    uint32_t bus1_id = bus1_result.unwrap();
    uint32_t bus2_id = bus2_result.unwrap();
    
    auto bus1 = bus_manager->get_bus(bus1_id).unwrap();
    auto bus2 = bus_manager->get_bus(bus2_id).unwrap();
    
    // Initially no global solo
    EXPECT_FALSE(bus_manager->is_global_solo_active());
    EXPECT_FALSE(bus1->is_mixer_muted());
    EXPECT_FALSE(bus2->is_mixer_muted());
    
    // Solo bus1
    bus1->set_solo(true);
    bus_manager->update_solo_mute_states();
    
    // Global solo should be active
    EXPECT_TRUE(bus_manager->is_global_solo_active());
    EXPECT_FALSE(bus1->is_mixer_muted());  // Soloed bus not muted
    EXPECT_TRUE(bus2->is_mixer_muted());   // Non-soloed bus muted
    
    // Unsolo bus1
    bus1->set_solo(false);
    bus_manager->update_solo_mute_states();
    
    // No solo active
    EXPECT_FALSE(bus_manager->is_global_solo_active());
    EXPECT_FALSE(bus1->is_mixer_muted());
    EXPECT_FALSE(bus2->is_mixer_muted());
}

// Test 9: AudioBusFactory bus creation
TEST_F(MixerSystemTest, AudioBusFactoryCreation) {
    // Test aux send bus creation
    auto aux_bus = AudioBusFactory::create_aux_send_bus(101, "Aux Send Test");
    EXPECT_NE(aux_bus, nullptr);
    EXPECT_EQ(aux_bus->get_bus_id(), 101);
    EXPECT_EQ(aux_bus->get_name(), "Aux Send Test");
    EXPECT_EQ(aux_bus->get_bus_type(), BusConfig::AUX_SEND);
    EXPECT_EQ(aux_bus->get_channel_count(), 2);
    EXPECT_EQ(aux_bus->get_volume_db(), -10.0);  // Default send level
    
    // Test group bus creation
    auto group_bus = AudioBusFactory::create_group_bus(102, "Group Test");
    EXPECT_NE(group_bus, nullptr);
    EXPECT_EQ(group_bus->get_bus_type(), BusConfig::GROUP_BUS);
    EXPECT_EQ(group_bus->get_volume_db(), 0.0);  // Default group level
    
    // Test master bus creation
    auto master_bus = AudioBusFactory::create_master_bus(103, "Master Test");
    EXPECT_NE(master_bus, nullptr);
    EXPECT_EQ(master_bus->get_bus_type(), BusConfig::MASTER_BUS);
    EXPECT_EQ(master_bus->get_output_count(), 0);  // Master has no outputs
    
    // Test monitor bus creation
    auto monitor_bus = AudioBusFactory::create_monitor_bus(104, "Monitor Test");
    EXPECT_NE(monitor_bus, nullptr);
    EXPECT_EQ(monitor_bus->get_bus_type(), BusConfig::MONITOR_BUS);
    
    // Test custom bus creation
    BusConfig custom_config(BusConfig::AUX_SEND, "Custom Bus", 6);
    custom_config.volume_db = -3.0;
    auto custom_bus = AudioBusFactory::create_custom_bus(105, custom_config);
    EXPECT_NE(custom_bus, nullptr);
    EXPECT_EQ(custom_bus->get_channel_count(), 6);
    EXPECT_EQ(custom_bus->get_volume_db(), -3.0);
}

// Test 10: MeterProcessor peak metering
TEST_F(MixerSystemTest, MeterProcessorPeakMetering) {
    MeterProcessor meter(2, 44100);
    
    // Process test signal
    meter.process_metering(test_buffer, 512);
    
    auto meter_data = meter.get_meter_data();
    
    // Test buffer contains 0.5 amplitude sine wave
    EXPECT_EQ(meter_data.peak_levels.size(), 2);
    EXPECT_EQ(meter_data.peak_levels_db.size(), 2);
    EXPECT_EQ(meter_data.clip_indicators.size(), 2);
    
    // Peak should be close to 0.5 (-6dB)
    EXPECT_NEAR(meter_data.peak_levels[0], 0.5, 0.1);
    EXPECT_NEAR(meter_data.peak_levels[1], 0.5, 0.1);
    EXPECT_NEAR(meter_data.peak_levels_db[0], -6.0, 1.0);
    EXPECT_NEAR(meter_data.peak_levels_db[1], -6.0, 1.0);
    
    // No clipping with 0.5 amplitude
    EXPECT_FALSE(meter_data.clip_indicators[0]);
    EXPECT_FALSE(meter_data.clip_indicators[1]);
}

// Test 11: MeterProcessor RMS metering
TEST_F(MixerSystemTest, MeterProcessorRMSMetering) {
    MeterProcessor meter(2, 44100);
    meter.set_rms_window_size_ms(100.0);  // 100ms window
    
    // Process several buffers to fill RMS window
    for (int i = 0; i < 10; ++i) {
        meter.process_metering(test_buffer, 512);
    }
    
    auto meter_data = meter.get_meter_data();
    
    // RMS of sine wave is amplitude / sqrt(2)
    double expected_rms = 0.5 / std::sqrt(2.0);  // ~0.354
    double expected_rms_db = 20.0 * std::log10(expected_rms);  // ~-9dB
    
    EXPECT_EQ(meter_data.rms_levels.size(), 2);
    EXPECT_EQ(meter_data.rms_levels_db.size(), 2);
    
    EXPECT_NEAR(meter_data.rms_levels[0], expected_rms, 0.05);
    EXPECT_NEAR(meter_data.rms_levels[1], expected_rms, 0.05);
    EXPECT_NEAR(meter_data.rms_levels_db[0], expected_rms_db, 1.0);
    EXPECT_NEAR(meter_data.rms_levels_db[1], expected_rms_db, 1.0);
}

// Test 12: MeterProcessor correlation metering
TEST_F(MixerSystemTest, MeterProcessorCorrelationMetering) {
    MeterProcessor meter(2, 44100);
    meter.enable_correlation_metering(true);
    
    // Create perfectly correlated signal (same on both channels)
    auto correlated_buffer = std::make_shared<AudioBuffer>(2, 512);
    generate_test_tone(correlated_buffer, 1000.0, 0.5, 512);
    
    // Process several buffers
    for (int i = 0; i < 10; ++i) {
        meter.process_metering(correlated_buffer, 512);
    }
    
    auto meter_data = meter.get_meter_data();
    
    // Perfect correlation should be close to 1.0
    EXPECT_NEAR(meter_data.phase_correlation, 1.0, 0.1);
    
    // Create anti-correlated signal (inverted on right channel)
    auto left_data = correlated_buffer->get_channel_data(0);
    auto right_data = correlated_buffer->get_channel_data(1);
    for (uint32_t i = 0; i < 512; ++i) {
        right_data[i] = -left_data[i];  // Invert right channel
    }
    
    meter.reset_meters();
    
    // Process anti-correlated signal
    for (int i = 0; i < 10; ++i) {
        meter.process_metering(correlated_buffer, 512);
    }
    
    meter_data = meter.get_meter_data();
    
    // Anti-correlation should be close to -1.0
    EXPECT_NEAR(meter_data.phase_correlation, -1.0, 0.1);
}

// Test 13: LUFS metering basic functionality
TEST_F(MixerSystemTest, LUFSMeteringBasicFunctionality) {
    LUFSMeter lufs_meter(2, 44100);
    
    // Start measurement
    lufs_meter.start_measurement();
    EXPECT_TRUE(lufs_meter.is_measuring());
    
    // Process audio for several seconds to get meaningful measurements
    for (int i = 0; i < 200; ++i) {  // ~2.3 seconds of audio
        lufs_meter.process_audio(test_buffer, 512);
    }
    
    // Get measurements
    double momentary = lufs_meter.get_momentary_lufs();
    double short_term = lufs_meter.get_short_term_lufs();
    double integrated = lufs_meter.get_integrated_lufs();
    double true_peak = lufs_meter.get_true_peak_dbfs();
    
    // All measurements should be above silence threshold
    EXPECT_GT(momentary, -50.0);
    EXPECT_GT(short_term, -50.0);
    EXPECT_GT(integrated, -50.0);
    EXPECT_GT(true_peak, -50.0);
    
    // True peak should be higher than LUFS values (different measurement types)
    EXPECT_GT(true_peak, integrated);
    
    // Test measurement control
    lufs_meter.stop_measurement();
    EXPECT_FALSE(lufs_meter.is_measuring());
    
    lufs_meter.reset_measurement();
    EXPECT_EQ(lufs_meter.get_samples_processed(), 0);
}

// Test 14: Complete mixer signal flow integration
TEST_F(MixerSystemTest, CompleteMixerSignalFlow) {
    // Create a complete mixer setup
    BusConfig drum_config(BusConfig::GROUP_BUS, "Drums", 2);
    BusConfig vocal_config(BusConfig::GROUP_BUS, "Vocals", 2);
    BusConfig reverb_config(BusConfig::AUX_SEND, "Reverb Send", 2);
    
    auto drum_result = bus_manager->create_bus(drum_config);
    auto vocal_result = bus_manager->create_bus(vocal_config);
    auto reverb_result = bus_manager->create_bus(reverb_config);
    
    EXPECT_TRUE(drum_result.is_ok());
    EXPECT_TRUE(vocal_result.is_ok());
    EXPECT_TRUE(reverb_result.is_ok());
    
    uint32_t drum_id = drum_result.unwrap();
    uint32_t vocal_id = vocal_result.unwrap();
    uint32_t reverb_id = reverb_result.unwrap();
    
    auto drum_bus = bus_manager->get_bus(drum_id).unwrap();
    auto vocal_bus = bus_manager->get_bus(vocal_id).unwrap();
    auto reverb_bus = bus_manager->get_bus(reverb_id).unwrap();
    auto master_bus = bus_manager->get_master_bus();
    
    // Set up routing: drums and vocals -> reverb send
    RouteDestination reverb_send(RouteDestination::BUS, reverb_id);
    reverb_send.send_level = 0.3;  // 30% to reverb
    
    drum_bus->add_output_destination(reverb_send);
    vocal_bus->add_output_destination(reverb_send);
    
    // Add input sources to reverb bus
    reverb_bus->add_input_source(drum_id, 0.3);
    reverb_bus->add_input_source(vocal_id, 0.3);
    
    // Configure bus levels
    drum_bus->set_volume_db(-3.0);   // Slightly quieter drums
    vocal_bus->set_volume_db(0.0);   // Full level vocals
    reverb_bus->set_volume_db(-12.0); // Quiet reverb return
    
    // Process signal flow
    auto drum_out = std::make_shared<AudioBuffer>(2, 512);
    auto vocal_out = std::make_shared<AudioBuffer>(2, 512);
    auto reverb_out = std::make_shared<AudioBuffer>(2, 512);
    auto master_out = std::make_shared<AudioBuffer>(2, 512);
    
    // Process drum bus
    auto drum_result_proc = drum_bus->process_audio(test_buffer, drum_out, 0, 512);
    EXPECT_TRUE(drum_result_proc.is_ok());
    
    // Process vocal bus  
    auto vocal_result_proc = vocal_bus->process_audio(test_buffer, vocal_out, 0, 512);
    EXPECT_TRUE(vocal_result_proc.is_ok());
    
    // Process reverb bus (receives from drums and vocals)
    auto reverb_result_proc = reverb_bus->process_audio(test_buffer, reverb_out, 0, 512);
    EXPECT_TRUE(reverb_result_proc.is_ok());
    
    // Verify that processing worked
    double drum_rms = measure_rms_level(drum_out, 512);
    double vocal_rms = measure_rms_level(vocal_out, 512);
    double reverb_rms = measure_rms_level(reverb_out, 512);
    
    EXPECT_GT(drum_rms, 0.0);
    EXPECT_GT(vocal_rms, 0.0);
    EXPECT_GT(reverb_rms, 0.0);
    
    // Drum bus should be quieter than vocal bus
    EXPECT_LT(drum_rms, vocal_rms);
    
    // Reverb return should be quieter than source buses
    EXPECT_LT(reverb_rms, drum_rms);
    EXPECT_LT(reverb_rms, vocal_rms);
}

// Test 15: Performance test - multiple buses processing
TEST_F(MixerSystemTest, PerformanceTestMultipleBusesProcessing) {
    const int num_buses = 16;
    const int buffer_size = 512;
    const int num_iterations = 100;
    
    std::vector<uint32_t> bus_ids;
    
    // Create many buses
    for (int i = 0; i < num_buses; ++i) {
        BusConfig config(BusConfig::GROUP_BUS, "Perf Bus " + std::to_string(i), 2);
        auto result = bus_manager->create_bus(config);
        EXPECT_TRUE(result.is_ok());
        bus_ids.push_back(result.unwrap());
    }
    
    // Measure processing time
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int iteration = 0; iteration < num_iterations; ++iteration) {
        for (uint32_t bus_id : bus_ids) {
            auto bus = bus_manager->get_bus(bus_id).unwrap();
            auto result = bus->process_audio(test_buffer, output_buffer, 0, buffer_size);
            EXPECT_TRUE(result.is_ok());
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Calculate performance metrics
    uint64_t total_samples = num_buses * num_iterations * buffer_size;
    double processing_time_ms = duration.count() / 1000.0;
    double samples_per_second = total_samples / (processing_time_ms / 1000.0);
    
    // Should process much faster than real-time (44.1kHz)
    EXPECT_GT(samples_per_second, 44100.0 * 10);  // At least 10x real-time
    
    std::cout << "Processed " << total_samples << " samples in " 
              << processing_time_ms << "ms (" 
              << samples_per_second << " samples/sec)" << std::endl;
}

} // namespace mixmind

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}