#include "MIDIClip.h"
#include <algorithm>
#include <random>
#include <cmath>

namespace mixmind {

MIDIClip::MIDIClip(const std::string& name) : m_name(name) {
    // Set default length to 4 bars at 120 BPM
    m_length = bars_to_samples(4.0, 120.0, 4, 44100.0);
}

Result<bool> MIDIClip::add_note(const MIDINote& note) {
    if (!validate_note(note)) {
        return Result<bool>::error("Invalid note parameters");
    }
    
    m_notes.push_back(note);
    sort_notes_by_time();
    
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::remove_note(size_t note_index) {
    if (note_index >= m_notes.size()) {
        return Result<bool>::error("Note index out of range");
    }
    
    m_notes.erase(m_notes.begin() + note_index);
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::remove_selected_notes() {
    auto it = std::remove_if(m_notes.begin(), m_notes.end(),
        [](const MIDINote& note) { return note.selected; });
    
    size_t removed_count = std::distance(it, m_notes.end());
    m_notes.erase(it, m_notes.end());
    
    return Result<bool>::success(removed_count > 0);
}

Result<MIDINote*> MIDIClip::get_note_at_time(uint64_t time, uint8_t note_number) {
    for (auto& note : m_notes) {
        if (note.note_number == note_number && note.contains_time(time)) {
            return Result<MIDINote*>::success(&note);
        }
    }
    
    return Result<MIDINote*>::error("No note found at specified time and pitch");
}

std::vector<MIDINote*> MIDIClip::get_notes_in_range(uint64_t start_time, uint64_t end_time) {
    std::vector<MIDINote*> result;
    
    for (auto& note : m_notes) {
        if (note.overlaps(start_time, end_time)) {
            result.push_back(&note);
        }
    }
    
    return result;
}

std::vector<MIDINote*> MIDIClip::get_selected_notes() {
    std::vector<MIDINote*> result;
    
    for (auto& note : m_notes) {
        if (note.selected) {
            result.push_back(&note);
        }
    }
    
    return result;
}

void MIDIClip::select_all_notes() {
    for (auto& note : m_notes) {
        note.selected = true;
    }
}

void MIDIClip::deselect_all_notes() {
    for (auto& note : m_notes) {
        note.selected = false;
    }
}

void MIDIClip::select_notes_in_range(uint64_t start_time, uint64_t end_time, uint8_t min_note, uint8_t max_note) {
    for (auto& note : m_notes) {
        if (note.overlaps(start_time, end_time) && 
            note.note_number >= min_note && 
            note.note_number <= max_note) {
            note.selected = true;
        }
    }
}

Result<bool> MIDIClip::move_selected_notes(int64_t time_delta, int8_t pitch_delta) {
    for (auto& note : m_notes) {
        if (note.selected) {
            // Apply time delta
            int64_t new_start_time = static_cast<int64_t>(note.start_time) + time_delta;
            if (new_start_time < 0) {
                new_start_time = 0;
            }
            note.start_time = static_cast<uint64_t>(new_start_time);
            
            // Apply pitch delta
            int new_note_number = static_cast<int>(note.note_number) + pitch_delta;
            if (new_note_number < 0) new_note_number = 0;
            if (new_note_number > 127) new_note_number = 127;
            note.note_number = static_cast<uint8_t>(new_note_number);
        }
    }
    
    sort_notes_by_time();
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::resize_selected_notes(int64_t length_delta) {
    for (auto& note : m_notes) {
        if (note.selected) {
            int64_t new_length = static_cast<int64_t>(note.length) + length_delta;
            if (new_length < 100) { // Minimum note length (about 2ms at 44.1kHz)
                new_length = 100;
            }
            note.length = static_cast<uint64_t>(new_length);
        }
    }
    
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::set_selected_velocity(uint8_t velocity) {
    if (velocity == 0 || velocity > 127) {
        return Result<bool>::error("Invalid velocity value (must be 1-127)");
    }
    
    for (auto& note : m_notes) {
        if (note.selected) {
            note.velocity = velocity;
        }
    }
    
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::scale_selected_velocity(float scale) {
    if (scale <= 0.0f) {
        return Result<bool>::error("Velocity scale must be positive");
    }
    
    for (auto& note : m_notes) {
        if (note.selected) {
            float new_velocity = note.velocity * scale;
            if (new_velocity < 1.0f) new_velocity = 1.0f;
            if (new_velocity > 127.0f) new_velocity = 127.0f;
            note.velocity = static_cast<uint8_t>(new_velocity);
        }
    }
    
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::quantize_selected_notes(QuantizeResolution resolution, float strength) {
    if (strength < 0.0f || strength > 1.0f) {
        return Result<bool>::error("Quantize strength must be between 0.0 and 1.0");
    }
    
    for (auto& note : m_notes) {
        if (note.selected) {
            uint64_t quantized_time = quantize_time(note.start_time, resolution);
            
            // Apply quantization strength (linear interpolation)
            int64_t time_diff = static_cast<int64_t>(quantized_time) - static_cast<int64_t>(note.start_time);
            int64_t adjusted_diff = static_cast<int64_t>(time_diff * strength);
            note.start_time = static_cast<uint64_t>(static_cast<int64_t>(note.start_time) + adjusted_diff);
        }
    }
    
    sort_notes_by_time();
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::quantize_all_notes(QuantizeResolution resolution, float strength) {
    select_all_notes();
    auto result = quantize_selected_notes(resolution, strength);
    deselect_all_notes();
    return result;
}

Result<bool> MIDIClip::add_cc_event(const MIDIControlChange& cc) {
    m_cc_events.push_back(cc);
    sort_cc_by_time();
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::remove_cc_event(size_t cc_index) {
    if (cc_index >= m_cc_events.size()) {
        return Result<bool>::error("CC event index out of range");
    }
    
    m_cc_events.erase(m_cc_events.begin() + cc_index);
    return Result<bool>::success(true);
}

std::vector<MIDIControlChange*> MIDIClip::get_cc_events_for_controller(uint8_t controller) {
    std::vector<MIDIControlChange*> result;
    
    for (auto& cc : m_cc_events) {
        if (cc.controller == controller) {
            result.push_back(&cc);
        }
    }
    
    return result;
}

MIDIEventBuffer MIDIClip::generate_midi_events(uint64_t start_time, uint64_t end_time) const {
    MIDIEventBuffer events;
    
    // Generate note events
    for (const auto& note : m_notes) {
        if (!note.muted && note.overlaps(start_time, end_time)) {
            auto [note_on, note_off] = note.to_midi_events();
            
            // Only include events within the requested time range
            if (note_on.timestamp >= start_time && note_on.timestamp < end_time) {
                events.push_back(note_on);
            }
            if (note_off.timestamp >= start_time && note_off.timestamp < end_time) {
                events.push_back(note_off);
            }
        }
    }
    
    // Generate CC events
    for (const auto& cc : m_cc_events) {
        if (cc.time >= start_time && cc.time < end_time) {
            events.push_back(cc.to_midi_event());
        }
    }
    
    // Sort events by timestamp
    sort_midi_events(events);
    
    return events;
}

MIDIEventBuffer MIDIClip::generate_all_midi_events() const {
    return generate_midi_events(0, m_length);
}

Result<bool> MIDIClip::transpose_selected_notes(int8_t semitones) {
    for (auto& note : m_notes) {
        if (note.selected) {
            int new_note_number = static_cast<int>(note.note_number) + semitones;
            if (new_note_number < 0) new_note_number = 0;
            if (new_note_number > 127) new_note_number = 127;
            note.note_number = static_cast<uint8_t>(new_note_number);
        }
    }
    
    return Result<bool>::success(true);
}

Result<bool> MIDIClip::humanize_selected_notes(float timing_variance, float velocity_variance) {
    if (timing_variance < 0.0f || velocity_variance < 0.0f) {
        return Result<bool>::error("Variance values must be non-negative");
    }
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    for (auto& note : m_notes) {
        if (note.selected) {
            // Apply timing humanization
            if (timing_variance > 0.0f) {
                std::uniform_real_distribution<float> timing_dist(-timing_variance, timing_variance);
                float timing_offset = timing_dist(gen);
                int64_t sample_offset = static_cast<int64_t>(timing_offset * 44.1f); // Convert ms to samples
                
                int64_t new_start_time = static_cast<int64_t>(note.start_time) + sample_offset;
                if (new_start_time < 0) new_start_time = 0;
                note.start_time = static_cast<uint64_t>(new_start_time);
            }
            
            // Apply velocity humanization
            if (velocity_variance > 0.0f) {
                std::uniform_real_distribution<float> velocity_dist(-velocity_variance, velocity_variance);
                float velocity_offset = velocity_dist(gen);
                
                int new_velocity = static_cast<int>(note.velocity) + static_cast<int>(velocity_offset);
                if (new_velocity < 1) new_velocity = 1;
                if (new_velocity > 127) new_velocity = 127;
                note.velocity = static_cast<uint8_t>(new_velocity);
            }
        }
    }
    
    sort_notes_by_time();
    return Result<bool>::success(true);
}

// Time conversion helpers
uint64_t MIDIClip::beats_to_samples(double beats, double bpm, double sample_rate) {
    double seconds_per_beat = 60.0 / bpm;
    double seconds = beats * seconds_per_beat;
    return static_cast<uint64_t>(seconds * sample_rate);
}

double MIDIClip::samples_to_beats(uint64_t samples, double bpm, double sample_rate) {
    double seconds = samples / sample_rate;
    double seconds_per_beat = 60.0 / bpm;
    return seconds / seconds_per_beat;
}

uint64_t MIDIClip::bars_to_samples(double bars, double bpm, int time_sig_num, double sample_rate) {
    double beats_per_bar = static_cast<double>(time_sig_num);
    double total_beats = bars * beats_per_bar;
    return beats_to_samples(total_beats, bpm, sample_rate);
}

// Private helper functions
uint64_t MIDIClip::quantize_time(uint64_t time, QuantizeResolution resolution, double bpm, double sample_rate) {
    double resolution_value = static_cast<double>(static_cast<int>(resolution));
    if (resolution_value < 0) {
        // Triplet resolution
        resolution_value = -resolution_value * 2.0 / 3.0;
    }
    
    uint64_t samples_per_unit = beats_to_samples(4.0 / resolution_value, bpm, sample_rate);
    
    // Round to nearest quantization unit
    uint64_t quantized = ((time + samples_per_unit / 2) / samples_per_unit) * samples_per_unit;
    
    return quantized;
}

bool MIDIClip::validate_note(const MIDINote& note) const {
    if (note.note_number > 127) return false;
    if (note.velocity == 0 || note.velocity > 127) return false;
    if (note.length == 0) return false;
    if (note.channel > 15) return false;
    
    return true;
}

void MIDIClip::sort_notes_by_time() {
    std::sort(m_notes.begin(), m_notes.end(),
        [](const MIDINote& a, const MIDINote& b) {
            if (a.start_time == b.start_time) {
                return a.note_number < b.note_number;
            }
            return a.start_time < b.start_time;
        });
}

void MIDIClip::sort_cc_by_time() {
    std::sort(m_cc_events.begin(), m_cc_events.end(),
        [](const MIDIControlChange& a, const MIDIControlChange& b) {
            return a.time < b.time;
        });
}

// MIDIClipFactory implementations
std::shared_ptr<MIDIClip> MIDIClipFactory::create_empty_clip(const std::string& name, double length_bars, double bpm) {
    auto clip = std::make_shared<MIDIClip>(name);
    clip->set_length(MIDIClip::bars_to_samples(length_bars, bpm, 4, 44100.0));
    return clip;
}

std::shared_ptr<MIDIClip> MIDIClipFactory::create_chord_progression(
    const std::string& name,
    const std::vector<std::vector<uint8_t>>& chords,
    double chord_length_beats,
    uint8_t velocity,
    double bpm)
{
    auto clip = std::make_shared<MIDIClip>(name);
    
    uint64_t chord_length_samples = MIDIClip::beats_to_samples(chord_length_beats, bpm, 44100.0);
    uint64_t current_time = 0;
    
    for (const auto& chord : chords) {
        for (uint8_t note_number : chord) {
            MIDINote note(note_number, velocity, current_time, chord_length_samples);
            clip->add_note(note);
        }
        current_time += chord_length_samples;
    }
    
    clip->set_length(current_time);
    return clip;
}

std::shared_ptr<MIDIClip> MIDIClipFactory::create_drum_pattern(
    const std::string& name,
    const std::map<uint8_t, std::vector<double>>& drum_hits,
    double pattern_length_bars,
    uint8_t velocity,
    double bpm)
{
    auto clip = std::make_shared<MIDIClip>(name);
    
    uint64_t pattern_length_samples = MIDIClip::bars_to_samples(pattern_length_bars, bpm, 4, 44100.0);
    uint64_t note_length = MIDIClip::beats_to_samples(0.1, bpm, 44100.0); // 0.1 beat note length
    
    for (const auto& [note_number, beat_positions] : drum_hits) {
        for (double beat_pos : beat_positions) {
            uint64_t time = MIDIClip::beats_to_samples(beat_pos, bpm, 44100.0);
            if (time < pattern_length_samples) {
                MIDINote note(note_number, velocity, time, note_length);
                clip->add_note(note);
            }
        }
    }
    
    clip->set_length(pattern_length_samples);
    return clip;
}

std::shared_ptr<MIDIClip> MIDIClipFactory::create_scale_pattern(
    const std::string& name,
    uint8_t root_note,
    const std::vector<int>& scale_intervals,
    double note_length_beats,
    uint8_t velocity,
    double bpm)
{
    auto clip = std::make_shared<MIDIClip>(name);
    
    uint64_t note_length_samples = MIDIClip::beats_to_samples(note_length_beats, bpm, 44100.0);
    uint64_t current_time = 0;
    
    for (int interval : scale_intervals) {
        uint8_t note_number = static_cast<uint8_t>(std::clamp(static_cast<int>(root_note) + interval, 0, 127));
        MIDINote note(note_number, velocity, current_time, note_length_samples);
        clip->add_note(note);
        current_time += note_length_samples;
    }
    
    clip->set_length(current_time);
    return clip;
}

} // namespace mixmind