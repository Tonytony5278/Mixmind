#include "AutomationData.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace mixmind {

// AutomationParameterId implementation
bool AutomationParameterId::operator<(const AutomationParameterId& other) const {
    if (type != other.type) return type < other.type;
    if (track_id != other.track_id) return track_id < other.track_id;
    if (parameter_index != other.parameter_index) return parameter_index < other.parameter_index;
    if (plugin_instance_id != other.plugin_instance_id) return plugin_instance_id < other.plugin_instance_id;
    return custom_id < other.custom_id;
}

bool AutomationParameterId::operator==(const AutomationParameterId& other) const {
    return type == other.type &&
           track_id == other.track_id &&
           parameter_index == other.parameter_index &&
           plugin_instance_id == other.plugin_instance_id &&
           custom_id == other.custom_id;
}

std::string AutomationParameterId::get_display_name() const {
    std::stringstream ss;
    
    switch (type) {
        case TRACK_VOLUME:
            ss << "Track " << track_id << " Volume";
            break;
        case TRACK_PAN:
            ss << "Track " << track_id << " Pan";
            break;
        case TRACK_MUTE:
            ss << "Track " << track_id << " Mute";
            break;
        case TRACK_SOLO:
            ss << "Track " << track_id << " Solo";
            break;
        case TRACK_SEND_LEVEL:
            ss << "Track " << track_id << " Send " << parameter_index << " Level";
            break;
        case TRACK_SEND_PAN:
            ss << "Track " << track_id << " Send " << parameter_index << " Pan";
            break;
        case VST_PARAMETER:
            ss << "Track " << track_id << " Plugin " << plugin_instance_id << " Param " << parameter_index;
            break;
        case MIDI_CC:
            ss << "Track " << track_id << " CC " << parameter_index;
            break;
        case CUSTOM:
            ss << custom_id;
            break;
        default:
            ss << "Unknown Parameter";
            break;
    }
    
    return ss.str();
}

// AutomationLane implementation
AutomationLane::AutomationLane(const AutomationParameterId& param_id, double default_value)
    : m_parameter_id(param_id), m_default_value(default_value) {
    
    // Set appropriate color based on parameter type
    switch (param_id.type) {
        case AutomationParameterId::TRACK_VOLUME:
            m_color = 0xFFFF8040; // Orange
            break;
        case AutomationParameterId::TRACK_PAN:
            m_color = 0xFF40FF40; // Green
            break;
        case AutomationParameterId::VST_PARAMETER:
            m_color = 0xFF4080FF; // Blue
            break;
        case AutomationParameterId::MIDI_CC:
            m_color = 0xFFFF4080; // Pink
            break;
        default:
            m_color = 0xFF8080C0; // Purple
            break;
    }
}

