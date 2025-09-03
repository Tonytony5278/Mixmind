#include <gtest/gtest.h>
#include "../src/automation/AutomationData.h"
#include "../src/automation/AutomationRecorder.h"
#include "../src/automation/AutomationEditor.h"
#include "../src/automation/AutomationEngine.h"
#include <memory>
#include <chrono>
#include <thread>

using namespace mixmind;

class AutomationTest : public ::testing::Test {
protected:
    void SetUp() override {
        automation_data = std::make_shared<AutomationData>();
        
        // Create test parameter IDs
        volume_param = AutomationUtils::create_track_volume_id(1);
        pan_param = AutomationUtils::create_track_pan_id(1);
        vst_param = AutomationUtils::create_vst_parameter_id(1, 100, 5);
        cc_param = AutomationUtils::create_midi_cc_id(1, 1);
    }

    std::shared_ptr<AutomationData> automation_data;
    AutomationParameterId volume_param;
    AutomationParameterId pan_param;
    AutomationParameterId vst_param;
    AutomationParameterId cc_param;
};

class AutomationRecorderTest : public AutomationTest {
protected:
    void SetUp() override {
        AutomationTest::SetUp();
        recorder = std::make_unique<AutomationRecorder>(automation_data);
    }

    std::unique_ptr<AutomationRecorder> recorder;
};

class AutomationEditorTest : public AutomationTest {
protected:
    void SetUp() override {
        AutomationTest::SetUp();
        editor = AutomationEditorFactory::create_standard_editor(automation_data);
        
        // Create a test lane
        auto lane_result = automation_data->create_lane(volume_param, 0.5);
        ASSERT_TRUE(lane_result.is_success());
        test_lane = lane_result.get_value();
        editor->set_current_lane(test_lane);
    }

    std::unique_ptr<AutomationEditor> editor;
    std::shared_ptr<AutomationLane> test_lane;
};

class AutomationEngineTest : public AutomationTest {
protected:
    void SetUp() override {
        AutomationTest::SetUp();
        engine = AutomationEngineFactory::create_standard_engine(automation_data);
    }

    std::unique_ptr<AutomationEngine> engine;
};

// Test 1: AutomationData Basic Operations
TEST_F(AutomationTest, AutomationDataBasicOperations) {
    ASSERT_NE(automation_data, nullptr);
    EXPECT_EQ(automation_data->get_lane_count(), 0);
    
    // Test creating lanes
    auto result = automation_data->create_lane(volume_param, 0.8);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(automation_data->get_lane_count(), 1);
    
    auto lane = result.get_value();
    EXPECT_NE(lane, nullptr);
    EXPECT_EQ(lane->get_default_value(), 0.8);
    EXPECT_EQ(lane->get_parameter_id().type, AutomationParameterId::TRACK_VOLUME);
    
    // Test getting lane
    auto retrieved_lane = automation_data->get_lane(volume_param);
    EXPECT_EQ(retrieved_lane, lane);
    
    // Test removing lane
    auto remove_result = automation_data->remove_lane(volume_param);
    EXPECT_TRUE(remove_result.is_success());
    EXPECT_EQ(automation_data->get_lane_count(), 0);
}

// Test 2: AutomationLane Point Operations
TEST_F(AutomationTest, AutomationLanePointOperations) {
    auto lane_result = automation_data->create_lane(volume_param, 0.5);
    ASSERT_TRUE(lane_result.is_success());
    auto lane = lane_result.get_value();
    
    // Test adding points
    AutomationPoint point1(1000, 0.8); // 1000 samples, 0.8 value
    auto add_result = lane->add_point(point1);
    EXPECT_TRUE(add_result.is_success());
    EXPECT_EQ(lane->get_point_count(), 1);
    
    AutomationPoint point2(2000, 0.6); // 2000 samples, 0.6 value
    add_result = lane->add_point(point2);
    EXPECT_TRUE(add_result.is_success());
    EXPECT_EQ(lane->get_point_count(), 2);
    
    // Test getting value at time (interpolation)
    double value_at_1500 = lane->get_value_at_time(1500); // Halfway between points
    EXPECT_GT(value_at_1500, 0.6);
    EXPECT_LT(value_at_1500, 0.8);
    
    // Test value before first point
    double value_at_500 = lane->get_value_at_time(500);
    EXPECT_EQ(value_at_500, 0.5); // Should return default value
    
    // Test value after last point
    double value_at_3000 = lane->get_value_at_time(3000);
    EXPECT_EQ(value_at_3000, 0.6); // Should return last point value
    
    // Test removing points
    auto remove_result = lane->remove_point(0);
    EXPECT_TRUE(remove_result.is_success());
    EXPECT_EQ(lane->get_point_count(), 1);
}

