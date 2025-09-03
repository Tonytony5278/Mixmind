#include "MeterProcessor.h"
#include "AudioBuffer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace mixmind {

// LUFS Meter Implementation

LUFSMeter::KWeightingFilter::KWeightingFilter() {
    // Pre-calculated coefficients for 44.1kHz sample rate
    // High-pass filter (f_h = 38 Hz, Q = 0.5)
    double f_h = 38.0 / 44100.0;  // Normalized frequency
    double w_h = 2.0 * M_PI * f_h;
    double cos_wh = std::cos(w_h);
    double sin_wh = std::sin(w_h);
    double alpha_h = sin_wh / (2.0 * 0.5);  // Q = 0.5
    
    double b0_norm = 1.0 + alpha_h;
    hpf_b0 = (1.0 + cos_wh) / (2.0 * b0_norm);
    hpf_b1 = -(1.0 + cos_wh) / b0_norm;
    hpf_b2 = (1.0 + cos_wh) / (2.0 * b0_norm);
    hpf_a1 = -2.0 * cos_wh / b0_norm;
    hpf_a2 = (1.0 - alpha_h) / b0_norm;
    
    // High-frequency shelving filter (f_s = 1500 Hz, +4 dB)
    double f_s = 1500.0 / 44100.0;  // Normalized frequency
    double w_s = 2.0 * M_PI * f_s;
    double A = std::pow(10.0, 4.0 / 40.0);  // +4 dB gain
    double cos_ws = std::cos(w_s);
    double sin_ws = std::sin(w_s);
    double alpha_s = sin_ws / 2.0 * std::sqrt((A + 1.0/A) * (1.0/0.707 - 1.0) + 2.0);  // Q = 0.707
    
    double b0_s_norm = (A + 1.0) + (A - 1.0) * cos_ws + alpha_s;
    hf_b0 = A * ((A + 1.0) - (A - 1.0) * cos_ws + alpha_s) / b0_s_norm;
    hf_b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_ws) / b0_s_norm;
    hf_b2 = A * ((A + 1.0) - (A - 1.0) * cos_ws - alpha_s) / b0_s_norm;
    hf_a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_ws) / b0_s_norm;
    hf_a2 = ((A + 1.0) + (A - 1.0) * cos_ws - alpha_s) / b0_s_norm;
    
    reset();
}

void LUFSMeter::KWeightingFilter::reset() {
    hpf_z1 = hpf_z2 = 0.0;
    hf_z1 = hf_z2 = 0.0;
}

double LUFSMeter::KWeightingFilter::process_sample(double sample) {
    // High-pass filter
    double hpf_out = hpf_b0 * sample + hpf_b1 * hpf_z1 + hpf_b2 * hpf_z2 
                     - hpf_a1 * hpf_z1 - hpf_a2 * hpf_z2;
    hpf_z2 = hpf_z1;
    hpf_z1 = sample;
    
    // High-frequency shelving filter
    double hf_out = hf_b0 * hpf_out + hf_b1 * hf_z1 + hf_b2 * hf_z2 
                    - hf_a1 * hf_z1 - hf_a2 * hf_z2;
    hf_z2 = hf_z1;
    hf_z1 = hpf_out;
    
    return hf_out;
}

LUFSMeter::TruePeakDetector::TruePeakDetector() {
    // 4x oversampling filter coefficients (simple linear interpolation)
    m_upsample_filter = {0.0, 0.25, 0.5, 0.75, 1.0, 0.75, 0.5, 0.25};
    m_delay_line.resize(8, 0.0);
    reset();
}

double LUFSMeter::TruePeakDetector::process_sample(double sample) {
    // Add sample to delay line
    m_delay_line[m_delay_pos] = sample;
    m_delay_pos = (m_delay_pos + 1) % m_delay_line.size();
    
    // Calculate 4x oversampled values
    for (size_t i = 0; i < 4; ++i) {
        double upsampled = 0.0;
        for (size_t j = 0; j < m_upsample_filter.size(); ++j) {
            size_t delay_idx = (m_delay_pos + j) % m_delay_line.size();
            upsampled += m_delay_line[delay_idx] * m_upsample_filter[j];
        }
        
        double abs_sample = std::abs(upsampled);
        if (abs_sample > m_peak_level) {
            m_peak_level = abs_sample;
        }
    }
    
    return m_peak_level;
}

