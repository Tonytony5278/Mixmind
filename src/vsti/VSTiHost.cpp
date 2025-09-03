#include "VSTiHost.h"
#include <chrono>
#include <random>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace mixmind {

VSTiInstance::VSTiInstance(const VST3PluginInfo& plugin_info)
    : m_plugin_info(plugin_info)
{
    m_instance_id = generate_instance_id();
    reset_performance_stats();
}

Result<bool> VSTiInstance::initialize(double sample_rate, int buffer_size) {
    if (sample_rate <= 0 || buffer_size <= 0) {
        return Result<bool>::error("Invalid audio parameters for VSTi");
    }
    
    m_sample_rate = sample_rate;
    m_buffer_size = buffer_size;
    
    // Initialize plugin parameters based on plugin type
    initialize_parameters();
    
    // Initialize audio generation state
    {
        std::lock_guard<std::mutex> lock(m_audio_mutex);
        for (auto& osc : m_oscillators) {
            osc = OscillatorState{}; // Reset all oscillators
        }
    }
    
    // Clear MIDI event buffer
    {
        std::lock_guard<std::mutex> lock(m_midi_mutex);
        m_pending_midi_events.clear();
    }
    
    m_is_initialized = true;
    return Result<bool>::success(true);
}

Result<bool> VSTiInstance::activate() {
    if (!m_is_initialized) {
        return Result<bool>::error("VSTi instance not initialized");
    }
    
    m_is_active = true;
    return Result<bool>::success(true);
}

Result<bool> VSTiInstance::deactivate() {
    m_is_active = false;
    
    // Send all notes off
    {
        std::lock_guard<std::mutex> lock(m_audio_mutex);
        for (auto& osc : m_oscillators) {
            osc.is_active = false;
            osc.amplitude = 0.0f;
        }
    }
    
    return Result<bool>::success(true);
}

void VSTiInstance::shutdown() {
    deactivate();
    m_is_initialized = false;
}

