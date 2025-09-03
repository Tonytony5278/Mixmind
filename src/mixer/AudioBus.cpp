#include "AudioBus.h"
#include "../audio/AudioBuffer.h"
#include "../audio/MeterProcessor.h"
#include "../effects/AudioEffect.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace mixmind {

AudioBus::AudioBus(uint32_t bus_id, const BusConfig& config)
    : m_bus_id(bus_id), m_config(config) {
    
    // Initialize internal audio buffers
    m_internal_buffer = std::make_shared<AudioBuffer>(m_config.channel_count, 1024);
    m_delay_buffer = std::make_shared<AudioBuffer>(m_config.channel_count, 4096);  // Max delay buffer
    
    // Initialize meter processor
    m_meter_processor = std::make_unique<MeterProcessor>(m_config.channel_count, 44100);
    
    // Initialize delay line for PDC
    m_delay_line.resize(m_config.channel_count * 4096, 0.0);
}

AudioBus::~AudioBus() = default;

Result<bool> AudioBus::set_config(const BusConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    
    if (config.channel_count == 0 || config.channel_count > 32) {
        return Error("Invalid channel count: " + std::to_string(config.channel_count));
    }
    
    // If channel count changed, recreate buffers
    if (config.channel_count != m_config.channel_count) {
        m_internal_buffer = std::make_shared<AudioBuffer>(config.channel_count, 1024);
        m_delay_buffer = std::make_shared<AudioBuffer>(config.channel_count, 4096);
        m_delay_line.resize(config.channel_count * 4096, 0.0);
        m_meter_processor = std::make_unique<MeterProcessor>(config.channel_count, 44100);
    }
    
    m_config = config;
    return Ok(true);
}

BusConfig AudioBus::get_config() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

Result<bool> AudioBus::set_channel_count(uint32_t channels) {
    if (channels == 0 || channels > 32) {
        return Error("Invalid channel count: " + std::to_string(channels));
    }
    
    std::lock_guard<std::mutex> config_lock(m_config_mutex);
    std::lock_guard<std::mutex> proc_lock(m_processing_mutex);
    
    m_config.channel_count = channels;
    m_internal_buffer = std::make_shared<AudioBuffer>(channels, 1024);
    m_delay_buffer = std::make_shared<AudioBuffer>(channels, 4096);
    m_delay_line.resize(channels * 4096, 0.0);
    m_meter_processor = std::make_unique<MeterProcessor>(channels, 44100);
    
    return Ok(true);
}

Result<bool> AudioBus::set_bus_type(BusConfig::BusType type) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.type = type;
    
    // Adjust default routing based on bus type
    if (type == BusConfig::MASTER_BUS) {
        // Master bus typically doesn't route anywhere by default
        m_config.outputs.clear();
    } else {
        // Other buses typically route to master
        m_config.outputs.clear();
        m_config.outputs.emplace_back(RouteDestination::MASTER_OUT, 0);
    }
    
    return Ok(true);
}

void AudioBus::set_volume_db(double volume_db) {
    // Clamp to reasonable range
    volume_db = std::clamp(volume_db, -70.0, 20.0);
    
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.volume_db = volume_db;
}

double AudioBus::get_volume_linear() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    if (m_config.volume_db <= -70.0) {
        return 0.0;
    }
    return std::pow(10.0, m_config.volume_db / 20.0);
}

void AudioBus::set_pan_position(double pan) {
    pan = std::clamp(pan, -1.0, 1.0);
    
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.pan_position = pan;
}

Result<bool> AudioBus::add_input_source(uint32_t source_id, double level) {
    if (level < 0.0 || level > 10.0) {
        return Error("Invalid input level: " + std::to_string(level));
    }
    
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    m_input_sources[source_id] = level;
    return Ok(true);
}

Result<bool> AudioBus::remove_input_source(uint32_t source_id) {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    
    auto it = m_input_sources.find(source_id);
    if (it == m_input_sources.end()) {
        return Error("Input source not found: " + std::to_string(source_id));
    }
    
    m_input_sources.erase(it);
    return Ok(true);
}

