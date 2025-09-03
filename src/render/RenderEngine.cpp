#include "RenderEngine.h"
#include "../mixer/MixerEngine.h"
#include "../audio/AudioBuffer.h"
#include "../audio/MeterProcessor.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace mixmind {

// RenderEngine Implementation

RenderEngine::RenderEngine() {
    // Initialize loudness and resample processors
    m_loudness_processor = std::make_unique<LoudnessProcessor>(2, 44100);
    m_resample_processor = std::make_unique<ResampleProcessor>();
}

RenderEngine::~RenderEngine() {
    shutdown();
}

Result<bool> RenderEngine::initialize(std::shared_ptr<MixerEngine> mixer_engine) {
    if (!mixer_engine) {
        return Error("Invalid mixer engine");
    }
    
    m_mixer_engine = mixer_engine;
    
    // Start render threads
    m_stop_threads.store(false);
    for (int i = 0; i < m_render_thread_count; ++i) {
        m_render_threads.emplace_back(&RenderEngine::render_thread_worker, this);
    }
    
    m_initialized.store(true);
    return Ok(true);
}

Result<bool> RenderEngine::shutdown() {
    if (!m_initialized.load()) {
        return Ok(true);  // Already shutdown
    }
    
    // Stop render threads
    m_stop_threads.store(true);
    m_queue_cv.notify_all();
    
    for (auto& thread : m_render_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_render_threads.clear();
    
    // Cancel any active jobs
    {
        std::lock_guard<std::mutex> lock(m_jobs_mutex);
        for (auto& [job_id, job] : m_active_jobs) {
            job->cancel_requested.store(true);
        }
    }
    
    m_initialized.store(false);
    return Ok(true);
}

Result<uint32_t> RenderEngine::submit_render_job(const RenderJobConfig& config) {
    if (!m_initialized.load()) {
        return Error("Render engine not initialized");
    }
    
    // Validate configuration
    if (config.output_path.empty()) {
        return Error("Output path is required");
    }
    
    if (config.region.get_length_samples() == 0) {
        return Error("Invalid render region");
    }
    
    // Create render job
    uint32_t job_id = generate_job_id();
    auto job = std::make_shared<RenderJob>(job_id, config);
    job->start_time = std::chrono::steady_clock::now();
    
    // Add to job queue
    {
        std::lock_guard<std::mutex> jobs_lock(m_jobs_mutex);
        std::lock_guard<std::mutex> queue_lock(m_queue_mutex);
        
        m_active_jobs[job_id] = job;
        m_job_queue.push(job);
    }
    
    m_queue_cv.notify_one();
    return Ok(job_id);
}

Result<bool> RenderEngine::cancel_render_job(uint32_t job_id) {
    std::lock_guard<std::mutex> lock(m_jobs_mutex);
    
    auto it = m_active_jobs.find(job_id);
    if (it == m_active_jobs.end()) {
        return Error("Render job not found: " + std::to_string(job_id));
    }
    
    it->second->cancel_requested.store(true);
    return Ok(true);
}

Result<RenderProgress> RenderEngine::get_render_progress(uint32_t job_id) const {
    std::lock_guard<std::mutex> lock(m_jobs_mutex);
    
    auto active_it = m_active_jobs.find(job_id);
    if (active_it != m_active_jobs.end()) {
        return Ok(active_it->second->progress);
    }
    
    auto completed_it = m_completed_jobs.find(job_id);
    if (completed_it != m_completed_jobs.end()) {
        return Ok(completed_it->second->progress);
    }
    
    return Error("Render job not found: " + std::to_string(job_id));
}

Result<RenderResult> RenderEngine::get_render_result(uint32_t job_id) const {
    std::lock_guard<std::mutex> lock(m_jobs_mutex);
    
    auto it = m_completed_jobs.find(job_id);
    if (it == m_completed_jobs.end()) {
        return Error("Render job not completed: " + std::to_string(job_id));
    }
    
    return Ok(it->second->result);
}

std::vector<AudioFormat> RenderEngine::get_supported_formats() const {
    return {
        AudioFormat::WAV_PCM_16,
        AudioFormat::WAV_PCM_24,
        AudioFormat::WAV_PCM_32,
        AudioFormat::WAV_FLOAT_32,
        AudioFormat::AIFF_PCM_16,
        AudioFormat::AIFF_PCM_24,
        AudioFormat::AIFF_FLOAT_32,
        AudioFormat::FLAC_16,
        AudioFormat::FLAC_24
        // MP3 and other lossy formats would require additional libraries
    };
}

bool RenderEngine::is_format_supported(AudioFormat format) const {
    auto supported = get_supported_formats();
    return std::find(supported.begin(), supported.end(), format) != supported.end();
}

std::vector<RenderEngine::RenderPreset> RenderEngine::get_builtin_presets() const {
    std::vector<RenderPreset> presets;
    
    // High Quality Master preset
    {
        RenderPreset preset;
        preset.name = "High Quality Master";
        preset.description = "24-bit WAV master with loudness normalization";
        preset.config.audio_format = AudioFormat::WAV_PCM_24;
        preset.config.quality = RenderQuality::HIGH_QUALITY;
        preset.config.target.type = RenderTarget::MASTER_MIX;
        preset.config.processing.loudness_standard = LoudnessStandard::EBU_R128_23;
        preset.config.processing.enable_limiter = true;
        preset.config.processing.true_peak_limiting = true;
        presets.push_back(preset);
    }
    
    // Streaming Master preset
    {
        RenderPreset preset;
        preset.name = "Streaming Master";
        preset.description = "Optimized for streaming platforms (-14 LUFS)";
        preset.config.audio_format = AudioFormat::WAV_PCM_24;
        preset.config.target.type = RenderTarget::MASTER_MIX;
        preset.config.processing.loudness_standard = LoudnessStandard::SPOTIFY_14;
        preset.config.processing.enable_limiter = true;
        preset.config.processing.true_peak_limiting = true;
        presets.push_back(preset);
    }
    
    // Individual Stems preset
    {
        RenderPreset preset;
        preset.name = "Individual Stems";
        preset.description = "Render all tracks as separate stems";
        preset.config.audio_format = AudioFormat::WAV_PCM_24;
        preset.config.target.type = RenderTarget::STEMS;
        preset.config.normalize_stems = true;
        preset.config.separate_directories = true;
        presets.push_back(preset);
    }
    
    // Draft Preview preset
    {
        RenderPreset preset;
        preset.name = "Draft Preview";
        preset.description = "Fast 16-bit render for preview";
        preset.config.audio_format = AudioFormat::WAV_PCM_16;
        preset.config.quality = RenderQuality::DRAFT;
        preset.config.mode = RenderMode::REAL_TIME;
        preset.config.target.type = RenderTarget::MASTER_MIX;
        presets.push_back(preset);
    }
    
    return presets;
}

RenderEngine::RenderEngineStats RenderEngine::get_engine_statistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_statistics;
}

