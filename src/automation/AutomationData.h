#pragma once

#include "../core/result.h"
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <string>

namespace mixmind {

// Forward declarations
class Track;
class VSTiHost;

// Automation parameter identifier
struct AutomationParameterId {
    enum Type {
        TRACK_VOLUME,         // Track volume (0.0 - 2.0)
        TRACK_PAN,            // Track pan (-1.0 - 1.0)
        TRACK_MUTE,           // Track mute (0 or 1)
        TRACK_SOLO,           // Track solo (0 or 1)
        TRACK_SEND_LEVEL,     // Send level (0.0 - 1.0)
        TRACK_SEND_PAN,       // Send pan (-1.0 - 1.0)
        VST_PARAMETER,        // VST3 plugin parameter (0.0 - 1.0)
        MIDI_CC,              // MIDI CC value (0 - 127)
        CUSTOM                // Custom parameter
    };
    
    Type type = TRACK_VOLUME;
    uint32_t track_id = 0;             // Track identifier
    uint32_t parameter_index = 0;      // Parameter index (VST param ID, CC number, send index, etc.)
    uint32_t plugin_instance_id = 0;   // Plugin instance ID for VST parameters
    std::string custom_id = "";        // Custom parameter identifier
    
    AutomationParameterId() = default;
    AutomationParameterId(Type t, uint32_t track = 0, uint32_t param = 0, uint32_t plugin = 0)
        : type(t), track_id(track), parameter_index(param), plugin_instance_id(plugin) {}
    
    // Comparison operators for use as map key
    bool operator<(const AutomationParameterId& other) const;
    bool operator==(const AutomationParameterId& other) const;
    
    // Generate human-readable name
    std::string get_display_name() const;
};

// Automation curve interpolation types
enum class AutomationCurveType {
    LINEAR,         // Linear interpolation
    EXPONENTIAL,    // Exponential curve
    LOGARITHMIC,    // Logarithmic curve
    BEZIER,         // Cubic bezier curve
    SMOOTH,         // Smooth s-curve
    STEPPED         // No interpolation (stepped)
};

// Individual automation point
struct AutomationPoint {
    uint64_t time_samples;                      // Time position in samples
    double value;                               // Parameter value (normalized 0.0 - 1.0)
    AutomationCurveType curve_type = AutomationCurveType::LINEAR;
    bool selected = false;                      // Selected for editing
    
    // Bezier curve control points (for BEZIER curve type)
    double control_point_1 = 0.0;              // First control point (-1.0 to 1.0)
    double control_point_2 = 0.0;              // Second control point (-1.0 to 1.0)
    
    AutomationPoint() = default;
    AutomationPoint(uint64_t time, double val, AutomationCurveType curve = AutomationCurveType::LINEAR)
        : time_samples(time), value(val), curve_type(curve) {}
    
    // Get end time (same as time for points, used for consistency)
    uint64_t get_end_time() const { return time_samples; }
    
    // Check if point is at specific time (with tolerance)
    bool is_at_time(uint64_t time, uint64_t tolerance_samples = 100) const {
        return std::abs(static_cast<int64_t>(time_samples) - static_cast<int64_t>(time)) <= static_cast<int64_t>(tolerance_samples);
    }
};

// Automation lane containing all points for a parameter
class AutomationLane {
public:
    AutomationLane(const AutomationParameterId& param_id, double default_value = 0.0);
    ~AutomationLane() = default;
    
    // Parameter identification
    const AutomationParameterId& get_parameter_id() const { return m_parameter_id; }
    void set_parameter_id(const AutomationParameterId& param_id) { m_parameter_id = param_id; }
    
    std::string get_display_name() const { return m_parameter_id.get_display_name(); }
    
    // Default value when no automation points exist
    double get_default_value() const { return m_default_value; }
    void set_default_value(double value) { m_default_value = value; }
    
    // Point management
    Result<bool> add_point(const AutomationPoint& point);
    Result<bool> remove_point(size_t point_index);
    Result<bool> remove_point_at_time(uint64_t time_samples, uint64_t tolerance_samples = 100);
    Result<bool> remove_selected_points();
    
    // Point access
    const std::vector<AutomationPoint>& get_points() const { return m_points; }
    std::vector<AutomationPoint>& get_points_mutable() { return m_points; }
    size_t get_point_count() const { return m_points.size(); }
    
