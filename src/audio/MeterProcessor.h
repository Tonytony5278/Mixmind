#pragma once

#include "../mixer/MixerTypes.h"
#include "../core/result.h"
#include "../core/audio_types.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <deque>

namespace mixmind {

class AudioBuffer;

// LUFS metering implementation (EBU R128/ITU-R BS.1770-4)
class LUFSMeter {
public:
    LUFSMeter(uint32_t channels, uint32_t sample_rate);
    ~LUFSMeter();
    
    // Process audio for LUFS measurement
    void process_audio(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    
    // Get LUFS measurements
    double get_momentary_lufs() const;      // 400ms sliding window
    double get_short_term_lufs() const;     // 3s sliding window
    double get_integrated_lufs() const;     // Program integrated
    double get_loudness_range() const;      // LRA (10th to 95th percentile)
    double get_true_peak_dbfs() const;      // Maximum true peak
    
    // Control measurement
    void start_measurement();
    void stop_measurement();
    void reset_measurement();
    bool is_measuring() const { return m_is_measuring.load(); }
    
    // Gating settings (EBU R128)
    void set_gating_enabled(bool enabled) { m_gating_enabled = enabled; }
    bool is_gating_enabled() const { return m_gating_enabled; }
    
    void set_absolute_gate_threshold(double threshold_lufs) { m_absolute_gate_threshold = threshold_lufs; }
    double get_absolute_gate_threshold() const { return m_absolute_gate_threshold; }
    
    void set_relative_gate_offset(double offset_lu) { m_relative_gate_offset = offset_lu; }
    double get_relative_gate_offset() const { return m_relative_gate_offset; }
    
    // Statistics
    uint64_t get_samples_processed() const { return m_samples_processed.load(); }
    std::chrono::milliseconds get_measurement_duration() const;
    
private:
    uint32_t m_channels;
    uint32_t m_sample_rate;
    std::atomic<bool> m_is_measuring{false};
    
    // Pre-filtering (K-weighting filter)
    struct KWeightingFilter {
        // High-pass filter coefficients (f_h = 38 Hz)
        double hpf_b0, hpf_b1, hpf_b2;
        double hpf_a1, hpf_a2;
        double hpf_z1, hpf_z2;
        
        // High-frequency shelving filter (f_s = 1500 Hz)
        double hf_b0, hf_b1, hf_b2;
        double hf_a1, hf_a2;
        double hf_z1, hf_z2;
        
        KWeightingFilter();
        void reset();
        double process_sample(double sample);
    };
    
    std::vector<KWeightingFilter> m_k_filters;  // One per channel
    
    // Channel weighting coefficients
    std::vector<double> m_channel_weights;
    
    // Mean square calculation with sliding windows
    struct SlidingWindow {
        std::deque<double> samples;
        double sum = 0.0;
        uint32_t max_samples;
        
        SlidingWindow(uint32_t max_size) : max_samples(max_size) {}
        
        void add_sample(double sample) {
            samples.push_back(sample);
            sum += sample;
            
            while (samples.size() > max_samples) {
                sum -= samples.front();
                samples.pop_front();
            }
        }
        
        double get_mean() const {
            return samples.empty() ? 0.0 : sum / samples.size();
        }
        
        void clear() {
            samples.clear();
            sum = 0.0;
        }
    };
    
    // Momentary loudness (400ms blocks)
    SlidingWindow m_momentary_window;
    double m_momentary_lufs = -70.0;
    
    // Short-term loudness (3s blocks)
    SlidingWindow m_short_term_window;
    double m_short_term_lufs = -70.0;
    
    // Integrated loudness (entire measurement)
    std::vector<double> m_integrated_blocks;
    double m_integrated_lufs = -70.0;
    
    // Loudness range calculation
    std::vector<double> m_short_term_history;
    double m_loudness_range = 0.0;
    
    // True peak detection (4x oversampling)
    struct TruePeakDetector {
        std::vector<double> m_upsample_filter;
        std::vector<double> m_delay_line;
        uint32_t m_delay_pos = 0;
        double m_peak_level = 0.0;
        
        TruePeakDetector();
        double process_sample(double sample);
        void reset();
    };
    
    std::vector<TruePeakDetector> m_true_peak_detectors;
    double m_true_peak_dbfs = -70.0;
    
    // Gating
    bool m_gating_enabled = true;
    double m_absolute_gate_threshold = -70.0;  // LUFS
    double m_relative_gate_offset = -10.0;     // LU below mean
    
    // Processing state
    uint32_t m_block_size;
    uint32_t m_samples_in_block = 0;
    double m_block_sum = 0.0;
    
    std::atomic<uint64_t> m_samples_processed{0};
    std::chrono::steady_clock::time_point m_measurement_start;
    
    mutable std::mutex m_processing_mutex;
    
    // Helper methods
    void init_k_weighting_filters();
    void init_channel_weights();
    void process_block_for_loudness(double mean_square);
    void update_integrated_loudness();
    void update_loudness_range();
    double mean_square_to_lufs(double mean_square) const;
    bool passes_absolute_gate(double lufs) const;
    bool passes_relative_gate(double lufs, double relative_threshold) const;
};

// Comprehensive meter processor
class MeterProcessor {
public:
    MeterProcessor(uint32_t channels, uint32_t sample_rate);
    ~MeterProcessor();
    
    // Process audio for metering
    void process_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    
    // Get complete meter data
    MeterData get_meter_data() const;
    