void LUFSMeter::TruePeakDetector::reset() {
    std::fill(m_delay_line.begin(), m_delay_line.end(), 0.0);
    m_delay_pos = 0;
    m_peak_level = 0.0;
}

LUFSMeter::LUFSMeter(uint32_t channels, uint32_t sample_rate)
    : m_channels(channels), m_sample_rate(sample_rate),
      m_momentary_window(sample_rate * 400 / 1000),  // 400ms
      m_short_term_window(sample_rate * 3),          // 3s
      m_block_size(sample_rate / 10) {               // 100ms blocks
    
    // Initialize K-weighting filters
    m_k_filters.resize(m_channels);
    
    // Initialize channel weights (EBU R128)
    init_channel_weights();
    
    // Initialize true peak detectors
    m_true_peak_detectors.resize(m_channels);
    
    m_measurement_start = std::chrono::steady_clock::now();
}

LUFSMeter::~LUFSMeter() = default;

void LUFSMeter::init_channel_weights() {
    m_channel_weights.resize(m_channels, 1.0);
    
    if (m_channels >= 2) {
        // Standard stereo/surround weights
        m_channel_weights[0] = 1.0;  // Left
        m_channel_weights[1] = 1.0;  // Right
    }
    
    if (m_channels >= 5) {
        // 5.1 surround weights
        m_channel_weights[2] = 1.0;  // Center
        m_channel_weights[3] = 0.0;  // LFE (not measured)
        m_channel_weights[4] = 1.41; // Left Surround
        if (m_channels > 5) {
            m_channel_weights[5] = 1.41; // Right Surround
        }
    }
}

void LUFSMeter::process_audio(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    if (!m_is_measuring.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_processing_mutex);
    
    uint32_t channels = std::min(m_channels, buffer->get_channel_count());
    
    for (uint32_t sample = 0; sample < buffer_size; ++sample) {
        double sum_weighted = 0.0;
        
        // Process each channel
        for (uint32_t ch = 0; ch < channels; ++ch) {
            auto channel_data = buffer->get_channel_data(ch);
            double input_sample = channel_data[sample];
            
            // Apply K-weighting filter
            double filtered_sample = m_k_filters[ch].process_sample(input_sample);
            
            // Apply channel weighting and accumulate
            double weighted_sample = filtered_sample * m_channel_weights[ch];
            sum_weighted += weighted_sample * weighted_sample;
            
            // True peak detection
            m_true_peak_detectors[ch].process_sample(input_sample);
        }
        
        // Accumulate for block processing
        m_block_sum += sum_weighted;
        m_samples_in_block++;
        
        // Process when we have a complete block
        if (m_samples_in_block >= m_block_size) {
            double mean_square = m_block_sum / m_samples_in_block;
            process_block_for_loudness(mean_square);
            
            m_block_sum = 0.0;
            m_samples_in_block = 0;
        }
    }
    
    // Update true peak
    double max_true_peak = 0.0;
    for (auto& detector : m_true_peak_detectors) {
        max_true_peak = std::max(max_true_peak, detector.m_peak_level);
    }
    
    if (max_true_peak > 0.0) {
        m_true_peak_dbfs = std::max(m_true_peak_dbfs, 20.0 * std::log10(max_true_peak));
    }
    
    m_samples_processed.fetch_add(buffer_size);
}

void LUFSMeter::process_block_for_loudness(double mean_square) {
    if (mean_square <= 0.0) {
        return;
    }
    
    double block_lufs = mean_square_to_lufs(mean_square);
    
    // Momentary loudness (400ms sliding window)
    m_momentary_window.add_sample(mean_square);
    double momentary_mean = m_momentary_window.get_mean();
    if (momentary_mean > 0.0) {
        m_momentary_lufs = mean_square_to_lufs(momentary_mean);
    }
    
    // Short-term loudness (3s sliding window)  
    m_short_term_window.add_sample(mean_square);
    double short_term_mean = m_short_term_window.get_mean();
    if (short_term_mean > 0.0) {
        m_short_term_lufs = mean_square_to_lufs(short_term_mean);
        m_short_term_history.push_back(m_short_term_lufs);
    }
    
    // Integrated loudness (gated)
    if (passes_absolute_gate(block_lufs)) {
        m_integrated_blocks.push_back(mean_square);
        update_integrated_loudness();
    }
    
    // Update loudness range
    update_loudness_range();
}