    AutomationPoint* find_point_at_time(uint64_t time_samples, uint64_t tolerance_samples = 100);
    std::vector<AutomationPoint*> get_points_in_range(uint64_t start_time, uint64_t end_time);
    std::vector<AutomationPoint*> get_selected_points();
    
    // Value interpolation
    double get_value_at_time(uint64_t time_samples) const;
    std::vector<std::pair<uint64_t, double>> get_interpolated_values(uint64_t start_time, uint64_t end_time, uint64_t resolution_samples = 256) const;
    
    // Selection
    void select_all_points();
    void deselect_all_points();
    void select_points_in_range(uint64_t start_time, uint64_t end_time);
    
    // Point editing
    Result<bool> move_selected_points(int64_t time_delta_samples, double value_delta);
    Result<bool> scale_selected_values(double scale_factor, double pivot_value = 0.5);
    Result<bool> set_selected_curve_type(AutomationCurveType curve_type);
    
    // Automation operations
    Result<bool> clear_all_points();
    Result<bool> clear_range(uint64_t start_time, uint64_t end_time);
    Result<bool> quantize_points_timing(uint64_t grid_samples);
    Result<bool> smooth_points(float strength = 0.5f);
    Result<bool> thin_points(double tolerance = 0.001); // Remove redundant points
    
    // Lane state
    bool is_enabled() const { return m_enabled; }
    void set_enabled(bool enabled) { m_enabled = enabled; }
    
    bool is_armed_for_recording() const { return m_recording_armed; }
    void set_armed_for_recording(bool armed) { m_recording_armed = armed; }
    
    // Visual properties
    uint32_t get_color() const { return m_color; }
    void set_color(uint32_t color) { m_color = color; }
    
    bool is_visible() const { return m_visible; }
    void set_visible(bool visible) { m_visible = visible; }
    
    // Time conversion helpers
    static uint64_t beats_to_samples(double beats, double bpm = 120.0, double sample_rate = 44100.0);
    static double samples_to_beats(uint64_t samples, double bpm = 120.0, double sample_rate = 44100.0);

private:
    AutomationParameterId m_parameter_id;
    std::vector<AutomationPoint> m_points;
    double m_default_value;
    
    bool m_enabled = true;
    bool m_recording_armed = false;
    bool m_visible = true;
    uint32_t m_color = 0xFF4080FF;
    
    // Helper methods
    void sort_points_by_time();
    double interpolate_value(const AutomationPoint& p1, const AutomationPoint& p2, uint64_t time) const;
    double apply_curve_interpolation(double t, AutomationCurveType curve_type, double cp1 = 0.0, double cp2 = 0.0) const;
    bool validate_point(const AutomationPoint& point) const;
};

// Automation data container for a project/session
class AutomationData {
public:
    AutomationData() = default;
    ~AutomationData() = default;
    
    // Lane management
    Result<std::shared_ptr<AutomationLane>> create_lane(const AutomationParameterId& param_id, double default_value = 0.0);
    Result<bool> remove_lane(const AutomationParameterId& param_id);
    std::shared_ptr<AutomationLane> get_lane(const AutomationParameterId& param_id);
    std::vector<std::shared_ptr<AutomationLane>> get_all_lanes() const;
    
    // Lane access by type
    std::vector<std::shared_ptr<AutomationLane>> get_track_lanes(uint32_t track_id) const;
    std::vector<std::shared_ptr<AutomationLane>> get_vst_parameter_lanes(uint32_t plugin_instance_id) const;
    std::vector<std::shared_ptr<AutomationLane>> get_midi_cc_lanes(uint32_t track_id) const;
    
    // Batch operations
    void clear_all_automation();
    void clear_automation_in_range(uint64_t start_time, uint64_t end_time);
    
    // Global settings
    bool is_automation_enabled() const { return m_automation_enabled; }
    void set_automation_enabled(bool enabled) { m_automation_enabled = enabled; }
    
    bool is_automation_visible() const { return m_automation_visible; }
    void set_automation_visible(bool visible) { m_automation_visible = visible; }
    
    // Statistics
    size_t get_total_point_count() const;
    size_t get_lane_count() const { return m_lanes.size(); }
    
