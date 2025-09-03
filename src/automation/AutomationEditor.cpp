#include "AutomationEditor.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace mixmind {

AutomationEditor::AutomationEditor(std::shared_ptr<AutomationData> automation_data)
    : m_automation_data(automation_data) {
}

Result<bool> AutomationEditor::draw_point_at_time(uint64_t time_samples, double value) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    // Apply snapping
    uint64_t snapped_time = m_snap_enabled ? snap_time_to_grid(time_samples) : time_samples;
    
    // Constrain value
    double constrained_value = constrain_value(value);
    
    AutomationPoint point(snapped_time, constrained_value);
    auto result = m_current_lane->add_point(point);
    
    if (result.is_success()) {
        notify_edit_changed();
    }
    
    return result;
}

Result<bool> AutomationEditor::draw_line_segment(uint64_t start_time, uint64_t end_time, double start_value, double end_value, AutomationCurveType curve_type) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    if (start_time >= end_time) {
        return Result<bool>::error("Start time must be before end time");
    }
    
    save_state_snapshot();
    
    // Apply snapping
    uint64_t snapped_start = m_snap_enabled ? snap_time_to_grid(start_time) : start_time;
    uint64_t snapped_end = m_snap_enabled ? snap_time_to_grid(end_time) : end_time;
    
    // Constrain values
    start_value = constrain_value(start_value);
    end_value = constrain_value(end_value);
    
    // Generate points along the curve
    uint64_t duration = snapped_end - snapped_start;
    uint64_t num_points = std::max(2ULL, duration / m_drawing_resolution);
    
    for (uint64_t i = 0; i < num_points; ++i) {
        double t = static_cast<double>(i) / (num_points - 1);
        uint64_t point_time = snapped_start + static_cast<uint64_t>(t * duration);
        
        // Apply curve interpolation
        double curve_t = t;
        switch (curve_type) {
            case AutomationCurveType::EXPONENTIAL:
                curve_t = std::pow(t, 2.0);
                break;
            case AutomationCurveType::LOGARITHMIC:
                curve_t = std::sqrt(t);
                break;
            case AutomationCurveType::SMOOTH:
                curve_t = (std::sin((t - 0.5) * M_PI) + 1.0) * 0.5;
                break;
            case AutomationCurveType::STEPPED:
                curve_t = (i < num_points - 1) ? 0.0 : 1.0;
                break;
            default: // LINEAR and BEZIER
                break;
        }
        
        double point_value = start_value + (end_value - start_value) * curve_t;
        
        AutomationPoint point(point_time, point_value, curve_type);
        m_current_lane->add_point(point);
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::draw_curve_with_points(const std::vector<std::pair<uint64_t, double>>& points, AutomationCurveType curve_type) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    if (points.empty()) {
        return Result<bool>::error("No points provided");
    }
    
    save_state_snapshot();
    
    for (const auto& [time, value] : points) {
        uint64_t snapped_time = m_snap_enabled ? snap_time_to_grid(time) : time;
        double constrained_value = constrain_value(value);
        
        AutomationPoint point(snapped_time, constrained_value, curve_type);
        m_current_lane->add_point(point);
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::draw_sine_wave(uint64_t start_time, uint64_t duration, double frequency_hz, double amplitude, double offset) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    // Generate sine wave points
    auto points = AutomationCurveTemplates::create_sine_lfo(start_time, duration, frequency_hz, amplitude, offset);
    
    for (const auto& point : points) {
        m_current_lane->add_point(point);
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::erase_point_at_time(uint64_t time_samples, uint64_t tolerance_samples) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    auto result = m_current_lane->remove_point_at_time(time_samples, tolerance_samples);
    
    if (result.is_success()) {
        notify_edit_changed();
    }
    
    return result;
}

Result<bool> AutomationEditor::erase_points_in_range(uint64_t start_time, uint64_t end_time) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    auto result = m_current_lane->clear_range(start_time, end_time);
    
    if (result.is_success()) {
        notify_edit_changed();
    }
    
    return result;
}

Result<bool> AutomationEditor::select_point_at_time(uint64_t time_samples, uint64_t tolerance_samples, bool add_to_selection) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    if (!add_to_selection) {
        m_current_lane->deselect_all_points();
    }
    
    auto point = m_current_lane->find_point_at_time(time_samples, tolerance_samples);
    if (point) {
        point->selected = true;
        notify_edit_changed();
        return Result<bool>::success(true);
    }
    
    return Result<bool>::error("No point found at specified time");
}

