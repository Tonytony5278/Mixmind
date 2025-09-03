#pragma once

#include "TrackTypes.h"
#include "../midi/MIDIProcessor.h"
#include "../vsti/VSTiHost.h"
#include "../core/result.h"
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace mixmind {

// Professional instrument track: MIDI in → Audio out
// This is the key track type that makes MixMind musical
class InstrumentTrack {
public:
    InstrumentTrack(const std::string& track_name);
    ~InstrumentTrack() = default;
    
    // Track lifecycle
    Result<bool> initialize(double sample_rate, int buffer_size, std::shared_ptr<VSTiHost> vsti_host);
    void shutdown();
    
    // VST instrument management
    Result<bool> load_instrument(const std::string& plugin_path);
    Result<bool> unload_instrument();
    bool has_instrument() const { return !m_vsti_instance_id.empty(); }
    std::string get_instrument_name() const;
    
    // MIDI input processing (MIDI in)
    Result<bool> process_midi_input(const MIDIEventBuffer& midi_events, uint64_t start_sample);
    
    // Audio output generation (Audio out) ⭐ Key functionality
    Result<std::vector<std::vector<float>>> render_audio(int num_samples);
    
    // Track properties
    const std::string& get_name() const { return m_track_name; }
    void set_name(const std::string& name) { m_track_name = name; }
    
    TrackType get_track_type() const { return TrackType::INSTRUMENT; }
    TrackSignalFlow get_signal_flow() const { return TrackSignalFlow::for_track_type(TrackType::INSTRUMENT); }
    
    // Track state
    void set_armed(bool armed) { m_is_armed = armed; }
    bool is_armed() const { return m_is_armed; }
    
    void set_monitoring(bool monitoring) { m_is_monitoring = monitoring; }
    bool is_monitoring() const { return m_is_monitoring; }
    
    void set_solo(bool solo) { m_is_solo = solo; }
    bool is_solo() const { return m_is_solo; }
    
    void set_muted(bool muted) { m_is_muted = muted; }
    bool is_muted() const { return m_is_muted; }
    
    // Track levels
    void set_volume(float volume) { m_volume = std::clamp(volume, 0.0f, 2.0f); }
    float get_volume() const { return m_volume; }
    
    void set_pan(float pan) { m_pan = std::clamp(pan, -1.0f, 1.0f); }
    float get_pan() const { return m_pan; }
    
    // MIDI processing controls
    void set_transpose(int semitones) { if (m_midi_processor) m_midi_processor->set_transpose(semitones); }
    int get_transpose() const { return m_midi_processor ? m_midi_processor->get_transpose() : 0; }
    
    void set_velocity_curve(float curve) { if (m_midi_processor) m_midi_processor->set_velocity_curve(curve); }
    float get_velocity_curve() const { return m_midi_processor ? m_midi_processor->get_velocity_curve() : 1.0f; }
    
    void set_midi_channel(int channel) { if (m_midi_processor) m_midi_processor->set_channel_filter(channel); }
    int get_midi_channel() const { return m_midi_processor ? m_midi_processor->get_channel_filter() : -1; }
    
    // Quantization controls
    void set_quantize_enabled(bool enabled) { if (m_midi_processor) m_midi_processor->set_quantize_enabled(enabled); }
    bool is_quantize_enabled() const { return m_midi_processor ? m_midi_processor->is_quantize_enabled() : false; }
    
    void set_quantize_resolution(int resolution) { if (m_midi_processor) m_midi_processor->set_quantize_resolution(resolution); }
    int get_quantize_resolution() const { return m_midi_processor ? m_midi_processor->get_quantize_resolution() : 16; }
    
    // Instrument parameter control
    Result<bool> set_instrument_parameter(const std::string& param_name, float value);
    Result<float> get_instrument_parameter(const std::string& param_name);
    Result<std::vector<std::string>> get_instrument_parameter_names();
    
    // Performance monitoring
    struct TrackPerformance {
        uint64_t midi_events_processed;
        uint64_t audio_samples_rendered;
        double midi_latency_ms;
        double audio_render_time_ms;
        bool performance_warning; // True if performance is degraded
    };
    
    TrackPerformance get_performance_stats() const;
    void reset_performance_stats();
    
    // Track recording (for future MIDI recording)
    void set_recording(bool recording) { m_is_recording = recording; }
    bool is_recording() const { return m_is_recording; }
    
    // Live MIDI input injection (for real-time playing)
    Result<bool> inject_live_midi(const MIDIEvent& event);
    
    // Track validation
    bool validate_configuration() const;
    std::vector<std::string> get_configuration_warnings() const;
    
private:
    // Track identity
    std::string m_track_name;
    
    // Audio engine parameters
    std::atomic<double> m_sample_rate{44100.0};
    std::atomic<int> m_buffer_size{512};
    std::atomic<bool> m_is_initialized{false};
    
    // MIDI processing chain
    std::unique_ptr<MIDIProcessor> m_midi_processor;
    
    // VST instrument hosting
    std::shared_ptr<VSTiHost> m_vsti_host;
    std::string m_vsti_instance_id;
    
    // Track state
    std::atomic<bool> m_is_armed{false};
    std::atomic<bool> m_is_monitoring{false};
    std::atomic<bool> m_is_solo{false};
    std::atomic<bool> m_is_muted{false};
    std::atomic<bool> m_is_recording{false};
    
    // Track levels and processing
    std::atomic<float> m_volume{0.8f};       // 0.0 - 2.0 (0dB - +6dB)
    std::atomic<float> m_pan{0.0f};          // -1.0 (left) to +1.0 (right)
    
    // Performance tracking
    mutable TrackPerformance m_performance{};
    mutable std::mutex m_performance_mutex;
    
    // Internal audio processing
    std::vector<std::vector<float>> apply_track_processing(const std::vector<std::vector<float>>& input_audio);
    void apply_volume_and_pan(std::vector<std::vector<float>>& audio);
    void update_performance_metrics(uint64_t midi_events, uint64_t audio_samples, double process_time_ms);
};

// Instrument track factory for easy creation
class InstrumentTrackFactory {
public:
    static std::shared_ptr<InstrumentTrack> create_track(
        const std::string& track_name, 
        double sample_rate, 
        int buffer_size,
        std::shared_ptr<VSTiHost> vsti_host
    );
    
    static std::shared_ptr<InstrumentTrack> create_track_with_instrument(
        const std::string& track_name,
        const std::string& instrument_path,
        double sample_rate,
        int buffer_size,
        std::shared_ptr<VSTiHost> vsti_host
    );
    
    // Create tracks for specific instruments
    static Result<std::shared_ptr<InstrumentTrack>> create_serum_track(
        const std::string& track_name,
        double sample_rate,
        int buffer_size,
        std::shared_ptr<VSTiHost> vsti_host
    );
    
    static Result<std::shared_ptr<InstrumentTrack>> create_arcade_track(
        const std::string& track_name,
        double sample_rate,
        int buffer_size,
        std::shared_ptr<VSTiHost> vsti_host
    );
};

} // namespace mixmind