void LUFSMeter::update_integrated_loudness() {
    if (m_integrated_blocks.empty()) {
        return;
    }
    
    // Calculate mean without gating first
    double sum_blocks = std::accumulate(m_integrated_blocks.begin(), m_integrated_blocks.end(), 0.0);
    double mean_lufs = mean_square_to_lufs(sum_blocks / m_integrated_blocks.size());
    
    // Apply relative gating if enabled
    if (m_gating_enabled) {
        double relative_threshold = mean_lufs + m_relative_gate_offset;
        
        std::vector<double> gated_blocks;
        for (double block : m_integrated_blocks) {
            double block_lufs = mean_square_to_lufs(block);
            if (passes_relative_gate(block_lufs, relative_threshold)) {
                gated_blocks.push_back(block);
            }
        }
        
        if (!gated_blocks.empty()) {
            double gated_sum = std::accumulate(gated_blocks.begin(), gated_blocks.end(), 0.0);
            m_integrated_lufs = mean_square_to_lufs(gated_sum / gated_blocks.size());
        }
    } else {
        m_integrated_lufs = mean_lufs;
    }
}

void LUFSMeter::update_loudness_range() {
    if (m_short_term_history.size() < 10) {
        return;  // Need at least 10 measurements
    }
    
    // Copy and sort short-term history for percentile calculation
    std::vector<double> sorted_history = m_short_term_history;
    std::sort(sorted_history.begin(), sorted_history.end());
    
    // Calculate 10th and 95th percentiles
    size_t n = sorted_history.size();
    size_t p10_idx = static_cast<size_t>(n * 0.1);
    size_t p95_idx = static_cast<size_t>(n * 0.95);
    
    if (p95_idx < n && p10_idx < n) {
        double p10 = sorted_history[p10_idx];
        double p95 = sorted_history[p95_idx];
        m_loudness_range = p95 - p10;
    }
}

double LUFSMeter::mean_square_to_lufs(double mean_square) const {
    if (mean_square <= 0.0) {
        return -70.0;  // -70 LUFS is practical silence
    }
    return -0.691 + 10.0 * std::log10(mean_square);
}

bool LUFSMeter::passes_absolute_gate(double lufs) const {
    return lufs >= m_absolute_gate_threshold;
}

bool LUFSMeter::passes_relative_gate(double lufs, double relative_threshold) const {
    return lufs >= relative_threshold;
}

void LUFSMeter::start_measurement() {
    std::lock_guard<std::mutex> lock(m_processing_mutex);
    m_is_measuring.store(true);
    m_measurement_start = std::chrono::steady_clock::now();
}

void LUFSMeter::stop_measurement() {
    m_is_measuring.store(false);
}

void LUFSMeter::reset_measurement() {
    std::lock_guard<std::mutex> lock(m_processing_mutex);
    
    // Reset filters
    for (auto& filter : m_k_filters) {
        filter.reset();
    }
    
    // Reset true peak detectors
    for (auto& detector : m_true_peak_detectors) {
        detector.reset();
    }
    
    // Reset measurements
    m_momentary_window.clear();
    m_short_term_window.clear();
    m_integrated_blocks.clear();
    m_short_term_history.clear();
    
    m_momentary_lufs = m_short_term_lufs = m_integrated_lufs = -70.0;
    m_loudness_range = 0.0;
    m_true_peak_dbfs = -70.0;
    
    m_block_sum = 0.0;
    m_samples_in_block = 0;
    m_samples_processed.store(0);
    
    m_measurement_start = std::chrono::steady_clock::now();
}

double LUFSMeter::get_momentary_lufs() const {
    return m_momentary_lufs;
}

double LUFSMeter::get_short_term_lufs() const {
    return m_short_term_lufs;
}

double LUFSMeter::get_integrated_lufs() const {
    return m_integrated_lufs;
}

double LUFSMeter::get_loudness_range() const {
    return m_loudness_range;
}

double LUFSMeter::get_true_peak_dbfs() const {
    return m_true_peak_dbfs;
}

std::chrono::milliseconds LUFSMeter::get_measurement_duration() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_measurement_start);
}

// MeterProcessor Implementation