// Test 3: AutomationLane Selection and Editing
TEST_F(AutomationTest, AutomationLaneSelectionAndEditing) {
    auto lane_result = automation_data->create_lane(pan_param, 0.5);
    ASSERT_TRUE(lane_result.is_success());
    auto lane = lane_result.get_value();
    
    // Add test points
    lane->add_point(AutomationPoint(1000, 0.2));
    lane->add_point(AutomationPoint(2000, 0.8));
    lane->add_point(AutomationPoint(3000, 0.4));
    
    // Test selection
    lane->select_points_in_range(1500, 2500);
    auto selected = lane->get_selected_points();
    EXPECT_EQ(selected.size(), 1); // Only middle point should be selected
    EXPECT_EQ(selected[0]->time_samples, 2000);
    
    // Test moving selected points
    auto move_result = lane->move_selected_points(500, 0.1); // Move right 500 samples, up 0.1
    EXPECT_TRUE(move_result.is_success());
    
    // Check that point moved
    const auto& points = lane->get_points();
    bool found_moved_point = false;
    for (const auto& point : points) {
        if (point.time_samples == 2500) { // Original 2000 + 500
            EXPECT_FLOAT_EQ(point.value, 0.9f); // Original 0.8 + 0.1
            found_moved_point = true;
        }
    }
    EXPECT_TRUE(found_moved_point);
}

// Test 4: AutomationLane Quantization
TEST_F(AutomationTest, AutomationLaneQuantization) {
    auto lane_result = automation_data->create_lane(volume_param, 0.5);
    ASSERT_TRUE(lane_result.is_success());
    auto lane = lane_result.get_value();
    
    // Add slightly off-grid points
    lane->add_point(AutomationPoint(1050, 0.3)); // Slightly after 1024
    lane->add_point(AutomationPoint(2100, 0.7)); // Slightly after 2048
    
    lane->select_all_points();
    
    // Quantize to 1024-sample grid
    auto quantize_result = lane->quantize_points_timing(1024);
    EXPECT_TRUE(quantize_result.is_success());
    
    // Check that points are now on grid
    const auto& points = lane->get_points();
    EXPECT_EQ(points[0].time_samples, 1024); // Should snap to nearest grid
    EXPECT_EQ(points[1].time_samples, 2048);
}

// Test 5: Automation Parameter ID Utilities
TEST_F(AutomationTest, AutomationParameterIdUtilities) {
    // Test parameter ID creation
    auto track_vol_id = AutomationUtils::create_track_volume_id(5);
    EXPECT_EQ(track_vol_id.type, AutomationParameterId::TRACK_VOLUME);
    EXPECT_EQ(track_vol_id.track_id, 5);
    
    auto vst_param_id = AutomationUtils::create_vst_parameter_id(2, 123, 7);
    EXPECT_EQ(vst_param_id.type, AutomationParameterId::VST_PARAMETER);
    EXPECT_EQ(vst_param_id.track_id, 2);
    EXPECT_EQ(vst_param_id.plugin_instance_id, 123);
    EXPECT_EQ(vst_param_id.parameter_index, 7);
    
    // Test display name generation
    std::string display_name = track_vol_id.get_display_name();
    EXPECT_NE(display_name.find("Track 5"), std::string::npos);
    EXPECT_NE(display_name.find("Volume"), std::string::npos);
    
    // Test comparison operators
    auto same_id = AutomationUtils::create_track_volume_id(5);
    auto different_id = AutomationUtils::create_track_volume_id(6);
    
    EXPECT_TRUE(track_vol_id == same_id);
    EXPECT_FALSE(track_vol_id == different_id);
}