Result<bool> AutomationLane::add_point(const AutomationPoint& point) {
    if (!validate_point(point)) {
        return Result<bool>::error("Invalid automation point");
    }
    
    // Check if point already exists at this time
    auto existing = find_point_at_time(point.time_samples);
    if (existing) {
        // Update existing point
        existing->value = point.value;
        existing->curve_type = point.curve_type;
        existing->control_point_1 = point.control_point_1;
        existing->control_point_2 = point.control_point_2;
    } else {
        // Add new point
        m_points.push_back(point);
        sort_points_by_time();
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::remove_point(size_t point_index) {
    if (point_index >= m_points.size()) {
        return Result<bool>::error("Point index out of range");
    }
    
    m_points.erase(m_points.begin() + point_index);
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::remove_point_at_time(uint64_t time_samples, uint64_t tolerance_samples) {
    auto it = std::find_if(m_points.begin(), m_points.end(),
        [time_samples, tolerance_samples](const AutomationPoint& point) {
            return point.is_at_time(time_samples, tolerance_samples);
        });
    
    if (it == m_points.end()) {
        return Result<bool>::error("No point found at specified time");
    }
    
    m_points.erase(it);
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::remove_selected_points() {
    auto it = std::remove_if(m_points.begin(), m_points.end(),
        [](const AutomationPoint& point) { return point.selected; });
    
    size_t removed_count = std::distance(it, m_points.end());
    m_points.erase(it, m_points.end());
    
    return Result<bool>::success(removed_count > 0);
}

AutomationPoint* AutomationLane::find_point_at_time(uint64_t time_samples, uint64_t tolerance_samples) {
    for (auto& point : m_points) {
        if (point.is_at_time(time_samples, tolerance_samples)) {
            return &point;
        }
    }
    return nullptr;
}

std::vector<AutomationPoint*> AutomationLane::get_points_in_range(uint64_t start_time, uint64_t end_time) {
    std::vector<AutomationPoint*> result;
    
    for (auto& point : m_points) {
        if (point.time_samples >= start_time && point.time_samples <= end_time) {
            result.push_back(&point);
        }
    }
    
    return result;
}

std::vector<AutomationPoint*> AutomationLane::get_selected_points() {
    std::vector<AutomationPoint*> result;
    
    for (auto& point : m_points) {
        if (point.selected) {
            result.push_back(&point);
        }
    }
    
    return result;
}

double AutomationLane::get_value_at_time(uint64_t time_samples) const {
    if (m_points.empty()) {
        return m_default_value;
    }
    
    // Find surrounding points
    const AutomationPoint* before = nullptr;
    const AutomationPoint* after = nullptr;
    
    for (const auto& point : m_points) {
        if (point.time_samples <= time_samples) {
            if (!before || point.time_samples > before->time_samples) {
                before = &point;
            }
        }
        if (point.time_samples > time_samples) {
            if (!after || point.time_samples < after->time_samples) {
                after = &point;
            }
        }
    }
    
    // If no point before, use default value
    if (!before) {
        return m_default_value;
    }
    
    // If no point after, use the before point's value
    if (!after) {
        return before->value;
    }
    
    // Interpolate between points
    return interpolate_value(*before, *after, time_samples);
}

std::vector<std::pair<uint64_t, double>> AutomationLane::get_interpolated_values(uint64_t start_time, uint64_t end_time, uint64_t resolution_samples) const {
    std::vector<std::pair<uint64_t, double>> result;
    
    if (start_time >= end_time) {
        return result;
    }
    
    uint64_t duration = end_time - start_time;
    size_t num_samples = static_cast<size_t>(duration / resolution_samples) + 1;
    
    result.reserve(num_samples);
    
    for (size_t i = 0; i < num_samples; ++i) {
        uint64_t time = start_time + i * resolution_samples;
        if (time > end_time) time = end_time;
        
        double value = get_value_at_time(time);
        result.emplace_back(time, value);
        
        if (time == end_time) break;
    }
    
    return result;
}

void AutomationLane::select_all_points() {
    for (auto& point : m_points) {
        point.selected = true;
    }
}

void AutomationLane::deselect_all_points() {
    for (auto& point : m_points) {
        point.selected = false;
    }
}

void AutomationLane::select_points_in_range(uint64_t start_time, uint64_t end_time) {
    for (auto& point : m_points) {
        if (point.time_samples >= start_time && point.time_samples <= end_time) {
            point.selected = true;
        }
    }
}

Result<bool> AutomationLane::move_selected_points(int64_t time_delta_samples, double value_delta) {
    for (auto& point : m_points) {
        if (point.selected) {
            // Apply time delta
            int64_t new_time = static_cast<int64_t>(point.time_samples) + time_delta_samples;
            if (new_time < 0) new_time = 0;
            point.time_samples = static_cast<uint64_t>(new_time);
            
            // Apply value delta
            point.value = std::clamp(point.value + value_delta, 0.0, 1.0);
        }
    }
    
    sort_points_by_time();
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::scale_selected_values(double scale_factor, double pivot_value) {
    if (scale_factor <= 0.0) {
        return Result<bool>::error("Scale factor must be positive");
    }
    
    for (auto& point : m_points) {
        if (point.selected) {
            double offset_from_pivot = point.value - pivot_value;
            double scaled_offset = offset_from_pivot * scale_factor;
            point.value = std::clamp(pivot_value + scaled_offset, 0.0, 1.0);
        }
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::set_selected_curve_type(AutomationCurveType curve_type) {
    for (auto& point : m_points) {
        if (point.selected) {
            point.curve_type = curve_type;
        }
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::clear_all_points() {
    m_points.clear();
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::clear_range(uint64_t start_time, uint64_t end_time) {
    auto it = std::remove_if(m_points.begin(), m_points.end(),
        [start_time, end_time](const AutomationPoint& point) {
            return point.time_samples >= start_time && point.time_samples <= end_time;
        });
    
    size_t removed_count = std::distance(it, m_points.end());
    m_points.erase(it, m_points.end());
    
    return Result<bool>::success(removed_count > 0);
}

Result<bool> AutomationLane::quantize_points_timing(uint64_t grid_samples) {
    if (grid_samples == 0) {
        return Result<bool>::error("Grid size must be greater than 0");
    }
    
    for (auto& point : m_points) {
        uint64_t quantized_time = ((point.time_samples + grid_samples / 2) / grid_samples) * grid_samples;
        point.time_samples = quantized_time;
    }
    
    sort_points_by_time();
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::smooth_points(float strength) {
    if (m_points.size() < 3) {
        return Result<bool>::success(true); // Nothing to smooth
    }
    
    strength = std::clamp(strength, 0.0f, 1.0f);
    
    // Apply smoothing to interior points
    for (size_t i = 1; i < m_points.size() - 1; ++i) {
        double prev_value = m_points[i - 1].value;
        double curr_value = m_points[i].value;
        double next_value = m_points[i + 1].value;
        
        double smoothed_value = (prev_value + curr_value * 2.0 + next_value) / 4.0;
        m_points[i].value = curr_value + (smoothed_value - curr_value) * strength;
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationLane::thin_points(double tolerance) {
    if (m_points.size() <= 2) {
        return Result<bool>::success(true); // Keep at least 2 points
    }
    
    std::vector<AutomationPoint> thinned_points;
    thinned_points.push_back(m_points[0]); // Always keep first point
    
    for (size_t i = 1; i < m_points.size() - 1; ++i) {
        const auto& prev = m_points[i - 1];
        const auto& curr = m_points[i];
        const auto& next = m_points[i + 1];
        
        // Calculate interpolated value at current time
        double interpolated = interpolate_value(prev, next, curr.time_samples);
        
        // Keep point if it deviates significantly from interpolated value
        if (std::abs(curr.value - interpolated) > tolerance) {
            thinned_points.push_back(curr);
        }
    }
    
    thinned_points.push_back(m_points.back()); // Always keep last point
    
    m_points = std::move(thinned_points);
    return Result<bool>::success(true);
}

uint64_t AutomationLane::beats_to_samples(double beats, double bpm, double sample_rate) {
    double seconds_per_beat = 60.0 / bpm;
    double seconds = beats * seconds_per_beat;
    return static_cast<uint64_t>(seconds * sample_rate);
}

double AutomationLane::samples_to_beats(uint64_t samples, double bpm, double sample_rate) {
    double seconds = samples / sample_rate;
    double seconds_per_beat = 60.0 / bpm;
    return seconds / seconds_per_beat;
}

void AutomationLane::sort_points_by_time() {
    std::sort(m_points.begin(), m_points.end(),
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.time_samples < b.time_samples;
        });
}

double AutomationLane::interpolate_value(const AutomationPoint& p1, const AutomationPoint& p2, uint64_t time) const {
    if (time <= p1.time_samples) return p1.value;
    if (time >= p2.time_samples) return p2.value;
    
    double duration = static_cast<double>(p2.time_samples - p1.time_samples);
    double position = static_cast<double>(time - p1.time_samples);
    double t = position / duration; // Normalized time (0.0 to 1.0)
    
    return p1.value + (p2.value - p1.value) * apply_curve_interpolation(t, p1.curve_type, p1.control_point_1, p1.control_point_2);
}

double AutomationLane::apply_curve_interpolation(double t, AutomationCurveType curve_type, double cp1, double cp2) const {
    t = std::clamp(t, 0.0, 1.0);
    
    switch (curve_type) {
        case AutomationCurveType::LINEAR:
            return t;
            
        case AutomationCurveType::EXPONENTIAL:
            return std::pow(t, 2.0);
            
        case AutomationCurveType::LOGARITHMIC:
            return std::sqrt(t);
            
        case AutomationCurveType::BEZIER: {
            // Cubic Bezier curve with control points
            double u = 1.0 - t;
            return u * u * u * 0.0 +                    // P0 = 0 (start)
                   3.0 * u * u * t * (0.0 + cp1) +      // P1 with control
                   3.0 * u * t * t * (1.0 + cp2) +      // P2 with control
                   t * t * t * 1.0;                      // P3 = 1 (end)
        }
        
        case AutomationCurveType::SMOOTH:
            // Smooth S-curve using sine interpolation
            return (std::sin((t - 0.5) * M_PI) + 1.0) * 0.5;
            
        case AutomationCurveType::STEPPED:
            return (t < 1.0) ? 0.0 : 1.0;
            
        default:
            return t; // Fallback to linear
    }
}

bool AutomationLane::validate_point(const AutomationPoint& point) const {
    if (point.value < 0.0 || point.value > 1.0) {
        return false;
    }
    
    if (point.control_point_1 < -1.0 || point.control_point_1 > 1.0) {
        return false;
    }
    
    if (point.control_point_2 < -1.0 || point.control_point_2 > 1.0) {
        return false;
    }
    
    return true;
}

// AutomationData implementation
Result<std::shared_ptr<AutomationLane>> AutomationData::create_lane(const AutomationParameterId& param_id, double default_value) {
    if (m_lanes.find(param_id) != m_lanes.end()) {
        return Result<std::shared_ptr<AutomationLane>>::error("Automation lane already exists for this parameter");
    }
    
    auto lane = std::make_shared<AutomationLane>(param_id, default_value);
    m_lanes[param_id] = lane;
    
    notify_edit_changed(param_id);
    return Result<std::shared_ptr<AutomationLane>>::success(lane);
}

Result<bool> AutomationData::remove_lane(const AutomationParameterId& param_id) {
    auto it = m_lanes.find(param_id);
    if (it == m_lanes.end()) {
        return Result<bool>::error("Automation lane not found");
    }
    
    m_lanes.erase(it);
    notify_edit_changed(param_id);
    return Result<bool>::success(true);
}

std::shared_ptr<AutomationLane> AutomationData::get_lane(const AutomationParameterId& param_id) {
    auto it = m_lanes.find(param_id);
    return (it != m_lanes.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<AutomationLane>> AutomationData::get_all_lanes() const {
    std::vector<std::shared_ptr<AutomationLane>> result;
    result.reserve(m_lanes.size());
    
    for (const auto& [param_id, lane] : m_lanes) {
        result.push_back(lane);
    }
    
    return result;
}

std::vector<std::shared_ptr<AutomationLane>> AutomationData::get_track_lanes(uint32_t track_id) const {
    std::vector<std::shared_ptr<AutomationLane>> result;
    
    for (const auto& [param_id, lane] : m_lanes) {
        if (param_id.track_id == track_id && 
            (param_id.type == AutomationParameterId::TRACK_VOLUME ||
             param_id.type == AutomationParameterId::TRACK_PAN ||
             param_id.type == AutomationParameterId::TRACK_MUTE ||
             param_id.type == AutomationParameterId::TRACK_SOLO ||
             param_id.type == AutomationParameterId::TRACK_SEND_LEVEL ||
             param_id.type == AutomationParameterId::TRACK_SEND_PAN)) {
            result.push_back(lane);
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<AutomationLane>> AutomationData::get_vst_parameter_lanes(uint32_t plugin_instance_id) const {
    std::vector<std::shared_ptr<AutomationLane>> result;
    
    for (const auto& [param_id, lane] : m_lanes) {
        if (param_id.type == AutomationParameterId::VST_PARAMETER && 
            param_id.plugin_instance_id == plugin_instance_id) {
            result.push_back(lane);
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<AutomationLane>> AutomationData::get_midi_cc_lanes(uint32_t track_id) const {
    std::vector<std::shared_ptr<AutomationLane>> result;
    
    for (const auto& [param_id, lane] : m_lanes) {
        if (param_id.type == AutomationParameterId::MIDI_CC && param_id.track_id == track_id) {
            result.push_back(lane);
        }
    }
    
    return result;
}

void AutomationData::clear_all_automation() {
    m_lanes.clear();
}

void AutomationData::clear_automation_in_range(uint64_t start_time, uint64_t end_time) {
    for (auto& [param_id, lane] : m_lanes) {
        lane->clear_range(start_time, end_time);
    }
}

size_t AutomationData::get_total_point_count() const {
    size_t total = 0;
    for (const auto& [param_id, lane] : m_lanes) {
        total += lane->get_point_count();
    }
    return total;
}

void AutomationData::notify_edit_changed(const AutomationParameterId& param_id) {
    if (m_edit_callback) {
        m_edit_callback(param_id);
    }
}

// AutomationUtils implementation
double AutomationUtils::normalize_track_volume(double linear_volume) {
    // Convert 0.0-2.0 linear volume to 0.0-1.0 normalized
    return std::clamp(linear_volume / 2.0, 0.0, 1.0);
}

double AutomationUtils::denormalize_track_volume(double normalized) {
    // Convert 0.0-1.0 normalized to 0.0-2.0 linear volume
    return std::clamp(normalized * 2.0, 0.0, 2.0);
}

double AutomationUtils::normalize_track_pan(double pan_position) {
    // Convert -1.0-1.0 pan to 0.0-1.0 normalized
    return std::clamp((pan_position + 1.0) / 2.0, 0.0, 1.0);
}

double AutomationUtils::denormalize_track_pan(double normalized) {
    // Convert 0.0-1.0 normalized to -1.0-1.0 pan
    return std::clamp(normalized * 2.0 - 1.0, -1.0, 1.0);
}

double AutomationUtils::normalize_midi_cc(uint8_t cc_value) {
    // Convert 0-127 MIDI CC to 0.0-1.0 normalized
    return static_cast<double>(std::clamp(cc_value, static_cast<uint8_t>(0), static_cast<uint8_t>(127))) / 127.0;
}

uint8_t AutomationUtils::denormalize_midi_cc(double normalized) {
    // Convert 0.0-1.0 normalized to 0-127 MIDI CC
    return static_cast<uint8_t>(std::clamp(normalized * 127.0, 0.0, 127.0));
}

bool AutomationUtils::is_valid_normalized_value(double value) {
    return value >= 0.0 && value <= 1.0;
}

double AutomationUtils::clamp_normalized_value(double value) {
    return std::clamp(value, 0.0, 1.0);
}

AutomationParameterId AutomationUtils::create_track_volume_id(uint32_t track_id) {
    return AutomationParameterId(AutomationParameterId::TRACK_VOLUME, track_id);
}

AutomationParameterId AutomationUtils::create_track_pan_id(uint32_t track_id) {
    return AutomationParameterId(AutomationParameterId::TRACK_PAN, track_id);
}

AutomationParameterId AutomationUtils::create_vst_parameter_id(uint32_t track_id, uint32_t plugin_id, uint32_t param_index) {
    return AutomationParameterId(AutomationParameterId::VST_PARAMETER, track_id, param_index, plugin_id);
}

AutomationParameterId AutomationUtils::create_midi_cc_id(uint32_t track_id, uint32_t cc_number) {
    return AutomationParameterId(AutomationParameterId::MIDI_CC, track_id, cc_number);
}

uint32_t AutomationUtils::generate_lane_color(const AutomationParameterId& param_id) {
    // Generate consistent colors based on parameter type and ID
    uint32_t hash = 0;
    hash = hash * 31 + static_cast<uint32_t>(param_id.type);
    hash = hash * 31 + param_id.track_id;
    hash = hash * 31 + param_id.parameter_index;
    hash = hash * 31 + param_id.plugin_instance_id;
    
    // Convert hash to RGB color with good saturation and brightness
    uint8_t r = static_cast<uint8_t>(64 + (hash & 0xFF) / 2);
    uint8_t g = static_cast<uint8_t>(64 + ((hash >> 8) & 0xFF) / 2);
    uint8_t b = static_cast<uint8_t>(64 + ((hash >> 16) & 0xFF) / 2);
    
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

uint64_t AutomationUtils::snap_to_grid(uint64_t time_samples, uint64_t grid_size_samples) {
    if (grid_size_samples == 0) return time_samples;
    return ((time_samples + grid_size_samples / 2) / grid_size_samples) * grid_size_samples;
}

std::vector<uint64_t> AutomationUtils::get_grid_points(uint64_t start_time, uint64_t end_time, uint64_t grid_size) {
    std::vector<uint64_t> points;
    
    if (grid_size == 0 || start_time >= end_time) {
        return points;
    }
    
    uint64_t first_grid_point = snap_to_grid(start_time, grid_size);
    
    for (uint64_t t = first_grid_point; t <= end_time; t += grid_size) {
        if (t >= start_time) {
            points.push_back(t);
        }
    }
    
    return points;
}

} // namespace mixmind