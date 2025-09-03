#pragma once

#include "AutomationData.h"
#include "../core/result.h"
#include <memory>
#include <vector>
#include <functional>

namespace mixmind {

// Automation editing modes
enum class AutomationEditMode {
    DRAW,           // Draw new automation curves
    ERASE,          // Erase automation points
    SELECT,         // Select points for editing
    TRIM,           // Trim automation curve start/end
    SMOOTH,         // Smooth automation curves
    SCALE,          // Scale automation values
    MOVE            // Move automation points
};

// Automation editor tool for interactive curve editing
class AutomationEditor {
public:
    AutomationEditor(std::shared_ptr<AutomationData> automation_data);
    ~AutomationEditor() = default;
    
    // Automation data access
    void set_automation_data(std::shared_ptr<AutomationData> data) { m_automation_data = data; }
    std::shared_ptr<AutomationData> get_automation_data() const { return m_automation_data; }
    
    // Current lane selection
    void set_current_lane(std::shared_ptr<AutomationLane> lane) { m_current_lane = lane; }
    std::shared_ptr<AutomationLane> get_current_lane() const { return m_current_lane; }
    
    // Editing mode
    void set_edit_mode(AutomationEditMode mode) { m_edit_mode = mode; }
    AutomationEditMode get_edit_mode() const { return m_edit_mode; }
    
    // Drawing operations
    Result<bool> draw_point_at_time(uint64_t time_samples, double value);
    Result<bool> draw_line_segment(uint64_t start_time, uint64_t end_time, double start_value, double end_value, AutomationCurveType curve_type = AutomationCurveType::LINEAR);
    Result<bool> draw_curve_with_points(const std::vector<std::pair<uint64_t, double>>& points, AutomationCurveType curve_type = AutomationCurveType::LINEAR);
    
    // Curve shape drawing
    Result<bool> draw_ramp(uint64_t start_time, uint64_t end_time, double start_value, double end_value);
    Result<bool> draw_exponential_curve(uint64_t start_time, uint64_t end_time, double start_value, double end_value, double exponent = 2.0);
    Result<bool> draw_sine_wave(uint64_t start_time, uint64_t duration, double frequency_hz, double amplitude, double offset = 0.5);
    Result<bool> draw_lfo_pattern(uint64_t start_time, uint64_t duration, double frequency_hz, double depth, double offset = 0.5);
    
    // Erasing operations
    Result<bool> erase_point_at_time(uint64_t time_samples, uint64_t tolerance_samples = 1000);
    Result<bool> erase_points_in_range(uint64_t start_time, uint64_t end_time);
    Result<bool> erase_selected_points();
    
    // Selection operations
    Result<bool> select_point_at_time(uint64_t time_samples, uint64_t tolerance_samples = 1000, bool add_to_selection = false);
    Result<bool> select_points_in_range(uint64_t start_time, uint64_t end_time, bool add_to_selection = false);
    Result<bool> select_all_points();
    Result<bool> deselect_all_points();
    Result<bool> invert_selection();
    
    // Point editing operations
    Result<bool> move_selected_points(int64_t time_delta_samples, double value_delta);
    Result<bool> scale_selected_values(double scale_factor, double pivot_value = 0.5);
    Result<bool> set_selected_curve_type(AutomationCurveType curve_type);
    Result<bool> adjust_selected_bezier_handles(double control_point_1_delta, double control_point_2_delta);
    
    // Curve operations
    Result<bool> smooth_selected_points(float strength = 0.5f);
    Result<bool> quantize_selected_points(uint64_t grid_size_samples);
    Result<bool> thin_selected_points(double tolerance = 0.005);
    
    // Advanced curve editing
    Result<bool> insert_point_on_curve(uint64_t time_samples);
    Result<bool> split_curve_at_time(uint64_t time_samples);
    Result<bool> join_curves_at_selection();
    
    // Value transformation
    Result<bool> normalize_selected_values();                    // Scale to 0.0-1.0 range
    Result<bool> invert_selected_values();                       // Flip values (1.0 - value)
    Result<bool> offset_selected_values(double offset);          // Add offset to values
    Result<bool> compress_selected_values(double ratio);         // Compress dynamic range
    Result<bool> expand_selected_values(double ratio);           // Expand dynamic range
    