// Test 6: AutomationRecorder Basic Operations
TEST_F(AutomationRecorderTest, AutomationRecorderBasicOperations) {
    ASSERT_NE(recorder, nullptr);
    
    // Test initial state
    EXPECT_FALSE(recorder->is_recording());
    EXPECT_EQ(recorder->get_recording_mode(), RecordingMode::LATCH);
    
    // Test arming parameters
    auto arm_result = recorder->arm_parameter(volume_param);
    EXPECT_TRUE(arm_result.is_success());
    EXPECT_TRUE(recorder->is_parameter_armed(volume_param));
    
    auto armed_params = recorder->get_armed_parameters();
    EXPECT_EQ(armed_params.size(), 1);
    EXPECT_TRUE(std::find(armed_params.begin(), armed_params.end(), volume_param) != armed_params.end());
    
    // Test starting recording
    auto start_result = recorder->start_recording();
    EXPECT_TRUE(start_result.is_success());
    EXPECT_TRUE(recorder->is_recording());
    
    // Test stopping recording
    auto stop_result = recorder->stop_recording();
    EXPECT_TRUE(stop_result.is_success());
    EXPECT_FALSE(recorder->is_recording());
}

// Test 7: AutomationRecorder Hardware Control Mapping
TEST_F(AutomationRecorderTest, AutomationRecorderControlMapping) {
    // Create a control mapping
    HardwareControlMapping mapping = AutomationRecorderFactory::create_mod_wheel_mapping(volume_param);
    EXPECT_EQ(mapping.control_type, HardwareControlMapping::MIDI_CC);
    EXPECT_EQ(mapping.midi_cc_number, 1); // Mod wheel is CC 1
    EXPECT_EQ(mapping.target_parameter, volume_param);
    
    // Add mapping to recorder
    auto add_result = recorder->add_control_mapping(mapping);
    EXPECT_TRUE(add_result.is_success());
    
    // Test getting mappings
    auto all_mappings = recorder->get_all_mappings();
    EXPECT_EQ(all_mappings.size(), 1);
    EXPECT_EQ(all_mappings[0].midi_cc_number, 1);
    
    // Test updating mapping
    mapping.sensitivity = 0.5;
    auto update_result = recorder->update_control_mapping(volume_param, mapping);
    EXPECT_TRUE(update_result.is_success());
    
    auto updated_mapping = recorder->get_mapping(volume_param);
    EXPECT_NE(updated_mapping, nullptr);
    EXPECT_FLOAT_EQ(updated_mapping->sensitivity, 0.5f);
    
    // Test removing mapping
    auto remove_result = recorder->remove_control_mapping(volume_param);
    EXPECT_TRUE(remove_result.is_success());
    EXPECT_EQ(recorder->get_all_mappings().size(), 0);
}

// Test 8: AutomationRecorder MIDI Processing
TEST_F(AutomationRecorderTest, AutomationRecorderMidiProcessing) {
    // Set up recording
    recorder->arm_parameter(volume_param);
    
    // Add control mapping for mod wheel
    HardwareControlMapping mapping = AutomationRecorderFactory::create_mod_wheel_mapping(volume_param);
    recorder->add_control_mapping(mapping);
    
    recorder->start_recording();
    
    // Process MIDI CC
    uint64_t timestamp = 44100; // 1 second at 44.1kHz
    recorder->process_midi_cc(0, 1, 64, timestamp); // Channel 0, CC 1 (mod wheel), value 64
    
    // Give processing thread time to work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Check if automation was recorded
    auto lane = automation_data->get_lane(volume_param);
    if (lane) {
        EXPECT_GT(lane->get_point_count(), 0);
    }
    
    recorder->stop_recording();
}