Result<bool> AutomationEditor::select_points_in_range(uint64_t start_time, uint64_t end_time, bool add_to_selection) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    if (!add_to_selection) {
        m_current_lane->deselect_all_points();
    }
    
    m_current_lane->select_points_in_range(start_time, end_time);
    notify_edit_changed();
    
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::move_selected_points(int64_t time_delta_samples, double value_delta) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    auto result = m_current_lane->move_selected_points(time_delta_samples, value_delta);
    
    if (result.is_success()) {
        notify_edit_changed();
    }
    
    return result;
}

Result<bool> AutomationEditor::scale_selected_values(double scale_factor, double pivot_value) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    auto result = m_current_lane->scale_selected_values(scale_factor, pivot_value);
    
    if (result.is_success()) {
        notify_edit_changed();
    }
    
    return result;
}

Result<bool> AutomationEditor::smooth_selected_points(float strength) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    // Apply smoothing only to selected points
    auto& points = m_current_lane->get_points_mutable();
    std::vector<double> original_values;
    
    // Store original values of selected points
    for (size_t i = 0; i < points.size(); ++i) {
        if (points[i].selected) {
            original_values.push_back(points[i].value);
        } else {
            original_values.push_back(0.0); // Placeholder
        }
    }
    
    // Apply smoothing
    for (size_t i = 1; i < points.size() - 1; ++i) {
        if (points[i].selected) {
            double prev_value = points[i - 1].value;
            double curr_value = original_values[i];
            double next_value = points[i + 1].value;
            
            double smoothed_value = (prev_value + curr_value * 2.0 + next_value) / 4.0;
            points[i].value = curr_value + (smoothed_value - curr_value) * strength;
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::quantize_selected_points(uint64_t grid_size_samples) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    auto& points = m_current_lane->get_points_mutable();
    
    for (auto& point : points) {
        if (point.selected) {
            point.time_samples = AutomationUtils::snap_to_grid(point.time_samples, grid_size_samples);
        }
    }
    
    // Re-sort points by time
    std::sort(points.begin(), points.end(),
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.time_samples < b.time_samples;
        });
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::insert_point_on_curve(uint64_t time_samples) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    // Get interpolated value at the time
    double value = m_current_lane->get_value_at_time(time_samples);
    
    // Apply snapping
    uint64_t snapped_time = m_snap_enabled ? snap_time_to_grid(time_samples) : time_samples;
    
    AutomationPoint point(snapped_time, value);
    auto result = m_current_lane->add_point(point);
    
    if (result.is_success()) {
        notify_edit_changed();
    }
    
    return result;
}

