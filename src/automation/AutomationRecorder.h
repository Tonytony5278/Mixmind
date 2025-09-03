#pragma once

#include "AutomationData.h"
#include "../core/result.h"
#include "../core/audio_types.h"
#include <memory>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>
#include <queue>

namespace mixmind {

// Forward declarations
class MIDIProcessor;
class VSTiHost;

// Automation recording mode
enum class RecordingMode {
    LATCH,          // Record until stopped
    TOUCH,          // Record while touching parameter
    WRITE,          // Overwrite existing automation
    TRIM,           // Only record changes to existing points
    READ            // Read automation only (no recording)
};

// Automation recording event
struct AutomationRecordEvent {
    AutomationParameterId parameter_id;
    double value;               // Normalized value (0.0 - 1.0)
    uint64_t time_samples;      // Timestamp in samples
    double raw_value;           // Original value before normalization
    bool is_touch_start = false; // Touch mode start
    bool is_touch_end = false;   // Touch mode end
    
    AutomationRecordEvent() = default;
    AutomationRecordEvent(const AutomationParameterId& param_id, double val, uint64_t time, double raw = 0.0)
        : parameter_id(param_id), value(val), time_samples(time), raw_value(raw) {}
};

// Hardware control mapping for automation recording
struct HardwareControlMapping {
    enum ControlType {
        MIDI_CC,            // MIDI CC controller
        MIDI_AFTERTOUCH,    // Channel aftertouch  
        MIDI_PITCH_BEND,    // Pitch bend wheel
        AUDIO_INTERFACE,    // Audio interface control (if supported)
        MOUSE_CONTROL,      // Mouse/touch control
        CUSTOM              // Custom control source
    };
    
    ControlType control_type = MIDI_CC;
    uint8_t midi_channel = 0;       // MIDI channel (0-15)
    uint8_t midi_cc_number = 0;     // MIDI CC number (0-127)
    AutomationParameterId target_parameter;
    
    // Control curve settings
    bool invert = false;            // Invert control direction
    double sensitivity = 1.0;       // Sensitivity multiplier
    double deadzone = 0.01;         // Deadzone for jitter reduction
    
    // Value range mapping
    double min_value = 0.0;         // Minimum mapped value
    double max_value = 1.0;         // Maximum mapped value
    
    std::string name = "Control";   // Human-readable name
    bool enabled = true;            // Control mapping enabled
    
    HardwareControlMapping() = default;
    HardwareControlMapping(ControlType type, uint8_t channel, uint8_t cc, const AutomationParameterId& target)
        : control_type(type), midi_channel(channel), midi_cc_number(cc), target_parameter(target) {}
};

// Automation recorder for real-time parameter capture
class AutomationRecorder {
public:
    AutomationRecorder(std::shared_ptr<AutomationData> automation_data);
    ~AutomationRecorder();
    
    // Recording control
    Result<bool> start_recording(RecordingMode mode = RecordingMode::LATCH);
    Result<bool> stop_recording();
    bool is_recording() const { return m_is_recording.load(); }
    
    RecordingMode get_recording_mode() const { return m_recording_mode; }
    void set_recording_mode(RecordingMode mode) { m_recording_mode = mode; }
    
    // Parameter arming for recording
    Result<bool> arm_parameter(const AutomationParameterId& param_id);
    Result<bool> disarm_parameter(const AutomationParameterId& param_id);
    void arm_all_parameters();
    void disarm_all_parameters();
    
    bool is_parameter_armed(const AutomationParameterId& param_id) const;
    std::vector<AutomationParameterId> get_armed_parameters() const;
    
    // Hardware control mapping
    Result<bool> add_control_mapping(const HardwareControlMapping& mapping);
    Result<bool> remove_control_mapping(const AutomationParameterId& param_id);
    Result<bool> update_control_mapping(const AutomationParameterId& param_id, const HardwareControlMapping& mapping);
    
    std::vector<HardwareControlMapping> get_all_mappings() const;
    HardwareControlMapping* get_mapping(const AutomationParameterId& param_id);
    
    // MIDI input processing
    void process_midi_cc(uint8_t channel, uint8_t cc_number, uint8_t value, uint64_t timestamp);
    void process_midi_aftertouch(uint8_t channel, uint8_t pressure, uint64_t timestamp);
    void process_midi_pitch_bend(uint8_t channel, uint16_t value, uint64_t timestamp);
    
    // Direct parameter input (for mouse/UI control)
    void record_parameter_change(const AutomationParameterId& param_id, double value, uint64_t timestamp, bool is_touch_start = false, bool is_touch_end = false);
    
    // Touch mode support
    void set_parameter_touch_state(const AutomationParameterId& param_id, bool touching);
    bool is_parameter_touched(const AutomationParameterId& param_id) const;
    
    // Recording settings
    void set_punch_in_time(uint64_t time_samples) { m_punch_in_time = time_samples; }
    void set_punch_out_time(uint64_t time_samples) { m_punch_out_time = time_samples; }
    uint64_t get_punch_in_time() const { return m_punch_in_time; }
    uint64_t get_punch_out_time() const { return m_punch_out_time; }
    
    void set_pre_roll_samples(uint64_t samples) { m_pre_roll_samples = samples; }
    uint64_t get_pre_roll_samples() const { return m_pre_roll_samples; }
    
    // Recording filters
    void set_minimum_change_threshold(double threshold) { m_min_change_threshold = threshold; }
    double get_minimum_change_threshold() const { return m_min_change_threshold; }
    
    void set_recording_resolution(uint64_t samples) { m_recording_resolution = samples; }
    uint64_t get_recording_resolution() const { return m_recording_resolution; }
    
