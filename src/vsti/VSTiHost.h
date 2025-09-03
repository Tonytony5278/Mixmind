#pragma once

#include "../vst3/RealVST3Scanner.h"
#include "../midi/MIDIEvent.h"
#include "../core/result.h"
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

namespace mixmind {

// VST instrument instance for hosting synthesizers and samplers
class VSTiInstance {
public:
    VSTiInstance(const VST3PluginInfo& plugin_info);
    ~VSTiInstance() = default;
    
    // Plugin lifecycle
    Result<bool> initialize(double sample_rate, int buffer_size);
    Result<bool> activate();
    Result<bool> deactivate();
    void shutdown();
    
    // MIDI processing
    Result<bool> process_midi_events(const MIDIEventBuffer& midi_events);
    
    // Audio rendering (MIDI → Audio conversion)
    Result<std::vector<float>> render_audio(int num_samples);
    Result<std::vector<std::vector<float>>> render_audio_stereo(int num_samples);
    
    // Parameter control
    Result<bool> set_parameter(const std::string& param_name, float value);
    Result<float> get_parameter(const std::string& param_name);
    Result<std::vector<std::string>> get_parameter_names();
    
    // Preset management
    Result<bool> load_preset(const std::string& preset_path);
    Result<bool> save_preset(const std::string& preset_path);
    Result<std::vector<uint8_t>> get_state_data();
    Result<bool> set_state_data(const std::vector<uint8_t>& data);
    
    // Plugin information
    const VST3PluginInfo& get_plugin_info() const { return m_plugin_info; }
    const std::string& get_instance_id() const { return m_instance_id; }
    bool is_initialized() const { return m_is_initialized; }
    bool is_active() const { return m_is_active; }
    
    // Performance monitoring
    struct PerformanceStats {
        uint64_t midi_events_processed;
        uint64_t audio_samples_rendered;
        double average_render_time_ms;
        double peak_render_time_ms;
        bool underrun_detected;
    };
    
    PerformanceStats get_performance_stats() const { return m_stats; }
    void reset_performance_stats();
    
private:
    VST3PluginInfo m_plugin_info;
    std::string m_instance_id;
    
    // Plugin state
    std::atomic<bool> m_is_initialized{false};
    std::atomic<bool> m_is_active{false};
    std::atomic<double> m_sample_rate{44100.0};
    std::atomic<int> m_buffer_size{512};
    
    // VST3 plugin parameters (mock implementation for now)
    struct PluginParameter {
        std::string name;
        float value;
        float min_value;
        float max_value;
        std::string unit;
    };
    std::vector<PluginParameter> m_parameters;
    mutable std::mutex m_parameters_mutex;
    
    // MIDI event processing
    MIDIEventBuffer m_pending_midi_events;
    mutable std::mutex m_midi_mutex;
    
    // Audio generation state
    struct OscillatorState {
        double phase = 0.0;
        double frequency = 440.0;
        float amplitude = 0.0;
        bool is_active = false;
        uint8_t note = 60;
        uint8_t velocity = 127;
    };
    std::array<OscillatorState, 128> m_oscillators; // One per MIDI note
    mutable std::mutex m_audio_mutex;
    
    // Performance monitoring
    mutable PerformanceStats m_stats{};
    mutable std::mutex m_stats_mutex;
    
    // Internal methods
    void initialize_parameters();
    void initialize_mock_synth_parameters();  // For Serum-like synth
    void initialize_mock_sampler_parameters(); // For Arcade-like sampler
    
    void process_note_on(uint8_t note, uint8_t velocity);
    void process_note_off(uint8_t note);
    void process_control_change(uint8_t controller, uint8_t value);
    void process_pitch_bend(uint16_t bend_value);
    
    float generate_audio_sample();
    float generate_oscillator_sample(OscillatorState& osc);
    
    void update_performance_stats(double render_time_ms, int samples_rendered);
    std::string generate_instance_id();
};

// VST instrument host manager
class VSTiHost {
public:
    VSTiHost();
    ~VSTiHost() = default;
    
    // Host lifecycle
    Result<bool> initialize(double sample_rate, int buffer_size);
    void shutdown();
    
    // VST instrument management
    Result<std::string> load_vsti(const std::string& plugin_path);
    Result<bool> unload_vsti(const std::string& instance_id);
    Result<std::shared_ptr<VSTiInstance>> get_vsti_instance(const std::string& instance_id);
    
    // Discover available VST instruments
    Result<std::vector<VST3PluginInfo>> scan_available_instruments();
    Result<VST3PluginInfo> find_instrument_by_name(const std::string& name);
    
    // Host-level processing
    Result<bool> process_all_midi(const std::string& instance_id, const MIDIEventBuffer& midi_events);
    Result<std::vector<std::vector<float>>> render_all_audio(int num_samples);
    
    // Global host parameters
    void set_global_sample_rate(double sample_rate);
    void set_global_buffer_size(int buffer_size);
    double get_global_sample_rate() const { return m_sample_rate; }
    int get_global_buffer_size() const { return m_buffer_size; }
    
    // Host statistics
    struct HostStats {
        int active_instances;
        int total_instances_created;
        uint64_t total_midi_events_processed;
        uint64_t total_audio_samples_rendered;
        double cpu_usage_percent;
    };
    
    HostStats get_host_stats() const { return m_host_stats; }
    void reset_host_stats();
    
private:
    std::unique_ptr<RealVST3Scanner> m_scanner;
    
    // Host configuration
    std::atomic<double> m_sample_rate{44100.0};
    std::atomic<int> m_buffer_size{512};
    std::atomic<bool> m_is_initialized{false};
    
    // Loaded VSTi instances
    std::map<std::string, std::shared_ptr<VSTiInstance>> m_instances;
    mutable std::mutex m_instances_mutex;
    
    // Host-level statistics
    mutable HostStats m_host_stats{};
    mutable std::mutex m_stats_mutex;
    
    // Internal methods
    void update_host_stats();
};

} // namespace mixmind