void RenderEngine::render_thread_worker() {
    while (!m_stop_threads.load()) {
        std::shared_ptr<RenderJob> job;
        
        // Wait for job
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_queue_cv.wait(lock, [this] { return !m_job_queue.empty() || m_stop_threads.load(); });
            
            if (m_stop_threads.load()) {
                break;
            }
            
            if (m_job_queue.empty()) {
                continue;
            }
            
            job = m_job_queue.front();
            m_job_queue.pop();
        }
        
        // Process the job
        if (job && !job->cancel_requested.load()) {
            update_job_progress(job, 0.0, "Starting render job");
            auto result = process_render_job(job);
            
            job->end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                job->end_time - job->start_time);
            job->result.total_render_time_seconds = duration.count() / 1000.0;
            
            if (result.is_ok()) {
                job->progress.status = RenderProgress::COMPLETED;
                job->progress.progress_percent = 100.0;
                job->result.success = true;
            } else {
                job->progress.status = RenderProgress::ERROR;
                job->progress.error_message = result.error().message;
                job->result.success = false;
            }
            
            // Move job to completed
            {
                std::lock_guard<std::mutex> lock(m_jobs_mutex);
                m_completed_jobs[job->job_id] = job;
                m_active_jobs.erase(job->job_id);
            }
            
            // Update statistics
            {
                std::lock_guard<std::mutex> lock(m_stats_mutex);
                m_statistics.total_jobs_processed++;
                if (!job->result.success) {
                    m_statistics.failed_jobs++;
                }
                m_statistics.total_render_time_hours += job->result.total_render_time_seconds / 3600.0;
            }
            
            // Notify completion
            if (m_completion_callback) {
                notify_completion_callback(job->result);
            }
        }
    }
}