Result<bool> AudioBus::set_input_level(uint32_t source_id, double level) {
    if (level < 0.0 || level > 10.0) {
        return Error("Invalid input level: " + std::to_string(level));
    }
    
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    
    auto it = m_input_sources.find(source_id);
    if (it == m_input_sources.end()) {
        return Error("Input source not found: " + std::to_string(source_id));
    }
    
    it->second = level;
    return Ok(true);
}

double AudioBus::get_input_level(uint32_t source_id) const {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    
    auto it = m_input_sources.find(source_id);
    if (it == m_input_sources.end()) {
        return 0.0;
    }
    
    return it->second;
}

std::vector<uint32_t> AudioBus::get_input_sources() const {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    
    std::vector<uint32_t> sources;
    sources.reserve(m_input_sources.size());
    
    for (const auto& [source_id, level] : m_input_sources) {
        sources.push_back(source_id);
    }
    
    return sources;
}

size_t AudioBus::get_input_count() const {
    std::lock_guard<std::mutex> lock(m_inputs_mutex);
    return m_input_sources.size();
}

Result<bool> AudioBus::add_output_destination(const RouteDestination& destination) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    
    // Check if destination already exists
    for (const auto& existing : m_config.outputs) {
        if (existing.type == destination.type && existing.destination_id == destination.destination_id) {
            return Error("Output destination already exists");
        }
    }
    
    m_config.outputs.push_back(destination);
    return Ok(true);
}

Result<bool> AudioBus::remove_output_destination(uint32_t destination_id) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    
    auto it = std::remove_if(m_config.outputs.begin(), m_config.outputs.end(),
        [destination_id](const RouteDestination& dest) {
            return dest.destination_id == destination_id;
        });
    
    if (it == m_config.outputs.end()) {
        return Error("Output destination not found: " + std::to_string(destination_id));
    }
    
    m_config.outputs.erase(it, m_config.outputs.end());
    return Ok(true);
}

Result<bool> AudioBus::update_output_destination(const RouteDestination& destination) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    
    for (auto& existing : m_config.outputs) {
        if (existing.type == destination.type && existing.destination_id == destination.destination_id) {
            existing = destination;
            return Ok(true);
        }
    }
    
    return Error("Output destination not found for update");
}

std::vector<RouteDestination> AudioBus::get_output_destinations() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config.outputs;
}

size_t AudioBus::get_output_count() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config.outputs.size();
}

Result<bool> AudioBus::process_audio(std::shared_ptr<AudioBuffer> input_buffer,
                                     std::shared_ptr<AudioBuffer> output_buffer,
                                     uint64_t start_time_samples,
                                     uint32_t buffer_size) {
    
    std::lock_guard<std::mutex> lock(m_processing_mutex);
    
    if (!input_buffer || !output_buffer) {
        return Error("Invalid audio buffers");
    }
    
    if (input_buffer->get_channel_count() != m_config.channel_count ||
        output_buffer->get_channel_count() != m_config.channel_count) {
        return Error("Channel count mismatch");
    }
    
    // Check if bus is muted or bypassed
    if (is_mixer_muted() && !is_soloed()) {
        output_buffer->clear();
        return Ok(true);
    }
    
    // Copy input to internal buffer for processing
    if (m_internal_buffer->get_max_buffer_size() < buffer_size) {
        m_internal_buffer->resize_buffers(buffer_size);
    }
    m_internal_buffer->copy_from(*input_buffer, buffer_size);
    
    // Apply delay compensation first (before effects)
    apply_delay_compensation(m_internal_buffer, buffer_size);
    
    // Process effects chain
    process_effects_chain(m_internal_buffer, start_time_samples, buffer_size);
    
    // Apply volume and pan
    apply_volume_and_pan(m_internal_buffer, buffer_size);
    
    // Update metering
    if (m_metering_enabled) {
        update_metering(m_internal_buffer, buffer_size);
    }
    
    // Copy to output buffer
    output_buffer->copy_from(*m_internal_buffer, buffer_size);
    
    // Update activity state
    update_activity_state(output_buffer, buffer_size);
    
    // Update sample count
    m_samples_processed.fetch_add(buffer_size);
    
    return Ok(true);
}

