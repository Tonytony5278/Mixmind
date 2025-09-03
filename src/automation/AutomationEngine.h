#pragma once

#include "AutomationData.h"
#include "../core/result.h"
#include "../core/audio_types.h"
#include <memory>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>

namespace mixmind {

// Forward declarations
class VSTiHost;
class MIDIProcessor;

// Parameter target for automation
struct AutomationTarget {
    enum TargetType {
        VST_PARAMETER,      // VST3 plugin parameter
        TRACK_VOLUME,       // Track volume control
        TRACK_PAN,          // Track pan control
        TRACK_MUTE,         // Track mute state
        TRACK_SOLO,         // Track solo state
        SEND_LEVEL,         // Send level
        SEND_PAN,           // Send pan
        MIDI_CC_OUTPUT      // MIDI CC output
    };
    
    TargetType type;
    uint32_t track_id;
    uint32_t parameter_index;
    uint32_t plugin_instance_id;
    
    // Target object references (weak references to avoid circular dependencies)
    std::weak_ptr<VSTiHost> vst_host;
    std::weak_ptr<MIDIProcessor> midi_processor;
    
    AutomationTarget(TargetType t = VST_PARAMETER, uint32_t track = 0, uint32_t param = 0, uint32_t plugin = 0)
        : type(t), track_id(track), parameter_index(param), plugin_instance_id(plugin) {}
};

// Automation engine for real-time parameter modulation
class AutomationEngine {
public:
    AutomationEngine(std::shared_ptr<AutomationData> automation_data);
    ~AutomationEngine();
    
    // Engine control
    Result<bool> start_playback();
    Result<bool> stop_playback();
    bool is_playing() const { return m_is_playing.load(); }
    
    Result<bool> enable_automation();
    Result<bool> disable_automation();
    bool is_automation_enabled() const { return m_automation_enabled.load(); }
    
    // Transport control
    void set_playback_position(uint64_t position_samples);
    uint64_t get_playback_position() const { return m_playback_position.load(); }
    
    void set_playback_rate(double rate) { m_playback_rate.store(rate); }
    double get_playback_rate() const { return m_playback_rate.load(); }
    
    void set_loop_range(uint64_t start_samples, uint64_t end_samples);
    void set_loop_enabled(bool enabled) { m_loop_enabled.store(enabled); }
    bool is_loop_enabled() const { return m_loop_enabled.load(); }
    
    // Parameter target registration
    Result<bool> register_automation_target(const AutomationParameterId& param_id, const AutomationTarget& target);
    Result<bool> unregister_automation_target(const AutomationParameterId& param_id);
    Result<bool> update_automation_target(const AutomationParameterId& param_id, const AutomationTarget& target);
    
    std::vector<AutomationParameterId> get_registered_parameters() const;
    bool is_parameter_registered(const AutomationParameterId& param_id) const;
    
    // Real-time automation processing
    void process_automation_block(uint64_t start_time_samples, uint64_t end_time_samples, uint32_t buffer_size);
    
    // Parameter value access (for UI display)
    double get_current_parameter_value(const AutomationParameterId& param_id) const;
    std::map<AutomationParameterId, double> get_all_current_parameter_values() const;
    
    // Automation lane control
    Result<bool> enable_lane(const AutomationParameterId& param_id);
    Result<bool> disable_lane(const AutomationParameterId& param_id);
    bool is_lane_enabled(const AutomationParameterId& param_id) const;
    
    // Performance settings
    void set_interpolation_quality(int quality) { m_interpolation_quality = std::clamp(quality, 1, 4); }
    int get_interpolation_quality() const { return m_interpolation_quality; }
    
    void set_update_rate_hz(double rate) { m_update_rate_hz = std::clamp(rate, 60.0, 1000.0); }
    double get_update_rate_hz() const { return m_update_rate_hz; }
    
    // Automation smoothing (to avoid zipper noise)
    void set_smoothing_enabled(bool enabled) { m_smoothing_enabled = enabled; }
    bool is_smoothing_enabled() const { return m_smoothing_enabled; }
    
    void set_smoothing_time_ms(double time_ms) { m_smoothing_time_ms = std::clamp(time_ms, 1.0, 100.0); }
    double get_smoothing_time_ms() const { return m_smoothing_time_ms; }
    
    // Read mode for lanes (prevents automation from writing while recording)
    void set_lane_read_mode(const AutomationParameterId& param_id, bool read_mode);
    bool is_lane_in_read_mode(const AutomationParameterId& param_id) const;
    
    // Statistics and monitoring
    size_t get_active_lane_count() const;
    size_t get_registered_target_count() const;
    
    struct PerformanceStats {
        double cpu_usage_percent;
        uint32_t parameters_processed;
        uint32_t automation_events_sent;
        std::chrono::microseconds processing_time;
        uint32_t buffer_underruns;
    };
    
    PerformanceStats get_performance_stats() const { return m_performance_stats; }
    void reset_performance_stats();
    
    // Event callbacks
    using ParameterChangedCallback = std::function<void(const AutomationParameterId&, double)>;
    using AutOverrideCallback = std::function<void(const AutomationParameterId&, bool)>;
    
    void set_parameter_changed_callback(ParameterChangedCallback callback) { m_parameter_changed_callback = callback; }
    void set_automation_override_callback(AutOverrideCallback callback) { m_automation_override_callback = callback; }
    