Result<bool> RenderEngine::process_render_job(std::shared_ptr<RenderJob> job) {
    if (!job || job->cancel_requested.load()) {
        return Error("Job cancelled");
    }
    
    job->progress.status = RenderProgress::RENDERING;
    job->result.render_log += create_render_log_entry("Started render job", "Job ID: " + std::to_string(job->job_id));
    
    // Route to appropriate render function based on target type
    switch (job->config.target.type) {
        case RenderTarget::MASTER_MIX:
            return render_master_mix(job);
            
        case RenderTarget::STEMS:
            return render_stems(job);
            
        case RenderTarget::SELECTED_TRACKS:
            return render_selected_tracks(job);
            
        case RenderTarget::BUS_OUTPUT:
            return render_bus_output(job);
            
        default:
            return Error("Unsupported render target type");
    }
}

Result<bool> RenderEngine::render_master_mix(std::shared_ptr<RenderJob> job) {
    update_job_progress(job, 10.0, "Preparing master mix render");
    
    // Render the audio region
    auto audio_result = render_audio_region(job->config.target, job->config.region, 
                                           job->config.processing, job);
    if (!audio_result.is_ok()) {
        return Error("Failed to render audio: " + audio_result.error().message);
    }
    
    auto rendered_buffer = audio_result.unwrap();
    
    if (job->cancel_requested.load()) {
        return Error("Job cancelled");
    }
    
    update_job_progress(job, 60.0, "Applying post-processing");
    
    // Apply post-processing
    auto processing_result = apply_render_processing(rendered_buffer, job->config.processing, job);
    if (!processing_result.is_ok()) {
        return Error("Failed to apply processing: " + processing_result.error().message);
    }
    
    update_job_progress(job, 80.0, "Writing audio file");
    
    // Generate output filename
    std::string output_file = generate_output_filename(job->config);
    job->result.output_file_path = output_file;
    
    // Write audio file
    auto write_result = write_audio_file(rendered_buffer, output_file, 
                                        job->config.audio_format, job->config.metadata);
    if (!write_result.is_ok()) {
        return Error("Failed to write audio file: " + write_result.error().message);
    }
    
    update_job_progress(job, 90.0, "Analyzing rendered audio");
    
    // Perform analysis
    auto analysis_result = analyze_rendered_audio(rendered_buffer, job->config.processing);
    if (analysis_result.is_ok()) {
        job->result.analysis = analysis_result.unwrap();
    }
    
    job->result.render_log += create_render_log_entry("Master mix render completed", output_file);
    
    return Ok(true);
}