    // Reset all meters
    void reset_meters();
    
    // Peak metering settings
    void set_peak_ballistics(double attack_ms, double release_ms);
    void set_peak_hold_enabled(bool enabled) { m_peak_hold_enabled = enabled; }
    void set_peak_hold_time_ms(double time_ms) { m_peak_hold_time_ms = time_ms; }
    
    // RMS metering settings  
    void set_rms_window_size_ms(double window_ms);
    
    // LUFS metering control
    void enable_lufs_metering(bool enabled);
    bool is_lufs_metering_enabled() const { return m_lufs_meter != nullptr; }
    
    void start_lufs_measurement() { if (m_lufs_meter) m_lufs_meter->start_measurement(); }
    void stop_lufs_measurement() { if (m_lufs_meter) m_lufs_meter->stop_measurement(); }
    void reset_lufs_measurement() { if (m_lufs_meter) m_lufs_meter->reset_measurement(); }
    
    // Correlation metering (for stereo)
    void enable_correlation_metering(bool enabled) { m_correlation_enabled = enabled; }
    bool is_correlation_metering_enabled() const { return m_correlation_enabled; }
    
    // Meter enabling (for CPU optimization)
    void set_peak_metering_enabled(bool enabled) { m_peak_metering_enabled = enabled; }
    void set_rms_metering_enabled(bool enabled) { m_rms_metering_enabled = enabled; }
    
    bool is_peak_metering_enabled() const { return m_peak_metering_enabled; }
    bool is_rms_metering_enabled() const { return m_rms_metering_enabled; }
    
private:
    uint32_t m_channels;
    uint32_t m_sample_rate;
    
    // Peak metering
    bool m_peak_metering_enabled = true;
    std::vector<double> m_peak_levels;
    std::vector<double> m_peak_hold_levels;
    std::vector<uint32_t> m_peak_hold_counters;
    std::vector<bool> m_clip_indicators;
    
    double m_peak_attack_coeff = 1.0;
    double m_peak_release_coeff = 0.999;
    bool m_peak_hold_enabled = true;
    double m_peak_hold_time_ms = 1500.0;
    uint32_t m_peak_hold_samples;
    
    // RMS metering  
    bool m_rms_metering_enabled = true;
    struct RMSCalculator {
        std::deque<double> samples;
        double sum_of_squares = 0.0;
        uint32_t window_samples;
        
        RMSCalculator(uint32_t window_size) : window_samples(window_size) {}
        
        double add_sample(double sample) {
            double sample_squared = sample * sample;
            samples.push_back(sample_squared);
            sum_of_squares += sample_squared;
            
            while (samples.size() > window_samples) {
                sum_of_squares -= samples.front();
                samples.pop_front();
            }
            
            return samples.empty() ? 0.0 : std::sqrt(sum_of_squares / samples.size());
        }
        
        void clear() {
            samples.clear();
            sum_of_squares = 0.0;
        }
    };
    
    std::vector<RMSCalculator> m_rms_calculators;
    std::vector<double> m_rms_levels;
    double m_rms_window_ms = 300.0;
    
    // LUFS metering
    std::unique_ptr<LUFSMeter> m_lufs_meter;
    
    // Correlation metering
    bool m_correlation_enabled = true;
    struct CorrelationMeter {
        std::deque<std::pair<double, double>> samples;
        double sum_left = 0.0;
        double sum_right = 0.0;
        double sum_left_sq = 0.0;
        double sum_right_sq = 0.0;
        double sum_product = 0.0;
        uint32_t window_samples;
        
        CorrelationMeter(uint32_t window_size) : window_samples(window_size) {}
        
        double add_sample_pair(double left, double right) {
            samples.emplace_back(left, right);
            sum_left += left;
            sum_right += right;
            sum_left_sq += left * left;
            sum_right_sq += right * right;
            sum_product += left * right;
            
            while (samples.size() > window_samples) {
                auto old_sample = samples.front();
                samples.pop_front();
                sum_left -= old_sample.first;
                sum_right -= old_sample.second;
                sum_left_sq -= old_sample.first * old_sample.first;
                sum_right_sq -= old_sample.second * old_sample.second;
                sum_product -= old_sample.first * old_sample.second;
            }
            
            if (samples.size() < 2) return 0.0;
            
            double n = static_cast<double>(samples.size());
            double mean_left = sum_left / n;
            double mean_right = sum_right / n;
            
            double variance_left = (sum_left_sq / n) - (mean_left * mean_left);
            double variance_right = (sum_right_sq / n) - (mean_right * mean_right);
            double covariance = (sum_product / n) - (mean_left * mean_right);
            
            double denominator = std::sqrt(variance_left * variance_right);
            if (denominator < 1e-10) return 0.0;
            
            return std::clamp(covariance / denominator, -1.0, 1.0);
        }
        
        void clear() {
            samples.clear();
            sum_left = sum_right = sum_left_sq = sum_right_sq = sum_product = 0.0;
        }
    };
    
    std::unique_ptr<CorrelationMeter> m_correlation_meter;
    double m_phase_correlation = 0.0;
    
    mutable std::mutex m_meter_mutex;
    
    // Helper methods
    void process_peak_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    void process_rms_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    void process_correlation_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size);
    
    double linear_to_db(double linear) const;
    void update_ballistics_coefficients();
};

} // namespace mixmind