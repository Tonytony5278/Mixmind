#include "CCLaneEditor.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace mixmind {

CCLaneEditor::CCLaneEditor(std::shared_ptr<MIDIClip> clip, const CCLaneConfig& config)
    : m_clip(clip), m_config(config) {
}

void CCLaneEditor::set_controller(uint8_t controller) {
    m_config.controller = controller;
    // Update name if it's still default
    if (m_config.name == "Mod Wheel" && controller != 1) {
        // Try to set a reasonable default name
        static const std::map<uint8_t, std::string> cc_names = {
            {1, "Mod Wheel"}, {2, "Breath"}, {4, "Foot"}, {5, "Portamento"}, 
            {7, "Volume"}, {8, "Balance"}, {10, "Pan"}, {11, "Expression"},
            {64, "Sustain"}, {65, "Portamento On/Off"}, {66, "Sostenuto"},
            {67, "Soft Pedal"}, {71, "Resonance"}, {74, "Cutoff"},
            {91, "Reverb"}, {93, "Chorus"}
        };
        
        auto it = cc_names.find(controller);
        if (it != cc_names.end()) {
            m_config.name = it->second;
        } else {
            m_config.name = "CC " + std::to_string(controller);
        }
    }
}

Result<bool> CCLaneEditor::draw_cc_point(double time_beats, uint8_t value, CurveType curve_type) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    if (value > 127) {
        return Result<bool>::error("CC value must be 0-127");
    }
    
    uint64_t time_samples = beats_to_samples(time_beats);
    
    // Check if CC event already exists at this time
    auto cc_events = m_clip->get_cc_events_for_controller(m_config.controller);
    for (auto cc : cc_events) {
        if (std::abs(static_cast<int64_t>(cc->time) - static_cast<int64_t>(time_samples)) < 100) {
            // Update existing event
            cc->value = value;
            notify_edit_changed();
            return Result<bool>::success(true);
        }
    }
    
    // Create new CC event
    MIDIControlChange cc_event(m_config.controller, value, time_samples);
    auto result = m_clip->add_cc_event(cc_event);
    if (result.is_success()) {
        notify_edit_changed();
    }
    return result;
}

Result<bool> CCLaneEditor::draw_cc_line(double start_time_beats, double end_time_beats, uint8_t start_value, uint8_t end_value, CurveType curve_type) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    if (start_time_beats >= end_time_beats) {
        return Result<bool>::error("Start time must be before end time");
    }
    
    double duration_beats = end_time_beats - start_time_beats;
    double step_beats = std::min(0.125, duration_beats / 8.0); // At least 8 points, max 1/8 note resolution
    
    for (double t = start_time_beats; t <= end_time_beats; t += step_beats) {
        if (t > end_time_beats) t = end_time_beats;
        
        // Interpolate value based on curve type
        double progress = (t - start_time_beats) / duration_beats;
        uint8_t interpolated_value;
        
        switch (curve_type) {
            case CurveType::LINEAR:
                interpolated_value = static_cast<uint8_t>(start_value + (end_value - start_value) * progress);
                break;
            case CurveType::EXPONENTIAL:
                interpolated_value = static_cast<uint8_t>(start_value + (end_value - start_value) * std::pow(progress, 2.0));
                break;
            case CurveType::LOGARITHMIC:
                interpolated_value = static_cast<uint8_t>(start_value + (end_value - start_value) * std::sqrt(progress));
                break;
            case CurveType::SMOOTH:
                // Smooth S-curve using sine interpolation
                interpolated_value = static_cast<uint8_t>(start_value + (end_value - start_value) * (std::sin((progress - 0.5) * M_PI) + 1.0) * 0.5);
                break;
            case CurveType::STEPPED:
                interpolated_value = (progress < 1.0) ? start_value : end_value;
                break;
            default:
                interpolated_value = static_cast<uint8_t>(start_value + (end_value - start_value) * progress);
                break;
        }
        
        auto result = draw_cc_point(t, interpolated_value, curve_type);
        if (!result.is_success()) {
            return result;
        }
        
        if (t == end_time_beats) break;
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> CCLaneEditor::draw_cc_ramp(double start_time_beats, double end_time_beats, uint8_t start_value, uint8_t end_value) {
    return draw_cc_line(start_time_beats, end_time_beats, start_value, end_value, CurveType::LINEAR);
}

Result<bool> CCLaneEditor::erase_cc_at_time(double time_beats, double tolerance_beats) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    uint64_t time_samples = beats_to_samples(time_beats);
    uint64_t tolerance_samples = beats_to_samples(tolerance_beats);
    
    auto& cc_events = m_clip->get_cc_events_mutable();
    size_t removed_count = 0;
    
    auto it = std::remove_if(cc_events.begin(), cc_events.end(),
        [this, time_samples, tolerance_samples, &removed_count](const MIDIControlChange& cc) {
            if (cc.controller == m_config.controller) {
                int64_t time_diff = std::abs(static_cast<int64_t>(cc.time) - static_cast<int64_t>(time_samples));
                if (time_diff <= static_cast<int64_t>(tolerance_samples)) {
                    removed_count++;
                    return true;
                }
            }
            return false;
        });
    
    cc_events.erase(it, cc_events.end());
    
    if (removed_count > 0) {
        notify_edit_changed();
        return Result<bool>::success(true);
    }
    
    return Result<bool>::error("No CC events found at specified time");
}

