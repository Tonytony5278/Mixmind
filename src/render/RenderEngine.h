#pragma once

#include "RenderTypes.h"
#include "../core/result.h"
#include "../core/audio_types.h"
#include "../mixer/AudioBus.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace mixmind {

// Forward declarations
class AudioBuffer;
class MixerEngine;
class AudioFileWriter;
class LoudnessProcessor;
class ResampleProcessor;

// Render engine for bouncing audio to disk
class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();
    
    // Engine initialization
    Result<bool> initialize(std::shared_ptr<MixerEngine> mixer_engine);
    Result<bool> shutdown();
    bool is_initialized() const { return m_initialized.load(); }
    
    // Render job management
    Result<uint32_t> submit_render_job(const RenderJobConfig& config);
    Result<bool> cancel_render_job(uint32_t job_id);
    Result<bool> pause_render_job(uint32_t job_id);
    Result<bool> resume_render_job(uint32_t job_id);
    
    std::vector<uint32_t> get_active_render_jobs() const;
    std::vector<uint32_t> get_completed_render_jobs() const;
    
    // Job status and progress
    Result<RenderProgress> get_render_progress(uint32_t job_id) const;
    Result<RenderResult> get_render_result(uint32_t job_id) const;
    bool is_job_active(uint32_t job_id) const;
    bool is_job_completed(uint32_t job_id) const;
    
    // Render callbacks
    void set_progress_callback(RenderProgressCallback callback) { m_progress_callback = callback; }
    void set_completion_callback(RenderCompletionCallback callback) { m_completion_callback = callback; }
    
    // Real-time rendering (for monitoring during render)
    Result<bool> enable_real_time_monitoring(bool enabled);
    bool is_real_time_monitoring_enabled() const { return m_real_time_monitoring.load(); }
    
    // Render statistics
    struct RenderEngineStats {
        uint32_t total_jobs_processed = 0;
        uint32_t active_jobs = 0;
        uint32_t failed_jobs = 0;
        double total_render_time_hours = 0.0;
        uint64_t total_samples_rendered = 0;
        double average_render_speed = 1.0;      // Average speed factor
        size_t memory_usage_bytes = 0;
    };
    
    RenderEngineStats get_engine_statistics() const;
    void reset_engine_statistics();
    
    // Audio format support
    std::vector<AudioFormat> get_supported_formats() const;
    bool is_format_supported(AudioFormat format) const;
    Result<std::string> get_format_info(AudioFormat format) const;
    
    // Loudness standards support
    std::vector<LoudnessStandard> get_supported_loudness_standards() const;
    Result<RenderProcessingSettings> get_standard_processing_settings(LoudnessStandard standard) const;
    
    // Render presets
    struct RenderPreset {
        std::string name;
        std::string description;
        RenderJobConfig config;
    };
    
    std::vector<RenderPreset> get_builtin_presets() const;
    Result<bool> save_preset(const std::string& name, const RenderJobConfig& config);
    Result<RenderJobConfig> load_preset(const std::string& name) const;
    Result<bool> delete_preset(const std::string& name);
    
    // Advanced rendering features
    Result<bool> render_stems_parallel(const RenderJobConfig& config);
    Result<bool> render_with_custom_processor(const RenderJobConfig& config,
                                              std::function<void(std::shared_ptr<AudioBuffer>)> processor);
    
    // Performance settings
    void set_render_thread_count(int count) { m_render_thread_count = std::max(1, count); }
    int get_render_thread_count() const { return m_render_thread_count; }
    
    void set_render_buffer_size(uint32_t size) { m_render_buffer_size = std::clamp(size, 64u, 8192u); }
    uint32_t get_render_buffer_size() const { return m_render_buffer_size; }
    
    void set_memory_limit_mb(uint32_t limit_mb) { m_memory_limit_mb = limit_mb; }
    uint32_t get_memory_limit_mb() const { return m_memory_limit_mb; }
    
