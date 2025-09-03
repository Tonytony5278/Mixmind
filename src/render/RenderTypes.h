#pragma once

#include "../core/result.h"
#include "../core/audio_types.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <functional>

namespace mixmind {

// Audio file format types
enum class AudioFormat {
    WAV_PCM_16,         // 16-bit PCM WAV
    WAV_PCM_24,         // 24-bit PCM WAV
    WAV_PCM_32,         // 32-bit PCM WAV
    WAV_FLOAT_32,       // 32-bit float WAV
    AIFF_PCM_16,        // 16-bit PCM AIFF
    AIFF_PCM_24,        // 24-bit PCM AIFF
    AIFF_FLOAT_32,      // 32-bit float AIFF
    FLAC_16,            // 16-bit FLAC lossless
    FLAC_24,            // 24-bit FLAC lossless
    MP3_128,            // MP3 128 kbps
    MP3_192,            // MP3 192 kbps
    MP3_320,            // MP3 320 kbps
    OGG_VORBIS_Q6,      // Ogg Vorbis quality 6
    AAC_128,            // AAC 128 kbps
    AAC_256             // AAC 256 kbps
};

// Render quality settings
enum class RenderQuality {
    DRAFT,              // Fast rendering for previews
    STANDARD,           // Normal quality rendering
    HIGH_QUALITY,       // High quality with oversampling
    MASTERING          // Maximum quality for final masters
};

// Render mode
enum class RenderMode {
    REAL_TIME,          // Real-time rendering (playback speed)
    OFFLINE,            // Offline rendering (faster than real-time)
    REAL_TIME_PREVIEW   // Real-time with monitoring
};

// Loudness normalization standards
enum class LoudnessStandard {
    NONE,               // No loudness normalization
    EBU_R128_23,        // EBU R128 -23 LUFS (broadcast)
    EBU_R128_16,        // EBU R128 -16 LUFS (streaming)
    ATSC_A85_24,        // ATSC A/85 -24 LKFS (US broadcast)
    SPOTIFY_14,         // Spotify -14 LUFS
    YOUTUBE_14,         // YouTube -14 LUFS
    APPLE_MUSIC_16,     // Apple Music -16 LUFS
    TIDAL_14,           // Tidal -14 LUFS
    CUSTOM              // User-defined target
};

// Render region definition
struct RenderRegion {
    uint64_t start_samples = 0;
    uint64_t end_samples = 0;
    bool loop_enabled = false;
    uint32_t loop_count = 1;
    
    RenderRegion() = default;
    RenderRegion(uint64_t start, uint64_t end) : start_samples(start), end_samples(end) {}
    
    uint64_t get_length_samples() const {
        return end_samples > start_samples ? end_samples - start_samples : 0;
    }
    
    double get_length_seconds(uint32_t sample_rate) const {
        return static_cast<double>(get_length_samples()) / sample_rate;
    }
};

// Render target configuration
struct RenderTarget {
    enum TargetType {
        MASTER_MIX,         // Full master mix
        STEMS,              // Individual stems/tracks
        SELECTED_TRACKS,    // Only selected tracks
        BUS_OUTPUT,         // Specific bus output
        CUSTOM_ROUTING      // Custom signal routing
    };
    
    TargetType type = MASTER_MIX;
    std::vector<uint32_t> track_ids;        // For SELECTED_TRACKS
    std::vector<uint32_t> bus_ids;          // For BUS_OUTPUT
    std::string custom_name = "";           // Custom naming
    bool include_effects = true;            // Include track/bus effects
    bool include_automation = true;         // Include automation
    
    RenderTarget(TargetType t = MASTER_MIX) : type(t) {}
};

// Audio processing settings for rendering
struct RenderProcessingSettings {
    // Resampling
    uint32_t output_sample_rate = 44100;
    bool enable_resampling = false;         // Auto-detect if needed
    int resampling_quality = 4;             // 1-10 scale
    
    // Bit depth conversion
    bool enable_dithering = true;
    enum DitheringType {
        TRIANGULAR_PDF,
        RECTANGULAR_PDF,
        NOISE_SHAPING
    } dithering_type = TRIANGULAR_PDF;
    
    // Dynamic range processing
    bool enable_limiter = false;
    double limiter_threshold_dbfs = -1.0;
    double limiter_release_ms = 10.0;
    bool limiter_isr = true;                // Inter-sample peaks
    
    // Loudness processing
    LoudnessStandard loudness_standard = LoudnessStandard::NONE;
    double custom_lufs_target = -23.0;
    bool true_peak_limiting = true;
    double max_true_peak_dbfs = -1.0;
    
    // EQ processing
    bool enable_master_eq = false;
    struct MasterEQ {
        bool high_pass_filter = false;
        double hpf_frequency = 20.0;
        bool low_pass_filter = false;
        double lpf_frequency = 20000.0;
        
        // Parametric EQ bands
        struct EQBand {
            bool enabled = false;
            double frequency = 1000.0;
            double gain_db = 0.0;
            double q_factor = 1.0;
            enum FilterType { BELL, HIGH_SHELF, LOW_SHELF } type = BELL;
        };
        
        std::vector<EQBand> eq_bands;
    } master_eq;
    
    // Analysis and metering
    bool generate_loudness_report = false;
    bool measure_true_peak = true;
    bool measure_dynamic_range = false;
    bool generate_spectrum_analysis = false;
};

// Render job configuration
struct RenderJobConfig {
    // Basic settings
    std::string output_path = "";
    std::string filename_template = "{project}_{timestamp}";
    AudioFormat audio_format = AudioFormat::WAV_PCM_24;
    RenderQuality quality = RenderQuality::STANDARD;
    RenderMode mode = RenderMode::OFFLINE;
    