    // Musical operations
    Result<bool> reverse_selected_curve();
    Result<bool> duplicate_selected_curve(uint64_t time_offset);
    Result<bool> mirror_selected_curve(bool horizontal = false, bool vertical = true);
    
    // Clipboard operations
    Result<bool> copy_selected_points();
    Result<bool> cut_selected_points();
    Result<bool> paste_points_at_time(uint64_t time_samples);
    
    // Automation shapes and patterns
    Result<bool> create_fade_in(uint64_t start_time, uint64_t duration, double target_value = 1.0);
    Result<bool> create_fade_out(uint64_t start_time, uint64_t duration, double start_value = 1.0);
    Result<bool> create_tremolo(uint64_t start_time, uint64_t duration, double rate_hz, double depth);
    Result<bool> create_filter_sweep(uint64_t start_time, uint64_t duration, bool low_to_high = true);
    
    // Grid and snapping
    void set_snap_enabled(bool enabled) { m_snap_enabled = enabled; }
    bool is_snap_enabled() const { return m_snap_enabled; }
    
    void set_snap_grid_size(uint64_t grid_size) { m_snap_grid_size = grid_size; }
    uint64_t get_snap_grid_size() const { return m_snap_grid_size; }
    
    uint64_t snap_time_to_grid(uint64_t time_samples) const;
    
    // Value constraints
    void set_value_constraints(double min_value, double max_value) {
        m_min_value = std::clamp(min_value, 0.0, 1.0);
        m_max_value = std::clamp(max_value, 0.0, 1.0);
        if (m_min_value > m_max_value) std::swap(m_min_value, m_max_value);
    }
    
    double get_min_value() const { return m_min_value; }
    double get_max_value() const { return m_max_value; }
    double constrain_value(double value) const;
    
    // Drawing resolution
    void set_drawing_resolution(uint64_t resolution_samples) { m_drawing_resolution = resolution_samples; }
    uint64_t get_drawing_resolution() const { return m_drawing_resolution; }
    
    // Bezier curve editing
    struct BezierHandle {
        uint64_t time_samples;
        double value;
        double control_point_1;
        double control_point_2;
        bool selected = false;
        
        BezierHandle(uint64_t time, double val, double cp1 = 0.0, double cp2 = 0.0)
            : time_samples(time), value(val), control_point_1(cp1), control_point_2(cp2) {}
    };
    
    std::vector<BezierHandle> get_bezier_handles_for_selection() const;
    Result<bool> update_bezier_handle(const BezierHandle& handle);
    
    // Curve analysis
    struct CurveAnalysis {
        double min_value;
        double max_value;
        double average_value;
        double rms_value;
        size_t point_count;
        uint64_t duration_samples;
        double curve_length;        // Approximate curve length
        double smoothness_factor;   // 0.0 = very jagged, 1.0 = very smooth
    };
    
    CurveAnalysis analyze_selected_curve() const;
    CurveAnalysis analyze_entire_lane() const;
    
    // Undo/Redo support
    void save_state_snapshot();
    Result<bool> undo_last_operation();
    Result<bool> redo_last_operation();
    void clear_undo_history();
    
    // Event callbacks
    using EditCallback = std::function<void()>;
    void set_edit_callback(EditCallback callback) { m_edit_callback = callback; }
    
    // Time conversion utilities
    static uint64_t beats_to_samples(double beats, double bpm = 120.0, double sample_rate = 44100.0);
    static double samples_to_beats(uint64_t samples, double bpm = 120.0, double sample_rate = 44100.0);
    
private:
    std::shared_ptr<AutomationData> m_automation_data;
    std::shared_ptr<AutomationLane> m_current_lane;
    AutomationEditMode m_edit_mode = AutomationEditMode::DRAW;
    
    // Editing settings
    bool m_snap_enabled = true;
    uint64_t m_snap_grid_size = 1024;    // Default to ~23ms at 44.1kHz
    double m_min_value = 0.0;
    double m_max_value = 1.0;
    uint64_t m_drawing_resolution = 256; // Minimum samples between drawn points
    
