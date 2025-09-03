#include "StepSequencer.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace mixmind {

StepSequencer::StepSequencer(std::shared_ptr<MIDIClip> clip) : m_clip(clip) {
    initialize_pattern();
}

void StepSequencer::set_clip(std::shared_ptr<MIDIClip> clip) {
    m_clip = clip;
    
    // Update the pattern from the existing MIDI clip
    if (m_clip) {
        update_midi_clip();
    }
}

void StepSequencer::set_pattern_length(PatternLength length) {
    m_pattern_length = length;
    resize_pattern();
    notify_edit_changed();
}

void StepSequencer::set_step_resolution(StepResolution resolution) {
    m_step_resolution = resolution;
    resize_pattern();
    notify_edit_changed();
}

size_t StepSequencer::get_total_steps() const {
    size_t steps_per_bar = get_steps_per_bar();
    return steps_per_bar * static_cast<size_t>(m_pattern_length);
}

size_t StepSequencer::get_steps_per_bar() const {
    int resolution = static_cast<int>(m_step_resolution);
    if (resolution < 0) {
        // Triplet resolution
        return static_cast<size_t>(-resolution * 3 / 2);
    }
    return static_cast<size_t>(resolution);
}

double StepSequencer::get_step_length_beats() const {
    int resolution = static_cast<int>(m_step_resolution);
    if (resolution < 0) {
        // Triplet resolution: 1/8 triplet = 1/12 beats, 1/16 triplet = 1/24 beats
        return 4.0 / (-resolution * 3.0 / 2.0);
    }
    return 4.0 / resolution;
}

double StepSequencer::get_pattern_length_beats() const {
    return get_step_length_beats() * get_total_steps();
}

