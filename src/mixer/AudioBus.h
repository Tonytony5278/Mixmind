#pragma once

#include "MixerTypes.h"
#include "../core/result.h"
#include "../core/audio_types.h"
#include "../audio/AudioBuffer.h"
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

namespace mixmind {

// Forward declarations
class AudioEffect;
class MeterProcessor;

// Audio bus for routing and mixing
class AudioBus {
public:
    AudioBus(uint32_t bus_id, const BusConfig& config);
    ~AudioBus();
    
    // Bus identification
    uint32_t get_bus_id() const { return m_bus_id; }
    void set_name(const std::string& name) { m_config.name = name; }
    std::string get_name() const { return m_config.name; }
    
    // Bus configuration
    Result<bool> set_config(const BusConfig& config);
    BusConfig get_config() const;
    
    Result<bool> set_channel_count(uint32_t channels);
    uint32_t get_channel_count() const { return m_config.channel_count; }
    
    // Bus type and behavior
    BusConfig::BusType get_bus_type() const { return m_config.type; }
    Result<bool> set_bus_type(BusConfig::BusType type);
    
    // Volume and pan controls
    void set_volume_db(double volume_db);
    double get_volume_db() const { return m_config.volume_db; }
    double get_volume_linear() const;
    
    void set_pan_position(double pan);  // -1.0 to 1.0
    double get_pan_position() const { return m_config.pan_position; }
    
    // Mute and solo
    void set_mute(bool mute) { m_config.mute = mute; }
    bool is_muted() const { return m_config.mute; }
    
    void set_solo(bool solo) { m_config.solo = solo; }
    bool is_soloed() const { return m_config.solo; }
    
    // Input management (for receiving audio from tracks/sends)
    Result<bool> add_input_source(uint32_t source_id, double level = 1.0);
    Result<bool> remove_input_source(uint32_t source_id);
    Result<bool> set_input_level(uint32_t source_id, double level);
    double get_input_level(uint32_t source_id) const;
    
    std::vector<uint32_t> get_input_sources() const;
    size_t get_input_count() const;
    
    // Output routing
    Result<bool> add_output_destination(const RouteDestination& destination);
    Result<bool> remove_output_destination(uint32_t destination_id);
    Result<bool> update_output_destination(const RouteDestination& destination);
    
    std::vector<RouteDestination> get_output_destinations() const;
    size_t get_output_count() const;
    
    // Audio processing
    Result<bool> process_audio(std::shared_ptr<AudioBuffer> input_buffer,
                               std::shared_ptr<AudioBuffer> output_buffer,
                               uint64_t start_time_samples,
                               uint32_t buffer_size);
    
    // Clear audio buffers (for muted buses or initialization)
    void clear_audio_buffers();
    
    // Plugin Delay Compensation
    void set_delay_compensation_samples(uint32_t samples);
    uint32_t get_delay_compensation_samples() const;
    double get_delay_compensation_ms() const;
    
    // Effects chain management
    Result<bool> add_effect(std::shared_ptr<AudioEffect> effect, int position = -1);
    Result<bool> remove_effect(uint32_t effect_id);
    Result<bool> move_effect(uint32_t effect_id, int new_position);
    Result<bool> bypass_effect(uint32_t effect_id, bool bypass);
    
    std::vector<std::shared_ptr<AudioEffect>> get_effects_chain() const;
    size_t get_effects_count() const;
    
    // Audio metering
    MeterData get_meter_data() const;
    void reset_meters();
    
    // Enable/disable metering to save CPU
    void set_metering_enabled(bool enabled) { m_metering_enabled = enabled; }
    bool is_metering_enabled() const { return m_metering_enabled; }
    
    // Bus activity and statistics
    bool is_active() const { return m_is_active.load(); }  // Has received audio recently
    uint64_t get_samples_processed() const { return m_samples_processed.load(); }
    
    // Bus solo/mute state for mixer logic
    void set_mixer_mute_override(bool mute) { m_mixer_mute_override = mute; }
    bool is_mixer_muted() const { return m_mixer_mute_override || m_config.mute; }
    
    void set_solo_active_in_mixer(bool solo_active) { m_solo_active_in_mixer = solo_active; }
    bool is_solo_active_in_mixer() const { return m_solo_active_in_mixer; }
    
    // Thread safety
    std::mutex& get_processing_mutex() { return m_processing_mutex; }
    
private:
    uint32_t m_bus_id;
    BusConfig m_config;
    
    // Input sources (source_id -> level)
    mutable std::mutex m_inputs_mutex;
    std::map<uint32_t, double> m_input_sources;
    