    // Clipboard
    std::vector<AutomationPoint> m_clipboard_points;
    
    // Undo/Redo state management
    struct StateSnapshot {
        std::vector<AutomationPoint> points;
        AutomationParameterId parameter_id;
    };
    
    std::vector<StateSnapshot> m_undo_stack;
    std::vector<StateSnapshot> m_redo_stack;
    static constexpr size_t MAX_UNDO_STATES = 50;
    
    EditCallback m_edit_callback;
    
    // Helper methods
    void notify_edit_changed();
    StateSnapshot create_state_snapshot() const;
    void restore_state_snapshot(const StateSnapshot& snapshot);
    
    std::vector<AutomationPoint> generate_curve_points(uint64_t start_time, uint64_t end_time, 
                                                       std::function<double(double)> curve_function) const;
    
    double calculate_curve_smoothness(const std::vector<AutomationPoint>& points) const;
    double calculate_curve_length(const std::vector<AutomationPoint>& points) const;
};

// Factory for creating automation editing presets
class AutomationEditorFactory {
public:
    // Create editor with standard settings
    static std::unique_ptr<AutomationEditor> create_standard_editor(std::shared_ptr<AutomationData> automation_data);
    
    // Create editor for precise editing (fine grid, high resolution)
    static std::unique_ptr<AutomationEditor> create_precision_editor(std::shared_ptr<AutomationData> automation_data);
    
    // Create editor for musical editing (musical grid, beat-based snapping)
    static std::unique_ptr<AutomationEditor> create_musical_editor(std::shared_ptr<AutomationData> automation_data);
    
    // Create editor for performance automation (optimized for real-time recording)
    static std::unique_ptr<AutomationEditor> create_performance_editor(std::shared_ptr<AutomationData> automation_data);
};

// Automation curve templates for common patterns
class AutomationCurveTemplates {
public:
    // Volume automation templates
    static std::vector<AutomationPoint> create_linear_fade_in(uint64_t start_time, uint64_t duration, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_linear_fade_out(uint64_t start_time, uint64_t duration, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_exponential_fade_in(uint64_t start_time, uint64_t duration, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_exponential_fade_out(uint64_t start_time, uint64_t duration, double sample_rate = 44100.0);
    
    // Filter automation templates
    static std::vector<AutomationPoint> create_filter_sweep_up(uint64_t start_time, uint64_t duration, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_filter_sweep_down(uint64_t start_time, uint64_t duration, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_filter_wobble(uint64_t start_time, uint64_t duration, double rate_hz, double sample_rate = 44100.0);
    
    // Pan automation templates
    static std::vector<AutomationPoint> create_auto_pan(uint64_t start_time, uint64_t duration, double rate_hz, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_pan_sweep_left_to_right(uint64_t start_time, uint64_t duration, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_pan_sweep_right_to_left(uint64_t start_time, uint64_t duration, double sample_rate = 44100.0);
    
    // Modulation templates (LFO-style)
    static std::vector<AutomationPoint> create_sine_lfo(uint64_t start_time, uint64_t duration, double frequency_hz, double depth, double offset = 0.5, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_triangle_lfo(uint64_t start_time, uint64_t duration, double frequency_hz, double depth, double offset = 0.5, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_sawtooth_lfo(uint64_t start_time, uint64_t duration, double frequency_hz, double depth, double offset = 0.5, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_square_lfo(uint64_t start_time, uint64_t duration, double frequency_hz, double depth, double offset = 0.5, double sample_rate = 44100.0);
    
    // Musical automation templates
    static std::vector<AutomationPoint> create_gate_pattern(uint64_t start_time, uint64_t duration, const std::vector<bool>& pattern, double bpm = 120.0, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_stutter_pattern(uint64_t start_time, uint64_t duration, double stutter_rate_hz, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_build_up(uint64_t start_time, uint64_t duration, AutomationCurveType curve_type = AutomationCurveType::EXPONENTIAL, double sample_rate = 44100.0);
    static std::vector<AutomationPoint> create_drop_down(uint64_t start_time, uint64_t duration, AutomationCurveType curve_type = AutomationCurveType::EXPONENTIAL, double sample_rate = 44100.0);
};

} // namespace mixmind