#include "AutomationRecorder.h"
#include <algorithm>
#include <fstream>
#include <chrono>
#include <cmath>

namespace mixmind {

AutomationRecorder::AutomationRecorder(std::shared_ptr<AutomationData> automation_data)
    : m_automation_data(automation_data) {
    
    // Start processing thread
    m_processing_thread = std::thread(&AutomationRecorder::processing_thread_loop, this);
}

AutomationRecorder::~AutomationRecorder() {
    stop_recording();
    
    // Stop processing thread
    m_should_stop_processing.store(true);
    if (m_processing_thread.joinable()) {
        m_processing_thread.join();
    }
}

Result<bool> AutomationRecorder::start_recording(RecordingMode mode) {
    if (m_is_recording.load()) {
        return Result<bool>::error("Already recording");
    }
    
    m_recording_mode = mode;
    
    // Clear previous values for change detection
    {
        std::lock_guard<std::mutex> lock(m_prev_values_mutex);
        m_previous_values.clear();
        m_last_record_times.clear();
    }
    
    // Clear event queue
    {
        std::lock_guard<std::mutex> lock(m_event_queue_mutex);
        std::queue<AutomationRecordEvent> empty;
        m_event_queue.swap(empty);
    }
    
    m_is_recording.store(true);
    
    if (m_start_callback) {
        m_start_callback();
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationRecorder::stop_recording() {
    if (!m_is_recording.load()) {
        return Result<bool>::error("Not currently recording");
    }
    
    m_is_recording.store(false);
    
    // Process remaining events in queue
    process_event_queue();
    
    if (m_stop_callback) {
        m_stop_callback();
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationRecorder::arm_parameter(const AutomationParameterId& param_id) {
    std::lock_guard<std::mutex> lock(m_armed_params_mutex);
    
    auto it = std::find(m_armed_parameters.begin(), m_armed_parameters.end(), param_id);
    if (it != m_armed_parameters.end()) {
        return Result<bool>::error("Parameter already armed");
    }
    
    m_armed_parameters.push_back(param_id);
    
    // Ensure automation lane exists
    if (!m_automation_data->get_lane(param_id)) {
        m_automation_data->create_lane(param_id);
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationRecorder::disarm_parameter(const AutomationParameterId& param_id) {
    std::lock_guard<std::mutex> lock(m_armed_params_mutex);
    
    auto it = std::find(m_armed_parameters.begin(), m_armed_parameters.end(), param_id);
    if (it == m_armed_parameters.end()) {
        return Result<bool>::error("Parameter not armed");
    }
    
    m_armed_parameters.erase(it);
    return Result<bool>::success(true);
}

void AutomationRecorder::arm_all_parameters() {
    std::lock_guard<std::mutex> lock(m_armed_params_mutex);
    
    m_armed_parameters.clear();
    auto all_lanes = m_automation_data->get_all_lanes();
    
    for (const auto& lane : all_lanes) {
        m_armed_parameters.push_back(lane->get_parameter_id());
    }
}

void AutomationRecorder::disarm_all_parameters() {
    std::lock_guard<std::mutex> lock(m_armed_params_mutex);
    m_armed_parameters.clear();
}

bool AutomationRecorder::is_parameter_armed(const AutomationParameterId& param_id) const {
    std::lock_guard<std::mutex> lock(m_armed_params_mutex);
    return std::find(m_armed_parameters.begin(), m_armed_parameters.end(), param_id) != m_armed_parameters.end();
}

std::vector<AutomationParameterId> AutomationRecorder::get_armed_parameters() const {
    std::lock_guard<std::mutex> lock(m_armed_params_mutex);
    return m_armed_parameters;
}

Result<bool> AutomationRecorder::add_control_mapping(const HardwareControlMapping& mapping) {
    std::lock_guard<std::mutex> lock(m_mappings_mutex);
    
    if (m_control_mappings.find(mapping.target_parameter) != m_control_mappings.end()) {
        return Result<bool>::error("Control mapping already exists for this parameter");
    }
    
    m_control_mappings[mapping.target_parameter] = mapping;
    return Result<bool>::success(true);
}

Result<bool> AutomationRecorder::remove_control_mapping(const AutomationParameterId& param_id) {
    std::lock_guard<std::mutex> lock(m_mappings_mutex);
    
    auto it = m_control_mappings.find(param_id);
    if (it == m_control_mappings.end()) {
        return Result<bool>::error("Control mapping not found");
    }
    
    m_control_mappings.erase(it);
    return Result<bool>::success(true);
}

Result<bool> AutomationRecorder::update_control_mapping(const AutomationParameterId& param_id, const HardwareControlMapping& mapping) {
    std::lock_guard<std::mutex> lock(m_mappings_mutex);
    
    auto it = m_control_mappings.find(param_id);
    if (it == m_control_mappings.end()) {
        return Result<bool>::error("Control mapping not found");
    }
    
    it->second = mapping;
    return Result<bool>::success(true);
}

std::vector<HardwareControlMapping> AutomationRecorder::get_all_mappings() const {
    std::lock_guard<std::mutex> lock(m_mappings_mutex);
    
    std::vector<HardwareControlMapping> mappings;
    for (const auto& [param_id, mapping] : m_control_mappings) {
        mappings.push_back(mapping);
    }
    
    return mappings;
}

HardwareControlMapping* AutomationRecorder::get_mapping(const AutomationParameterId& param_id) {
    std::lock_guard<std::mutex> lock(m_mappings_mutex);
    
    auto it = m_control_mappings.find(param_id);
    return (it != m_control_mappings.end()) ? &it->second : nullptr;
}

void AutomationRecorder::process_midi_cc(uint8_t channel, uint8_t cc_number, uint8_t value, uint64_t timestamp) {
    AutomationParameterId mapped_param = find_mapped_parameter(HardwareControlMapping::MIDI_CC, channel, cc_number);
    if (mapped_param.type == AutomationParameterId::TRACK_VOLUME) { // Use as "invalid" check
        return;
    }
    
    auto mapping = get_mapping(mapped_param);
    if (!mapping || !mapping->enabled) {
        return;
    }
    
    double normalized_input = static_cast<double>(value) / 127.0;
    double mapped_value = apply_control_mapping(*mapping, normalized_input);
    
    AutomationRecordEvent event(mapped_param, mapped_value, timestamp, static_cast<double>(value));
    queue_event(event);
}

void AutomationRecorder::process_midi_aftertouch(uint8_t channel, uint8_t pressure, uint64_t timestamp) {
    AutomationParameterId mapped_param = find_mapped_parameter(HardwareControlMapping::MIDI_AFTERTOUCH, channel, 0);
    if (mapped_param.type == AutomationParameterId::TRACK_VOLUME) { // Use as "invalid" check
        return;
    }
    
    auto mapping = get_mapping(mapped_param);
    if (!mapping || !mapping->enabled) {
        return;
    }
    
    double normalized_input = static_cast<double>(pressure) / 127.0;
    double mapped_value = apply_control_mapping(*mapping, normalized_input);
    
    AutomationRecordEvent event(mapped_param, mapped_value, timestamp, static_cast<double>(pressure));
    queue_event(event);
}

void AutomationRecorder::process_midi_pitch_bend(uint8_t channel, uint16_t value, uint64_t timestamp) {
    AutomationParameterId mapped_param = find_mapped_parameter(HardwareControlMapping::MIDI_PITCH_BEND, channel, 0);
    if (mapped_param.type == AutomationParameterId::TRACK_VOLUME) { // Use as "invalid" check
        return;
    }
    
    auto mapping = get_mapping(mapped_param);
    if (!mapping || !mapping->enabled) {
        return;
    }
    
    // Convert 14-bit pitch bend to normalized value (0.5 = center)
    double normalized_input = static_cast<double>(value) / 16383.0;
    double mapped_value = apply_control_mapping(*mapping, normalized_input);
    
    AutomationRecordEvent event(mapped_param, mapped_value, timestamp, static_cast<double>(value));
    queue_event(event);
}

void AutomationRecorder::record_parameter_change(const AutomationParameterId& param_id, double value, uint64_t timestamp, bool is_touch_start, bool is_touch_end) {
    AutomationRecordEvent event(param_id, value, timestamp, value);
    event.is_touch_start = is_touch_start;
    event.is_touch_end = is_touch_end;
    
    queue_event(event);
}

void AutomationRecorder::set_parameter_touch_state(const AutomationParameterId& param_id, bool touching) {
    std::lock_guard<std::mutex> lock(m_touch_state_mutex);
    
    bool was_touching = m_parameter_touch_states[param_id];
    m_parameter_touch_states[param_id] = touching;
    
    if (touching && !was_touching) {
        m_touch_start_times[param_id] = m_current_position.load();
    } else if (!touching && was_touching) {
        m_touch_start_times.erase(param_id);
    }
}

bool AutomationRecorder::is_parameter_touched(const AutomationParameterId& param_id) const {
    std::lock_guard<std::mutex> lock(m_touch_state_mutex);
    
    auto it = m_parameter_touch_states.find(param_id);
    return (it != m_parameter_touch_states.end()) ? it->second : false;
}

size_t AutomationRecorder::get_recorded_events_count() const {
    std::lock_guard<std::mutex> lock(m_event_queue_mutex);
    return m_event_queue.size();
}

size_t AutomationRecorder::get_active_mappings_count() const {
    std::lock_guard<std::mutex> lock(m_mappings_mutex);
    
    size_t active_count = 0;
    for (const auto& [param_id, mapping] : m_control_mappings) {
        if (mapping.enabled) {
            active_count++;
        }
    }
    
    return active_count;
}

void AutomationRecorder::processing_thread_loop() {
    const auto sleep_duration = std::chrono::milliseconds(1); // 1ms processing interval
    
    while (!m_should_stop_processing.load()) {
        process_event_queue();
        std::this_thread::sleep_for(sleep_duration);
    }
    
    // Final processing
    process_event_queue();
}

void AutomationRecorder::process_event_queue() {
    std::lock_guard<std::mutex> lock(m_event_queue_mutex);
    
    while (!m_event_queue.empty()) {
        AutomationRecordEvent event = m_event_queue.front();
        m_event_queue.pop();
        
        if (should_record_event(event)) {
            process_recording_event(event);
        }
    }
}

void AutomationRecorder::process_recording_event(const AutomationRecordEvent& event) {
    if (!m_automation_data) {
        return;
    }
    
    auto lane = m_automation_data->get_lane(event.parameter_id);
    if (!lane) {
        // Create lane if it doesn't exist
        auto result = m_automation_data->create_lane(event.parameter_id);
        if (!result.is_success()) {
            return;
        }
        lane = result.get_value();
    }
    
    // Apply recording mode logic
    switch (m_recording_mode) {
        case RecordingMode::LATCH:
            // Always record
            break;
            
        case RecordingMode::TOUCH:
            if (!is_parameter_touched(event.parameter_id)) {
                return; // Only record when touching
            }
            break;
            
        case RecordingMode::WRITE:
            // Clear existing automation in range if this is the first write
            if (event.is_touch_start || 
                (m_previous_values.find(event.parameter_id) == m_previous_values.end())) {
                uint64_t clear_start = event.time_samples;
                uint64_t clear_end = m_punch_out_time;
                if (clear_end == UINT64_MAX) {
                    clear_end = clear_start + 44100; // 1 second default
                }
                lane->clear_range(clear_start, clear_end);
            }
            break;
            
        case RecordingMode::TRIM:
            // Only record if there are existing points nearby
            if (!lane->find_point_at_time(event.time_samples, m_recording_resolution * 4)) {
                return;
            }
            break;
            
        case RecordingMode::READ:
            return; // No recording in read mode
    }
    
    // Create automation point
    uint64_t point_time = event.time_samples;
    
    // Apply auto-quantization
    if (m_auto_quantize && m_quantize_grid_size > 0) {
        point_time = AutomationUtils::snap_to_grid(point_time, m_quantize_grid_size);
    }
    
    AutomationPoint point(point_time, event.value);
    
    // Add point to lane
    auto result = lane->add_point(point);
    if (result.is_success()) {
        notify_parameter_recorded(event.parameter_id, event.value);
        
        // Apply auto-thinning
        if (m_auto_thin) {
            lane->thin_points(m_thin_tolerance);
        }
    }
    
    // Update previous value tracking
    {
        std::lock_guard<std::mutex> lock(m_prev_values_mutex);
        m_previous_values[event.parameter_id] = event.value;
        m_last_record_times[event.parameter_id] = event.time_samples;
    }
}

bool AutomationRecorder::should_record_event(const AutomationRecordEvent& event) const {
    if (!m_is_recording.load()) {
        return false;
    }
    
    // Check if parameter is armed
    if (!is_parameter_armed(event.parameter_id)) {
        return false;
    }
    
    // Check time range
    if (!is_in_recording_time_range(event.time_samples)) {
        return false;
    }
    
    // Check minimum change threshold
    {
        std::lock_guard<std::mutex> lock(m_prev_values_mutex);
        auto prev_it = m_previous_values.find(event.parameter_id);
        if (prev_it != m_previous_values.end()) {
            double change = std::abs(event.value - prev_it->second);
            if (change < m_min_change_threshold) {
                return false; // Change too small
            }
        }
        
        // Check minimum time resolution
        auto time_it = m_last_record_times.find(event.parameter_id);
        if (time_it != m_last_record_times.end()) {
            uint64_t time_delta = event.time_samples - time_it->second;
            if (time_delta < m_recording_resolution) {
                return false; // Too soon since last recording
            }
        }
    }
    
    return true;
}

bool AutomationRecorder::is_in_recording_time_range(uint64_t time_samples) const {
    if (time_samples < m_punch_in_time) {
        return false;
    }
    
    if (m_punch_out_time != UINT64_MAX && time_samples > m_punch_out_time) {
        return false;
    }
    
    return true;
}

double AutomationRecorder::apply_control_mapping(const HardwareControlMapping& mapping, double input_value) const {
    // Apply deadzone
    if (std::abs(input_value - 0.5) < mapping.deadzone / 2.0) {
        input_value = 0.5; // Center value
    }
    
    // Apply inversion
    if (mapping.invert) {
        input_value = 1.0 - input_value;
    }
    
    // Apply sensitivity
    double centered = (input_value - 0.5) * mapping.sensitivity;
    input_value = std::clamp(0.5 + centered, 0.0, 1.0);
    
    // Apply range mapping
    double mapped_value = mapping.min_value + (mapping.max_value - mapping.min_value) * input_value;
    
    return std::clamp(mapped_value, 0.0, 1.0);
}

void AutomationRecorder::queue_event(const AutomationRecordEvent& event) {
    std::lock_guard<std::mutex> lock(m_event_queue_mutex);
    m_event_queue.push(event);
}

AutomationParameterId AutomationRecorder::find_mapped_parameter(HardwareControlMapping::ControlType type, uint8_t channel, uint8_t controller) const {
    std::lock_guard<std::mutex> lock(m_mappings_mutex);
    
    for (const auto& [param_id, mapping] : m_control_mappings) {
        if (mapping.control_type == type &&
            mapping.midi_channel == channel &&
            (type == HardwareControlMapping::MIDI_CC ? mapping.midi_cc_number == controller : true)) {
            return param_id;
        }
    }
    
    // Return invalid parameter ID if not found
    return AutomationParameterId();
}

void AutomationRecorder::notify_parameter_recorded(const AutomationParameterId& param_id, double value) {
    if (m_recorded_callback) {
        m_recorded_callback(param_id, value);
    }
}

// AutomationRecorderFactory implementations
std::unique_ptr<AutomationRecorder> AutomationRecorderFactory::create_standard_recorder(std::shared_ptr<AutomationData> automation_data) {
    auto recorder = std::make_unique<AutomationRecorder>(automation_data);
    
    // Set up standard MIDI CC mappings for common parameters
    // Note: These would typically be configured based on available tracks/plugins
    
    // Example track volume mapping (CC 7)
    AutomationParameterId volume_param(AutomationParameterId::TRACK_VOLUME, 1);
    recorder->add_control_mapping(create_volume_mapping(volume_param));
    
    // Example track pan mapping (CC 10)
    AutomationParameterId pan_param(AutomationParameterId::TRACK_PAN, 1);
    recorder->add_control_mapping(create_pan_mapping(pan_param));
    
    // Mod wheel mapping (CC 1)
    AutomationParameterId mod_param(AutomationParameterId::MIDI_CC, 1, 1);
    recorder->add_control_mapping(create_mod_wheel_mapping(mod_param));
    
    return recorder;
}

std::unique_ptr<AutomationRecorder> AutomationRecorderFactory::create_performance_recorder(std::shared_ptr<AutomationData> automation_data) {
    auto recorder = std::make_unique<AutomationRecorder>(automation_data);
    
    // Set up performance-oriented mappings
    recorder->set_recording_mode(RecordingMode::TOUCH);
    recorder->set_auto_thin_enabled(true);
    recorder->set_thin_tolerance(0.01);
    
    // Expressive control mappings
    AutomationParameterId mod_param(AutomationParameterId::MIDI_CC, 1, 1);
    recorder->add_control_mapping(create_mod_wheel_mapping(mod_param));
    
    AutomationParameterId expr_param(AutomationParameterId::MIDI_CC, 1, 11);
    recorder->add_control_mapping(create_expression_mapping(expr_param));
    
    return recorder;
}

HardwareControlMapping AutomationRecorderFactory::create_mod_wheel_mapping(const AutomationParameterId& target) {
    HardwareControlMapping mapping(HardwareControlMapping::MIDI_CC, 0, 1, target);
    mapping.name = "Mod Wheel";
    mapping.sensitivity = 1.0;
    mapping.deadzone = 0.005;
    return mapping;
}

HardwareControlMapping AutomationRecorderFactory::create_expression_mapping(const AutomationParameterId& target) {
    HardwareControlMapping mapping(HardwareControlMapping::MIDI_CC, 0, 11, target);
    mapping.name = "Expression";
    mapping.sensitivity = 1.0;
    mapping.deadzone = 0.01;
    return mapping;
}

HardwareControlMapping AutomationRecorderFactory::create_volume_mapping(const AutomationParameterId& target) {
    HardwareControlMapping mapping(HardwareControlMapping::MIDI_CC, 0, 7, target);
    mapping.name = "Volume";
    mapping.sensitivity = 0.8; // Slightly less sensitive for volume
    mapping.deadzone = 0.02;
    return mapping;
}

HardwareControlMapping AutomationRecorderFactory::create_pan_mapping(const AutomationParameterId& target) {
    HardwareControlMapping mapping(HardwareControlMapping::MIDI_CC, 0, 10, target);
    mapping.name = "Pan";
    mapping.sensitivity = 0.7;
    mapping.deadzone = 0.03; // Larger deadzone for pan centering
    return mapping;
}

HardwareControlMapping AutomationRecorderFactory::create_pitch_bend_mapping(const AutomationParameterId& target) {
    HardwareControlMapping mapping(HardwareControlMapping::MIDI_PITCH_BEND, 0, 0, target);
    mapping.name = "Pitch Bend";
    mapping.sensitivity = 1.0;
    mapping.deadzone = 0.01;
    return mapping;
}

} // namespace mixmind