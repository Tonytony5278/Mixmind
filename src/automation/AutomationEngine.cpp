#include "AutomationEngine.h"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace mixmind {

AutomationEngine::AutomationEngine(std::shared_ptr<AutomationData> automation_data)
    : m_automation_data(automation_data) {
    
    // Initialize performance stats
    reset_performance_stats();
}

AutomationEngine::~AutomationEngine() {
    stop_playback();
}

Result<bool> AutomationEngine::start_playback() {
    if (m_is_playing.load()) {
        return Result<bool>::error("Already playing");
    }
    
    m_is_playing.store(true);
    
    // Reset smoothing states
    {
        std::lock_guard<std::mutex> lock(m_value_cache_mutex);
        m_target_parameter_values.clear();
        m_smoothing_coefficients.clear();
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationEngine::stop_playback() {
    if (!m_is_playing.load()) {
        return Result<bool>::error("Not currently playing");
    }
    
    m_is_playing.store(false);
    return Result<bool>::success(true);
}

Result<bool> AutomationEngine::enable_automation() {
    m_automation_enabled.store(true);
    return Result<bool>::success(true);
}

Result<bool> AutomationEngine::disable_automation() {
    m_automation_enabled.store(false);
    return Result<bool>::success(true);
}

void AutomationEngine::set_playback_position(uint64_t position_samples) {
    m_playback_position.store(position_samples);
    
    // Handle looping
    if (m_loop_enabled.load() && position_samples >= m_loop_end) {
        uint64_t loop_length = m_loop_end - m_loop_start;
        if (loop_length > 0) {
            uint64_t wrapped_position = m_loop_start + ((position_samples - m_loop_start) % loop_length);
            m_playback_position.store(wrapped_position);
        }
    }
}

void AutomationEngine::set_loop_range(uint64_t start_samples, uint64_t end_samples) {
    if (end_samples > start_samples) {
        m_loop_start = start_samples;
        m_loop_end = end_samples;
    }
}

Result<bool> AutomationEngine::register_automation_target(const AutomationParameterId& param_id, const AutomationTarget& target) {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    
    if (m_automation_targets.find(param_id) != m_automation_targets.end()) {
        return Result<bool>::error("Parameter already registered");
    }
    
    m_automation_targets[param_id] = target;
    
    // Initialize lane state
    {
        std::lock_guard<std::mutex> state_lock(m_lane_states_mutex);
        m_lane_enabled_states[param_id] = true;
        m_lane_read_modes[param_id] = false;
    }
    
    return Result<bool>::success(true);
}

Result<bool> AutomationEngine::unregister_automation_target(const AutomationParameterId& param_id) {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    
    auto it = m_automation_targets.find(param_id);
    if (it == m_automation_targets.end()) {
        return Result<bool>::error("Parameter not registered");
    }
    
    m_automation_targets.erase(it);
    
    // Clean up lane state
    {
        std::lock_guard<std::mutex> state_lock(m_lane_states_mutex);
        m_lane_enabled_states.erase(param_id);
        m_lane_read_modes.erase(param_id);
    }
    
    // Clean up value cache
    {
        std::lock_guard<std::mutex> cache_lock(m_value_cache_mutex);
        m_current_parameter_values.erase(param_id);
        m_target_parameter_values.erase(param_id);
        m_smoothing_coefficients.erase(param_id);
    }
    
    return Result<bool>::success(true);
}

std::vector<AutomationParameterId> AutomationEngine::get_registered_parameters() const {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    
    std::vector<AutomationParameterId> parameters;
    parameters.reserve(m_automation_targets.size());
    
    for (const auto& [param_id, target] : m_automation_targets) {
        parameters.push_back(param_id);
    }
    
    return parameters;
}

bool AutomationEngine::is_parameter_registered(const AutomationParameterId& param_id) const {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    return m_automation_targets.find(param_id) != m_automation_targets.end();
}

void AutomationEngine::process_automation_block(uint64_t start_time_samples, uint64_t end_time_samples, uint32_t buffer_size) {
    if (!m_is_playing.load() || !m_automation_enabled.load() || !m_automation_data) {
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    uint32_t parameters_processed = 0;
    uint32_t events_sent = 0;
    
    // Get all automation lanes
    auto all_lanes = m_automation_data->get_all_lanes();
    
    for (const auto& lane : all_lanes) {
        if (!lane || !is_lane_enabled(lane->get_parameter_id())) {
            continue;
        }
        
        // Skip lanes in read mode during recording
        if (is_lane_in_read_mode(lane->get_parameter_id())) {
            continue;
        }
        
        process_parameter_automation(lane->get_parameter_id(), lane, start_time_samples, end_time_samples, buffer_size);
        parameters_processed++;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    update_performance_stats(processing_time, parameters_processed);
}

double AutomationEngine::get_current_parameter_value(const AutomationParameterId& param_id) const {
    std::lock_guard<std::mutex> lock(m_value_cache_mutex);
    
    auto it = m_current_parameter_values.find(param_id);
    if (it != m_current_parameter_values.end()) {
        return it->second;
    }
    
    // Return automation data value if not cached
    if (m_automation_data) {
        auto lane = m_automation_data->get_lane(param_id);
        if (lane) {
            uint64_t current_time = m_playback_position.load();
            return lane->get_value_at_time(current_time);
        }
    }
    
    return 0.0; // Default value
}

std::map<AutomationParameterId, double> AutomationEngine::get_all_current_parameter_values() const {
    std::lock_guard<std::mutex> lock(m_value_cache_mutex);
    return m_current_parameter_values;
}

Result<bool> AutomationEngine::enable_lane(const AutomationParameterId& param_id) {
    std::lock_guard<std::mutex> lock(m_lane_states_mutex);
    m_lane_enabled_states[param_id] = true;
    return Result<bool>::success(true);
}

Result<bool> AutomationEngine::disable_lane(const AutomationParameterId& param_id) {
    std::lock_guard<std::mutex> lock(m_lane_states_mutex);
    m_lane_enabled_states[param_id] = false;
    return Result<bool>::success(true);
}

bool AutomationEngine::is_lane_enabled(const AutomationParameterId& param_id) const {
    std::lock_guard<std::mutex> lock(m_lane_states_mutex);
    
    auto it = m_lane_enabled_states.find(param_id);
    return (it != m_lane_enabled_states.end()) ? it->second : true; // Default enabled
}

void AutomationEngine::set_lane_read_mode(const AutomationParameterId& param_id, bool read_mode) {
    std::lock_guard<std::mutex> lock(m_lane_states_mutex);
    m_lane_read_modes[param_id] = read_mode;
}

bool AutomationEngine::is_lane_in_read_mode(const AutomationParameterId& param_id) const {
    std::lock_guard<std::mutex> lock(m_lane_states_mutex);
    
    auto it = m_lane_read_modes.find(param_id);
    return (it != m_lane_read_modes.end()) ? it->second : false; // Default not in read mode
}

size_t AutomationEngine::get_active_lane_count() const {
    std::lock_guard<std::mutex> lock(m_lane_states_mutex);
    
    size_t active_count = 0;
    for (const auto& [param_id, enabled] : m_lane_enabled_states) {
        if (enabled && !is_lane_in_read_mode(param_id)) {
            active_count++;
        }
    }
    
    return active_count;
}

size_t AutomationEngine::get_registered_target_count() const {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    return m_automation_targets.size();
}

void AutomationEngine::reset_performance_stats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_performance_stats = PerformanceStats{};
    m_last_stats_reset = std::chrono::high_resolution_clock::now();
}

void AutomationEngine::override_parameter(const AutomationParameterId& param_id, double value, bool temporary) {
    std::lock_guard<std::mutex> lock(m_overrides_mutex);
    
    double clamped_value = std::clamp(value, 0.0, 1.0);
    m_parameter_overrides[param_id] = clamped_value;
    m_temporary_overrides[param_id] = temporary;
    
    // Apply the override immediately
    apply_parameter_value(param_id, clamped_value);
    
    notify_automation_override(param_id, true);
}

void AutomationEngine::release_parameter_override(const AutomationParameterId& param_id) {
    std::lock_guard<std::mutex> lock(m_overrides_mutex);
    
    m_parameter_overrides.erase(param_id);
    m_temporary_overrides.erase(param_id);
    
    notify_automation_override(param_id, false);
}

bool AutomationEngine::is_parameter_overridden(const AutomationParameterId& param_id) const {
    std::lock_guard<std::mutex> lock(m_overrides_mutex);
    return m_parameter_overrides.find(param_id) != m_parameter_overrides.end();
}

void AutomationEngine::process_parameter_automation(const AutomationParameterId& param_id, 
                                                     std::shared_ptr<AutomationLane> lane,
                                                     uint64_t start_time, uint64_t end_time,
                                                     uint32_t buffer_size) {
    if (!lane) return;
    
    // Check for parameter override
    {
        std::lock_guard<std::mutex> override_lock(m_overrides_mutex);
        if (m_parameter_overrides.find(param_id) != m_parameter_overrides.end()) {
            return; // Skip automation if parameter is overridden
        }
    }
    
    // Get automation value at current time
    uint64_t current_time = m_playback_position.load();
    double automation_value = lane->get_value_at_time(current_time);
    
    // Apply smoothing if enabled
    if (m_smoothing_enabled) {
        apply_parameter_smoothing(param_id, automation_value, buffer_size);
    } else {
        apply_parameter_value(param_id, automation_value);
    }
}

void AutomationEngine::apply_parameter_value(const AutomationParameterId& param_id, double value) {
    // Update value cache
    {
        std::lock_guard<std::mutex> lock(m_value_cache_mutex);
        m_current_parameter_values[param_id] = value;
    }
    
    // Send to target
    {
        std::lock_guard<std::mutex> lock(m_targets_mutex);
        auto target_it = m_automation_targets.find(param_id);
        if (target_it != m_automation_targets.end()) {
            bool sent = send_parameter_to_target(param_id, target_it->second, value);
            if (sent) {
                // Update performance stats
                std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
                m_performance_stats.automation_events_sent++;
            }
        }
    }
    
    notify_parameter_changed(param_id, value);
}

void AutomationEngine::apply_parameter_smoothing(const AutomationParameterId& param_id, double target_value, uint32_t buffer_size) {
    std::lock_guard<std::mutex> lock(m_value_cache_mutex);
    
    // Get current smoothed value
    double current_value = 0.0;
    auto current_it = m_current_parameter_values.find(param_id);
    if (current_it != m_current_parameter_values.end()) {
        current_value = current_it->second;
    }
    
    // Calculate or get smoothing coefficient
    double smoothing_coeff = 0.0;
    auto coeff_it = m_smoothing_coefficients.find(param_id);
    if (coeff_it != m_smoothing_coefficients.end()) {
        smoothing_coeff = coeff_it->second;
    } else {
        smoothing_coeff = calculate_smoothing_coefficient(m_smoothing_time_ms, 44100.0, buffer_size);
        m_smoothing_coefficients[param_id] = smoothing_coeff;
    }
    
    // Apply exponential smoothing
    double smoothed_value = current_value + smoothing_coeff * (target_value - current_value);
    
    // Update cache
    m_current_parameter_values[param_id] = smoothed_value;
    m_target_parameter_values[param_id] = target_value;
    
    // Send smoothed value to target (unlock before sending)
    lock.~lock_guard();
    apply_parameter_value(param_id, smoothed_value);
}

bool AutomationEngine::send_parameter_to_target(const AutomationParameterId& param_id, const AutomationTarget& target, double value) {
    switch (target.type) {
        case AutomationTarget::VST_PARAMETER: {
            auto vst_host = target.vst_host.lock();
            if (vst_host) {
                // Convert normalized value to VST parameter value
                // This would typically require knowledge of the parameter's range
                // For now, assume the value is already in the correct range
                // vst_host->setParameter(target.parameter_index, value);
                return true;
            }
            break;
        }
        
        case AutomationTarget::TRACK_VOLUME: {
            // Convert normalized value to track volume (typically 0.0-2.0 linear or dB)
            double track_volume = AutomationUtils::denormalize_track_volume(value);
            // Apply to track volume control
            // track->setVolume(track_volume);
            return true;
        }
        
        case AutomationTarget::TRACK_PAN: {
            // Convert normalized value to pan position (-1.0 to 1.0)
            double pan_position = AutomationUtils::denormalize_track_pan(value);
            // Apply to track pan control
            // track->setPan(pan_position);
            return true;
        }
        
        case AutomationTarget::MIDI_CC_OUTPUT: {
            auto midi_processor = target.midi_processor.lock();
            if (midi_processor) {
                // Convert normalized value to MIDI CC value
                uint8_t cc_value = AutomationUtils::denormalize_midi_cc(value);
                // Send MIDI CC message
                // midi_processor->sendCC(target.parameter_index, cc_value);
                return true;
            }
            break;
        }
        
        // Add other target types as needed
        default:
            break;
    }
    
    return false;
}

double AutomationEngine::calculate_smoothing_coefficient(double time_ms, double sample_rate, uint32_t buffer_size) {
    // Calculate coefficient for exponential smoothing
    double time_constant_samples = (time_ms / 1000.0) * sample_rate;
    double buffer_time_samples = static_cast<double>(buffer_size);
    
    // Exponential decay coefficient
    return 1.0 - std::exp(-buffer_time_samples / time_constant_samples);
}

void AutomationEngine::update_performance_stats(std::chrono::microseconds processing_time, uint32_t parameters_processed) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_performance_stats.processing_time = processing_time;
    m_performance_stats.parameters_processed = parameters_processed;
    
    // Calculate CPU usage as a percentage of available time
    // Assume 44.1kHz sample rate and typical buffer sizes
    double buffer_time_us = (512.0 / 44100.0) * 1000000.0; // ~11.6ms for 512 samples
    m_performance_stats.cpu_usage_percent = (processing_time.count() / buffer_time_us) * 100.0;
}

void AutomationEngine::notify_parameter_changed(const AutomationParameterId& param_id, double value) {
    if (m_parameter_changed_callback) {
        m_parameter_changed_callback(param_id, value);
    }
}

void AutomationEngine::notify_automation_override(const AutomationParameterId& param_id, bool overridden) {
    if (m_automation_override_callback) {
        m_automation_override_callback(param_id, overridden);
    }
}

// AutomationEngineFactory implementations
std::unique_ptr<AutomationEngine> AutomationEngineFactory::create_high_quality_engine(std::shared_ptr<AutomationData> automation_data) {
    auto engine = std::make_unique<AutomationEngine>(automation_data);
    
    // High quality settings
    engine->set_interpolation_quality(4);      // Maximum interpolation quality
    engine->set_update_rate_hz(500.0);         // High update rate
    engine->set_smoothing_enabled(true);
    engine->set_smoothing_time_ms(5.0);        // Fast but smooth transitions
    
    return engine;
}

std::unique_ptr<AutomationEngine> AutomationEngineFactory::create_low_latency_engine(std::shared_ptr<AutomationData> automation_data) {
    auto engine = std::make_unique<AutomationEngine>(automation_data);
    
    // Low latency settings
    engine->set_interpolation_quality(1);      // Minimal interpolation
    engine->set_update_rate_hz(1000.0);        // Very high update rate
    engine->set_smoothing_enabled(false);      // No smoothing for minimal latency
    
    return engine;
}

std::unique_ptr<AutomationEngine> AutomationEngineFactory::create_mixing_engine(std::shared_ptr<AutomationData> automation_data) {
    auto engine = std::make_unique<AutomationEngine>(automation_data);
    
    // Mixing/mastering settings
    engine->set_interpolation_quality(4);      // Maximum precision
    engine->set_update_rate_hz(200.0);         // Moderate update rate
    engine->set_smoothing_enabled(true);
    engine->set_smoothing_time_ms(20.0);       // Longer smoothing for musical transitions
    
    return engine;
}

// AutomationParameterMapper implementations
double AutomationParameterMapper::map_to_track_volume_db(double normalized_value) {
    if (normalized_value <= 0.0) return -60.0; // Minimum -60dB
    
    // Map 0.0-1.0 to -60dB to +12dB
    return -60.0 + (normalized_value * 72.0);
}

double AutomationParameterMapper::map_from_track_volume_db(double db_value) {
    // Map -60dB to +12dB to 0.0-1.0
    double clamped_db = std::clamp(db_value, -60.0, 12.0);
    return (clamped_db + 60.0) / 72.0;
}

double AutomationParameterMapper::map_to_track_pan_position(double normalized_value) {
    // Map 0.0-1.0 to -1.0-1.0 (left-right)
    return (normalized_value * 2.0) - 1.0;
}

double AutomationParameterMapper::map_from_track_pan_position(double pan_position) {
    // Map -1.0-1.0 to 0.0-1.0
    return (pan_position + 1.0) * 0.5;
}

uint8_t AutomationParameterMapper::map_to_midi_cc(double normalized_value) {
    return static_cast<uint8_t>(std::clamp(normalized_value * 127.0, 0.0, 127.0));
}

double AutomationParameterMapper::map_from_midi_cc(uint8_t cc_value) {
    return static_cast<double>(cc_value) / 127.0;
}

double AutomationParameterMapper::map_to_frequency_hz(double normalized_value, double min_freq, double max_freq) {
    // Logarithmic mapping for frequency
    double log_min = std::log(min_freq);
    double log_max = std::log(max_freq);
    double log_freq = log_min + normalized_value * (log_max - log_min);
    return std::exp(log_freq);
}

double AutomationParameterMapper::map_from_frequency_hz(double frequency_hz, double min_freq, double max_freq) {
    double clamped_freq = std::clamp(frequency_hz, min_freq, max_freq);
    double log_min = std::log(min_freq);
    double log_max = std::log(max_freq);
    double log_freq = std::log(clamped_freq);
    return (log_freq - log_min) / (log_max - log_min);
}

double AutomationParameterMapper::map_to_percentage(double normalized_value) {
    return normalized_value * 100.0;
}

double AutomationParameterMapper::map_from_percentage(double percentage) {
    return std::clamp(percentage, 0.0, 100.0) / 100.0;
}

double AutomationParameterMapper::map_to_logarithmic(double normalized_value, double min_value, double max_value) {
    double log_min = std::log(min_value);
    double log_max = std::log(max_value);
    double log_val = log_min + normalized_value * (log_max - log_min);
    return std::exp(log_val);
}

double AutomationParameterMapper::map_from_logarithmic(double log_value, double min_value, double max_value) {
    double clamped_val = std::clamp(log_value, min_value, max_value);
    double log_min = std::log(min_value);
    double log_max = std::log(max_value);
    double log_val = std::log(clamped_val);
    return (log_val - log_min) / (log_max - log_min);
}

} // namespace mixmind