void AudioBus::clear_audio_buffers() {
    std::lock_guard<std::mutex> lock(m_processing_mutex);
    
    if (m_internal_buffer) {
        m_internal_buffer->clear();
    }
    if (m_delay_buffer) {
        m_delay_buffer->clear();
    }
    
    // Clear delay line
    std::fill(m_delay_line.begin(), m_delay_line.end(), 0.0);
    m_delay_write_pos = 0;
}

void AudioBus::set_delay_compensation_samples(uint32_t samples) {
    std::lock_guard<std::mutex> lock(m_processing_mutex);
    
    m_delay_compensation = DelayCompensation(samples);
    
    // Ensure delay line is large enough
    uint32_t required_size = m_config.channel_count * (samples + 1024);  // Extra headroom
    if (m_delay_line.size() < required_size) {
        m_delay_line.resize(required_size, 0.0);
    }
}

uint32_t AudioBus::get_delay_compensation_samples() const {
    std::lock_guard<std::mutex> lock(m_processing_mutex);
    return m_delay_compensation.samples_delay;
}

double AudioBus::get_delay_compensation_ms() const {
    std::lock_guard<std::mutex> lock(m_processing_mutex);
    return m_delay_compensation.ms_delay;
}

Result<bool> AudioBus::add_effect(std::shared_ptr<AudioEffect> effect, int position) {
    if (!effect) {
        return Error("Invalid effect");
    }
    
    std::lock_guard<std::mutex> lock(m_effects_mutex);
    
    if (position < 0 || position >= static_cast<int>(m_effects_chain.size())) {
        m_effects_chain.push_back(effect);
    } else {
        m_effects_chain.insert(m_effects_chain.begin() + position, effect);
    }
    
    return Ok(true);
}

Result<bool> AudioBus::remove_effect(uint32_t effect_id) {
    std::lock_guard<std::mutex> lock(m_effects_mutex);
    
    auto it = std::remove_if(m_effects_chain.begin(), m_effects_chain.end(),
        [effect_id](const std::weak_ptr<AudioEffect>& effect) {
            if (auto locked_effect = effect.lock()) {
                return locked_effect->get_effect_id() == effect_id;
            }
            return true;  // Remove expired weak_ptrs
        });
    
    if (it == m_effects_chain.end()) {
        return Error("Effect not found: " + std::to_string(effect_id));
    }
    
    m_effects_chain.erase(it, m_effects_chain.end());
    return Ok(true);
}

std::vector<std::shared_ptr<AudioEffect>> AudioBus::get_effects_chain() const {
    std::lock_guard<std::mutex> lock(m_effects_mutex);
    return m_effects_chain;
}

size_t AudioBus::get_effects_count() const {
    std::lock_guard<std::mutex> lock(m_effects_mutex);
    return m_effects_chain.size();
}

MeterData AudioBus::get_meter_data() const {
    if (m_meter_processor) {
        return m_meter_processor->get_meter_data();
    }
    
    MeterData empty_data;
    empty_data.peak_levels.resize(m_config.channel_count, 0.0);
    empty_data.peak_levels_db.resize(m_config.channel_count, -70.0);
    empty_data.rms_levels.resize(m_config.channel_count, 0.0);
    empty_data.rms_levels_db.resize(m_config.channel_count, -70.0);
    empty_data.clip_indicators.resize(m_config.channel_count, false);
    return empty_data;
}

void AudioBus::reset_meters() {
    if (m_meter_processor) {
        m_meter_processor->reset_meters();
    }
}

// Private helper methods

void AudioBus::apply_volume_and_pan(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    double volume_linear = get_volume_linear();
    double pan_position = get_pan_position();
    
    if (m_config.channel_count >= 2) {
        // Stereo panning
        double left_gain = volume_linear * std::sqrt(0.5 * (1.0 - pan_position));
        double right_gain = volume_linear * std::sqrt(0.5 * (1.0 + pan_position));
        
        buffer->apply_gain(0, left_gain, buffer_size);
        buffer->apply_gain(1, right_gain, buffer_size);
        
        // Apply volume to additional channels
        for (uint32_t ch = 2; ch < m_config.channel_count; ++ch) {
            buffer->apply_gain(ch, volume_linear, buffer_size);
        }
    } else {
        // Mono - just apply volume
        buffer->apply_gain(0, volume_linear, buffer_size);
    }
}