Result<bool> StepSequencer::add_drum_lane(uint8_t note_number, const std::string& name) {
    if (m_pattern.find(note_number) != m_pattern.end()) {
        return Result<bool>::error("Drum lane already exists for note " + std::to_string(note_number));
    }
    
    // Initialize empty pattern for this note
    m_pattern[note_number] = std::vector<Step>(get_total_steps());
    
    // Set up default lane config
    std::string lane_name = name.empty() ? ("Note " + std::to_string(note_number)) : name;
    m_lane_configs[note_number] = create_default_lane_config(note_number, lane_name);
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::remove_drum_lane(uint8_t note_number) {
    auto pattern_it = m_pattern.find(note_number);
    auto config_it = m_lane_configs.find(note_number);
    
    if (pattern_it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    m_pattern.erase(pattern_it);
    if (config_it != m_lane_configs.end()) {
        m_lane_configs.erase(config_it);
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

std::vector<uint8_t> StepSequencer::get_drum_notes() const {
    std::vector<uint8_t> notes;
    for (const auto& [note, pattern] : m_pattern) {
        notes.push_back(note);
    }
    
    // Sort notes in ascending order
    std::sort(notes.begin(), notes.end());
    return notes;
}

void StepSequencer::set_lane_config(uint8_t note_number, const DrumLaneConfig& config) {
    m_lane_configs[note_number] = config;
    notify_edit_changed();
}

DrumLaneConfig StepSequencer::get_lane_config(uint8_t note_number) const {
    auto it = m_lane_configs.find(note_number);
    return (it != m_lane_configs.end()) ? it->second : create_default_lane_config(note_number, "");
}

Result<bool> StepSequencer::toggle_step(uint8_t note_number, size_t step_index) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    if (step_index >= it->second.size()) {
        return Result<bool>::error("Step index out of range");
    }
    
    it->second[step_index].active = !it->second[step_index].active;
    
    // Set default velocity if activating step
    if (it->second[step_index].active && it->second[step_index].velocity == 0) {
        auto config_it = m_lane_configs.find(note_number);
        uint8_t default_vel = (config_it != m_lane_configs.end()) ? config_it->second.default_velocity : 100;
        it->second[step_index].velocity = default_vel;
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::set_step(uint8_t note_number, size_t step_index, bool active) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    if (step_index >= it->second.size()) {
        return Result<bool>::error("Step index out of range");
    }
    
    it->second[step_index].active = active;
    
    // Set default velocity if activating step
    if (active && it->second[step_index].velocity == 0) {
        auto config_it = m_lane_configs.find(note_number);
        uint8_t default_vel = (config_it != m_lane_configs.end()) ? config_it->second.default_velocity : 100;
        it->second[step_index].velocity = default_vel;
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::set_step_velocity(uint8_t note_number, size_t step_index, uint8_t velocity) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    if (step_index >= it->second.size()) {
        return Result<bool>::error("Step index out of range");
    }
    
    if (velocity == 0 || velocity > 127) {
        return Result<bool>::error("Velocity must be 1-127");
    }
    
    it->second[step_index].velocity = velocity;
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::set_step_probability(uint8_t note_number, size_t step_index, uint8_t probability) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    if (step_index >= it->second.size()) {
        return Result<bool>::error("Step index out of range");
    }
    
    if (probability > 100) {
        return Result<bool>::error("Probability must be 0-100");
    }
    
    it->second[step_index].probability = probability;
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::set_step_micro_timing(uint8_t note_number, size_t step_index, int8_t micro_timing) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    if (step_index >= it->second.size()) {
        return Result<bool>::error("Step index out of range");
    }
    
    if (micro_timing < -50 || micro_timing > 50) {
        return Result<bool>::error("Micro timing must be -50 to +50");
    }
    
    it->second[step_index].micro_timing = micro_timing;
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::set_step_length(uint8_t note_number, size_t step_index, double length_multiplier) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    if (step_index >= it->second.size()) {
        return Result<bool>::error("Step index out of range");
    }
    
    if (length_multiplier <= 0.0 || length_multiplier > 4.0) {
        return Result<bool>::error("Length multiplier must be 0.0-4.0");
    }
    
    it->second[step_index].length_multiplier = length_multiplier;
    notify_edit_changed();
    return Result<bool>::success(true);
}

Step StepSequencer::get_step(uint8_t note_number, size_t step_index) const {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end() || step_index >= it->second.size()) {
        return Step(); // Return inactive step
    }
    
    return it->second[step_index];
}

bool StepSequencer::is_step_active(uint8_t note_number, size_t step_index) const {
    return get_step(note_number, step_index).active;
}

Result<bool> StepSequencer::clear_pattern() {
    for (auto& [note, pattern] : m_pattern) {
        for (auto& step : pattern) {
            step = Step(); // Reset to inactive
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::clear_lane(uint8_t note_number) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    for (auto& step : it->second) {
        step = Step(); // Reset to inactive
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::copy_lane(uint8_t source_note, uint8_t dest_note) {
    auto source_it = m_pattern.find(source_note);
    if (source_it == m_pattern.end()) {
        return Result<bool>::error("Source drum lane not found for note " + std::to_string(source_note));
    }
    
    // Create destination lane if it doesn't exist
    if (m_pattern.find(dest_note) == m_pattern.end()) {
        add_drum_lane(dest_note, "Note " + std::to_string(dest_note));
    }
    
    m_pattern[dest_note] = source_it->second;
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::shift_lane(uint8_t note_number, int step_offset) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    if (step_offset == 0) {
        return Result<bool>::success(true);
    }
    
    std::vector<Step> original = it->second;
    size_t total_steps = original.size();
    
    // Clear the pattern first
    for (auto& step : it->second) {
        step = Step();
    }
    
    // Copy steps with offset
    for (size_t i = 0; i < total_steps; ++i) {
        if (original[i].active) {
            int new_index = static_cast<int>(i) + step_offset;
            
            // Wrap around if necessary
            while (new_index < 0) new_index += static_cast<int>(total_steps);
            while (new_index >= static_cast<int>(total_steps)) new_index -= static_cast<int>(total_steps);
            
            it->second[new_index] = original[i];
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::reverse_lane(uint8_t note_number) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    std::reverse(it->second.begin(), it->second.end());
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::randomize_lane(uint8_t note_number, float probability) {
    auto it = m_pattern.find(note_number);
    if (it == m_pattern.end()) {
        return Result<bool>::error("Drum lane not found for note " + std::to_string(note_number));
    }
    
    if (probability < 0.0f || probability > 1.0f) {
        return Result<bool>::error("Probability must be 0.0-1.0");
    }
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::uniform_int_distribution<uint8_t> vel_dist(80, 127);
    
    auto config_it = m_lane_configs.find(note_number);
    uint8_t default_vel = (config_it != m_lane_configs.end()) ? config_it->second.default_velocity : 100;
    
    for (auto& step : it->second) {
        step.active = (dist(gen) < probability);
        if (step.active) {
            step.velocity = vel_dist(gen);
        } else {
            step.velocity = default_vel;
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::generate_midi_from_pattern() {
    if (!m_clip) {
        return Result<bool>::error("No MIDI clip loaded");
    }
    
    // Clear existing notes in the clip
    m_clip->clear_all_notes();
    
    double step_length_beats = get_step_length_beats();
    
    for (const auto& [note_number, pattern] : m_pattern) {
        auto config_it = m_lane_configs.find(note_number);
        bool lane_muted = (config_it != m_lane_configs.end()) ? config_it->second.muted : false;
        
        if (lane_muted) continue;
        
        for (size_t step_index = 0; step_index < pattern.size(); ++step_index) {
            const Step& step = pattern[step_index];
            
            if (!step.active || step.muted) continue;
            
            // Calculate timing
            double step_time_beats = step_index * step_length_beats;
            uint64_t start_time_samples = MIDIClip::beats_to_samples(step_time_beats, m_bpm, 44100.0);
            
            // Apply swing
            apply_swing_to_step(step_index, start_time_samples);
            
            // Apply micro timing
            if (step.micro_timing != 0) {
                double micro_offset_ms = step.micro_timing * 2.0; // -50 to +50 becomes -100ms to +100ms
                int64_t micro_offset_samples = static_cast<int64_t>((micro_offset_ms / 1000.0) * 44100.0);
                start_time_samples = static_cast<uint64_t>(std::max(0LL, static_cast<int64_t>(start_time_samples) + micro_offset_samples));
            }
            
            // Apply humanization
            if (m_humanize_timing > 0.0f) {
                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::uniform_real_distribution<float> timing_dist(-m_humanize_timing, m_humanize_timing);
                
                float timing_offset_ms = timing_dist(gen);
                int64_t timing_offset_samples = static_cast<int64_t>((timing_offset_ms / 1000.0) * 44100.0);
                start_time_samples = static_cast<uint64_t>(std::max(0LL, static_cast<int64_t>(start_time_samples) + timing_offset_samples));
            }
            
            // Calculate note length
            double note_length_beats = step_length_beats * step.length_multiplier;
            uint64_t note_length_samples = MIDIClip::beats_to_samples(note_length_beats, m_bpm, 44100.0);
            
            // Calculate velocity with humanization
            uint8_t velocity = step.velocity;
            if (m_humanize_velocity > 0.0f) {
                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::uniform_int_distribution<int> vel_dist(-static_cast<int>(m_humanize_velocity * 20), static_cast<int>(m_humanize_velocity * 20));
                
                int new_velocity = static_cast<int>(velocity) + vel_dist(gen);
                velocity = static_cast<uint8_t>(std::clamp(new_velocity, 1, 127));
            }
            
            // Create MIDI note
            MIDINote note(note_number, velocity, start_time_samples, note_length_samples);
            auto result = m_clip->add_note(note);
            if (!result.is_success()) {
                return result;
            }
        }
    }
    
    notify_edit_changed();
    return Result<bool>::success(true);
}

Result<bool> StepSequencer::update_midi_clip() {
    return generate_midi_from_pattern();
}

Result<bool> StepSequencer::input_note_at_current_step(uint8_t note_number, uint8_t velocity) {
    if (!m_step_input_active) {
        return Result<bool>::error("Step input mode not active");
    }
    
    // Create lane if it doesn't exist
    if (m_pattern.find(note_number) == m_pattern.end()) {
        add_drum_lane(note_number, "Note " + std::to_string(note_number));
    }
    
    auto result = set_step(note_number, m_current_step, true);
    if (result.is_success()) {
        set_step_velocity(note_number, m_current_step, velocity);
    }
    
    return result;
}

void StepSequencer::set_swing(float swing_amount) {
    m_swing = std::clamp(swing_amount, 0.0f, 1.0f);
    notify_edit_changed();
}

void StepSequencer::create_basic_kick_pattern() {
    uint8_t kick_note = DrumKitPresets::KICK_1;
    add_drum_lane(kick_note, "Kick");
    
    // Basic four-on-the-floor pattern
    size_t steps_per_bar = get_steps_per_bar();
    for (size_t bar = 0; bar < static_cast<size_t>(m_pattern_length); ++bar) {
        for (size_t beat = 0; beat < 4; ++beat) {
            size_t step_index = bar * steps_per_bar + beat * (steps_per_bar / 4);
            set_step(kick_note, step_index, true);
            set_step_velocity(kick_note, step_index, 120);
        }
    }
}

void StepSequencer::create_basic_snare_pattern() {
    uint8_t snare_note = DrumKitPresets::SNARE_1;
    add_drum_lane(snare_note, "Snare");
    
    // Basic snare on beats 2 and 4
    size_t steps_per_bar = get_steps_per_bar();
    for (size_t bar = 0; bar < static_cast<size_t>(m_pattern_length); ++bar) {
        for (size_t beat : {1, 3}) { // Beats 2 and 4 (0-indexed)
            size_t step_index = bar * steps_per_bar + beat * (steps_per_bar / 4);
            set_step(snare_note, step_index, true);
            set_step_velocity(snare_note, step_index, 110);
        }
    }
}

void StepSequencer::create_basic_hihat_pattern() {
    uint8_t hihat_note = DrumKitPresets::HIHAT_CLOSED;
    add_drum_lane(hihat_note, "Hi-Hat");
    
    // Basic 16th note hi-hat pattern
    if (m_step_resolution == StepResolution::SIXTEENTH) {
        for (size_t step = 0; step < get_total_steps(); ++step) {
            set_step(hihat_note, step, true);
            
            // Vary velocity for groove
            uint8_t velocity = ((step % 4) == 0) ? 100 : 80;
            set_step_velocity(hihat_note, step, velocity);
        }
    }
}

void StepSequencer::initialize_pattern() {
    resize_pattern();
}

void StepSequencer::resize_pattern() {
    size_t new_total_steps = get_total_steps();
    
    for (auto& [note, pattern] : m_pattern) {
        pattern.resize(new_total_steps);
    }
    
    // Ensure current step is within bounds
    if (m_current_step >= new_total_steps) {
        m_current_step = 0;
    }
}

void StepSequencer::notify_edit_changed() {
    if (m_edit_callback) {
        m_edit_callback();
    }
}

uint64_t StepSequencer::step_to_samples(size_t step_index) const {
    double beats = step_to_beats(step_index);
    return MIDIClip::beats_to_samples(beats, m_bpm, 44100.0);
}

double StepSequencer::step_to_beats(size_t step_index) const {
    return step_index * get_step_length_beats();
}

void StepSequencer::apply_swing_to_step(size_t step_index, uint64_t& time_samples) const {
    if (m_swing <= 0.0f) return;
    
    // Apply swing to off-beat steps (odd 8th notes)
    size_t steps_per_eighth = get_steps_per_bar() / 8;
    if (steps_per_eighth == 0) return;
    
    size_t position_in_eighth = step_index % (steps_per_eighth * 2);
    if (position_in_eighth == steps_per_eighth) {
        // This is an off-beat 8th note, apply swing
        double swing_offset_beats = (get_step_length_beats() * steps_per_eighth) * (m_swing * 0.3); // Max 30% swing
        uint64_t swing_offset_samples = MIDIClip::beats_to_samples(swing_offset_beats, m_bpm, 44100.0);
        time_samples += swing_offset_samples;
    }
}

DrumLaneConfig StepSequencer::create_default_lane_config(uint8_t note_number, const std::string& name) const {
    DrumLaneConfig config;
    config.note_number = note_number;
    config.name = name.empty() ? ("Note " + std::to_string(note_number)) : name;
    config.default_velocity = 100;
    
    // Set color based on note range
    if (note_number >= 35 && note_number <= 38) {
        config.color = 0xFFFF4040; // Red for kicks/low drums
    } else if (note_number >= 39 && note_number <= 42) {
        config.color = 0xFF40FF40; // Green for snares/claps
    } else if (note_number >= 42 && note_number <= 46) {
        config.color = 0xFF4040FF; // Blue for hi-hats
    } else if (note_number >= 47 && note_number <= 53) {
        config.color = 0xFFFFFF40; // Yellow for toms
    } else {
        config.color = 0xFF8080C0; // Purple for cymbals/other
    }
    
    return config;
}

// StepSequencerFactory implementations
std::unique_ptr<StepSequencer> StepSequencerFactory::create_drum_sequencer(std::shared_ptr<MIDIClip> clip) {
    auto sequencer = std::make_unique<StepSequencer>(clip);
    
    // Add standard drum lanes
    sequencer->add_drum_lane(DrumKitPresets::KICK_1, "Kick");
    sequencer->add_drum_lane(DrumKitPresets::SNARE_1, "Snare");
    sequencer->add_drum_lane(DrumKitPresets::HIHAT_CLOSED, "Hi-Hat");
    sequencer->add_drum_lane(DrumKitPresets::HIHAT_OPEN, "Open Hat");
    sequencer->add_drum_lane(DrumKitPresets::CRASH_1, "Crash");
    sequencer->add_drum_lane(DrumKitPresets::RIDE, "Ride");
    
    return sequencer;
}

std::unique_ptr<StepSequencer> StepSequencerFactory::create_808_sequencer(std::shared_ptr<MIDIClip> clip) {
    auto sequencer = std::make_unique<StepSequencer>(clip);
    
    // Add 808-style drum lanes
    sequencer->add_drum_lane(36, "808 Kick");
    sequencer->add_drum_lane(38, "808 Snare");
    sequencer->add_drum_lane(42, "808 Hi-Hat");
    sequencer->add_drum_lane(39, "808 Clap");
    sequencer->add_drum_lane(45, "808 Tom");
    
    return sequencer;
}

// DrumKitPresets implementations
std::vector<DrumKitPresets::DrumMapping> DrumKitPresets::get_standard_kit() {
    return {
        {KICK_1, "Kick 1", 0xFFFF4040, 120},
        {SNARE_1, "Snare 1", 0xFF40FF40, 110},
        {HIHAT_CLOSED, "Hi-Hat Closed", 0xFF4040FF, 90},
        {HIHAT_OPEN, "Hi-Hat Open", 0xFF6060FF, 85},
        {CLAP, "Clap", 0xFF40FF80, 105},
        {CRASH_1, "Crash 1", 0xFFFFFF40, 100},
        {RIDE, "Ride", 0xFFC0C040, 95},
        {TOM_LOW, "Low Tom", 0xFFFF8040, 100},
        {TOM_MID, "Mid Tom", 0xFFC08040, 95},
        {TOM_HIGH, "High Tom", 0xFF8080C0, 90}
    };
}

} // namespace mixmind