Result<bool> CCLaneEditor::erase_cc_in_range(double start_time_beats, double end_time_beats) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    uint64_t start_samples = beats_to_samples(start_time_beats);
    uint64_t end_samples = beats_to_samples(end_time_beats);
    
    auto& cc_events = m_clip->get_cc_events_mutable();
    size_t removed_count = 0;
    
    auto it = std::remove_if(cc_events.begin(), cc_events.end(),
        [this, start_samples, end_samples, &removed_count](const MIDIControlChange& cc) {
            if (cc.controller == m_config.controller) {
                if (cc.time >= start_samples && cc.time <= end_samples) {
                    removed_count++;
                    return true;
                }
            }
            return false;
        });
    
    cc_events.erase(it, cc_events.end());
    
    if (removed_count > 0) {
        notify_edit_changed();
    }
    
    return Result<bool>::success(removed_count > 0);
}

Result<bool> CCLaneEditor::clear_all_cc() {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    auto& cc_events = m_clip->get_cc_events_mutable();
    size_t removed_count = 0;
    
    auto it = std::remove_if(cc_events.begin(), cc_events.end(),
        [this, &removed_count](const MIDIControlChange& cc) {
            if (cc.controller == m_config.controller) {
                removed_count++;
                return true;
            }
            return false;
        });
    
    cc_events.erase(it, cc_events.end());
    
    if (removed_count > 0) {
        notify_edit_changed();
    }
    
    return Result<bool>::success(removed_count > 0);
}

std::vector<MIDIControlChange*> CCLaneEditor::get_cc_events() {
    if (!m_clip) {
        return {};
    }
    
    return m_clip->get_cc_events_for_controller(m_config.controller);
}

std::vector<MIDIControlChange*> CCLaneEditor::get_cc_events_in_range(double start_time_beats, double end_time_beats) {
    auto all_events = get_cc_events();
    std::vector<MIDIControlChange*> filtered_events;
    
    uint64_t start_samples = beats_to_samples(start_time_beats);
    uint64_t end_samples = beats_to_samples(end_time_beats);
    
    for (auto cc : all_events) {
        if (cc->time >= start_samples && cc->time <= end_samples) {
            filtered_events.push_back(cc);
        }
    }
    
    return filtered_events;
}

std::vector<MIDIControlChange*> CCLaneEditor::get_selected_cc_events() {
    auto all_events = get_cc_events();
    std::vector<MIDIControlChange*> selected_events;
    
    for (auto cc : all_events) {
        if (cc->selected) {
            selected_events.push_back(cc);
        }
    }
    
    return selected_events;
}