    // Event callbacks for UI updates
    using AutomationEditCallback = std::function<void(const AutomationParameterId&)>;
    void set_edit_callback(AutomationEditCallback callback) { m_edit_callback = callback; }

private:
    std::map<AutomationParameterId, std::shared_ptr<AutomationLane>> m_lanes;
    
    bool m_automation_enabled = true;
    bool m_automation_visible = true;
    
    AutomationEditCallback m_edit_callback;
    
    void notify_edit_changed(const AutomationParameterId& param_id);
};

// Automation factory for creating common lane setups
class AutomationFactory {
public:
    // Create standard track automation lanes
    static std::vector<std::shared_ptr<AutomationLane>> create_track_automation(uint32_t track_id);
    
    // Create VST parameter automation lanes
    static std::vector<std::shared_ptr<AutomationLane>> create_vst_automation(uint32_t track_id, uint32_t plugin_instance_id, const std::vector<uint32_t>& parameter_indices);
    
    // Create MIDI CC automation lanes
    static std::vector<std::shared_ptr<AutomationLane>> create_midi_cc_automation(uint32_t track_id, const std::vector<uint32_t>& cc_numbers);
    
    // Create automation curves for common shapes
    static std::vector<AutomationPoint> create_linear_ramp(uint64_t start_time, uint64_t end_time, double start_value, double end_value);
    static std::vector<AutomationPoint> create_exponential_curve(uint64_t start_time, uint64_t end_time, double start_value, double end_value, double exponent = 2.0);
    static std::vector<AutomationPoint> create_sine_wave(uint64_t start_time, uint64_t duration, double frequency_hz, double amplitude, double offset = 0.5, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_lfo_curve(uint64_t start_time, uint64_t duration, double frequency_hz, double depth, double offset = 0.5, double sample_rate = 44100.0);
    
    // Create musical automation patterns
    static std::vector<AutomationPoint> create_volume_fade_in(uint64_t start_time, uint64_t duration, double target_level = 1.0);
    static std::vector<AutomationPoint> create_volume_fade_out(uint64_t start_time, uint64_t duration, double start_level = 1.0);
    static std::vector<AutomationPoint> create_filter_sweep(uint64_t start_time, uint64_t duration, bool open_to_close = true);
    static std::vector<AutomationPoint> create_tremolo_effect(uint64_t start_time, uint64_t duration, double rate_hz, double depth, double sample_rate = 44100.0);
};

// Utility functions for automation parameter mapping
class AutomationUtils {
public:
    // Parameter value conversion
    static double normalize_track_volume(double linear_volume);     // Convert 0.0-2.0 to 0.0-1.0
    static double denormalize_track_volume(double normalized);      // Convert 0.0-1.0 to 0.0-2.0
    
    static double normalize_track_pan(double pan_position);         // Convert -1.0-1.0 to 0.0-1.0
    static double denormalize_track_pan(double normalized);         // Convert 0.0-1.0 to -1.0-1.0
    
    static double normalize_midi_cc(uint8_t cc_value);              // Convert 0-127 to 0.0-1.0
    static uint8_t denormalize_midi_cc(double normalized);          // Convert 0.0-1.0 to 0-127
    
    // Value range validation
    static bool is_valid_normalized_value(double value);           // Check if value is in 0.0-1.0 range
    static double clamp_normalized_value(double value);            // Clamp to 0.0-1.0 range
    
    // Parameter ID utilities
    static AutomationParameterId create_track_volume_id(uint32_t track_id);
    static AutomationParameterId create_track_pan_id(uint32_t track_id);
    static AutomationParameterId create_vst_parameter_id(uint32_t track_id, uint32_t plugin_id, uint32_t param_index);
    static AutomationParameterId create_midi_cc_id(uint32_t track_id, uint32_t cc_number);
    
    // Generate unique colors for automation lanes
    static uint32_t generate_lane_color(const AutomationParameterId& param_id);
    
    // Time utilities
    static uint64_t snap_to_grid(uint64_t time_samples, uint64_t grid_size_samples);
    static std::vector<uint64_t> get_grid_points(uint64_t start_time, uint64_t end_time, uint64_t grid_size);
};

} // namespace mixmind