// Test 9: AutomationEditor Basic Drawing Operations
TEST_F(AutomationEditorTest, AutomationEditorBasicDrawing) {
    ASSERT_NE(editor, nullptr);
    ASSERT_NE(test_lane, nullptr);
    
    // Test drawing a single point
    auto draw_result = editor->draw_point_at_time(1000, 0.8);
    EXPECT_TRUE(draw_result.is_success());
    EXPECT_EQ(test_lane->get_point_count(), 1);
    
    // Test drawing a line segment
    auto line_result = editor->draw_line_segment(2000, 4000, 0.2, 0.9, AutomationCurveType::LINEAR);
    EXPECT_TRUE(line_result.is_success());
    EXPECT_GT(test_lane->get_point_count(), 1);
    
    // Test drawing a sine wave
    auto sine_result = editor->draw_sine_wave(5000, 44100, 2.0, 0.3, 0.5); // 1 second, 2Hz
    EXPECT_TRUE(sine_result.is_success());
    
    // Check that points were added
    size_t final_point_count = test_lane->get_point_count();
    EXPECT_GT(final_point_count, 10); // Should have added many points for sine wave
}

// Test 10: AutomationEditor Selection and Editing
TEST_F(AutomationEditorTest, AutomationEditorSelectionAndEditing) {
    // Add test points
    editor->draw_point_at_time(1000, 0.2);
    editor->draw_point_at_time(2000, 0.8);
    editor->draw_point_at_time(3000, 0.4);
    
    // Test point selection
    auto select_result = editor->select_point_at_time(2000);
    EXPECT_TRUE(select_result.is_success());
    
    auto selected_points = test_lane->get_selected_points();
    EXPECT_EQ(selected_points.size(), 1);
    EXPECT_EQ(selected_points[0]->time_samples, 2000);
    
    // Test moving selected points
    auto move_result = editor->move_selected_points(500, 0.1);
    EXPECT_TRUE(move_result.is_success());
    
    // Test scaling selected values
    auto scale_result = editor->scale_selected_values(1.2, 0.5);
    EXPECT_TRUE(scale_result.is_success());
    
    // Test setting curve type
    auto curve_result = editor->set_selected_curve_type(AutomationCurveType::EXPONENTIAL);
    EXPECT_TRUE(curve_result.is_success());
}

// Test 11: AutomationEditor Advanced Operations
TEST_F(AutomationEditorTest, AutomationEditorAdvancedOperations) {
    // Add test points with various values
    editor->draw_point_at_time(1000, 0.1);
    editor->draw_point_at_time(2000, 0.9);
    editor->draw_point_at_time(3000, 0.3);
    editor->draw_point_at_time(4000, 0.7);
    
    editor->select_all_points();
    
    // Test normalization
    auto normalize_result = editor->normalize_selected_values();
    EXPECT_TRUE(normalize_result.is_success());
    
    // Check that values are normalized (min should be 0.0, max should be 1.0)
    const auto& points = test_lane->get_points();
    double min_val = 1.0, max_val = 0.0;
    for (const auto& point : points) {
        min_val = std::min(min_val, point.value);
        max_val = std::max(max_val, point.value);
    }
    EXPECT_FLOAT_EQ(min_val, 0.0f);
    EXPECT_FLOAT_EQ(max_val, 1.0f);
    
    // Test inversion
    editor->select_all_points();
    auto invert_result = editor->invert_selected_values();
    EXPECT_TRUE(invert_result.is_success());
    
    // Test smoothing
    auto smooth_result = editor->smooth_selected_points(0.5f);
    EXPECT_TRUE(smooth_result.is_success());
}

// Test 12: AutomationEditor Copy/Paste Operations
TEST_F(AutomationEditorTest, AutomationEditorCopyPaste) {
    // Add test points
    editor->draw_point_at_time(1000, 0.3);
    editor->draw_point_at_time(2000, 0.7);
    
    // Select points
    editor->select_points_in_range(500, 2500);
    
    // Copy points
    auto copy_result = editor->copy_selected_points();
    EXPECT_TRUE(copy_result.is_success());
    
    // Paste at different location
    auto paste_result = editor->paste_points_at_time(5000);
    EXPECT_TRUE(paste_result.is_success());
    
    // Should now have 4 points total (2 original + 2 pasted)
    EXPECT_EQ(test_lane->get_point_count(), 4);
    
    // Check that pasted points exist at correct times
    auto point_at_6000 = test_lane->find_point_at_time(6000, 100); // 5000 + (2000 - 1000)
    EXPECT_NE(point_at_6000, nullptr);
}