void AudioBus::process_effects_chain(std::shared_ptr<AudioBuffer> buffer, uint64_t start_time, uint32_t buffer_size) {
    std::lock_guard<std::mutex> lock(m_effects_mutex);
    
    for (auto& effect : m_effects_chain) {
        if (effect && !effect->is_bypassed()) {
            effect->process_audio(buffer, buffer, start_time, buffer_size);
        }
    }
}

void AudioBus::apply_delay_compensation(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    uint32_t delay_samples = m_delay_compensation.samples_delay;
    if (delay_samples == 0) {
        return;  // No delay compensation needed
    }
    
    uint32_t channels = buffer->get_channel_count();
    uint32_t delay_line_size = m_delay_line.size() / channels;
    
    for (uint32_t ch = 0; ch < channels; ++ch) {
        auto channel_data = buffer->get_channel_data(ch);
        double* delay_channel = &m_delay_line[ch * delay_line_size];
        
        for (uint32_t sample = 0; sample < buffer_size; ++sample) {
            // Read delayed sample
            uint32_t read_pos = (m_delay_write_pos + delay_line_size - delay_samples) % delay_line_size;
            double delayed_sample = delay_channel[read_pos];
            
            // Write current sample to delay line
            delay_channel[m_delay_write_pos] = channel_data[sample];
            
            // Output delayed sample
            channel_data[sample] = delayed_sample;
            
            // Advance write position
            m_delay_write_pos = (m_delay_write_pos + 1) % delay_line_size;
        }
    }
}

void AudioBus::update_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    if (m_meter_processor) {
        m_meter_processor->process_metering(buffer, buffer_size);
    }
}

void AudioBus::update_activity_state(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    bool has_signal = has_audio_signal(buffer, buffer_size);
    m_is_active.store(has_signal);
}

bool AudioBus::has_audio_signal(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size, double threshold) {
    double threshold_linear = std::pow(10.0, threshold / 20.0);
    
    for (uint32_t ch = 0; ch < buffer->get_channel_count(); ++ch) {
        auto channel_data = buffer->get_channel_data(ch);
        
        for (uint32_t sample = 0; sample < buffer_size; ++sample) {
            if (std::abs(channel_data[sample]) > threshold_linear) {
                return true;
            }
        }
    }
    
    return false;
}

// AudioBusFactory implementations

std::unique_ptr<AudioBus> AudioBusFactory::create_aux_send_bus(uint32_t bus_id, const std::string& name) {
    BusConfig config(BusConfig::AUX_SEND, name, 2);
    config.volume_db = -10.0;  // Default send level
    return std::make_unique<AudioBus>(bus_id, config);
}

std::unique_ptr<AudioBus> AudioBusFactory::create_group_bus(uint32_t bus_id, const std::string& name) {
    BusConfig config(BusConfig::GROUP_BUS, name, 2);
    return std::make_unique<AudioBus>(bus_id, config);
}

std::unique_ptr<AudioBus> AudioBusFactory::create_master_bus(uint32_t bus_id, const std::string& name) {
    BusConfig config(BusConfig::MASTER_BUS, name, 2);
    config.outputs.clear();  // Master doesn't route anywhere by default
    return std::make_unique<AudioBus>(bus_id, config);
}

std::unique_ptr<AudioBus> AudioBusFactory::create_monitor_bus(uint32_t bus_id, const std::string& name) {
    BusConfig config(BusConfig::MONITOR_BUS, name, 2);
    config.outputs.clear();  // Monitor output handled separately
    return std::make_unique<AudioBus>(bus_id, config);
}

std::unique_ptr<AudioBus> AudioBusFactory::create_custom_bus(uint32_t bus_id, const BusConfig& config) {
    return std::make_unique<AudioBus>(bus_id, config);
}

// AudioBusManager implementations