    // Manual parameter override (for UI control during playback)
    void override_parameter(const AutomationParameterId& param_id, double value, bool temporary = true);
    void release_parameter_override(const AutomationParameterId& param_id);
    bool is_parameter_overridden(const AutomationParameterId& param_id) const;
    
private:
    std::shared_ptr<AutomationData> m_automation_data;
    
    // Playback state
    std::atomic<bool> m_is_playing{false};
    std::atomic<bool> m_automation_enabled{true};
    std::atomic<uint64_t> m_playback_position{0};
    std::atomic<double> m_playback_rate{1.0};
    
    // Loop settings
    std::atomic<bool> m_loop_enabled{false};
    uint64_t m_loop_start = 0;
    uint64_t m_loop_end = UINT64_MAX;
    
    // Parameter targets
    mutable std::mutex m_targets_mutex;
    std::map<AutomationParameterId, AutomationTarget> m_automation_targets;
    
    // Lane states
    mutable std::mutex m_lane_states_mutex;
    std::map<AutomationParameterId, bool> m_lane_enabled_states;
    std::map<AutomationParameterId, bool> m_lane_read_modes;
    
    // Parameter value cache (for smoothing and current value queries)
    mutable std::mutex m_value_cache_mutex;
    std::map<AutomationParameterId, double> m_current_parameter_values;
    std::map<AutomationParameterId, double> m_target_parameter_values;
    std::map<AutomationParameterId, double> m_smoothing_coefficients;
    
    // Parameter overrides
    mutable std::mutex m_overrides_mutex;
    std::map<AutomationParameterId, double> m_parameter_overrides;
    std::map<AutomationParameterId, bool> m_temporary_overrides;
    
    // Performance settings
    int m_interpolation_quality = 2;
    double m_update_rate_hz = 200.0;
    bool m_smoothing_enabled = true;
    double m_smoothing_time_ms = 10.0;
    
    // Performance monitoring
    mutable std::mutex m_stats_mutex;
    PerformanceStats m_performance_stats{};
    std::chrono::high_resolution_clock::time_point m_last_stats_reset;
    
    // Event callbacks
    ParameterChangedCallback m_parameter_changed_callback;
    AutOverrideCallback m_automation_override_callback;
    
    // Processing methods
    void process_parameter_automation(const AutomationParameterId& param_id, 
                                      std::shared_ptr<AutomationLane> lane,
                                      uint64_t start_time, uint64_t end_time,
                                      uint32_t buffer_size);
    
    void apply_parameter_value(const AutomationParameterId& param_id, double value);
    void apply_parameter_smoothing(const AutomationParameterId& param_id, double target_value, uint32_t buffer_size);
    
    bool send_parameter_to_target(const AutomationParameterId& param_id, const AutomationTarget& target, double value);
    
    // Helper methods
    double calculate_smoothing_coefficient(double time_ms, double sample_rate, uint32_t buffer_size);
    void update_performance_stats(std::chrono::microseconds processing_time, uint32_t parameters_processed);
    void notify_parameter_changed(const AutomationParameterId& param_id, double value);
    void notify_automation_override(const AutomationParameterId& param_id, bool overridden);
};

// Automation engine factory for different use cases
class AutomationEngineFactory {
public:
    // Create engine for high-quality playback (higher CPU usage)
    static std::unique_ptr<AutomationEngine> create_high_quality_engine(std::shared_ptr<AutomationData> automation_data);
    
    // Create engine for low-latency performance (optimized for speed)
    static std::unique_ptr<AutomationEngine> create_low_latency_engine(std::shared_ptr<AutomationData> automation_data);
    
    // Create engine for mixing/mastering (maximum precision)
    static std::unique_ptr<AutomationEngine> create_mixing_engine(std::shared_ptr<AutomationData> automation_data);
};

// Automation parameter mapper - converts between different parameter formats
class AutomationParameterMapper {
public:
    // VST parameter mapping
    static double map_to_vst_parameter(double normalized_value, double min_value, double max_value);
    static double map_from_vst_parameter(double vst_value, double min_value, double max_value);
    
    // Track parameter mapping
    static double map_to_track_volume_db(double normalized_value);      // Convert to dB
    static double map_from_track_volume_db(double db_value);            // Convert from dB
    
    static double map_to_track_pan_position(double normalized_value);   // Convert to -1.0 to 1.0
    static double map_from_track_pan_position(double pan_position);     // Convert from pan position
    
    // MIDI CC mapping
    static uint8_t map_to_midi_cc(double normalized_value);
    static double map_from_midi_cc(uint8_t cc_value);
    
    // Frequency mapping (for filters)
    static double map_to_frequency_hz(double normalized_value, double min_freq = 20.0, double max_freq = 20000.0);
    static double map_from_frequency_hz(double frequency_hz, double min_freq = 20.0, double max_freq = 20000.0);
    
    // Time-based mapping (for delays, envelopes)
    static double map_to_time_seconds(double normalized_value, double min_time = 0.001, double max_time = 10.0);
    static double map_from_time_seconds(double time_seconds, double min_time = 0.001, double max_time = 10.0);
    
    // Percentage mapping
    static double map_to_percentage(double normalized_value);
    static double map_from_percentage(double percentage);
    
    // Logarithmic mapping (for gain, resonance, etc.)
    static double map_to_logarithmic(double normalized_value, double min_value = 0.01, double max_value = 10.0);
    static double map_from_logarithmic(double log_value, double min_value = 0.01, double max_value = 10.0);
};

} // namespace mixmind