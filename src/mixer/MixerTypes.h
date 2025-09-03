#pragma once

#include "../core/result.h"
#include "../core/audio_types.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <atomic>

namespace mixmind {

// Forward declarations
class AudioBuffer;
class VSTiHost;
class AudioEffect;

// Audio routing destination
struct RouteDestination {
    enum DestinationType {
        TRACK,          // Route to another track
        BUS,            // Route to a bus
        MASTER_OUT,     // Route to master output
        EXTERNAL_OUT    // Route to external hardware output
    };
    
    DestinationType type;
    uint32_t destination_id;
    double send_level = 1.0;        // Linear gain (0.0 = -inf dB, 1.0 = 0 dB)
    double send_pan = 0.0;          // Pan position (-1.0 = full left, 1.0 = full right)
    bool pre_fader = false;         // Send before or after track fader
    bool enabled = true;
    
    RouteDestination(DestinationType t = MASTER_OUT, uint32_t id = 0)
        : type(t), destination_id(id) {}
};

// Audio bus configuration
struct BusConfig {
    enum BusType {
        AUX_SEND,       // Auxiliary send bus (reverb, delay sends)
        GROUP_BUS,      // Track grouping bus (drum bus, vocal bus)
        MASTER_BUS,     // Master output bus
        MONITOR_BUS     // Monitor/cue bus
    };
    
    BusType type = AUX_SEND;
    std::string name = "Bus";
    uint32_t channel_count = 2;     // Stereo by default
    double volume_db = 0.0;
    double pan_position = 0.0;
    bool mute = false;
    bool solo = false;
    
    // Bus routing
    std::vector<RouteDestination> outputs;
    
    BusConfig(BusType t = AUX_SEND, const std::string& n = "Bus", uint32_t channels = 2)
        : type(t), name(n), channel_count(channels) {
        // Default routing to master
        outputs.emplace_back(RouteDestination::MASTER_OUT, 0);
    }
};

// Plugin Delay Compensation information
struct DelayCompensation {
    uint32_t samples_delay = 0;     // Delay in samples
    double ms_delay = 0.0;          // Delay in milliseconds
    bool auto_compensation = true;   // Enable automatic PDC
    
    DelayCompensation(uint32_t samples = 0) : samples_delay(samples) {
        // Convert samples to ms at 44.1kHz
        ms_delay = (double)samples / 44100.0 * 1000.0;
    }
};

// Track channel configuration
struct ChannelConfig {
    uint32_t input_channels = 2;    // Number of input channels
    uint32_t output_channels = 2;   // Number of output channels
    bool stereo_linked = true;      // Link L/R processing
    
    ChannelConfig(uint32_t in = 2, uint32_t out = 2, bool linked = true)
        : input_channels(in), output_channels(out), stereo_linked(linked) {}
};

// Audio metering data
struct MeterData {
    // Peak metering
    std::vector<double> peak_levels;        // Peak level per channel (linear)
    std::vector<double> peak_levels_db;     // Peak level per channel (dB)
    std::vector<bool> clip_indicators;      // Clipping detection per channel
    
    // RMS metering
    std::vector<double> rms_levels;         // RMS level per channel (linear)
    std::vector<double> rms_levels_db;      // RMS level per channel (dB)
    
    // LUFS metering (for professional loudness monitoring)
    double momentary_lufs = -70.0;          // Momentary loudness (400ms)
    double short_term_lufs = -70.0;         // Short-term loudness (3s)
    double integrated_lufs = -70.0;         // Integrated loudness (entire program)
    double loudness_range = 0.0;            // LRA - dynamic range
    double true_peak_dbfs = -70.0;          // True peak level
    
    // Correlation metering (for stereo)
    double phase_correlation = 0.0;         // -1.0 to 1.0
    
    void reset() {
        std::fill(peak_levels.begin(), peak_levels.end(), 0.0);
        std::fill(peak_levels_db.begin(), peak_levels_db.end(), -70.0);
        std::fill(clip_indicators.begin(), clip_indicators.end(), false);
        std::fill(rms_levels.begin(), rms_levels.end(), 0.0);
        std::fill(rms_levels_db.begin(), rms_levels_db.end(), -70.0);
        momentary_lufs = short_term_lufs = integrated_lufs = true_peak_dbfs = -70.0;
        loudness_range = phase_correlation = 0.0;
    }
};

// Mixer channel strip configuration
struct ChannelStripConfig {
    // Basic controls
    double volume_db = 0.0;             // Fader level in dB
    double pan_position = 0.0;          // Pan position (-1.0 to 1.0)
    bool mute = false;
    bool solo = false;
    bool record_arm = false;
    bool monitor = false;
    
    // Input settings
    double input_gain_db = 0.0;         // Input gain/trim
    bool phase_invert = false;          // Phase inversion
    bool high_pass_filter = false;      // High-pass filter enable
    double hpf_frequency = 80.0;        // HPF frequency in Hz
    