Result<bool> RenderEngine::render_stems(std::shared_ptr<RenderJob> job) {
    update_job_progress(job, 5.0, "Preparing stems render");
    
    if (!m_mixer_engine) {
        return Error("Mixer engine not available");
    }
    
    // Get all tracks from mixer for stems rendering
    auto track_ids = m_mixer_engine->get_all_track_ids();
    if (track_ids.empty()) {
        return Error("No tracks available for stems rendering");
    }
    
    job->result.stem_file_paths.reserve(track_ids.size());
    double progress_per_track = 80.0 / track_ids.size();  // Reserve 20% for finalization
    
    for (size_t i = 0; i < track_ids.size(); ++i) {
        if (job->cancel_requested.load()) {
            return Error("Job cancelled");
        }
        
        uint32_t track_id = track_ids[i];
        double base_progress = 5.0 + (i * progress_per_track);
        
        update_job_progress(job, base_progress, "Rendering track " + std::to_string(track_id));
        
        // Create render target for this specific track
        RenderTarget track_target(RenderTarget::SELECTED_TRACKS);
        track_target.track_ids = {track_id};
        
        // Render this track
        auto audio_result = render_audio_region(track_target, job->config.region, 
                                               job->config.processing, job);
        if (!audio_result.is_ok()) {
            job->result.render_log += create_render_log_entry("Failed to render track " + std::to_string(track_id), 
                                                             audio_result.error().message);
            continue;  // Skip this track but continue with others
        }
        
        auto rendered_buffer = audio_result.unwrap();
        
        // Apply per-track processing if enabled
        RenderProcessingSettings track_processing = job->config.processing;
        if (job->config.normalize_stems) {
            // Normalize each stem individually to -1 dBFS peak
            double peak_level = 0.0;
            for (uint32_t ch = 0; ch < rendered_buffer->get_channel_count(); ++ch) {
                auto channel_data = rendered_buffer->get_channel_data(ch);
                for (uint32_t sample = 0; sample < rendered_buffer->get_buffer_size(); ++sample) {
                    peak_level = std::max(peak_level, std::abs(channel_data[sample]));
                }
            }
            
            if (peak_level > 0.0) {
                double normalize_gain = 0.99 / peak_level;  // Leave 1% headroom
                rendered_buffer->apply_gain(normalize_gain);
            }
        }
        
        auto processing_result = apply_render_processing(rendered_buffer, track_processing, job);
        if (!processing_result.is_ok()) {
            job->result.render_log += create_render_log_entry("Failed to process track " + std::to_string(track_id),
                                                             processing_result.error().message);
            continue;
        }
        
        // Generate stem filename
        std::string track_name = m_mixer_engine->get_track_name(track_id);
        if (track_name.empty()) {
            track_name = "Track_" + std::to_string(track_id);
        }
        
        std::string stem_file = generate_output_filename(job->config, track_name);
        
        // Create subdirectory if requested
        if (job->config.separate_directories) {
            std::filesystem::path output_dir = std::filesystem::path(stem_file).parent_path() / "Stems";
            std::filesystem::create_directories(output_dir);
            stem_file = output_dir / std::filesystem::path(stem_file).filename();
        }
        
        // Write stem file
        auto write_result = write_audio_file(rendered_buffer, stem_file.string(), 
                                            job->config.audio_format, job->config.metadata);
        if (write_result.is_ok()) {
            job->result.stem_file_paths.push_back(stem_file.string());
            job->result.render_log += create_render_log_entry("Stem rendered", track_name + " -> " + stem_file.string());
        } else {
            job->result.render_log += create_render_log_entry("Failed to write stem " + track_name,
                                                             write_result.error().message);
        }
    }
    
    update_job_progress(job, 90.0, "Finalizing stems render");
    
    if (job->result.stem_file_paths.empty()) {
        return Error("No stems were successfully rendered");
    }
    
    job->result.render_log += create_render_log_entry("Stems render completed", 
                                                     std::to_string(job->result.stem_file_paths.size()) + " stems rendered");
    
    return Ok(true);
}