// Test 13: AutomationEditor Undo/Redo
TEST_F(AutomationEditorTest, AutomationEditorUndoRedo) {
    // Initial state: no points
    EXPECT_EQ(test_lane->get_point_count(), 0);
    
    // Add a point
    editor->draw_point_at_time(1000, 0.5);
    EXPECT_EQ(test_lane->get_point_count(), 1);
    
    // Undo
    auto undo_result = editor->undo_last_operation();
    EXPECT_TRUE(undo_result.is_success());
    EXPECT_EQ(test_lane->get_point_count(), 0);
    
    // Redo
    auto redo_result = editor->redo_last_operation();
    EXPECT_TRUE(redo_result.is_success());
    EXPECT_EQ(test_lane->get_point_count(), 1);
}

// Test 14: AutomationEngine Basic Operations
TEST_F(AutomationEngineTest, AutomationEngineBasicOperations) {
    ASSERT_NE(engine, nullptr);
    
    // Test initial state
    EXPECT_FALSE(engine->is_playing());
    EXPECT_TRUE(engine->is_automation_enabled());
    EXPECT_EQ(engine->get_playback_position(), 0);
    
    // Test enabling/disabling automation
    auto disable_result = engine->disable_automation();
    EXPECT_TRUE(disable_result.is_success());
    EXPECT_FALSE(engine->is_automation_enabled());
    
    auto enable_result = engine->enable_automation();
    EXPECT_TRUE(enable_result.is_success());
    EXPECT_TRUE(engine->is_automation_enabled());
    
    // Test playback control
    auto start_result = engine->start_playback();
    EXPECT_TRUE(start_result.is_success());
    EXPECT_TRUE(engine->is_playing());
    
    auto stop_result = engine->stop_playback();
    EXPECT_TRUE(stop_result.is_success());
    EXPECT_FALSE(engine->is_playing());
}

// Test 15: AutomationEngine Parameter Registration and Processing
TEST_F(AutomationEngineTest, AutomationEngineParameterProcessing) {
    // Create automation lane with test data
    auto lane_result = automation_data->create_lane(volume_param, 0.5);
    ASSERT_TRUE(lane_result.is_success());
    auto lane = lane_result.get_value();
    
    // Add automation points
    lane->add_point(AutomationPoint(0, 0.0));
    lane->add_point(AutomationPoint(44100, 1.0)); // Linear ramp over 1 second
    
    // Create automation target
    AutomationTarget target(AutomationTarget::TRACK_VOLUME, 1);
    
    // Register parameter
    auto register_result = engine->register_automation_target(volume_param, target);
    EXPECT_TRUE(register_result.is_success());
    EXPECT_TRUE(engine->is_parameter_registered(volume_param));
    
    // Test getting registered parameters
    auto registered_params = engine->get_registered_parameters();
    EXPECT_EQ(registered_params.size(), 1);
    EXPECT_EQ(registered_params[0], volume_param);
    
    // Test getting parameter value at different times
    engine->set_playback_position(0);
    double value_at_start = engine->get_current_parameter_value(volume_param);
    EXPECT_FLOAT_EQ(value_at_start, 0.0f);
    
    engine->set_playback_position(22050); // Halfway point
    double value_at_middle = engine->get_current_parameter_value(volume_param);
    EXPECT_GT(value_at_middle, 0.25);
    EXPECT_LT(value_at_middle, 0.75);
    
    engine->set_playback_position(44100); // End point
    double value_at_end = engine->get_current_parameter_value(volume_param);
    EXPECT_FLOAT_EQ(value_at_end, 1.0f);
    
    // Test parameter override
    engine->override_parameter(volume_param, 0.8);
    EXPECT_TRUE(engine->is_parameter_overridden(volume_param));
    
    double overridden_value = engine->get_current_parameter_value(volume_param);
    EXPECT_FLOAT_EQ(overridden_value, 0.8f);
    
    // Release override
    engine->release_parameter_override(volume_param);
    EXPECT_FALSE(engine->is_parameter_overridden(volume_param));
}