    // EQ settings (basic 4-band)
    struct EQBand {
        bool enabled = false;
        double frequency = 1000.0;      // Center frequency in Hz
        double gain_db = 0.0;           // Gain in dB
        double q_factor = 1.0;          // Q factor/bandwidth
        
        EQBand(double freq = 1000.0) : frequency(freq) {}
    };
    
    EQBand eq_high_shelf{10000.0};      // High shelf
    EQBand eq_high_mid{3000.0};         // High mid
    EQBand eq_low_mid{300.0};           // Low mid
    EQBand eq_low_shelf{80.0};          // Low shelf
    
    // Dynamics
    struct CompressorConfig {
        bool enabled = false;
        double threshold_db = -20.0;
        double ratio = 4.0;
        double attack_ms = 10.0;
        double release_ms = 100.0;
        double knee_db = 2.0;
        double makeup_gain_db = 0.0;
    } compressor;
    
    struct GateConfig {
        bool enabled = false;
        double threshold_db = -40.0;
        double ratio = 10.0;
        double attack_ms = 1.0;
        double hold_ms = 10.0;
        double release_ms = 100.0;
    } gate;
    
    // Send levels
    std::map<uint32_t, RouteDestination> sends;  // Bus ID -> Send config
    
    // Plugin Delay Compensation
    DelayCompensation delay_compensation;
};

// Mixer bus strip (simplified version of channel strip)
struct BusStripConfig {
    double volume_db = 0.0;
    double pan_position = 0.0;
    bool mute = false;
    bool solo = false;
    
    // Basic EQ (simpler than channel strip)
    bool eq_enabled = false;
    double eq_low_gain_db = 0.0;        // Low shelf gain
    double eq_mid_gain_db = 0.0;        // Mid gain
    double eq_high_gain_db = 0.0;       // High shelf gain
    
    // Dynamics
    ChannelStripConfig::CompressorConfig compressor;
    
    // Routing
    std::vector<RouteDestination> outputs;
    
    // Plugin Delay Compensation
    DelayCompensation delay_compensation;
};

// Master section configuration
struct MasterSectionConfig {
    // Master fader
    double master_volume_db = 0.0;
    bool master_mute = false;
    double master_pan = 0.0;            // Master balance control
    
    // Master EQ (high-quality)
    struct MasterEQ {
        bool enabled = false;
        double low_shelf_freq = 80.0;
        double low_shelf_gain_db = 0.0;
        double low_mid_freq = 200.0;
        double low_mid_gain_db = 0.0;
        double low_mid_q = 1.0;
        double high_mid_freq = 3000.0;
        double high_mid_gain_db = 0.0;
        double high_mid_q = 1.0;
        double high_shelf_freq = 10000.0;
        double high_shelf_gain_db = 0.0;
    } master_eq;
    
    // Master compressor/limiter
    struct MasterDynamics {
        // Compressor
        bool compressor_enabled = false;
        double comp_threshold_db = -10.0;
        double comp_ratio = 3.0;
        double comp_attack_ms = 5.0;
        double comp_release_ms = 50.0;
        double comp_knee_db = 2.0;
        double comp_makeup_db = 0.0;
        
        // Limiter
        bool limiter_enabled = false;
        double limiter_threshold_db = -1.0;
        double limiter_release_ms = 10.0;
        double limiter_lookahead_ms = 5.0;
    } master_dynamics;
    
    // Master metering
    bool lufs_metering_enabled = true;
    double target_lufs = -23.0;         // Target loudness for broadcasting
    bool true_peak_limiting = true;
    double max_true_peak_dbfs = -1.0;
    
    // Monitoring
    double monitor_level_db = 0.0;      // Monitor speaker level
    double headphone_level_db = 0.0;    // Headphone level
    bool mono_monitoring = false;       // Sum to mono
    bool phase_invert_monitoring = false;
    
    // Talkback/listenback
    bool talkback_enabled = false;
    double talkback_level_db = -10.0;
    uint32_t talkback_destination = 0;  // Which bus to send talkback to
};

// Mixer session state
struct MixerSessionState {
    uint32_t session_sample_rate = 44100;
    uint32_t session_buffer_size = 512;
    uint32_t session_bit_depth = 24;
    
    // Global settings
    bool auto_pdc_enabled = true;
    double global_pdc_offset_samples = 0.0;
    bool solo_in_place = true;          // Solo-in-place vs solo-in-front
    bool solo_exclusive = false;        // Only one track can be soloed
    
    // Metering settings
    double meter_ballistics_attack_ms = 0.0;    // Peak meter attack
    double meter_ballistics_release_ms = 300.0; // Peak meter release
    bool peak_hold_enabled = true;
    double peak_hold_time_ms = 1500.0;
    
    // LUFS settings
    bool lufs_metering_enabled = true;
    double lufs_target_level = -23.0;   // EBU R128 standard
    bool lufs_gating_enabled = true;    // Enable gating for integrated measurement
};

} // namespace mixmind