Result<bool> RenderEngine::render_selected_tracks(std::shared_ptr<RenderJob> job) {
    if (job->config.target.track_ids.empty()) {
        return Error("No tracks selected for rendering");
    }
    
    update_job_progress(job, 10.0, "Rendering selected tracks");
    
    // Similar to master mix but only process selected tracks
    auto audio_result = render_audio_region(job->config.target, job->config.region, 
                                           job->config.processing, job);
    if (!audio_result.is_ok()) {
        return Error("Failed to render selected tracks: " + audio_result.error().message);
    }
    
    auto rendered_buffer = audio_result.unwrap();
    
    update_job_progress(job, 60.0, "Applying post-processing");
    
    auto processing_result = apply_render_processing(rendered_buffer, job->config.processing, job);
    if (!processing_result.is_ok()) {
        return Error("Failed to apply processing: " + processing_result.error().message);
    }
    
    update_job_progress(job, 80.0, "Writing audio file");
    
    std::string output_file = generate_output_filename(job->config, "Selected_Tracks");
    job->result.output_file_path = output_file;
    
    auto write_result = write_audio_file(rendered_buffer, output_file, 
                                        job->config.audio_format, job->config.metadata);
    if (!write_result.is_ok()) {
        return Error("Failed to write audio file: " + write_result.error().message);
    }
    
    job->result.render_log += create_render_log_entry("Selected tracks render completed", output_file);
    
    return Ok(true);
}

Result<bool> RenderEngine::render_bus_output(std::shared_ptr<RenderJob> job) {
    if (job->config.target.bus_ids.empty()) {
        return Error("No buses selected for rendering");
    }
    
    update_job_progress(job, 10.0, "Rendering bus outputs");
    
    // Similar implementation for bus output rendering
    auto audio_result = render_audio_region(job->config.target, job->config.region, 
                                           job->config.processing, job);
    if (!audio_result.is_ok()) {
        return Error("Failed to render bus output: " + audio_result.error().message);
    }
    
    auto rendered_buffer = audio_result.unwrap();
    
    update_job_progress(job, 60.0, "Applying post-processing");
    
    auto processing_result = apply_render_processing(rendered_buffer, job->config.processing, job);
    if (!processing_result.is_ok()) {
        return Error("Failed to apply processing: " + processing_result.error().message);
    }
    
    update_job_progress(job, 80.0, "Writing audio file");
    
    std::string output_file = generate_output_filename(job->config, "Bus_Output");
    job->result.output_file_path = output_file;
    
    auto write_result = write_audio_file(rendered_buffer, output_file, 
                                        job->config.audio_format, job->config.metadata);
    if (!write_result.is_ok()) {
        return Error("Failed to write audio file: " + write_result.error().message);
    }
    
    job->result.render_log += create_render_log_entry("Bus output render completed", output_file);
    
    return Ok(true);
}

Result<std::shared_ptr<AudioBuffer>> RenderEngine::render_audio_region(
    const RenderTarget& target,
    const RenderRegion& region,
    const RenderProcessingSettings& settings,
    std::shared_ptr<RenderJob> job) {
    
    if (!m_mixer_engine) {
        return Error("Mixer engine not available");
    }
    
    uint64_t total_samples = region.get_length_samples();
    if (total_samples == 0) {
        return Error("Invalid render region");
    }
    
    // Create output buffer for the entire region
    auto output_buffer = std::make_shared<AudioBuffer>(2, total_samples);
    uint64_t samples_rendered = 0;
    
    // Render in chunks
    uint32_t chunk_size = m_render_buffer_size;
    uint64_t current_position = region.start_samples;
    
    while (samples_rendered < total_samples && !job->cancel_requested.load()) {
        uint32_t samples_to_render = static_cast<uint32_t>(
            std::min(static_cast<uint64_t>(chunk_size), total_samples - samples_rendered));
        
        // Create temporary buffer for this chunk
        auto chunk_buffer = std::make_shared<AudioBuffer>(2, samples_to_render);
        
        // Process this chunk through the mixer
        auto process_result = m_mixer_engine->process_audio_block(
            current_position, current_position + samples_to_render, chunk_buffer);
        
        if (!process_result.is_ok()) {
            return Error("Failed to process audio block: " + process_result.error().message);
        }
        
        // Copy chunk to output buffer
        output_buffer->copy_from(*chunk_buffer, samples_to_render, samples_rendered);
        
        samples_rendered += samples_to_render;
        current_position += samples_to_render;
        
        // Update progress
        double progress = 10.0 + (static_cast<double>(samples_rendered) / total_samples) * 40.0;  // 10-50% for rendering
        update_job_progress(job, progress, "Rendering audio (" + 
            std::to_string(samples_rendered) + "/" + std::to_string(total_samples) + " samples)");
        
        // Check memory usage periodically
        if ((samples_rendered % (chunk_size * 10)) == 0) {
            if (!check_memory_usage()) {
                return Error("Memory usage exceeded limit");
            }
        }
    }
    
    if (job->cancel_requested.load()) {
        return Error("Render cancelled");
    }
    
    return Ok(output_buffer);
}