AudioBusManager::AudioBusManager() {
    // Create default master bus
    auto master_config = BusConfig(BusConfig::MASTER_BUS, "Master", 2);
    auto master_bus = AudioBusFactory::create_master_bus(generate_bus_id(), "Master");
    m_master_bus_id = master_bus->get_bus_id();
    m_buses[m_master_bus_id] = std::move(master_bus);
}

AudioBusManager::~AudioBusManager() = default;

Result<uint32_t> AudioBusManager::create_bus(const BusConfig& config) {
    std::lock_guard<std::mutex> lock(m_buses_mutex);
    
    uint32_t bus_id = generate_bus_id();
    auto bus = AudioBusFactory::create_custom_bus(bus_id, config);
    
    m_buses[bus_id] = std::move(bus);
    update_statistics();
    
    return Ok(bus_id);
}

Result<bool> AudioBusManager::remove_bus(uint32_t bus_id) {
    if (bus_id == m_master_bus_id) {
        return Error("Cannot remove master bus");
    }
    
    std::lock_guard<std::mutex> lock(m_buses_mutex);
    
    auto it = m_buses.find(bus_id);
    if (it == m_buses.end()) {
        return Error("Bus not found: " + std::to_string(bus_id));
    }
    
    m_buses.erase(it);
    update_statistics();
    
    return Ok(true);
}

Result<std::shared_ptr<AudioBus>> AudioBusManager::get_bus(uint32_t bus_id) {
    std::lock_guard<std::mutex> lock(m_buses_mutex);
    
    auto it = m_buses.find(bus_id);
    if (it == m_buses.end()) {
        return Error("Bus not found: " + std::to_string(bus_id));
    }
    
    return Ok(it->second);
}

std::vector<uint32_t> AudioBusManager::get_all_bus_ids() const {
    std::lock_guard<std::mutex> lock(m_buses_mutex);
    
    std::vector<uint32_t> bus_ids;
    bus_ids.reserve(m_buses.size());
    
    for (const auto& [bus_id, bus] : m_buses) {
        bus_ids.push_back(bus_id);
    }
    
    return bus_ids;
}

std::vector<std::shared_ptr<AudioBus>> AudioBusManager::get_all_buses() const {
    std::lock_guard<std::mutex> lock(m_buses_mutex);
    
    std::vector<std::shared_ptr<AudioBus>> buses;
    buses.reserve(m_buses.size());
    
    for (const auto& [bus_id, bus] : m_buses) {
        buses.push_back(bus);
    }
    
    return buses;
}

std::shared_ptr<AudioBus> AudioBusManager::get_master_bus() {
    auto result = get_bus(m_master_bus_id);
    return result.is_ok() ? result.unwrap() : nullptr;
}

void AudioBusManager::set_global_solo_active(bool active) {
    m_global_solo_active = active;
    update_solo_mute_states();
}

void AudioBusManager::update_solo_mute_states() {
    std::lock_guard<std::mutex> lock(m_buses_mutex);
    
    // Find if any bus is soloed
    bool any_soloed = false;
    for (const auto& [bus_id, bus] : m_buses) {
        if (bus && bus->is_soloed()) {
            any_soloed = true;
            break;
        }
    }
    
    m_global_solo_active = any_soloed;
    
    // Update each bus's solo state
    for (const auto& [bus_id, bus] : m_buses) {
        if (bus) {
            bus->set_solo_active_in_mixer(m_global_solo_active);
            
            // If global solo is active, mute non-soloed buses
            if (m_global_solo_active) {
                bus->set_mixer_mute_override(!bus->is_soloed());
            } else {
                bus->set_mixer_mute_override(false);
            }
        }
    }
}

uint32_t AudioBusManager::generate_bus_id() {
    return m_next_bus_id++;
}

void AudioBusManager::update_statistics() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_statistics.total_buses = m_buses.size();
    m_statistics.active_buses = 0;
    m_statistics.total_samples_processed = 0;
    
    for (const auto& [bus_id, bus] : m_buses) {
        if (bus) {
            if (bus->is_active()) {
                m_statistics.active_buses++;
            }
            m_statistics.total_samples_processed += bus->get_samples_processed();
        }
    }
}

} // namespace mixmind