private:
    // Render job internal representation
    struct RenderJob {
        uint32_t job_id;
        RenderJobConfig config;
        RenderProgress progress;
        RenderResult result;
        std::atomic<bool> cancel_requested{false};
        std::atomic<bool> pause_requested{false};
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        
        RenderJob(uint32_t id, const RenderJobConfig& cfg) : job_id(id), config(cfg) {}
    };
    
    std::atomic<bool> m_initialized{false};
    std::shared_ptr<MixerEngine> m_mixer_engine;
    
    // Job management
    mutable std::mutex m_jobs_mutex;
    std::map<uint32_t, std::shared_ptr<RenderJob>> m_active_jobs;
    std::map<uint32_t, std::shared_ptr<RenderJob>> m_completed_jobs;
    uint32_t m_next_job_id = 1;
    
    // Render threading
    std::vector<std::thread> m_render_threads;
    std::queue<std::shared_ptr<RenderJob>> m_job_queue;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::atomic<bool> m_stop_threads{false};
    int m_render_thread_count = 2;
    
    // Render settings
    uint32_t m_render_buffer_size = 1024;
    uint32_t m_memory_limit_mb = 1024;  // 1GB default
    std::atomic<bool> m_real_time_monitoring{false};
    
    // Callbacks
    RenderProgressCallback m_progress_callback;
    RenderCompletionCallback m_completion_callback;
    
    // Statistics
    mutable std::mutex m_stats_mutex;
    RenderEngineStats m_statistics;
    
    // Processors
    std::unique_ptr<LoudnessProcessor> m_loudness_processor;
    std::unique_ptr<ResampleProcessor> m_resample_processor;
    
    // Thread worker function
    void render_thread_worker();
    
    // Core rendering functions
    Result<bool> process_render_job(std::shared_ptr<RenderJob> job);
    Result<bool> render_master_mix(std::shared_ptr<RenderJob> job);
    Result<bool> render_stems(std::shared_ptr<RenderJob> job);
    Result<bool> render_selected_tracks(std::shared_ptr<RenderJob> job);
    Result<bool> render_bus_output(std::shared_ptr<RenderJob> job);
    
    // Audio processing pipeline
    Result<std::shared_ptr<AudioBuffer>> render_audio_region(
        const RenderTarget& target,
        const RenderRegion& region,
        const RenderProcessingSettings& settings,
        std::shared_ptr<RenderJob> job);
    
    Result<bool> apply_render_processing(
        std::shared_ptr<AudioBuffer> buffer,
        const RenderProcessingSettings& settings,
        std::shared_ptr<RenderJob> job);
    
    Result<bool> write_audio_file(
        std::shared_ptr<AudioBuffer> buffer,
        const std::string& file_path,
        AudioFormat format,
        const RenderJobConfig::Metadata& metadata);
    
    // Loudness processing
    Result<bool> apply_loudness_normalization(
        std::shared_ptr<AudioBuffer> buffer,
        LoudnessStandard standard,
        double custom_target,
        std::shared_ptr<RenderJob> job);
    
    Result<bool> apply_true_peak_limiting(
        std::shared_ptr<AudioBuffer> buffer,
        double max_true_peak_dbfs,
        std::shared_ptr<RenderJob> job);
    
    // Analysis and measurement
    Result<RenderAnalysis> analyze_rendered_audio(
        std::shared_ptr<AudioBuffer> buffer,
        const RenderProcessingSettings& settings);
    
    // Utility functions
    uint32_t generate_job_id();
    std::string generate_output_filename(const RenderJobConfig& config, const std::string& track_name = "");
    std::string create_render_log_entry(const std::string& operation, const std::string& details = "");
    
    void update_job_progress(std::shared_ptr<RenderJob> job, double progress, const std::string& operation);
    void notify_progress_callback(const RenderProgress& progress);
    void notify_completion_callback(const RenderResult& result);
    
    bool check_memory_usage();
    void cleanup_completed_jobs();
    
    // Format-specific writers
    Result<std::unique_ptr<AudioFileWriter>> create_audio_writer(AudioFormat format, const std::string& file_path);
};

// Audio file writer interface
class AudioFileWriter {
public:
    virtual ~AudioFileWriter() = default;
    
    virtual Result<bool> open(const std::string& file_path, uint32_t channels, 
                             uint32_t sample_rate, AudioFormat format) = 0;
    virtual Result<bool> write_samples(const std::vector<std::vector<double>>& channel_data, 
                                      uint32_t num_samples) = 0;
    virtual Result<bool> write_metadata(const RenderJobConfig::Metadata& metadata) = 0;
    virtual Result<bool> close() = 0;
    virtual uint64_t get_samples_written() const = 0;
    virtual uint64_t get_file_size_bytes() const = 0;
};

// WAV file writer
class WAVFileWriter : public AudioFileWriter {
public:
    WAVFileWriter() = default;
    ~WAVFileWriter() override;
    