Result<bool> AutomationEditor::normalize_selected_values() {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    auto selected_points = m_current_lane->get_selected_points();
    if (selected_points.empty()) {
        return Result<bool>::error("No points selected");
    }
    
    save_state_snapshot();
    
    // Find min and max values
    double min_val = selected_points[0]->value;
    double max_val = selected_points[0]->value;
    
    for (const auto& point : selected_points) {
        min_val = std::min(min_val, point->value);
        max_val = std::max(max_val, point->value);
    }
    
    // Avoid division by zero
    if (max_val == min_val) {
        return Result<bool>::success(true);
    }
    
    // Normalize to 0.0-1.0 range
    double range = max_val - min_val;
    for (auto& point : selected_points) {
        point->value = (point->value - min_val) / range;
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::invert_selected_values() {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    auto selected_points = m_current_lane->get_selected_points();
    for (auto& point : selected_points) {
        point->value = 1.0 - point->value;
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::copy_selected_points() {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    auto selected_points = m_current_lane->get_selected_points();
    if (selected_points.empty()) {
        return Result<bool>::error("No points selected");
    }
    
    m_clipboard_points.clear();
    for (const auto& point : selected_points) {
        m_clipboard_points.push_back(*point);
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::paste_points_at_time(uint64_t time_samples) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    if (m_clipboard_points.empty()) {
        return Result<bool>::error("No points in clipboard");
    }
    
    save_state_snapshot();
    
    // Calculate time offset
    uint64_t earliest_time = m_clipboard_points[0].time_samples;
    for (const auto& point : m_clipboard_points) {
        if (point.time_samples < earliest_time) {
            earliest_time = point.time_samples;
        }
    }
    
    int64_t time_offset = static_cast<int64_t>(time_samples) - static_cast<int64_t>(earliest_time);
    
    // Paste points with offset
    for (auto point : m_clipboard_points) {
        point.time_samples = static_cast<uint64_t>(static_cast<int64_t>(point.time_samples) + time_offset);
        point.selected = true; // Select pasted points
        
        m_current_lane->add_point(point);
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> AutomationEditor::create_fade_in(uint64_t start_time, uint64_t duration, double target_value) {
    if (!m_current_lane) {
        return Result<bool>::error("No automation lane selected");
    }
    
    save_state_snapshot();
    
    auto points = AutomationCurveTemplates::create_exponential_fade_in(start_time, duration);
    
    // Scale to target value
    for (auto& point : points) {
        point.value *= target_value;
    }
    
    for (const auto& point : points) {
        m_current_lane->add_point(point);
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

uint64_t AutomationEditor::snap_time_to_grid(uint64_t time_samples) const {
    return AutomationUtils::snap_to_grid(time_samples, m_snap_grid_size);
}

double AutomationEditor::constrain_value(double value) const {
    return std::clamp(value, m_min_value, m_max_value);
}

AutomationEditor::CurveAnalysis AutomationEditor::analyze_selected_curve() const {
    CurveAnalysis analysis{};
    
    if (!m_current_lane) {
        return analysis;
    }
    
    auto selected_points = m_current_lane->get_selected_points();
    if (selected_points.empty()) {
        return analysis;
    }
    
    std::vector<AutomationPoint> points;
    for (const auto& point : selected_points) {
        points.push_back(*point);
    }
    
    // Sort by time
    std::sort(points.begin(), points.end(),
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.time_samples < b.time_samples;
        });
    
    // Calculate statistics
    analysis.point_count = points.size();
    analysis.min_value = points[0].value;
    analysis.max_value = points[0].value;
    
    double sum = 0.0;
    double sum_squares = 0.0;
    
    for (const auto& point : points) {
        analysis.min_value = std::min(analysis.min_value, point.value);
        analysis.max_value = std::max(analysis.max_value, point.value);
        sum += point.value;
        sum_squares += point.value * point.value;
    }
    
    analysis.average_value = sum / points.size();
    analysis.rms_value = std::sqrt(sum_squares / points.size());
    
    if (points.size() > 1) {
        analysis.duration_samples = points.back().time_samples - points.front().time_samples;
        analysis.curve_length = calculate_curve_length(points);
        analysis.smoothness_factor = calculate_curve_smoothness(points);
    }
    
    return analysis;
}

void AutomationEditor::save_state_snapshot() {
    if (!m_current_lane) return;
    
    StateSnapshot snapshot;
    snapshot.points = m_current_lane->get_points();
    snapshot.parameter_id = m_current_lane->get_parameter_id();
    
    m_undo_stack.push_back(snapshot);
    
    // Limit undo stack size
    if (m_undo_stack.size() > MAX_UNDO_STATES) {
        m_undo_stack.erase(m_undo_stack.begin());
    }
    
    // Clear redo stack
    m_redo_stack.clear();
}

Result<bool> AutomationEditor::undo_last_operation() {
    if (!m_current_lane || m_undo_stack.empty()) {
        return Result<bool>::error("Nothing to undo");
    }
    
    // Save current state to redo stack
    StateSnapshot current_state;
    current_state.points = m_current_lane->get_points();
    current_state.parameter_id = m_current_lane->get_parameter_id();
    m_redo_stack.push_back(current_state);
    
    // Restore previous state
    StateSnapshot state = m_undo_stack.back();
    m_undo_stack.pop_back();
    restore_state_snapshot(state);
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

void AutomationEditor::notify_edit_changed() {
    if (m_edit_callback) {
        m_edit_callback();
    }
}

void AutomationEditor::restore_state_snapshot(const StateSnapshot& snapshot) {
    if (!m_current_lane) return;
    
    m_current_lane->clear_all_points();
    
    for (const auto& point : snapshot.points) {
        m_current_lane->add_point(point);
    }
}

double AutomationEditor::calculate_curve_smoothness(const std::vector<AutomationPoint>& points) const {
    if (points.size() < 3) return 1.0;
    
    double total_variation = 0.0;
    
    for (size_t i = 1; i < points.size() - 1; ++i) {
        // Calculate second derivative approximation
        double y_prev = points[i - 1].value;
        double y_curr = points[i].value;
        double y_next = points[i + 1].value;
        
        double second_derivative = y_prev - 2.0 * y_curr + y_next;
        total_variation += std::abs(second_derivative);
    }
    
    // Normalize and invert (higher values = smoother)
    double avg_variation = total_variation / (points.size() - 2);
    return 1.0 / (1.0 + avg_variation * 10.0); // Scale factor for reasonable range
}

double AutomationEditor::calculate_curve_length(const std::vector<AutomationPoint>& points) const {
    if (points.size() < 2) return 0.0;
    
    double total_length = 0.0;
    
    for (size_t i = 1; i < points.size(); ++i) {
        double time_delta = static_cast<double>(points[i].time_samples - points[i - 1].time_samples);
        double value_delta = points[i].value - points[i - 1].value;
        
        // Approximate arc length
        double segment_length = std::sqrt(time_delta * time_delta + value_delta * value_delta);
        total_length += segment_length;
    }
    
    return total_length;
}

uint64_t AutomationEditor::beats_to_samples(double beats, double bpm, double sample_rate) {
    return AutomationLane::beats_to_samples(beats, bpm, sample_rate);
}

double AutomationEditor::samples_to_beats(uint64_t samples, double bpm, double sample_rate) {
    return AutomationLane::samples_to_beats(samples, bpm, sample_rate);
}

// AutomationCurveTemplates implementations
std::vector<AutomationPoint> AutomationCurveTemplates::create_sine_lfo(uint64_t start_time, uint64_t duration, double frequency_hz, double depth, double offset, double sample_rate) {
    std::vector<AutomationPoint> points;
    
    double duration_seconds = duration / sample_rate;
    size_t num_points = static_cast<size_t>(std::max(8.0, frequency_hz * duration_seconds * 16.0)); // 16 points per cycle
    
    for (size_t i = 0; i < num_points; ++i) {
        double t = static_cast<double>(i) / (num_points - 1);
        uint64_t time = start_time + static_cast<uint64_t>(t * duration);
        
        double phase = t * (frequency_hz * duration_seconds) * 2.0 * M_PI;
        double lfo_value = std::sin(phase);
        double value = offset + lfo_value * depth * 0.5;
        value = std::clamp(value, 0.0, 1.0);
        
        points.emplace_back(time, value);
    }
    
    return points;
}

std::vector<AutomationPoint> AutomationCurveTemplates::create_exponential_fade_in(uint64_t start_time, uint64_t duration, double sample_rate) {
    std::vector<AutomationPoint> points;
    
    size_t num_points = 16; // Good resolution for fade
    
    for (size_t i = 0; i < num_points; ++i) {
        double t = static_cast<double>(i) / (num_points - 1);
        uint64_t time = start_time + static_cast<uint64_t>(t * duration);
        
        // Exponential curve
        double value = 1.0 - std::exp(-t * 4.0); // Fast rise, then levels off
        
        points.emplace_back(time, value, AutomationCurveType::EXPONENTIAL);
    }
    
    return points;
}

std::vector<AutomationPoint> AutomationCurveTemplates::create_exponential_fade_out(uint64_t start_time, uint64_t duration, double sample_rate) {
    std::vector<AutomationPoint> points;
    
    size_t num_points = 16;
    
    for (size_t i = 0; i < num_points; ++i) {
        double t = static_cast<double>(i) / (num_points - 1);
        uint64_t time = start_time + static_cast<uint64_t>(t * duration);
        
        // Exponential decay
        double value = std::exp(-t * 4.0);
        
        points.emplace_back(time, value, AutomationCurveType::EXPONENTIAL);
    }
    
    return points;
}

std::vector<AutomationPoint> AutomationCurveTemplates::create_auto_pan(uint64_t start_time, uint64_t duration, double rate_hz, double sample_rate) {
    std::vector<AutomationPoint> points;
    
    double duration_seconds = duration / sample_rate;
    size_t num_points = static_cast<size_t>(std::max(8.0, rate_hz * duration_seconds * 8.0));
    
    for (size_t i = 0; i < num_points; ++i) {
        double t = static_cast<double>(i) / (num_points - 1);
        uint64_t time = start_time + static_cast<uint64_t>(t * duration);
        
        double phase = t * (rate_hz * duration_seconds) * 2.0 * M_PI;
        double pan_value = (std::sin(phase) + 1.0) * 0.5; // 0.0 = left, 1.0 = right
        
        points.emplace_back(time, pan_value);
    }
    
    return points;
}

// AutomationEditorFactory implementations
std::unique_ptr<AutomationEditor> AutomationEditorFactory::create_standard_editor(std::shared_ptr<AutomationData> automation_data) {
    auto editor = std::make_unique<AutomationEditor>(automation_data);
    
    editor->set_snap_enabled(true);
    editor->set_snap_grid_size(1024);    // ~23ms at 44.1kHz
    editor->set_drawing_resolution(256);  // ~6ms resolution
    editor->set_value_constraints(0.0, 1.0);
    
    return editor;
}

std::unique_ptr<AutomationEditor> AutomationEditorFactory::create_precision_editor(std::shared_ptr<AutomationData> automation_data) {
    auto editor = std::make_unique<AutomationEditor>(automation_data);
    
    editor->set_snap_enabled(true);
    editor->set_snap_grid_size(64);      // Fine grid ~1.5ms
    editor->set_drawing_resolution(32);   // High resolution
    editor->set_value_constraints(0.0, 1.0);
    
    return editor;
}

std::unique_ptr<AutomationEditor> AutomationEditorFactory::create_musical_editor(std::shared_ptr<AutomationData> automation_data) {
    auto editor = std::make_unique<AutomationEditor>(automation_data);
    
    // 16th note grid at 120 BPM
    uint64_t sixteenth_note_samples = AutomationEditor::beats_to_samples(0.25, 120.0, 44100.0);
    
    editor->set_snap_enabled(true);
    editor->set_snap_grid_size(sixteenth_note_samples);
    editor->set_drawing_resolution(sixteenth_note_samples / 4);
    editor->set_value_constraints(0.0, 1.0);
    
    return editor;
}

} // namespace mixmind