Result<bool> RenderEngine::apply_render_processing(
    std::shared_ptr<AudioBuffer> buffer,
    const RenderProcessingSettings& settings,
    std::shared_ptr<RenderJob> job) {
    
    // Apply resampling if needed
    if (settings.enable_resampling && settings.output_sample_rate != 44100) {
        auto resample_result = m_resample_processor->configure(
            44100, settings.output_sample_rate, buffer->get_channel_count(), settings.resampling_quality);
        if (!resample_result.is_ok()) {
            return Error("Failed to configure resampler: " + resample_result.error().message);
        }
        
        auto resampled_result = m_resample_processor->process(buffer);
        if (!resampled_result.is_ok()) {
            return Error("Failed to resample audio: " + resampled_result.error().message);
        }
        
        buffer = resampled_result.unwrap();
    }
    
    // Apply loudness normalization
    if (settings.loudness_standard != LoudnessStandard::NONE) {
        double target_lufs = settings.custom_lufs_target;
        
        // Set target based on standard
        switch (settings.loudness_standard) {
            case LoudnessStandard::EBU_R128_23: target_lufs = -23.0; break;
            case LoudnessStandard::EBU_R128_16: target_lufs = -16.0; break;
            case LoudnessStandard::ATSC_A85_24: target_lufs = -24.0; break;
            case LoudnessStandard::SPOTIFY_14: target_lufs = -14.0; break;
            case LoudnessStandard::YOUTUBE_14: target_lufs = -14.0; break;
            case LoudnessStandard::APPLE_MUSIC_16: target_lufs = -16.0; break;
            case LoudnessStandard::TIDAL_14: target_lufs = -14.0; break;
            default: break;
        }
        
        auto normalize_result = apply_loudness_normalization(buffer, settings.loudness_standard, target_lufs, job);
        if (!normalize_result.is_ok()) {
            return Error("Failed to apply loudness normalization: " + normalize_result.error().message);
        }
    }
    
    // Apply true peak limiting
    if (settings.true_peak_limiting) {
        auto limit_result = apply_true_peak_limiting(buffer, settings.max_true_peak_dbfs, job);
        if (!limit_result.is_ok()) {
            return Error("Failed to apply true peak limiting: " + limit_result.error().message);
        }
    }
    
    // Apply standard limiting
    if (settings.enable_limiter) {
        // Simple brick-wall limiting
        double threshold_linear = std::pow(10.0, settings.limiter_threshold_dbfs / 20.0);
        
        for (uint32_t ch = 0; ch < buffer->get_channel_count(); ++ch) {
            auto channel_data = buffer->get_channel_data(ch);
            for (uint32_t sample = 0; sample < buffer->get_buffer_size(); ++sample) {
                if (std::abs(channel_data[sample]) > threshold_linear) {
                    channel_data[sample] = channel_data[sample] > 0 ? threshold_linear : -threshold_linear;
                }
            }
        }
    }
    
    return Ok(true);
}