// Test 16: AutomationEngine Lane Management
TEST_F(AutomationEngineTest, AutomationEngineLaneManagement) {
    // Create test lanes
    automation_data->create_lane(volume_param, 0.5);
    automation_data->create_lane(pan_param, 0.5);
    
    // Register targets
    AutomationTarget volume_target(AutomationTarget::TRACK_VOLUME, 1);
    AutomationTarget pan_target(AutomationTarget::TRACK_PAN, 1);
    
    engine->register_automation_target(volume_param, volume_target);
    engine->register_automation_target(pan_param, pan_target);
    
    // Test lane enable/disable
    EXPECT_TRUE(engine->is_lane_enabled(volume_param)); // Should be enabled by default
    
    auto disable_result = engine->disable_lane(volume_param);
    EXPECT_TRUE(disable_result.is_success());
    EXPECT_FALSE(engine->is_lane_enabled(volume_param));
    
    auto enable_result = engine->enable_lane(volume_param);
    EXPECT_TRUE(enable_result.is_success());
    EXPECT_TRUE(engine->is_lane_enabled(volume_param));
    
    // Test read mode
    EXPECT_FALSE(engine->is_lane_in_read_mode(volume_param));
    engine->set_lane_read_mode(volume_param, true);
    EXPECT_TRUE(engine->is_lane_in_read_mode(volume_param));
    
    // Test active lane count
    size_t active_count = engine->get_active_lane_count();
    EXPECT_EQ(active_count, 1); // Only pan lane is active (volume is in read mode)
}

// Test 17: AutomationCurveTemplates
TEST_F(AutomationTest, AutomationCurveTemplates) {
    // Test sine LFO generation
    auto sine_points = AutomationCurveTemplates::create_sine_lfo(0, 44100, 2.0, 0.4, 0.5);
    EXPECT_GT(sine_points.size(), 0);
    
    // Check that values are within expected range
    for (const auto& point : sine_points) {
        EXPECT_GE(point.value, 0.1); // 0.5 - 0.4 = 0.1 minimum
        EXPECT_LE(point.value, 0.9); // 0.5 + 0.4 = 0.9 maximum
    }
    
    // Test fade templates
    auto fade_in = AutomationCurveTemplates::create_exponential_fade_in(0, 44100);
    EXPECT_GT(fade_in.size(), 0);
    EXPECT_LT(fade_in[0].value, fade_in.back().value); // Should increase
    
    auto fade_out = AutomationCurveTemplates::create_exponential_fade_out(0, 44100);
    EXPECT_GT(fade_out.size(), 0);
    EXPECT_GT(fade_out[0].value, fade_out.back().value); // Should decrease
    
    // Test auto-pan
    auto auto_pan = AutomationCurveTemplates::create_auto_pan(0, 44100, 1.0); // 1Hz pan
    EXPECT_GT(auto_pan.size(), 0);
    
    // Check that pan values cycle
    bool found_left = false, found_right = false;
    for (const auto& point : auto_pan) {
        if (point.value < 0.25) found_left = true;
        if (point.value > 0.75) found_right = true;
    }
    EXPECT_TRUE(found_left);
    EXPECT_TRUE(found_right);
}