    // Processing state
    std::atomic<bool> m_is_active{false};
    std::atomic<uint64_t> m_samples_processed{0};
    
    // Audio buffers for internal processing
    std::shared_ptr<AudioBuffer> m_internal_buffer;
    std::shared_ptr<AudioBuffer> m_delay_buffer;
    
    // Effects chain
    mutable std::mutex m_effects_mutex;
    std::vector<std::shared_ptr<AudioEffect>> m_effects_chain;
    
    // Delay compensation
    DelayCompensation m_delay_compensation;
    std::vector<double> m_delay_line;  // Delay line for PDC
    uint32_t m_delay_write_pos = 0;
    
    // Metering
    std::unique_ptr<MeterProcessor> m_meter_processor;
    bool m_metering_enabled = true;
    
    // Mixer state overrides
    bool m_mixer_mute_override = false;
    bool m_solo_active_in_mixer = false;
    
    // Thread safety
    mutable std::mutex m_processing_mutex;
    mutable std::mutex m_config_mutex;
    
    // Processing helpers
    void apply_volume_and_pan(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    void process_effects_chain(std::shared_ptr<AudioBuffer> buffer, uint64_t start_time, uint32_t buffer_size);
    void apply_delay_compensation(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    void update_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    
    // Utility functions
    void update_activity_state(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    bool has_audio_signal(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size, double threshold = -60.0);
};

// Bus factory for creating different types of buses
class AudioBusFactory {
public:
    // Create auxiliary send bus (for reverb, delay effects)
    static std::unique_ptr<AudioBus> create_aux_send_bus(uint32_t bus_id, const std::string& name = "Aux Send");
    
    // Create group bus (for combining multiple tracks)
    static std::unique_ptr<AudioBus> create_group_bus(uint32_t bus_id, const std::string& name = "Group");
    
    // Create master bus (main output)
    static std::unique_ptr<AudioBus> create_master_bus(uint32_t bus_id, const std::string& name = "Master");
    
    // Create monitor bus (for headphone/speaker monitoring)
    static std::unique_ptr<AudioBus> create_monitor_bus(uint32_t bus_id, const std::string& name = "Monitor");
    
    // Create custom bus with specific configuration
    static std::unique_ptr<AudioBus> create_custom_bus(uint32_t bus_id, const BusConfig& config);
};

// Bus manager for organizing and managing multiple buses
class AudioBusManager {
public:
    AudioBusManager();
    ~AudioBusManager();
    
    // Bus creation and management
    Result<uint32_t> create_bus(const BusConfig& config);
    Result<bool> remove_bus(uint32_t bus_id);
    Result<std::shared_ptr<AudioBus>> get_bus(uint32_t bus_id);
    
    std::vector<uint32_t> get_all_bus_ids() const;
    std::vector<std::shared_ptr<AudioBus>> get_all_buses() const;
    std::vector<std::shared_ptr<AudioBus>> get_buses_of_type(BusConfig::BusType type) const;
    
    size_t get_bus_count() const;
    size_t get_bus_count_by_type(BusConfig::BusType type) const;
    
    // Bus routing helpers
    Result<bool> route_bus_to_bus(uint32_t source_bus_id, uint32_t destination_bus_id, double level = 1.0);
    Result<bool> unroute_bus_from_bus(uint32_t source_bus_id, uint32_t destination_bus_id);
    
    // Master bus management
    Result<bool> set_master_bus(uint32_t bus_id);
    uint32_t get_master_bus_id() const { return m_master_bus_id; }
    std::shared_ptr<AudioBus> get_master_bus();
    
    // Solo and mute management across all buses
    void set_global_solo_active(bool active);
    bool is_global_solo_active() const { return m_global_solo_active; }
    
    void update_solo_mute_states();  // Update all bus solo/mute states
    
    // Performance and statistics
    void reset_all_meters();
    MeterData get_master_meter_data() const;
    
    struct BusManagerStats {
        size_t total_buses;
        size_t active_buses;
        uint64_t total_samples_processed;
        double cpu_usage_percent;
    };
    
    BusManagerStats get_statistics() const;
    
private:
    mutable std::mutex m_buses_mutex;
    std::map<uint32_t, std::shared_ptr<AudioBus>> m_buses;
    
    uint32_t m_next_bus_id = 1;
    uint32_t m_master_bus_id = 0;
    
    // Global solo state
    bool m_global_solo_active = false;
    
    // Performance tracking
    mutable std::mutex m_stats_mutex;
    BusManagerStats m_statistics{};
    
    // Helper methods
    uint32_t generate_bus_id();
    void update_statistics();
};

} // namespace mixmind