Result<bool> VSTiInstance::process_midi_events(const MIDIEventBuffer& midi_events) {
    if (!m_is_active) {
        return Result<bool>::error("VSTi instance not active");
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(m_midi_mutex);
    
    // Process each MIDI event
    for (const auto& event : midi_events) {
        switch (event.type) {
            case MIDIEventType::NOTE_ON:
                if (event.data2 > 0) { // Velocity > 0
                    process_note_on(event.data1, event.data2);
                } else {
                    process_note_off(event.data1);
                }
                break;
                
            case MIDIEventType::NOTE_OFF:
                process_note_off(event.data1);
                break;
                
            case MIDIEventType::CONTROL_CHANGE:
                process_control_change(event.data1, event.data2);
                break;
                
            case MIDIEventType::PITCH_BEND:
                process_pitch_bend(event.get_pitch_bend_value());
                break;
                
            default:
                // Ignore other MIDI events for now
                break;
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
            m_stats.midi_events_processed++;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return Result<bool>::success(true);
}

Result<std::vector<std::vector<float>>> VSTiInstance::render_audio_stereo(int num_samples) {
    if (!m_is_active) {
        return Result<std::vector<std::vector<float>>>::error("VSTi instance not active");
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::vector<float>> stereo_output(2);
    stereo_output[0].resize(num_samples); // Left channel
    stereo_output[1].resize(num_samples); // Right channel
    
    {
        std::lock_guard<std::mutex> lock(m_audio_mutex);
        
        for (int sample = 0; sample < num_samples; ++sample) {
            float mono_sample = generate_audio_sample();
            
            // Simple stereo spread (could be enhanced with panning)
            stereo_output[0][sample] = mono_sample * 0.7f; // Left
            stereo_output[1][sample] = mono_sample * 0.7f; // Right
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    update_performance_stats(duration.count() / 1000.0, num_samples);
    
    return Result<std::vector<std::vector<float>>>::success(stereo_output);
}

Result<std::vector<float>> VSTiInstance::render_audio(int num_samples) {
    auto stereo_result = render_audio_stereo(num_samples);
    if (!stereo_result.isSuccess()) {
        return Result<std::vector<float>>::error(stereo_result.getError().toString());
    }
    
    // Convert stereo to mono by summing channels
    auto stereo = stereo_result.getValue();
    std::vector<float> mono(num_samples);
    
    for (int i = 0; i < num_samples; ++i) {
        mono[i] = (stereo[0][i] + stereo[1][i]) * 0.5f;
    }
    
    return Result<std::vector<float>>::success(mono);
}

void VSTiInstance::process_note_on(uint8_t note, uint8_t velocity) {
    if (note >= 128) return;
    
    std::lock_guard<std::mutex> lock(m_audio_mutex);
    
    auto& osc = m_oscillators[note];
    osc.note = note;
    osc.velocity = velocity;
    osc.amplitude = velocity / 127.0f;
    osc.is_active = true;
    osc.phase = 0.0; // Reset phase for new note
    
    // Calculate frequency from MIDI note number (A4 = 440 Hz = note 69)
    osc.frequency = 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

void VSTiInstance::process_note_off(uint8_t note) {
    if (note >= 128) return;
    
    std::lock_guard<std::mutex> lock(m_audio_mutex);
    
    auto& osc = m_oscillators[note];
    osc.is_active = false;
    osc.amplitude = 0.0f;
}

void VSTiInstance::process_control_change(uint8_t controller, uint8_t value) {
    // Handle common MIDI controllers
    switch (static_cast<MIDIController>(controller)) {
        case MIDIController::VOLUME:
            // Set master volume parameter
            set_parameter("master_volume", value / 127.0f);
            break;
            
        case MIDIController::MOD_WHEEL:
            // Set modulation parameter
            set_parameter("mod_wheel", value / 127.0f);
            break;
            
        case MIDIController::BRIGHTNESS:
            // Set filter cutoff
            set_parameter("filter_cutoff", value / 127.0f);
            break;
            
        case MIDIController::SUSTAIN:
            // Handle sustain pedal (simplified)
            break;
            
        default:
            // Ignore other controllers for now
            break;
    }
}

void VSTiInstance::process_pitch_bend(uint16_t bend_value) {
    // Convert 14-bit pitch bend to semitone offset (-2 to +2 semitones)
    float bend_semitones = ((float)bend_value - 8192.0f) / 4096.0f;
    
    // Apply pitch bend to all active oscillators
    std::lock_guard<std::mutex> lock(m_audio_mutex);
    for (auto& osc : m_oscillators) {
        if (osc.is_active) {
            // Recalculate frequency with pitch bend
            double base_freq = 440.0 * std::pow(2.0, (osc.note - 69) / 12.0);
            osc.frequency = base_freq * std::pow(2.0, bend_semitones / 12.0);
        }
    }
}

float VSTiInstance::generate_audio_sample() {
    float output = 0.0f;
    
    // Sum all active oscillators
    for (auto& osc : m_oscillators) {
        if (osc.is_active && osc.amplitude > 0.0f) {
            output += generate_oscillator_sample(osc);
        }
    }
    
    // Apply master volume
    float master_volume = 0.8f;
    auto volume_param = std::find_if(m_parameters.begin(), m_parameters.end(),
        [](const PluginParameter& p) { return p.name == "master_volume"; });
    if (volume_param != m_parameters.end()) {
        master_volume = volume_param->value;
    }
    
    output *= master_volume;
    
    // Soft clipping to prevent harsh distortion
    if (output > 1.0f) output = std::tanh(output);
    if (output < -1.0f) output = std::tanh(output);
    
    return output;
}

float VSTiInstance::generate_oscillator_sample(OscillatorState& osc) {
    double sample_rate = m_sample_rate;
    
    // Generate sine wave (could be enhanced with more waveforms)
    float sample = std::sin(2.0 * M_PI * osc.phase) * osc.amplitude;
    
    // Apply simple amplitude envelope (decay)
    osc.amplitude *= 0.9999f; // Slow decay
    if (osc.amplitude < 0.001f) {
        osc.is_active = false;
        osc.amplitude = 0.0f;
    }
    
    // Update phase
    osc.phase += osc.frequency / sample_rate;
    if (osc.phase >= 1.0) {
        osc.phase -= 1.0;
    }
    
    return sample;
}

void VSTiInstance::initialize_parameters() {
    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    m_parameters.clear();
    
    std::string plugin_name = m_plugin_info.name;
    std::transform(plugin_name.begin(), plugin_name.end(), plugin_name.begin(), ::tolower);
    
    if (plugin_name.find("serum") != std::string::npos) {
        initialize_mock_synth_parameters();
    } else if (plugin_name.find("arcade") != std::string::npos) {
        initialize_mock_sampler_parameters();
    } else {
        // Generic instrument parameters
        m_parameters = {
            {"master_volume", 0.8f, 0.0f, 1.0f, ""},
            {"filter_cutoff", 0.7f, 0.0f, 1.0f, "Hz"},
            {"filter_resonance", 0.2f, 0.0f, 1.0f, ""},
            {"mod_wheel", 0.0f, 0.0f, 1.0f, ""},
            {"pitch_bend", 0.5f, 0.0f, 1.0f, "semitones"}
        };
    }
}

void VSTiInstance::initialize_mock_synth_parameters() {
    // Serum-like synthesizer parameters
    m_parameters = {
        {"osc1_wave", 0.5f, 0.0f, 1.0f, ""},
        {"osc2_wave", 0.3f, 0.0f, 1.0f, ""},
        {"osc_mix", 0.5f, 0.0f, 1.0f, ""},
        {"filter_cutoff", 0.7f, 0.0f, 1.0f, "Hz"},
        {"filter_resonance", 0.2f, 0.0f, 1.0f, ""},
        {"env_attack", 0.1f, 0.0f, 1.0f, "s"},
        {"env_decay", 0.3f, 0.0f, 1.0f, "s"},
        {"env_sustain", 0.8f, 0.0f, 1.0f, ""},
        {"env_release", 0.4f, 0.0f, 1.0f, "s"},
        {"lfo_rate", 0.5f, 0.0f, 1.0f, "Hz"},
        {"lfo_amount", 0.0f, 0.0f, 1.0f, ""},
        {"master_volume", 0.8f, 0.0f, 1.0f, ""},
        {"mod_wheel", 0.0f, 0.0f, 1.0f, ""},
        {"pitch_bend", 0.5f, 0.0f, 1.0f, "semitones"}
    };
}

void VSTiInstance::initialize_mock_sampler_parameters() {
    // Arcade-like sampler parameters
    m_parameters = {
        {"sample_select", 0.0f, 0.0f, 1.0f, ""},
        {"pitch_shift", 0.5f, 0.0f, 1.0f, "semitones"},
        {"loop_start", 0.0f, 0.0f, 1.0f, ""},
        {"loop_length", 1.0f, 0.0f, 1.0f, ""},
        {"filter_freq", 0.8f, 0.0f, 1.0f, "Hz"},
        {"filter_resonance", 0.1f, 0.0f, 1.0f, ""},
        {"reverb_size", 0.3f, 0.0f, 1.0f, ""},
        {"reverb_decay", 0.5f, 0.0f, 1.0f, "s"},
        {"delay_time", 0.25f, 0.0f, 1.0f, "s"},
        {"delay_feedback", 0.4f, 0.0f, 1.0f, ""},
        {"master_gain", 0.75f, 0.0f, 1.0f, "dB"},
        {"mod_wheel", 0.0f, 0.0f, 1.0f, ""},
        {"pitch_bend", 0.5f, 0.0f, 1.0f, "semitones"}
    };
}

Result<bool> VSTiInstance::set_parameter(const std::string& param_name, float value) {
    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    
    auto param_it = std::find_if(m_parameters.begin(), m_parameters.end(),
        [&param_name](const PluginParameter& p) { return p.name == param_name; });
    
    if (param_it == m_parameters.end()) {
        return Result<bool>::error("Parameter not found: " + param_name);
    }
    
    // Clamp value to parameter range
    float clamped_value = std::clamp(value, param_it->min_value, param_it->max_value);
    param_it->value = clamped_value;
    
    return Result<bool>::success(true);
}

Result<float> VSTiInstance::get_parameter(const std::string& param_name) {
    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    
    auto param_it = std::find_if(m_parameters.begin(), m_parameters.end(),
        [&param_name](const PluginParameter& p) { return p.name == param_name; });
    
    if (param_it == m_parameters.end()) {
        return Result<float>::error("Parameter not found: " + param_name);
    }
    
    return Result<float>::success(param_it->value);
}

Result<std::vector<std::string>> VSTiInstance::get_parameter_names() {
    std::lock_guard<std::mutex> lock(m_parameters_mutex);
    
    std::vector<std::string> names;
    for (const auto& param : m_parameters) {
        names.push_back(param.name);
    }
    
    return Result<std::vector<std::string>>::success(names);
}

void VSTiInstance::update_performance_stats(double render_time_ms, int samples_rendered) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_stats.audio_samples_rendered += samples_rendered;
    
    // Update peak render time
    if (render_time_ms > m_stats.peak_render_time_ms) {
        m_stats.peak_render_time_ms = render_time_ms;
    }
    
    // Update rolling average render time
    const double alpha = 0.1;
    m_stats.average_render_time_ms = alpha * render_time_ms + (1.0 - alpha) * m_stats.average_render_time_ms;
    
    // Check for underrun (render time > buffer time)
    double buffer_time_ms = (samples_rendered * 1000.0) / m_sample_rate;
    if (render_time_ms > buffer_time_ms * 0.8) { // 80% of buffer time is concerning
        m_stats.underrun_detected = true;
    }
}

std::string VSTiInstance::generate_instance_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "vsti_" << m_plugin_info.name << "_";
    
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

void VSTiInstance::reset_performance_stats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_stats = PerformanceStats{};
}

// VSTiHost implementation

VSTiHost::VSTiHost() {
    m_scanner = std::make_unique<RealVST3Scanner>();
    reset_host_stats();
}

Result<bool> VSTiHost::initialize(double sample_rate, int buffer_size) {
    if (sample_rate <= 0 || buffer_size <= 0) {
        return Result<bool>::error("Invalid audio parameters for VSTi host");
    }
    
    m_sample_rate = sample_rate;
    m_buffer_size = buffer_size;
    
    m_is_initialized = true;
    return Result<bool>::success(true);
}

void VSTiHost::shutdown() {
    m_is_initialized = false;
    
    // Shutdown all instances
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    for (auto& pair : m_instances) {
        pair.second->shutdown();
    }
    m_instances.clear();
}

Result<std::string> VSTiHost::load_vsti(const std::string& plugin_path) {
    if (!m_is_initialized) {
        return Result<std::string>::error("VSTi host not initialized");
    }
    
    // Validate plugin path
    auto validation_result = m_scanner->validatePlugin(plugin_path);
    if (!validation_result.isSuccess()) {
        return Result<std::string>::error("Invalid VST3 plugin: " + validation_result.getError().toString());
    }
    
    auto plugin_info = validation_result.getValue();
    
    // Create new VSTi instance
    auto instance = std::make_shared<VSTiInstance>(plugin_info);
    
    // Initialize the instance
    auto init_result = instance->initialize(m_sample_rate, m_buffer_size);
    if (!init_result.isSuccess()) {
        return Result<std::string>::error("Failed to initialize VSTi: " + init_result.getError().toString());
    }
    
    // Activate the instance
    auto activate_result = instance->activate();
    if (!activate_result.isSuccess()) {
        return Result<std::string>::error("Failed to activate VSTi: " + activate_result.getError().toString());
    }
    
    // Add to instances map
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    std::string instance_id = instance->get_instance_id();
    m_instances[instance_id] = instance;
    
    // Update host statistics
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_host_stats.total_instances_created++;
        m_host_stats.active_instances = m_instances.size();
    }
    
    return Result<std::string>::success(instance_id);
}

Result<std::shared_ptr<VSTiInstance>> VSTiHost::get_vsti_instance(const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    auto it = m_instances.find(instance_id);
    if (it == m_instances.end()) {
        return Result<std::shared_ptr<VSTiInstance>>::error("VSTi instance not found: " + instance_id);
    }
    
    return Result<std::shared_ptr<VSTiInstance>>::success(it->second);
}

Result<std::vector<VST3PluginInfo>> VSTiHost::scan_available_instruments() {
    auto scan_result = m_scanner->scanSystemPlugins();
    if (!scan_result.isSuccess()) {
        return Result<std::vector<VST3PluginInfo>>::error("Failed to scan VST3 plugins: " + scan_result.getError().toString());
    }
    
    // Filter for instrument plugins (heuristic based on names)
    auto all_plugins = scan_result.getValue();
    std::vector<VST3PluginInfo> instruments;
    
    for (const auto& plugin : all_plugins) {
        std::string name_lower = plugin.name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        
        // Check if plugin name suggests it's an instrument
        if (name_lower.find("serum") != std::string::npos ||
            name_lower.find("arcade") != std::string::npos ||
            name_lower.find("synth") != std::string::npos ||
            name_lower.find("piano") != std::string::npos ||
            name_lower.find("instrument") != std::string::npos ||
            name_lower.find("sampler") != std::string::npos) {
            instruments.push_back(plugin);
        }
    }
    
    return Result<std::vector<VST3PluginInfo>>::success(instruments);
}

Result<VST3PluginInfo> VSTiHost::find_instrument_by_name(const std::string& name) {
    auto instruments_result = scan_available_instruments();
    if (!instruments_result.isSuccess()) {
        return Result<VST3PluginInfo>::error("Failed to scan instruments: " + instruments_result.getError().toString());
    }
    
    auto instruments = instruments_result.getValue();
    
    for (const auto& instrument : instruments) {
        if (instrument.name == name) {
            return Result<VST3PluginInfo>::success(instrument);
        }
    }
    
    return Result<VST3PluginInfo>::error("Instrument not found: " + name);
}

void VSTiHost::reset_host_stats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_host_stats = HostStats{};
}

} // namespace mixmind