    // Auto-quantization during recording
    void set_auto_quantize_enabled(bool enabled) { m_auto_quantize = enabled; }
    bool is_auto_quantize_enabled() const { return m_auto_quantize; }
    
    void set_quantize_grid_size(uint64_t grid_samples) { m_quantize_grid_size = grid_samples; }
    uint64_t get_quantize_grid_size() const { return m_quantize_grid_size; }
    
    // Automation thinning during recording
    void set_auto_thin_enabled(bool enabled) { m_auto_thin = enabled; }
    bool is_auto_thin_enabled() const { return m_auto_thin; }
    
    void set_thin_tolerance(double tolerance) { m_thin_tolerance = tolerance; }
    double get_thin_tolerance() const { return m_thin_tolerance; }
    
    // Transport integration
    void set_current_playback_position(uint64_t position_samples) { m_current_position = position_samples; }
    uint64_t get_current_playback_position() const { return m_current_position; }
    
    void set_loop_range(uint64_t start_samples, uint64_t end_samples) {
        m_loop_start = start_samples;
        m_loop_end = end_samples;
    }
    
    // Event callbacks
    using RecordingStartCallback = std::function<void()>;
    using RecordingStopCallback = std::function<void()>;
    using ParameterRecordedCallback = std::function<void(const AutomationParameterId&, double)>;
    
    void set_recording_start_callback(RecordingStartCallback callback) { m_start_callback = callback; }
    void set_recording_stop_callback(RecordingStopCallback callback) { m_stop_callback = callback; }
    void set_parameter_recorded_callback(ParameterRecordedCallback callback) { m_recorded_callback = callback; }
    
    // Statistics
    size_t get_recorded_events_count() const;
    size_t get_active_mappings_count() const;
    
    // Save/load control mappings
    Result<bool> save_control_mappings(const std::string& filename) const;
    Result<bool> load_control_mappings(const std::string& filename);
    
private:
    std::shared_ptr<AutomationData> m_automation_data;
    
    // Recording state
    std::atomic<bool> m_is_recording{false};
    RecordingMode m_recording_mode = RecordingMode::LATCH;
    
    // Armed parameters for recording
    mutable std::mutex m_armed_params_mutex;
    std::vector<AutomationParameterId> m_armed_parameters;
    
    // Hardware control mappings
    mutable std::mutex m_mappings_mutex;
    std::map<AutomationParameterId, HardwareControlMapping> m_control_mappings;
    
    // Touch state tracking
    mutable std::mutex m_touch_state_mutex;
    std::map<AutomationParameterId, bool> m_parameter_touch_states;
    std::map<AutomationParameterId, uint64_t> m_touch_start_times;
    
    // Recording event queue (thread-safe)
    mutable std::mutex m_event_queue_mutex;
    std::queue<AutomationRecordEvent> m_event_queue;
    
    // Processing thread
    std::thread m_processing_thread;
    std::atomic<bool> m_should_stop_processing{false};
    
    // Recording settings
    uint64_t m_punch_in_time = 0;
    uint64_t m_punch_out_time = UINT64_MAX;
    uint64_t m_pre_roll_samples = 0;
    double m_min_change_threshold = 0.001;
    uint64_t m_recording_resolution = 256;  // Minimum samples between points
    
    // Auto-processing settings
    bool m_auto_quantize = false;
    uint64_t m_quantize_grid_size = 1024;
    bool m_auto_thin = true;
    double m_thin_tolerance = 0.005;
    
    // Transport state
    std::atomic<uint64_t> m_current_position{0};
    uint64_t m_loop_start = 0;
    uint64_t m_loop_end = UINT64_MAX;
    
    // Previous values for change detection
    mutable std::mutex m_prev_values_mutex;
    std::map<AutomationParameterId, double> m_previous_values;
    std::map<AutomationParameterId, uint64_t> m_last_record_times;
    
    // Event callbacks
    RecordingStartCallback m_start_callback;
    RecordingStopCallback m_stop_callback;
    ParameterRecordedCallback m_recorded_callback;
    
    // Processing methods
    void processing_thread_loop();
    void process_event_queue();
    void process_recording_event(const AutomationRecordEvent& event);
    
    bool should_record_event(const AutomationRecordEvent& event) const;
    bool is_in_recording_time_range(uint64_t time_samples) const;
    
    double apply_control_mapping(const HardwareControlMapping& mapping, double input_value) const;
    void queue_event(const AutomationRecordEvent& event);
    
    // Helper methods
    AutomationParameterId find_mapped_parameter(HardwareControlMapping::ControlType type, uint8_t channel, uint8_t controller) const;
    void notify_parameter_recorded(const AutomationParameterId& param_id, double value);
};

// Automation recorder factory for common setups
class AutomationRecorderFactory {
public:
    // Create recorder with standard MIDI CC mappings
    static std::unique_ptr<AutomationRecorder> create_standard_recorder(std::shared_ptr<AutomationData> automation_data);
    
    // Create recorder with performance control mappings (mod wheel, aftertouch, etc.)
    static std::unique_ptr<AutomationRecorder> create_performance_recorder(std::shared_ptr<AutomationData> automation_data);
    
    // Create recorder for mixing console automation
    static std::unique_ptr<AutomationRecorder> create_mixing_recorder(std::shared_ptr<AutomationData> automation_data);
    
    // Standard MIDI CC mapping presets
    static HardwareControlMapping create_mod_wheel_mapping(const AutomationParameterId& target);
    static HardwareControlMapping create_expression_mapping(const AutomationParameterId& target);
    static HardwareControlMapping create_volume_mapping(const AutomationParameterId& target);
    static HardwareControlMapping create_pan_mapping(const AutomationParameterId& target);
    static HardwareControlMapping create_pitch_bend_mapping(const AutomationParameterId& target);
};

} // namespace mixmind