    // What to render
    RenderTarget target;
    RenderRegion region;
    
    // Processing settings
    RenderProcessingSettings processing;
    
    // Metadata
    struct Metadata {
        std::string title = "";
        std::string artist = "";
        std::string album = "";
        std::string genre = "";
        std::string comment = "";
        uint32_t year = 0;
        uint32_t track_number = 0;
        std::string isrc = "";              // International Standard Recording Code
        std::map<std::string, std::string> custom_tags;
    } metadata;
    
    // Advanced options
    bool normalize_stems = false;           // Normalize each stem individually
    bool create_cue_sheet = false;          // Generate .cue file
    bool create_playlist = false;           // Generate .m3u playlist
    double tail_length_seconds = 5.0;      // Reverb tail length
    bool render_in_background = true;      // Background rendering
    
    // File naming for stems
    std::string stem_naming_pattern = "{track_name}_{timestamp}";
    bool separate_directories = true;       // Create subdirs for stems
};

// Render progress information
struct RenderProgress {
    enum Status {
        PREPARING,
        RENDERING,
        POST_PROCESSING,
        FINALIZING,
        COMPLETED,
        CANCELLED,
        ERROR
    };
    
    Status status = PREPARING;
    double progress_percent = 0.0;          // 0.0 - 100.0
    uint64_t samples_rendered = 0;
    uint64_t total_samples = 0;
    std::string current_operation = "";
    std::string estimated_time_remaining = "";
    
    // Performance metrics
    double rendering_speed_factor = 1.0;   // 1.0 = real-time, >1.0 = faster
    double cpu_usage_percent = 0.0;
    uint64_t memory_usage_bytes = 0;
    
    // Error information
    std::string error_message = "";
    std::vector<std::string> warnings;
    
    void reset() {
        status = PREPARING;
        progress_percent = 0.0;
        samples_rendered = 0;
        current_operation = "";
        estimated_time_remaining = "";
        error_message = "";
        warnings.clear();
    }
};

// Render statistics and analysis
struct RenderAnalysis {
    // Loudness measurements
    double integrated_lufs = -70.0;
    double momentary_lufs_max = -70.0;
    double short_term_lufs_max = -70.0;
    double loudness_range = 0.0;            // LRA
    double true_peak_dbfs = -70.0;
    
    // Dynamic range
    double dynamic_range_db = 0.0;          // DR measurement
    double crest_factor_db = 0.0;           // Peak-to-RMS ratio
    
    // Clipping detection
    uint32_t intersample_peaks = 0;
    uint32_t sample_peaks = 0;
    std::vector<uint64_t> clipping_positions;
    
    // Frequency analysis
    struct SpectrumAnalysis {
        std::vector<double> frequency_bins;
        std::vector<double> magnitude_db;
        double spectral_centroid = 0.0;
        double spectral_rolloff = 0.0;
    } spectrum;
    
    // File information
    uint64_t file_size_bytes = 0;
    double duration_seconds = 0.0;
    std::string format_info = "";
    
    void reset() {
        integrated_lufs = momentary_lufs_max = short_term_lufs_max = true_peak_dbfs = -70.0;
        loudness_range = dynamic_range_db = crest_factor_db = 0.0;
        intersample_peaks = sample_peaks = 0;
        clipping_positions.clear();
        spectrum = SpectrumAnalysis{};
        file_size_bytes = 0;
        duration_seconds = 0.0;
        format_info = "";
    }
};

// Render job result
struct RenderResult {
    bool success = false;
    std::string output_file_path = "";
    std::vector<std::string> stem_file_paths;
    RenderAnalysis analysis;
    std::string render_log = "";
    double total_render_time_seconds = 0.0;
    
    void reset() {
        success = false;
        output_file_path = "";
        stem_file_paths.clear();
        analysis.reset();
        render_log = "";
        total_render_time_seconds = 0.0;
    }
};

// Callback function types for render progress
using RenderProgressCallback = std::function<void(const RenderProgress&)>;
using RenderCompletionCallback = std::function<void(const RenderResult&)>;

// Audio format utility functions
class AudioFormatUtils {
public:
    static std::string get_file_extension(AudioFormat format);
    static std::string get_format_name(AudioFormat format);
    static uint32_t get_bit_depth(AudioFormat format);
    static bool is_lossy_format(AudioFormat format);
    static bool supports_metadata(AudioFormat format);
    static uint32_t get_max_sample_rate(AudioFormat format);
    static std::vector<uint32_t> get_supported_sample_rates(AudioFormat format);
    
    // Quality presets
    static AudioFormat get_format_for_quality(RenderQuality quality, bool lossless = true);
    static RenderProcessingSettings get_processing_for_quality(RenderQuality quality);
    static RenderProcessingSettings get_processing_for_standard(LoudnessStandard standard);
};

// Filename template processor
class FilenameTemplateProcessor {
public:
    // Template variables: {project}, {timestamp}, {track_name}, {format}, {quality}, etc.
    static std::string process_template(const std::string& template_str,
                                       const std::map<std::string, std::string>& variables);
    
    static std::map<std::string, std::string> create_default_variables(
        const std::string& project_name = "",
        const std::string& track_name = "",
        AudioFormat format = AudioFormat::WAV_PCM_24);
    
    static std::string generate_timestamp_string();
    static std::string sanitize_filename(const std::string& filename);
};

} // namespace mixmind