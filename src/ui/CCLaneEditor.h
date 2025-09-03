#pragma once

#include "../midi/MIDIClip.h"
#include "../core/result.h"
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace mixmind {

// Common MIDI CC controllers with descriptive names
enum class StandardCC : uint8_t {
    MOD_WHEEL = 1,        // Modulation wheel
    BREATH = 2,           // Breath controller
    FOOT = 4,             // Foot controller
    PORTAMENTO = 5,       // Portamento time
    DATA_ENTRY_MSB = 6,   // Data entry MSB
    VOLUME = 7,           // Channel volume
    BALANCE = 8,          // Balance
    PAN = 10,             // Pan
    EXPRESSION = 11,      // Expression controller
    EFFECT1 = 12,         // Effect control 1
    EFFECT2 = 13,         // Effect control 2
    SUSTAIN = 64,         // Sustain pedal
    PORTAMENTO_ON_OFF = 65, // Portamento on/off
    SOSTENUTO = 66,       // Sostenuto pedal
    SOFT_PEDAL = 67,      // Soft pedal
    LEGATO = 68,          // Legato footswitch
    HOLD2 = 69,           // Hold 2
    SOUND_VARIATION = 70, // Sound variation
    HARMONIC_CONTENT = 71, // Harmonic content (filter resonance)
    RELEASE_TIME = 72,    // Release time
    ATTACK_TIME = 73,     // Attack time
    BRIGHTNESS = 74,      // Brightness (filter cutoff)
    DECAY_TIME = 75,      // Decay time
    VIBRATO_RATE = 76,    // Vibrato rate
    VIBRATO_DEPTH = 77,   // Vibrato depth
    VIBRATO_DELAY = 78,   // Vibrato delay
    REVERB_SEND = 91,     // Reverb send
    CHORUS_SEND = 93      // Chorus send
};

// CC lane configuration
struct CCLaneConfig {
    uint8_t controller = 1;              // CC number (0-127)
    std::string name = "Mod Wheel";      // Display name
    uint8_t default_value = 0;           // Default CC value
    uint8_t min_value = 0;               // Minimum CC value
    uint8_t max_value = 127;             // Maximum CC value
    bool is_toggle = false;              // Toggle mode (0/127 only)
    bool visible = true;                 // Lane visibility
    float lane_height = 64.0f;           // Lane height in pixels
    uint32_t color = 0xFF4080FF;         // Lane color (ARGB)
};

// CC automation curve types
enum class CurveType {
    LINEAR,      // Linear interpolation
    SMOOTH,      // Smooth curve (bezier-like)
    STEPPED,     // Stepped (no interpolation)
    EXPONENTIAL, // Exponential curve
    LOGARITHMIC  // Logarithmic curve
};

// CC automation point with curve information
struct CCAutomationPoint {
    uint64_t time;                    // Time in samples
    uint8_t value;                    // CC value (0-127)
    CurveType curve_type = CurveType::LINEAR; // Curve to next point
    bool selected = false;            // Selected for editing
    
    CCAutomationPoint() = default;
    CCAutomationPoint(uint64_t t, uint8_t v, CurveType curve = CurveType::LINEAR)
        : time(t), value(v), curve_type(curve) {}
};

// CC Lane Editor - manages CC automation for a single controller
class CCLaneEditor {
public:
    CCLaneEditor(std::shared_ptr<MIDIClip> clip, const CCLaneConfig& config);
    ~CCLaneEditor() = default;
    
    // Configuration
    void set_config(const CCLaneConfig& config) { m_config = config; }
    const CCLaneConfig& get_config() const { return m_config; }
    
    uint8_t get_controller() const { return m_config.controller; }
    void set_controller(uint8_t controller);
    
    // Clip management
    void set_clip(std::shared_ptr<MIDIClip> clip) { m_clip = clip; }
    std::shared_ptr<MIDIClip> get_clip() const { return m_clip; }
    
    // Drawing CC events
    Result<bool> draw_cc_point(double time_beats, uint8_t value, CurveType curve_type = CurveType::LINEAR);
    Result<bool> draw_cc_line(double start_time_beats, double end_time_beats, uint8_t start_value, uint8_t end_value, CurveType curve_type = CurveType::LINEAR);
    Result<bool> draw_cc_ramp(double start_time_beats, double end_time_beats, uint8_t start_value, uint8_t end_value);
    
    // Erasing CC events
    Result<bool> erase_cc_at_time(double time_beats, double tolerance_beats = 0.1);
    Result<bool> erase_cc_in_range(double start_time_beats, double end_time_beats);
    Result<bool> clear_all_cc();
    
    // Selection
    Result<bool> select_cc_at_time(double time_beats, bool add_to_selection = false);
    Result<bool> select_cc_in_range(double start_time_beats, double end_time_beats, bool add_to_selection = false);
    void select_all_cc();
    void deselect_all_cc();
    
    // Editing selected CC events
    Result<bool> move_selected_cc(double time_delta_beats, int value_delta);
    Result<bool> scale_selected_cc_values(float scale_factor, uint8_t pivot_value = 64);
    Result<bool> set_selected_cc_curve_type(CurveType curve_type);
    
    // CC event access
    std::vector<MIDIControlChange*> get_cc_events();
    std::vector<MIDIControlChange*> get_cc_events_in_range(double start_time_beats, double end_time_beats);
    std::vector<MIDIControlChange*> get_selected_cc_events();
    
    // Value interpolation for smooth playback
    uint8_t get_cc_value_at_time(double time_beats) const;
    std::vector<MIDIControlChange> generate_interpolated_cc_events(double start_time_beats, double end_time_beats, double resolution_beats = 0.01) const;
    
    // Automation shapes and patterns
    Result<bool> create_automation_shape(double start_time_beats, double end_time_beats, const std::string& shape_name);
    Result<bool> create_lfo_automation(double start_time_beats, double end_time_beats, double frequency_hz, uint8_t depth, uint8_t offset = 64);
    
    // Clipboard operations
    Result<bool> copy_selected_cc();
    Result<bool> cut_selected_cc();
    Result<bool> paste_cc_at_time(double time_beats);
    
    // Quantization
    Result<bool> quantize_selected_cc_timing(double grid_beats);
    Result<bool> quantize_selected_cc_values(uint8_t step_size);
    
    // Utility functions
    size_t get_cc_event_count() const;
    void thin_automation_data(double tolerance = 1.0); // Remove redundant points
    
    // Time conversion helpers
    uint64_t beats_to_samples(double beats) const;
    double samples_to_beats(uint64_t samples) const;
    
    // Event callback for UI updates
    using CCEditCallback = std::function<void()>;
    void set_edit_callback(CCEditCallback callback) { m_edit_callback = callback; }

private:
    std::shared_ptr<MIDIClip> m_clip;
    CCLaneConfig m_config;
    double m_bpm = 120.0;
    
    // Clipboard for copy/paste
    std::vector<MIDIControlChange> m_cc_clipboard;
    
    CCEditCallback m_edit_callback;
    
    // Helper methods
    void notify_edit_changed();
    uint8_t interpolate_cc_value(const MIDIControlChange& start, const MIDIControlChange& end, uint64_t time, CurveType curve_type) const;
    std::vector<uint8_t> generate_shape_values(const std::string& shape_name, size_t point_count, uint8_t min_val, uint8_t max_val) const;
};

// CC Lane Manager - manages multiple CC lanes for a clip
class CCLaneManager {
public:
    CCLaneManager(std::shared_ptr<MIDIClip> clip = nullptr);
    ~CCLaneManager() = default;
    
    // Clip management
    void set_clip(std::shared_ptr<MIDIClip> clip);
    std::shared_ptr<MIDIClip> get_clip() const { return m_clip; }
    
    // Lane management
    Result<std::shared_ptr<CCLaneEditor>> add_cc_lane(uint8_t controller, const std::string& name = "");
    Result<bool> remove_cc_lane(uint8_t controller);
    std::shared_ptr<CCLaneEditor> get_cc_lane(uint8_t controller);
    std::vector<std::shared_ptr<CCLaneEditor>> get_all_lanes() const;
    
    // Presets for common CC lane setups
    void setup_standard_lanes();      // Mod wheel, expression, pan, volume
    void setup_filter_lanes();        // Cutoff, resonance, filter type
    void setup_synth_lanes();         // Attack, decay, sustain, release
    void setup_performance_lanes();   // Mod wheel, aftertouch, breath, expression
    
    // Lane visibility and ordering
    void set_lane_visible(uint8_t controller, bool visible);
    bool is_lane_visible(uint8_t controller) const;
    void reorder_lanes(const std::vector<uint8_t>& controller_order);
    
    // Global operations
    void clear_all_lanes();
    void quantize_all_lanes(double grid_beats);
    void thin_all_automation_data(double tolerance = 1.0);
    
    // CC lane configurations
    static CCLaneConfig create_standard_cc_config(StandardCC cc);
    static CCLaneConfig create_custom_cc_config(uint8_t controller, const std::string& name);
    
    // Event callback for UI updates
    using ManagerEditCallback = std::function<void()>;
    void set_edit_callback(ManagerEditCallback callback);

private:
    std::shared_ptr<MIDIClip> m_clip;
    std::map<uint8_t, std::shared_ptr<CCLaneEditor>> m_lanes;
    std::vector<uint8_t> m_lane_order;
    
    ManagerEditCallback m_edit_callback;
    
    void notify_edit_changed();
};

// Factory for creating common CC automation patterns
class CCAutomationFactory {
public:
    // Create common automation shapes
    static std::vector<CCAutomationPoint> create_linear_ramp(double start_time_beats, double end_time_beats, uint8_t start_value, uint8_t end_value, double bpm = 120.0);
    static std::vector<CCAutomationPoint> create_exponential_curve(double start_time_beats, double end_time_beats, uint8_t start_value, uint8_t end_value, double exponent = 2.0, double bpm = 120.0);
    static std::vector<CCAutomationPoint> create_sine_wave(double start_time_beats, double length_beats, double frequency_hz, uint8_t amplitude, uint8_t offset = 64, double bpm = 120.0);
    static std::vector<CCAutomationPoint> create_sawtooth_wave(double start_time_beats, double length_beats, double frequency_hz, uint8_t amplitude, uint8_t offset = 64, double bpm = 120.0);
    static std::vector<CCAutomationPoint> create_square_wave(double start_time_beats, double length_beats, double frequency_hz, uint8_t amplitude, uint8_t offset = 64, double bpm = 120.0);
    
    // Create musical automation patterns
    static std::vector<CCAutomationPoint> create_filter_sweep(double start_time_beats, double length_beats, bool open_to_close = true, double bpm = 120.0);
    static std::vector<CCAutomationPoint> create_volume_fade(double start_time_beats, double length_beats, bool fade_in = true, double bpm = 120.0);
    static std::vector<CCAutomationPoint> create_tremolo_effect(double start_time_beats, double length_beats, double rate_hz, uint8_t depth, double bpm = 120.0);
    static std::vector<CCAutomationPoint> create_vibrato_effect(double start_time_beats, double length_beats, double rate_hz, uint8_t depth, double bpm = 120.0);
};

} // namespace mixmind