    Result<bool> open(const std::string& file_path, uint32_t channels, 
                     uint32_t sample_rate, AudioFormat format) override;
    Result<bool> write_samples(const std::vector<std::vector<double>>& channel_data, 
                              uint32_t num_samples) override;
    Result<bool> write_metadata(const RenderJobConfig::Metadata& metadata) override;
    Result<bool> close() override;
    uint64_t get_samples_written() const override { return m_samples_written; }
    uint64_t get_file_size_bytes() const override;
    
private:
    std::string m_file_path;
    std::unique_ptr<std::ofstream> m_file_stream;
    uint32_t m_channels = 0;
    uint32_t m_sample_rate = 0;
    AudioFormat m_format = AudioFormat::WAV_PCM_24;
    uint64_t m_samples_written = 0;
    uint32_t m_bytes_per_sample = 0;
    
    void write_wav_header();
    void update_wav_header();
    template<typename SampleType>
    void write_samples_typed(const std::vector<std::vector<double>>& channel_data, uint32_t num_samples);
};

// AIFF file writer
class AIFFFileWriter : public AudioFileWriter {
public:
    AIFFFileWriter() = default;
    ~AIFFFileWriter() override;
    
    Result<bool> open(const std::string& file_path, uint32_t channels, 
                     uint32_t sample_rate, AudioFormat format) override;
    Result<bool> write_samples(const std::vector<std::vector<double>>& channel_data, 
                              uint32_t num_samples) override;
    Result<bool> write_metadata(const RenderJobConfig::Metadata& metadata) override;
    Result<bool> close() override;
    uint64_t get_samples_written() const override { return m_samples_written; }
    uint64_t get_file_size_bytes() const override;
    
private:
    std::string m_file_path;
    std::unique_ptr<std::ofstream> m_file_stream;
    uint32_t m_channels = 0;
    uint32_t m_sample_rate = 0;
    AudioFormat m_format = AudioFormat::AIFF_PCM_24;
    uint64_t m_samples_written = 0;
    uint32_t m_bytes_per_sample = 0;
    
    void write_aiff_header();
    void update_aiff_header();
    void write_ieee_extended(double value, uint8_t* bytes);
};

// Loudness processor for normalization
class LoudnessProcessor {
public:
    LoudnessProcessor(uint32_t channels, uint32_t sample_rate);
    ~LoudnessProcessor();
    
    // Loudness measurement
    Result<bool> analyze_loudness(std::shared_ptr<AudioBuffer> buffer);
    double get_integrated_lufs() const { return m_integrated_lufs; }
    double get_true_peak_dbfs() const { return m_true_peak_dbfs; }
    double get_loudness_range() const { return m_loudness_range; }
    
    // Normalization
    Result<bool> normalize_to_lufs(std::shared_ptr<AudioBuffer> buffer, double target_lufs);
    Result<bool> limit_true_peak(std::shared_ptr<AudioBuffer> buffer, double max_true_peak_dbfs);
    
    // Reset for new analysis
    void reset();
    
private:
    uint32_t m_channels;
    uint32_t m_sample_rate;
    double m_integrated_lufs = -70.0;
    double m_true_peak_dbfs = -70.0;
    double m_loudness_range = 0.0;
    
    // Internal LUFS meter and limiter instances
    std::unique_ptr<class LUFSMeter> m_lufs_meter;
    std::unique_ptr<class TruePeakLimiter> m_limiter;
};

// Resample processor for sample rate conversion
class ResampleProcessor {
public:
    ResampleProcessor();
    ~ResampleProcessor();
    
    Result<bool> configure(uint32_t input_rate, uint32_t output_rate, 
                          uint32_t channels, int quality = 4);
    
    Result<std::shared_ptr<AudioBuffer>> process(std::shared_ptr<AudioBuffer> input_buffer);
    Result<std::shared_ptr<AudioBuffer>> flush();  // Get remaining samples
    
    void reset();
    
private:
    uint32_t m_input_rate = 0;
    uint32_t m_output_rate = 0;
    uint32_t m_channels = 0;
    int m_quality = 4;
    bool m_configured = false;
    
    // Resampling state (implementation-specific)
    struct ResampleState;
    std::unique_ptr<ResampleState> m_state;
    
    double calculate_resample_ratio() const;
    Result<bool> initialize_resampler();
};

} // namespace mixmind