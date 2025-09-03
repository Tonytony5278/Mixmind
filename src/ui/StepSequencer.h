#pragma once

#include "../midi/MIDIClip.h"
#include "PianoRollEditor.h"
#include "../core/result.h"
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace mixmind {

// Step sequencer pattern length options
enum class PatternLength {
    BARS_1 = 1,   // 1 bar (16 steps at 16th note resolution)
    BARS_2 = 2,   // 2 bars (32 steps)
    BARS_4 = 4,   // 4 bars (64 steps)
    BARS_8 = 8    // 8 bars (128 steps)
};

// Step resolution options
enum class StepResolution {
    QUARTER = 4,        // 1/4 notes (4 steps per bar)
    EIGHTH = 8,         // 1/8 notes (8 steps per bar)
    SIXTEENTH = 16,     // 1/16 notes (16 steps per bar)
    THIRTY_SECOND = 32, // 1/32 notes (32 steps per bar)
    TRIPLET_EIGHTH = -8, // 1/8 triplets (12 steps per bar)
    TRIPLET_SIXTEENTH = -16 // 1/16 triplets (24 steps per bar)
};

// Individual step data
struct Step {
    bool active = false;              // Step is enabled
    uint8_t velocity = 100;           // Step velocity (1-127)
    double length_multiplier = 1.0;   // Length as multiplier of step size
    bool selected = false;            // Selected for editing
    uint8_t probability = 100;        // Probability (0-100%)
    int8_t micro_timing = 0;          // Micro timing offset (-50 to +50)
    bool muted = false;               // Step is muted
    
    Step() = default;
    Step(bool act, uint8_t vel = 100) : active(act), velocity(vel) {}
};

// Drum lane configuration for step sequencer
struct DrumLaneConfig {
    uint8_t note_number = 36;         // MIDI note (typically 35-81 for drums)
    std::string name = "Kick";        // Lane display name  
    uint8_t default_velocity = 100;   // Default velocity for new steps
    uint32_t color = 0xFFFF4040;      // Lane color (ARGB)
    bool muted = false;               // Lane is muted
    bool solo = false;                // Lane is soloed
    bool visible = true;              // Lane is visible
    
    // Drum-specific properties
    bool choke_group = false;         // Part of a choke group
    uint8_t choke_id = 0;             // Choke group ID
    double pan = 0.0;                 // Pan (-1.0 to 1.0)
    float volume = 1.0f;              // Volume (0.0 to 2.0)
};

// Step Sequencer - grid-based pattern editor optimized for drums
class StepSequencer {
public:
    StepSequencer(std::shared_ptr<MIDIClip> clip = nullptr);
    ~StepSequencer() = default;
    
    // Clip management
    void set_clip(std::shared_ptr<MIDIClip> clip);
    std::shared_ptr<MIDIClip> get_clip() const { return m_clip; }
    
    // Pattern configuration
    void set_pattern_length(PatternLength length);
    PatternLength get_pattern_length() const { return m_pattern_length; }
    
    void set_step_resolution(StepResolution resolution);
    StepResolution get_step_resolution() const { return m_step_resolution; }
    
    size_t get_total_steps() const;
    size_t get_steps_per_bar() const;
    
    double get_step_length_beats() const;
    double get_pattern_length_beats() const;
    
    // Lane management
    Result<bool> add_drum_lane(uint8_t note_number, const std::string& name = "");
    Result<bool> remove_drum_lane(uint8_t note_number);
    std::vector<uint8_t> get_drum_notes() const;
    
    void set_lane_config(uint8_t note_number, const DrumLaneConfig& config);
    DrumLaneConfig get_lane_config(uint8_t note_number) const;
    
    // Step editing
    Result<bool> toggle_step(uint8_t note_number, size_t step_index);
    Result<bool> set_step(uint8_t note_number, size_t step_index, bool active);
    Result<bool> set_step_velocity(uint8_t note_number, size_t step_index, uint8_t velocity);
    Result<bool> set_step_probability(uint8_t note_number, size_t step_index, uint8_t probability);
    Result<bool> set_step_micro_timing(uint8_t note_number, size_t step_index, int8_t micro_timing);
    Result<bool> set_step_length(uint8_t note_number, size_t step_index, double length_multiplier);
    
    // Step access
    Step get_step(uint8_t note_number, size_t step_index) const;
    bool is_step_active(uint8_t note_number, size_t step_index) const;
    
    // Pattern operations
    Result<bool> clear_pattern();
    Result<bool> clear_lane(uint8_t note_number);
    Result<bool> copy_lane(uint8_t source_note, uint8_t dest_note);
    Result<bool> shift_lane(uint8_t note_number, int step_offset);
    Result<bool> reverse_lane(uint8_t note_number);
    Result<bool> randomize_lane(uint8_t note_number, float probability = 0.5f);
    
    // Selection and editing
    void select_step(uint8_t note_number, size_t step_index, bool add_to_selection = false);
    void select_lane(uint8_t note_number, bool add_to_selection = false);
    void select_step_column(size_t step_index, bool add_to_selection = false);
    void select_all_steps();
    void deselect_all_steps();
    
    std::vector<std::pair<uint8_t, size_t>> get_selected_steps() const;
    
    // Selected step operations
    Result<bool> set_selected_velocity(uint8_t velocity);
    Result<bool> adjust_selected_velocity(int delta);
    Result<bool> set_selected_probability(uint8_t probability);
    Result<bool> randomize_selected_velocity(uint8_t min_vel = 80, uint8_t max_vel = 127);
    
    // Playback and timing
    void set_swing(float swing_amount); // 0.0 = no swing, 1.0 = maximum swing
    float get_swing() const { return m_swing; }
    
    void set_shuffle(float shuffle_amount); // Micro-timing variation
    float get_shuffle() const { return m_shuffle; }
    
    void set_humanize_velocity(float amount); // Random velocity variation
    float get_humanize_velocity() const { return m_humanize_velocity; }
    
    void set_humanize_timing(float amount); // Random timing variation
    float get_humanize_timing() const { return m_humanize_timing; }
    
    // MIDI generation
    Result<bool> generate_midi_from_pattern();
    Result<bool> update_midi_clip();
    
    // Pattern presets
    Result<bool> load_drum_kit_preset(const std::string& kit_name);
    Result<bool> load_pattern_preset(const std::string& pattern_name);
    
    // Common drum patterns
    void create_basic_kick_pattern();
    void create_basic_snare_pattern();
    void create_basic_hihat_pattern();
    void create_four_on_floor_pattern();
    void create_breakbeat_pattern();
    
    // Step input mode integration
    bool is_step_input_active() const { return m_step_input_active; }
    void set_step_input_active(bool active) { m_step_input_active = active; }
    
    size_t get_current_step() const { return m_current_step; }
    void set_current_step(size_t step) { m_current_step = std::min(step, get_total_steps() - 1); }
    void advance_step() { m_current_step = (m_current_step + 1) % get_total_steps(); }
    
    // Real-time input (for step input mode)
    Result<bool> input_note_at_current_step(uint8_t note_number, uint8_t velocity = 100);
    
    // Event callbacks
    using StepEditCallback = std::function<void()>;
    void set_edit_callback(StepEditCallback callback) { m_edit_callback = callback; }
    
private:
    std::shared_ptr<MIDIClip> m_clip;
    PatternLength m_pattern_length = PatternLength::BARS_1;
    StepResolution m_step_resolution = StepResolution::SIXTEENTH;
    
    // Pattern data: note_number -> step_index -> Step
    std::map<uint8_t, std::vector<Step>> m_pattern;
    std::map<uint8_t, DrumLaneConfig> m_lane_configs;
    
    // Playback settings
    float m_swing = 0.0f;
    float m_shuffle = 0.0f;
    float m_humanize_velocity = 0.0f;
    float m_humanize_timing = 0.0f;
    double m_bpm = 120.0;
    
    // Step input
    bool m_step_input_active = false;
    size_t m_current_step = 0;
    
    StepEditCallback m_edit_callback;
    
    // Helper methods
    void initialize_pattern();
    void resize_pattern();
    void notify_edit_changed();
    uint64_t step_to_samples(size_t step_index) const;
    double step_to_beats(size_t step_index) const;
    void apply_swing_to_step(size_t step_index, uint64_t& time_samples) const;
    DrumLaneConfig create_default_lane_config(uint8_t note_number, const std::string& name) const;
};

// Step Sequencer Factory for common setups
class StepSequencerFactory {
public:
    // Create step sequencer with standard drum kit layout
    static std::unique_ptr<StepSequencer> create_drum_sequencer(std::shared_ptr<MIDIClip> clip = nullptr);
    
    // Create step sequencer with 808-style drum kit
    static std::unique_ptr<StepSequencer> create_808_sequencer(std::shared_ptr<MIDIClip> clip = nullptr);
    
    // Create step sequencer with minimal techno setup
    static std::unique_ptr<StepSequencer> create_techno_sequencer(std::shared_ptr<MIDIClip> clip = nullptr);
    
    // Create step sequencer for trap/hip-hop
    static std::unique_ptr<StepSequencer> create_trap_sequencer(std::shared_ptr<MIDIClip> clip = nullptr);
};

// Drum kit presets
class DrumKitPresets {
public:
    struct DrumMapping {
        uint8_t note;
        std::string name;
        uint32_t color;
        uint8_t default_velocity;
    };
    
    // Standard GM drum mappings
    static std::vector<DrumMapping> get_standard_kit();
    static std::vector<DrumMapping> get_808_kit();
    static std::vector<DrumMapping> get_techno_kit();
    static std::vector<DrumMapping> get_trap_kit();
    
    // Common note mappings
    static constexpr uint8_t KICK_1 = 36;        // C2
    static constexpr uint8_t KICK_2 = 35;        // B1
    static constexpr uint8_t SNARE_1 = 38;       // D2
    static constexpr uint8_t SNARE_2 = 40;       // E2
    static constexpr uint8_t CLAP = 39;          // D#2
    static constexpr uint8_t HIHAT_CLOSED = 42;  // F#2
    static constexpr uint8_t HIHAT_OPEN = 46;    // A#2
    static constexpr uint8_t HIHAT_PEDAL = 44;   // G#2
    static constexpr uint8_t CRASH_1 = 49;       // C#3
    static constexpr uint8_t CRASH_2 = 57;       // A3
    static constexpr uint8_t RIDE = 51;          // D#3
    static constexpr uint8_t TOM_LOW = 41;       // F2
    static constexpr uint8_t TOM_MID = 45;       // A2
    static constexpr uint8_t TOM_HIGH = 48;      // C3
};

} // namespace mixmind