MeterProcessor::MeterProcessor(uint32_t channels, uint32_t sample_rate)
    : m_channels(channels), m_sample_rate(sample_rate) {
    
    // Initialize peak metering
    m_peak_levels.resize(m_channels, 0.0);
    m_peak_hold_levels.resize(m_channels, 0.0);
    m_peak_hold_counters.resize(m_channels, 0);
    m_clip_indicators.resize(m_channels, false);
    
    update_ballistics_coefficients();
    
    // Initialize RMS metering
    uint32_t rms_window_samples = static_cast<uint32_t>(m_sample_rate * m_rms_window_ms / 1000.0);
    m_rms_calculators.reserve(m_channels);
    for (uint32_t ch = 0; ch < m_channels; ++ch) {
        m_rms_calculators.emplace_back(rms_window_samples);
    }
    m_rms_levels.resize(m_channels, 0.0);
    
    // Initialize correlation metering for stereo
    if (m_channels >= 2) {
        uint32_t correlation_window = static_cast<uint32_t>(m_sample_rate * 0.1);  // 100ms
        m_correlation_meter = std::make_unique<CorrelationMeter>(correlation_window);
    }
    
    // Initialize LUFS metering
    enable_lufs_metering(true);
}

MeterProcessor::~MeterProcessor() = default;

void MeterProcessor::process_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_meter_mutex);
    
    uint32_t channels = std::min(m_channels, buffer->get_channel_count());
    
    // Process peak metering
    if (m_peak_metering_enabled) {
        process_peak_metering(buffer, buffer_size);
    }
    
    // Process RMS metering
    if (m_rms_metering_enabled) {
        process_rms_metering(buffer, buffer_size);
    }
    
    // Process correlation metering
    if (m_correlation_enabled && channels >= 2) {
        process_correlation_metering(buffer, buffer_size);
    }
    
    // Process LUFS metering
    if (m_lufs_meter) {
        m_lufs_meter->process_audio(buffer, buffer_size);
    }
}

void MeterProcessor::process_peak_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    uint32_t channels = std::min(m_channels, buffer->get_channel_count());
    
    for (uint32_t ch = 0; ch < channels; ++ch) {
        auto channel_data = buffer->get_channel_data(ch);
        double channel_peak = 0.0;
        bool clipped = false;
        
        // Find peak in buffer
        for (uint32_t sample = 0; sample < buffer_size; ++sample) {
            double abs_sample = std::abs(channel_data[sample]);
            channel_peak = std::max(channel_peak, abs_sample);
            
            // Clip detection (above 0 dBFS)
            if (abs_sample >= 0.995) {  // Slightly below 1.0 to avoid false positives
                clipped = true;
            }
        }
        
        // Apply ballistics
        if (channel_peak > m_peak_levels[ch]) {
            // Fast attack
            m_peak_levels[ch] = m_peak_attack_coeff * channel_peak + 
                                (1.0 - m_peak_attack_coeff) * m_peak_levels[ch];
        } else {
            // Slow release
            m_peak_levels[ch] = m_peak_release_coeff * m_peak_levels[ch] + 
                                (1.0 - m_peak_release_coeff) * channel_peak;
        }
        
        // Peak hold
        if (m_peak_hold_enabled) {
            if (channel_peak > m_peak_hold_levels[ch]) {
                m_peak_hold_levels[ch] = channel_peak;
                m_peak_hold_counters[ch] = m_peak_hold_samples;
            } else if (m_peak_hold_counters[ch] > 0) {
                m_peak_hold_counters[ch]--;
            } else {
                m_peak_hold_levels[ch] = m_peak_levels[ch];
            }
        }
        
        // Clip indicator
        m_clip_indicators[ch] = clipped;
    }
}

void MeterProcessor::process_rms_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    uint32_t channels = std::min(m_channels, buffer->get_channel_count());
    
    for (uint32_t ch = 0; ch < channels && ch < m_rms_calculators.size(); ++ch) {
        auto channel_data = buffer->get_channel_data(ch);
        
        double rms_level = 0.0;
        for (uint32_t sample = 0; sample < buffer_size; ++sample) {
            rms_level = m_rms_calculators[ch].add_sample(channel_data[sample]);
        }
        
        m_rms_levels[ch] = rms_level;
    }
}

void MeterProcessor::process_correlation_metering(std::shared_ptr<AudioBuffer> buffer, uint32_t buffer_size) {
    if (!m_correlation_meter || buffer->get_channel_count() < 2) {
        return;
    }
    
    auto left_data = buffer->get_channel_data(0);
    auto right_data = buffer->get_channel_data(1);
    
    double correlation = 0.0;
    for (uint32_t sample = 0; sample < buffer_size; ++sample) {
        correlation = m_correlation_meter->add_sample_pair(left_data[sample], right_data[sample]);
    }
    
    m_phase_correlation = correlation;
}

