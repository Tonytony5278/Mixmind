#include "InstrumentTrack.h"
#include <chrono>
#include <algorithm>
#include <cmath>

namespace mixmind {

InstrumentTrack::InstrumentTrack(const std::string& track_name)
    : m_track_name(track_name)
{
    m_midi_processor = std::make_unique<MIDIProcessor>();
}

Result<bool> InstrumentTrack::initialize(double sample_rate, int buffer_size, std::shared_ptr<VSTiHost> vsti_host) {
    if (sample_rate <= 0 || buffer_size <= 0) {
        return Result<bool>::error("Invalid audio parameters for instrument track");
    }
    
    if (!vsti_host) {
        return Result<bool>::error("VSTi host is required for instrument track");
    }
    
    m_sample_rate = sample_rate;
    m_buffer_size = buffer_size;
    m_vsti_host = vsti_host;
    
    // Initialize MIDI processor
    auto midi_result = m_midi_processor->initialize(sample_rate, buffer_size);
    if (!midi_result.isSuccess()) {
        return Result<bool>::error("Failed to initialize MIDI processor: " + midi_result.getError().toString());
    }
    
    // Reset performance tracking
    reset_performance_stats();
    
    m_is_initialized = true;
    return Result<bool>::success(true);
}

void InstrumentTrack::shutdown() {
    m_is_initialized = false;
    
    // Unload any loaded instrument
    unload_instrument();
    
    // Shutdown MIDI processor
    if (m_midi_processor) {
        m_midi_processor->shutdown();
    }
}

Result<bool> InstrumentTrack::load_instrument(const std::string& plugin_path) {
    if (!m_is_initialized) {
        return Result<bool>::error("Instrument track not initialized");
    }
    
    if (!m_vsti_host) {
        return Result<bool>::error("No VSTi host available");
    }
    
    // Unload any existing instrument
    if (has_instrument()) {
        auto unload_result = unload_instrument();
        if (!unload_result.isSuccess()) {
            return unload_result;
        }
    }
    
    // Load new instrument
    auto load_result = m_vsti_host->load_vsti(plugin_path);
    if (!load_result.isSuccess()) {
        return Result<bool>::error("Failed to load instrument: " + load_result.getError().toString());
    }
    
    m_vsti_instance_id = load_result.getValue();
    
    return Result<bool>::success(true);
}

Result<bool> InstrumentTrack::unload_instrument() {
    if (!has_instrument()) {
        return Result<bool>::success(true); // Nothing to unload
    }
    
    if (!m_vsti_host) {
        m_vsti_instance_id.clear();
        return Result<bool>::success(true);
    }
    
    auto unload_result = m_vsti_host->unload_vsti(m_vsti_instance_id);
    m_vsti_instance_id.clear();
    
    if (!unload_result.isSuccess()) {
        return Result<bool>::error("Failed to unload instrument: " + unload_result.getError().toString());
    }
    
    return Result<bool>::success(true);
}

std::string InstrumentTrack::get_instrument_name() const {
    if (!has_instrument() || !m_vsti_host) {
        return "No Instrument";
    }
    
    auto instance_result = m_vsti_host->get_vsti_instance(m_vsti_instance_id);
    if (!instance_result.isSuccess()) {
        return "Unknown Instrument";
    }
    
    auto instance = instance_result.getValue();
    return instance->get_plugin_info().name;
}

Result<bool> InstrumentTrack::process_midi_input(const MIDIEventBuffer& midi_events, uint64_t start_sample) {
    if (!m_is_initialized || !m_midi_processor) {
        return Result<bool>::error("Instrument track not initialized");
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Process MIDI through our processor (transpose, velocity curve, etc.)
    m_midi_processor->process_midi_input(midi_events);
    
    // Get processed MIDI events
    auto processed_events = m_midi_processor->get_processed_midi(start_sample, m_buffer_size);
    
    // Send to VST instrument if loaded
    if (has_instrument() && m_vsti_host) {
        auto instance_result = m_vsti_host->get_vsti_instance(m_vsti_instance_id);
        if (instance_result.isSuccess()) {
            auto instance = instance_result.getValue();
            auto vsti_result = instance->process_midi_events(processed_events);
            if (!vsti_result.isSuccess()) {
                return Result<bool>::error("VST instrument MIDI processing failed: " + vsti_result.getError().toString());
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Update performance metrics
    update_performance_metrics(midi_events.size(), 0, duration.count() / 1000.0);
    
    return Result<bool>::success(true);
}

Result<std::vector<std::vector<float>>> InstrumentTrack::render_audio(int num_samples) {
    if (!m_is_initialized) {
        return Result<std::vector<std::vector<float>>>::error("Instrument track not initialized");
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::vector<float>> output_audio(2); // Stereo output
    output_audio[0].resize(num_samples, 0.0f); // Left channel
    output_audio[1].resize(num_samples, 0.0f); // Right channel
    
    // Generate audio from VST instrument if loaded and not muted
    if (has_instrument() && !m_is_muted && m_vsti_host) {
        auto instance_result = m_vsti_host->get_vsti_instance(m_vsti_instance_id);
        if (instance_result.isSuccess()) {
            auto instance = instance_result.getValue();
            
            // Render stereo audio from instrument
            auto render_result = instance->render_audio_stereo(num_samples);
            if (render_result.isSuccess()) {
                output_audio = render_result.getValue();
            }
        }
    }
    
    // Apply track processing (volume, pan, etc.)
    output_audio = apply_track_processing(output_audio);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Update performance metrics
    update_performance_metrics(0, num_samples, duration.count() / 1000.0);
    
    return Result<std::vector<std::vector<float>>>::success(output_audio);
}

std::vector<std::vector<float>> InstrumentTrack::apply_track_processing(const std::vector<std::vector<float>>& input_audio) {
    auto output_audio = input_audio;
    
    // Apply volume and pan
    apply_volume_and_pan(output_audio);
    
    // Apply solo/mute logic (handled at mixer level, but we can apply mute here)
    if (m_is_muted) {
        for (auto& channel : output_audio) {
            std::fill(channel.begin(), channel.end(), 0.0f);
        }
    }
    
    return output_audio;
}

void InstrumentTrack::apply_volume_and_pan(std::vector<std::vector<float>>& audio) {
    if (audio.size() != 2) return; // Only process stereo
    
    float volume = m_volume;
    float pan = m_pan;
    
    // Calculate left/right gains from pan position
    float left_gain = volume * std::cos((pan + 1.0f) * M_PI / 4.0f);
    float right_gain = volume * std::sin((pan + 1.0f) * M_PI / 4.0f);
    
    // Apply gains to channels
    for (size_t i = 0; i < audio[0].size(); ++i) {
        float left_sample = audio[0][i];
        float right_sample = audio[1][i];
        
        // Apply volume and pan
        audio[0][i] = left_sample * left_gain;
        audio[1][i] = right_sample * right_gain;
    }
}

Result<bool> InstrumentTrack::inject_live_midi(const MIDIEvent& event) {
    if (!m_is_initialized || !m_midi_processor) {
        return Result<bool>::error("Instrument track not initialized");
    }
    
    // Inject MIDI event for real-time processing
    m_midi_processor->inject_midi_event(event);
    
    return Result<bool>::success(true);
}

Result<bool> InstrumentTrack::set_instrument_parameter(const std::string& param_name, float value) {
    if (!has_instrument() || !m_vsti_host) {
        return Result<bool>::error("No instrument loaded");
    }
    
    auto instance_result = m_vsti_host->get_vsti_instance(m_vsti_instance_id);
    if (!instance_result.isSuccess()) {
        return Result<bool>::error("Failed to get instrument instance");
    }
    
    auto instance = instance_result.getValue();
    return instance->set_parameter(param_name, value);
}

Result<float> InstrumentTrack::get_instrument_parameter(const std::string& param_name) {
    if (!has_instrument() || !m_vsti_host) {
        return Result<float>::error("No instrument loaded");
    }
    
    auto instance_result = m_vsti_host->get_vsti_instance(m_vsti_instance_id);
    if (!instance_result.isSuccess()) {
        return Result<float>::error("Failed to get instrument instance");
    }
    
    auto instance = instance_result.getValue();
    return instance->get_parameter(param_name);
}

Result<std::vector<std::string>> InstrumentTrack::get_instrument_parameter_names() {
    if (!has_instrument() || !m_vsti_host) {
        return Result<std::vector<std::string>>::error("No instrument loaded");
    }
    
    auto instance_result = m_vsti_host->get_vsti_instance(m_vsti_instance_id);
    if (!instance_result.isSuccess()) {
        return Result<std::vector<std::string>>::error("Failed to get instrument instance");
    }
    
    auto instance = instance_result.getValue();
    return instance->get_parameter_names();
}

InstrumentTrack::TrackPerformance InstrumentTrack::get_performance_stats() const {
    std::lock_guard<std::mutex> lock(m_performance_mutex);
    return m_performance;
}

void InstrumentTrack::reset_performance_stats() {
    std::lock_guard<std::mutex> lock(m_performance_mutex);
    m_performance = TrackPerformance{};
}

void InstrumentTrack::update_performance_metrics(uint64_t midi_events, uint64_t audio_samples, double process_time_ms) {
    std::lock_guard<std::mutex> lock(m_performance_mutex);
    
    m_performance.midi_events_processed += midi_events;
    m_performance.audio_samples_rendered += audio_samples;
    
    // Update processing times (rolling average)
    const double alpha = 0.1;
    if (midi_events > 0) {
        m_performance.midi_latency_ms = alpha * process_time_ms + (1.0 - alpha) * m_performance.midi_latency_ms;
    }
    if (audio_samples > 0) {
        m_performance.audio_render_time_ms = alpha * process_time_ms + (1.0 - alpha) * m_performance.audio_render_time_ms;
    }
    
    // Check for performance warnings
    double buffer_time_ms = (m_buffer_size * 1000.0) / m_sample_rate;
    m_performance.performance_warning = (process_time_ms > buffer_time_ms * 0.7);
}

bool InstrumentTrack::validate_configuration() const {
    if (!m_is_initialized) return false;
    if (!m_midi_processor) return false;
    if (!m_vsti_host) return false;
    
    return true;
}

std::vector<std::string> InstrumentTrack::get_configuration_warnings() const {
    std::vector<std::string> warnings;
    
    if (!m_is_initialized) {
        warnings.push_back("Track not initialized");
    }
    
    if (!m_midi_processor) {
        warnings.push_back("MIDI processor not available");
    }
    
    if (!m_vsti_host) {
        warnings.push_back("VSTi host not available");
    }
    
    if (!has_instrument()) {
        warnings.push_back("No instrument loaded");
    }
    
    auto perf = get_performance_stats();
    if (perf.performance_warning) {
        warnings.push_back("Performance warning: processing time excessive");
    }
    
    return warnings;
}

// InstrumentTrackFactory implementation

std::shared_ptr<InstrumentTrack> InstrumentTrackFactory::create_track(
    const std::string& track_name, 
    double sample_rate, 
    int buffer_size,
    std::shared_ptr<VSTiHost> vsti_host)
{
    auto track = std::make_shared<InstrumentTrack>(track_name);
    
    auto init_result = track->initialize(sample_rate, buffer_size, vsti_host);
    if (!init_result.isSuccess()) {
        return nullptr;
    }
    
    return track;
}

std::shared_ptr<InstrumentTrack> InstrumentTrackFactory::create_track_with_instrument(
    const std::string& track_name,
    const std::string& instrument_path,
    double sample_rate,
    int buffer_size,
    std::shared_ptr<VSTiHost> vsti_host)
{
    auto track = create_track(track_name, sample_rate, buffer_size, vsti_host);
    if (!track) {
        return nullptr;
    }
    
    auto load_result = track->load_instrument(instrument_path);
    if (!load_result.isSuccess()) {
        return nullptr;
    }
    
    return track;
}

Result<std::shared_ptr<InstrumentTrack>> InstrumentTrackFactory::create_serum_track(
    const std::string& track_name,
    double sample_rate,
    int buffer_size,
    std::shared_ptr<VSTiHost> vsti_host)
{
    if (!vsti_host) {
        return Result<std::shared_ptr<InstrumentTrack>>::error("VSTi host required");
    }
    
    // Find Serum instrument
    auto serum_result = vsti_host->find_instrument_by_name("Serum");
    if (!serum_result.isSuccess()) {
        return Result<std::shared_ptr<InstrumentTrack>>::error("Serum not found: " + serum_result.getError().toString());
    }
    
    auto serum_info = serum_result.getValue();
    auto track = create_track_with_instrument(track_name, serum_info.path, sample_rate, buffer_size, vsti_host);
    
    if (!track) {
        return Result<std::shared_ptr<InstrumentTrack>>::error("Failed to create Serum track");
    }
    
    return Result<std::shared_ptr<InstrumentTrack>>::success(track);
}

Result<std::shared_ptr<InstrumentTrack>> InstrumentTrackFactory::create_arcade_track(
    const std::string& track_name,
    double sample_rate,
    int buffer_size,
    std::shared_ptr<VSTiHost> vsti_host)
{
    if (!vsti_host) {
        return Result<std::shared_ptr<InstrumentTrack>>::error("VSTi host required");
    }
    
    // Find Arcade instrument
    auto arcade_result = vsti_host->find_instrument_by_name("Arcade");
    if (!arcade_result.isSuccess()) {
        return Result<std::shared_ptr<InstrumentTrack>>::error("Arcade not found: " + arcade_result.getError().toString());
    }
    
    auto arcade_info = arcade_result.getValue();
    auto track = create_track_with_instrument(track_name, arcade_info.path, sample_rate, buffer_size, vsti_host);
    
    if (!track) {
        return Result<std::shared_ptr<InstrumentTrack>>::error("Failed to create Arcade track");
    }
    
    return Result<std::shared_ptr<InstrumentTrack>>::success(track);
}

} // namespace mixmind