Result<bool> RenderEngine::write_audio_file(
    std::shared_ptr<AudioBuffer> buffer,
    const std::string& file_path,
    AudioFormat format,
    const RenderJobConfig::Metadata& metadata) {
    
    auto writer_result = create_audio_writer(format, file_path);
    if (!writer_result.is_ok()) {
        return Error("Failed to create audio writer: " + writer_result.error().message);
    }
    
    auto writer = writer_result.unwrap();
    
    // Open file for writing
    auto open_result = writer->open(file_path, buffer->get_channel_count(), 44100, format);
    if (!open_result.is_ok()) {
        return Error("Failed to open output file: " + open_result.error().message);
    }
    
    // Convert buffer to channel vectors
    std::vector<std::vector<double>> channel_data;
    channel_data.resize(buffer->get_channel_count());
    
    for (uint32_t ch = 0; ch < buffer->get_channel_count(); ++ch) {
        channel_data[ch].resize(buffer->get_buffer_size());
        auto source_data = buffer->get_channel_data(ch);
        std::copy(source_data, source_data + buffer->get_buffer_size(), channel_data[ch].begin());
    }
    
    // Write samples
    auto write_result = writer->write_samples(channel_data, buffer->get_buffer_size());
    if (!write_result.is_ok()) {
        return Error("Failed to write samples: " + write_result.error().message);
    }
    
    // Write metadata
    auto metadata_result = writer->write_metadata(metadata);
    if (!metadata_result.is_ok()) {
        // Non-fatal error for metadata
    }
    
    // Close file
    auto close_result = writer->close();
    if (!close_result.is_ok()) {
        return Error("Failed to close output file: " + close_result.error().message);
    }
    
    return Ok(true);
}

Result<std::unique_ptr<AudioFileWriter>> RenderEngine::create_audio_writer(AudioFormat format, const std::string& file_path) {
    switch (format) {
        case AudioFormat::WAV_PCM_16:
        case AudioFormat::WAV_PCM_24:
        case AudioFormat::WAV_PCM_32:
        case AudioFormat::WAV_FLOAT_32:
            return Ok(std::make_unique<WAVFileWriter>());
            
        case AudioFormat::AIFF_PCM_16:
        case AudioFormat::AIFF_PCM_24:
        case AudioFormat::AIFF_FLOAT_32:
            return Ok(std::make_unique<AIFFFileWriter>());
            
        default:
            return Error("Unsupported audio format");
    }
}

// Utility function implementations

uint32_t RenderEngine::generate_job_id() {
    return m_next_job_id++;
}

std::string RenderEngine::generate_output_filename(const RenderJobConfig& config, const std::string& track_name) {
    // Use filename template processor
    std::map<std::string, std::string> variables = FilenameTemplateProcessor::create_default_variables(
        "Project", track_name, config.audio_format);
    
    std::string base_filename = FilenameTemplateProcessor::process_template(config.filename_template, variables);
    std::string extension = AudioFormatUtils::get_file_extension(config.audio_format);
    
    std::filesystem::path output_path(config.output_path);
    output_path /= (base_filename + extension);
    
    return output_path.string();
}

std::string RenderEngine::create_render_log_entry(const std::string& operation, const std::string& details) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream log_entry;
    log_entry << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
              << operation;
    
    if (!details.empty()) {
        log_entry << ": " << details;
    }
    
    log_entry << "\n";
    return log_entry.str();
}

void RenderEngine::update_job_progress(std::shared_ptr<RenderJob> job, double progress, const std::string& operation) {
    if (!job) return;
    
    job->progress.progress_percent = std::clamp(progress, 0.0, 100.0);
    job->progress.current_operation = operation;
    
    if (m_progress_callback) {
        notify_progress_callback(job->progress);
    }
}

void RenderEngine::notify_progress_callback(const RenderProgress& progress) {
    if (m_progress_callback) {
        try {
            m_progress_callback(progress);
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

void RenderEngine::notify_completion_callback(const RenderResult& result) {
    if (m_completion_callback) {
        try {
            m_completion_callback(result);
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

bool RenderEngine::check_memory_usage() {
    // Simple memory check - could be more sophisticated
    return true;  // For now, always pass
}

} // namespace mixmind