uint8_t CCLaneEditor::get_cc_value_at_time(double time_beats) const {
    if (!m_clip) {
        return m_config.default_value;
    }
    
    uint64_t time_samples = beats_to_samples(time_beats);
    auto cc_events = m_clip->get_cc_events_for_controller(m_config.controller);
    
    if (cc_events.empty()) {
        return m_config.default_value;
    }
    
    // Find the last CC event before or at the requested time
    MIDIControlChange* last_cc = nullptr;
    MIDIControlChange* next_cc = nullptr;
    
    for (auto cc : cc_events) {
        if (cc->time <= time_samples) {
            if (!last_cc || cc->time > last_cc->time) {
                last_cc = cc;
            }
        }
        if (cc->time > time_samples) {
            if (!next_cc || cc->time < next_cc->time) {
                next_cc = cc;
            }
        }
    }
    
    if (!last_cc) {
        return m_config.default_value;
    }
    
    if (!next_cc) {
        return last_cc->value;
    }
    
    // Interpolate between last_cc and next_cc
    return interpolate_cc_value(*last_cc, *next_cc, time_samples, CurveType::LINEAR);
}

Result<bool> CCLaneEditor::create_automation_shape(double start_time_beats, double end_time_beats, const std::string& shape_name) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    double duration_beats = end_time_beats - start_time_beats;
    if (duration_beats <= 0) {
        return Result<bool>::error("Invalid time range");
    }
    
    size_t point_count = static_cast<size_t>(std::max(8.0, duration_beats * 4.0)); // 4 points per beat minimum
    auto values = generate_shape_values(shape_name, point_count, m_config.min_value, m_config.max_value);
    
    if (values.empty()) {
        return Result<bool>::error("Unknown shape name: " + shape_name);
    }
    
    // Clear existing automation in the range
    erase_cc_in_range(start_time_beats, end_time_beats);
    
    // Add new automation points
    for (size_t i = 0; i < values.size(); ++i) {
        double t = start_time_beats + (duration_beats * i) / (values.size() - 1);
        auto result = draw_cc_point(t, values[i]);
        if (!result.is_success()) {
            return result;
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> CCLaneEditor::create_lfo_automation(double start_time_beats, double end_time_beats, double frequency_hz, uint8_t depth, uint8_t offset) {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    double duration_beats = end_time_beats - start_time_beats;
    if (duration_beats <= 0) {
        return Result<bool>::error("Invalid time range");
    }
    
    // Calculate number of points based on frequency
    double duration_seconds = (duration_beats / m_bpm) * 60.0;
    size_t point_count = static_cast<size_t>(std::max(16.0, frequency_hz * duration_seconds * 8.0)); // 8 points per cycle minimum
    
    // Clear existing automation in the range
    erase_cc_in_range(start_time_beats, end_time_beats);
    
    // Generate LFO values
    for (size_t i = 0; i < point_count; ++i) {
        double t = start_time_beats + (duration_beats * i) / (point_count - 1);
        double phase = (t - start_time_beats) / duration_beats * (frequency_hz * duration_seconds) * 2.0 * M_PI;
        
        double lfo_value = std::sin(phase);
        uint8_t cc_value = static_cast<uint8_t>(offset + (lfo_value * depth / 2));
        cc_value = std::clamp(cc_value, m_config.min_value, m_config.max_value);
        
        auto result = draw_cc_point(t, cc_value);
        if (!result.is_success()) {
            return result;
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

uint64_t CCLaneEditor::beats_to_samples(double beats) const {
    return MIDIClip::beats_to_samples(beats, m_bpm, 44100.0);
}

double CCLaneEditor::samples_to_beats(uint64_t samples) const {
    return MIDIClip::samples_to_beats(samples, m_bpm, 44100.0);
}

size_t CCLaneEditor::get_cc_event_count() const {
    return get_cc_events().size();
}

void CCLaneEditor::notify_edit_changed() {
    if (m_edit_callback) {
        m_edit_callback();
    }
}

uint8_t CCLaneEditor::interpolate_cc_value(const MIDIControlChange& start, const MIDIControlChange& end, uint64_t time, CurveType curve_type) const {
    if (time <= start.time) return start.value;
    if (time >= end.time) return end.value;
    
    double progress = static_cast<double>(time - start.time) / static_cast<double>(end.time - start.time);
    
    switch (curve_type) {
        case CurveType::LINEAR:
            return static_cast<uint8_t>(start.value + (end.value - start.value) * progress);
        case CurveType::EXPONENTIAL:
            return static_cast<uint8_t>(start.value + (end.value - start.value) * std::pow(progress, 2.0));
        case CurveType::LOGARITHMIC:
            return static_cast<uint8_t>(start.value + (end.value - start.value) * std::sqrt(progress));
        case CurveType::SMOOTH:
            return static_cast<uint8_t>(start.value + (end.value - start.value) * (std::sin((progress - 0.5) * M_PI) + 1.0) * 0.5);
        case CurveType::STEPPED:
            return (progress < 1.0) ? start.value : end.value;
        default:
            return static_cast<uint8_t>(start.value + (end.value - start.value) * progress);
    }
}

std::vector<uint8_t> CCLaneEditor::generate_shape_values(const std::string& shape_name, size_t point_count, uint8_t min_val, uint8_t max_val) const {
    std::vector<uint8_t> values;
    values.reserve(point_count);
    
    if (shape_name == "ramp_up") {
        for (size_t i = 0; i < point_count; ++i) {
            double progress = static_cast<double>(i) / (point_count - 1);
            values.push_back(static_cast<uint8_t>(min_val + (max_val - min_val) * progress));
        }
    } else if (shape_name == "ramp_down") {
        for (size_t i = 0; i < point_count; ++i) {
            double progress = static_cast<double>(i) / (point_count - 1);
            values.push_back(static_cast<uint8_t>(max_val - (max_val - min_val) * progress));
        }
    } else if (shape_name == "triangle") {
        for (size_t i = 0; i < point_count; ++i) {
            double progress = static_cast<double>(i) / (point_count - 1);
            double triangle_value = (progress <= 0.5) ? (progress * 2.0) : ((1.0 - progress) * 2.0);
            values.push_back(static_cast<uint8_t>(min_val + (max_val - min_val) * triangle_value));
        }
    } else if (shape_name == "sine") {
        for (size_t i = 0; i < point_count; ++i) {
            double progress = static_cast<double>(i) / (point_count - 1);
            double sine_value = (std::sin(progress * 2.0 * M_PI - M_PI_2) + 1.0) * 0.5;
            values.push_back(static_cast<uint8_t>(min_val + (max_val - min_val) * sine_value));
        }
    } else if (shape_name == "sawtooth") {
        for (size_t i = 0; i < point_count; ++i) {
            double progress = static_cast<double>(i) / (point_count - 1);
            double sawtooth_value = progress - std::floor(progress);
            values.push_back(static_cast<uint8_t>(min_val + (max_val - min_val) * sawtooth_value));
        }
    }
    
    return values;
}

// CCLaneManager implementation
CCLaneManager::CCLaneManager(std::shared_ptr<MIDIClip> clip) : m_clip(clip) {
}

void CCLaneManager::set_clip(std::shared_ptr<MIDIClip> clip) {
    m_clip = clip;
    
    // Update all existing lanes with the new clip
    for (auto& [controller, lane] : m_lanes) {
        lane->set_clip(clip);
    }
}

Result<std::shared_ptr<CCLaneEditor>> CCLaneManager::add_cc_lane(uint8_t controller, const std::string& name) {
    if (m_lanes.find(controller) != m_lanes.end()) {
        return Result<std::shared_ptr<CCLaneEditor>>::error("CC lane already exists for controller " + std::to_string(controller));
    }
    
    CCLaneConfig config;
    config.controller = controller;
    config.name = name.empty() ? ("CC " + std::to_string(controller)) : name;
    
    auto lane = std::make_shared<CCLaneEditor>(m_clip, config);
    lane->set_edit_callback([this]() { notify_edit_changed(); });
    
    m_lanes[controller] = lane;
    m_lane_order.push_back(controller);
    
    notify_edit_changed();
    return Result<std::shared_ptr<CCLaneEditor>>::success(lane);
}

Result<bool> CCLaneManager::remove_cc_lane(uint8_t controller) {
    auto it = m_lanes.find(controller);
    if (it == m_lanes.end()) {
        return Result<bool>::error("CC lane not found for controller " + std::to_string(controller));
    }
    
    m_lanes.erase(it);
    
    // Remove from order list
    auto order_it = std::find(m_lane_order.begin(), m_lane_order.end(), controller);
    if (order_it != m_lane_order.end()) {
        m_lane_order.erase(order_it);
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

std::shared_ptr<CCLaneEditor> CCLaneManager::get_cc_lane(uint8_t controller) {
    auto it = m_lanes.find(controller);
    return (it != m_lanes.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<CCLaneEditor>> CCLaneManager::get_all_lanes() const {
    std::vector<std::shared_ptr<CCLaneEditor>> result;
    
    // Return lanes in the specified order
    for (uint8_t controller : m_lane_order) {
        auto it = m_lanes.find(controller);
        if (it != m_lanes.end()) {
            result.push_back(it->second);
        }
    }
    
    return result;
}

void CCLaneManager::setup_standard_lanes() {
    add_cc_lane(1, "Mod Wheel");
    add_cc_lane(11, "Expression");
    add_cc_lane(10, "Pan");
    add_cc_lane(7, "Volume");
}

void CCLaneManager::setup_filter_lanes() {
    add_cc_lane(74, "Cutoff");
    add_cc_lane(71, "Resonance");
    add_cc_lane(72, "Release Time");
    add_cc_lane(73, "Attack Time");
}

void CCLaneManager::setup_synth_lanes() {
    add_cc_lane(73, "Attack");
    add_cc_lane(75, "Decay");
    add_cc_lane(79, "Sustain");
    add_cc_lane(72, "Release");
}

void CCLaneManager::setup_performance_lanes() {
    add_cc_lane(1, "Mod Wheel");
    add_cc_lane(2, "Breath");
    add_cc_lane(11, "Expression");
    add_cc_lane(64, "Sustain");
}

CCLaneConfig CCLaneManager::create_standard_cc_config(StandardCC cc) {
    CCLaneConfig config;
    config.controller = static_cast<uint8_t>(cc);
    
    switch (cc) {
        case StandardCC::MOD_WHEEL:
            config.name = "Mod Wheel";
            config.color = 0xFF4080FF;
            break;
        case StandardCC::EXPRESSION:
            config.name = "Expression";
            config.color = 0xFF40FF80;
            break;
        case StandardCC::VOLUME:
            config.name = "Volume";
            config.default_value = 100;
            config.color = 0xFFFF8040;
            break;
        case StandardCC::PAN:
            config.name = "Pan";
            config.default_value = 64;
            config.color = 0xFFFF4040;
            break;
        case StandardCC::SUSTAIN:
            config.name = "Sustain";
            config.is_toggle = true;
            config.color = 0xFF8040FF;
            break;
        case StandardCC::BRIGHTNESS:
            config.name = "Cutoff";
            config.default_value = 64;
            config.color = 0xFFFFFF40;
            break;
        case StandardCC::HARMONIC_CONTENT:
            config.name = "Resonance";
            config.default_value = 40;
            config.color = 0xFF40FFFF;
            break;
        default:
            config.name = "CC " + std::to_string(static_cast<uint8_t>(cc));
            break;
    }
    
    return config;
}

CCLaneConfig CCLaneManager::create_custom_cc_config(uint8_t controller, const std::string& name) {
    CCLaneConfig config;
    config.controller = controller;
    config.name = name;
    config.color = 0xFF8080C0;
    return config;
}

void CCLaneManager::notify_edit_changed() {
    if (m_edit_callback) {
        m_edit_callback();
    }
}

} // namespace mixmind