// Test 18: AutomationParameterMapper
TEST_F(AutomationTest, AutomationParameterMapper) {
    // Test track volume mapping (linear to dB)
    double db_min = AutomationParameterMapper::map_to_track_volume_db(0.0);
    double db_max = AutomationParameterMapper::map_to_track_volume_db(1.0);
    double db_mid = AutomationParameterMapper::map_to_track_volume_db(0.5);
    
    EXPECT_LT(db_min, db_mid);
    EXPECT_LT(db_mid, db_max);
    EXPECT_EQ(db_min, -60.0); // Should map to -60dB
    
    // Test round-trip conversion
    double normalized = AutomationParameterMapper::map_from_track_volume_db(db_mid);
    EXPECT_FLOAT_EQ(normalized, 0.5f);
    
    // Test pan mapping
    double pan_left = AutomationParameterMapper::map_to_track_pan_position(0.0);
    double pan_center = AutomationParameterMapper::map_to_track_pan_position(0.5);
    double pan_right = AutomationParameterMapper::map_to_track_pan_position(1.0);
    
    EXPECT_FLOAT_EQ(pan_left, -1.0f);
    EXPECT_FLOAT_EQ(pan_center, 0.0f);
    EXPECT_FLOAT_EQ(pan_right, 1.0f);
    
    // Test MIDI CC mapping
    uint8_t cc_min = AutomationParameterMapper::map_to_midi_cc(0.0);
    uint8_t cc_max = AutomationParameterMapper::map_to_midi_cc(1.0);
    uint8_t cc_mid = AutomationParameterMapper::map_to_midi_cc(0.5);
    
    EXPECT_EQ(cc_min, 0);
    EXPECT_EQ(cc_max, 127);
    EXPECT_EQ(cc_mid, 63); // Should be approximately half
    
    // Test frequency mapping (logarithmic)
    double freq_low = AutomationParameterMapper::map_to_frequency_hz(0.0, 20.0, 20000.0);
    double freq_high = AutomationParameterMapper::map_to_frequency_hz(1.0, 20.0, 20000.0);
    double freq_mid = AutomationParameterMapper::map_to_frequency_hz(0.5, 20.0, 20000.0);
    
    EXPECT_FLOAT_EQ(freq_low, 20.0f);
    EXPECT_FLOAT_EQ(freq_high, 20000.0f);
    EXPECT_GT(freq_mid, 100.0); // Should be geometric mean region
    EXPECT_LT(freq_mid, 1000.0);
}

// Test 19: Performance and Memory Tests
TEST_F(AutomationTest, AutomationPerformanceTest) {
    // Create many automation lanes with dense data
    const size_t num_lanes = 50;
    const size_t points_per_lane = 1000;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_lanes; ++i) {
        AutomationParameterId param_id(AutomationParameterId::VST_PARAMETER, i, 0, 0);
        auto lane_result = automation_data->create_lane(param_id, 0.5);
        ASSERT_TRUE(lane_result.is_success());
        
        auto lane = lane_result.get_value();
        
        // Add many points
        for (size_t j = 0; j < points_per_lane; ++j) {
            uint64_t time = j * 100; // Every 100 samples
            double value = 0.5 + 0.4 * std::sin(j * 0.1); // Sine wave
            lane->add_point(AutomationPoint(time, value));
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(automation_data->get_lane_count(), num_lanes);
    EXPECT_EQ(automation_data->get_total_point_count(), num_lanes * points_per_lane);
    EXPECT_LT(duration.count(), 1000); // Should complete in less than 1 second
    
    std::cout << "Created " << num_lanes << " lanes with " << points_per_lane 
              << " points each in " << duration.count() << "ms" << std::endl;
    
    // Test value lookup performance
    auto lookup_start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_lanes; ++i) {
        AutomationParameterId param_id(AutomationParameterId::VST_PARAMETER, i, 0, 0);
        auto lane = automation_data->get_lane(param_id);
        if (lane) {
            // Lookup values at various times
            for (uint64_t time = 0; time < 10000; time += 1000) {
                volatile double value = lane->get_value_at_time(time);
                (void)value; // Suppress unused variable warning
            }
        }
    }
    
    auto lookup_end = std::chrono::high_resolution_clock::now();
    auto lookup_duration = std::chrono::duration_cast<std::chrono::microseconds>(lookup_end - lookup_start);
    
    EXPECT_LT(lookup_duration.count(), 10000); // Should complete in less than 10ms
    
    std::cout << "Performed " << num_lanes * 10 << " value lookups in " 
              << lookup_duration.count() << "μs" << std::endl;
}