MeterData MeterProcessor::get_meter_data() const {
    std::lock_guard<std::mutex> lock(m_meter_mutex);
    
    MeterData data;
    
    // Peak levels
    data.peak_levels = m_peak_levels;
    data.peak_levels_db.resize(m_channels);
    data.clip_indicators = m_clip_indicators;
    
    for (uint32_t ch = 0; ch < m_channels; ++ch) {
        data.peak_levels_db[ch] = linear_to_db(m_peak_levels[ch]);
    }
    
    // RMS levels
    data.rms_levels = m_rms_levels;
    data.rms_levels_db.resize(m_channels);
    
    for (uint32_t ch = 0; ch < m_channels; ++ch) {
        data.rms_levels_db[ch] = linear_to_db(m_rms_levels[ch]);
    }
    
    // LUFS data
    if (m_lufs_meter) {
        data.momentary_lufs = m_lufs_meter->get_momentary_lufs();
        data.short_term_lufs = m_lufs_meter->get_short_term_lufs();
        data.integrated_lufs = m_lufs_meter->get_integrated_lufs();
        data.loudness_range = m_lufs_meter->get_loudness_range();
        data.true_peak_dbfs = m_lufs_meter->get_true_peak_dbfs();
    }
    
    // Correlation
    data.phase_correlation = m_phase_correlation;
    
    return data;
}

void MeterProcessor::reset_meters() {
    std::lock_guard<std::mutex> lock(m_meter_mutex);
    
    // Reset peak meters
    std::fill(m_peak_levels.begin(), m_peak_levels.end(), 0.0);
    std::fill(m_peak_hold_levels.begin(), m_peak_hold_levels.end(), 0.0);
    std::fill(m_peak_hold_counters.begin(), m_peak_hold_counters.end(), 0);
    std::fill(m_clip_indicators.begin(), m_clip_indicators.end(), false);
    
    // Reset RMS meters
    for (auto& calculator : m_rms_calculators) {
        calculator.clear();
    }
    std::fill(m_rms_levels.begin(), m_rms_levels.end(), 0.0);
    
    // Reset correlation meter
    if (m_correlation_meter) {
        m_correlation_meter->clear();
    }
    m_phase_correlation = 0.0;
    
    // Reset LUFS meter
    if (m_lufs_meter) {
        m_lufs_meter->reset_measurement();
    }
}

void MeterProcessor::enable_lufs_metering(bool enabled) {
    std::lock_guard<std::mutex> lock(m_meter_mutex);
    
    if (enabled && !m_lufs_meter) {
        m_lufs_meter = std::make_unique<LUFSMeter>(m_channels, m_sample_rate);
        m_lufs_meter->start_measurement();
    } else if (!enabled) {
        m_lufs_meter.reset();
    }
}

void MeterProcessor::set_peak_ballistics(double attack_ms, double release_ms) {
    m_peak_hold_time_ms = std::max(0.1, attack_ms);
    update_ballistics_coefficients();
}

void MeterProcessor::set_rms_window_size_ms(double window_ms) {
    m_rms_window_ms = std::clamp(window_ms, 10.0, 5000.0);
    
    std::lock_guard<std::mutex> lock(m_meter_mutex);
    
    uint32_t new_window_samples = static_cast<uint32_t>(m_sample_rate * m_rms_window_ms / 1000.0);
    m_rms_calculators.clear();
    m_rms_calculators.reserve(m_channels);
    
    for (uint32_t ch = 0; ch < m_channels; ++ch) {
        m_rms_calculators.emplace_back(new_window_samples);
    }
}

void MeterProcessor::update_ballistics_coefficients() {
    // Calculate attack coefficient (fast response)
    m_peak_attack_coeff = 1.0;  // Instantaneous attack
    
    // Calculate release coefficient (slow decay)
    double release_time_s = 300.0 / 1000.0;  // 300ms release
    m_peak_release_coeff = std::exp(-1.0 / (release_time_s * m_sample_rate));
    
    // Peak hold samples
    m_peak_hold_samples = static_cast<uint32_t>(m_sample_rate * m_peak_hold_time_ms / 1000.0);
}

double MeterProcessor::linear_to_db(double linear) const {
    if (linear <= 0.0) {
        return -70.0;  // Practical silence
    }
    return 20.0 * std::log10(linear);
}

} // namespace mixmind