// Test 20: Integration Test - Complete Automation Workflow
TEST_F(AutomationTest, AutomationIntegrationTest) {
    // Create a complete automation workflow
    
    // 1. Create automation data and lanes
    auto volume_lane_result = automation_data->create_lane(volume_param, 0.8);
    ASSERT_TRUE(volume_lane_result.is_success());
    auto volume_lane = volume_lane_result.get_value();
    
    auto pan_lane_result = automation_data->create_lane(pan_param, 0.5);
    ASSERT_TRUE(pan_lane_result.is_success());
    auto pan_lane = pan_lane_result.get_value();
    
    // 2. Set up automation editor and draw curves
    auto editor = AutomationEditorFactory::create_standard_editor(automation_data);
    editor->set_current_lane(volume_lane);
    
    // Draw volume automation (fade in)
    auto fade_result = editor->create_fade_in(0, 44100, 1.0); // 1 second fade in
    EXPECT_TRUE(fade_result.is_success());
    
    // Switch to pan lane and draw pan automation
    editor->set_current_lane(pan_lane);
    auto pan_points = AutomationCurveTemplates::create_auto_pan(0, 88200, 0.5); // 2 seconds auto-pan
    editor->draw_curve_with_points(
        std::vector<std::pair<uint64_t, double>>(pan_points.begin(), pan_points.end())
    );
    
    // 3. Set up automation recorder
    auto recorder = std::make_unique<AutomationRecorder>(automation_data);
    
    // Add MIDI CC mappings
    HardwareControlMapping volume_mapping = AutomationRecorderFactory::create_volume_mapping(volume_param);
    HardwareControlMapping pan_mapping = AutomationRecorderFactory::create_pan_mapping(pan_param);
    
    recorder->add_control_mapping(volume_mapping);
    recorder->add_control_mapping(pan_mapping);
    
    // 4. Set up automation engine
    auto engine = AutomationEngineFactory::create_standard_engine(automation_data);
    
    AutomationTarget volume_target(AutomationTarget::TRACK_VOLUME, 1);
    AutomationTarget pan_target(AutomationTarget::TRACK_PAN, 1);
    
    engine->register_automation_target(volume_param, volume_target);
    engine->register_automation_target(pan_param, pan_target);
    
    // 5. Test playback
    engine->start_playback();
    
    // Simulate playback at different times
    std::vector<uint64_t> test_times = {0, 11025, 22050, 44100, 66150, 88200};
    
    for (uint64_t time : test_times) {
        engine->set_playback_position(time);
        
        double volume_value = engine->get_current_parameter_value(volume_param);
        double pan_value = engine->get_current_parameter_value(pan_param);
        
        // Volume should increase over time (fade in)
        if (time <= 44100) {
            EXPECT_GE(volume_value, 0.0);
            EXPECT_LE(volume_value, 1.0);
        }
        
        // Pan should oscillate
        EXPECT_GE(pan_value, 0.0);
        EXPECT_LE(pan_value, 1.0);
        
        std::cout << "Time: " << time << ", Volume: " << volume_value 
                  << ", Pan: " << pan_value << std::endl;
    }
    
    engine->stop_playback();
    
    // 6. Test manual override
    engine->override_parameter(volume_param, 0.9);
    double overridden_volume = engine->get_current_parameter_value(volume_param);
    EXPECT_FLOAT_EQ(overridden_volume, 0.9f);
    
    // 7. Performance check
    auto stats = engine->get_performance_stats();
    EXPECT_LT(stats.cpu_usage_percent, 50.0); // Should use less than 50% CPU
    
    std::cout << "Integration test completed successfully!" << std::endl;
    std::cout << "CPU Usage: " << stats.cpu_usage_percent << "%" << std::endl;
    std::cout << "Parameters processed: " << stats.parameters_processed << std::endl;
    std::cout << "Events sent: " << stats.automation_events_sent << std::endl;
}