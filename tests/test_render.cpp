#include <gtest/gtest.h>
#include "../src/render/RenderEngine.h"
#include "../src/render/RenderTypes.h"
#include "../src/audio/AudioBuffer.h"
#include <memory>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace mixmind {

// Mock MixerEngine for testing
class MockMixerEngine {
public:
    MockMixerEngine() = default;
    ~MockMixerEngine() = default;
    
    Result<bool> process_audio_block(uint64_t start_samples, uint64_t end_samples, 
                                    std::shared_ptr<AudioBuffer> output_buffer) {
        // Generate test tone for rendering tests
        generate_test_tone(output_buffer, 1000.0, 0.5, end_samples - start_samples);
        return Ok(true);
    }
    
    std::vector<uint32_t> get_all_track_ids() const {
        return {1, 2, 3, 4};  // Mock track IDs
    }
    
    std::string get_track_name(uint32_t track_id) const {
        return "Track_" + std::to_string(track_id);
    }
    
private:
    void generate_test_tone(std::shared_ptr<AudioBuffer> buffer, double frequency, 
                           double amplitude, uint32_t samples) {
        double sample_rate = 44100.0;
        double phase_increment = 2.0 * M_PI * frequency / sample_rate;
        
        for (uint32_t ch = 0; ch < buffer->get_channel_count(); ++ch) {
            auto channel_data = buffer->get_channel_data(ch);
            for (uint32_t sample = 0; sample < samples; ++sample) {
                double phase = sample * phase_increment;
                channel_data[sample] = amplitude * std::sin(phase);
            }
        }
    }
};

class RenderSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock mixer engine
        mock_mixer = std::make_shared<MockMixerEngine>();
        
        // Create render engine
        render_engine = std::make_unique<RenderEngine>();
        auto init_result = render_engine->initialize(mock_mixer);
        ASSERT_TRUE(init_result.is_ok());
        
        // Create test output directory
        test_output_dir = std::filesystem::temp_directory_path() / "mixmind_render_test";
        std::filesystem::create_directories(test_output_dir);
    }
    
    void TearDown() override {
        render_engine->shutdown();
        render_engine.reset();
        
        // Clean up test files
        if (std::filesystem::exists(test_output_dir)) {
            std::filesystem::remove_all(test_output_dir);
        }
    }
    
    RenderJobConfig create_basic_config() {
        RenderJobConfig config;
        config.output_path = test_output_dir.string();
        config.filename_template = "test_{timestamp}";
        config.audio_format = AudioFormat::WAV_PCM_24;
        config.quality = RenderQuality::STANDARD;
        config.mode = RenderMode::OFFLINE;
        config.target.type = RenderTarget::MASTER_MIX;
        config.region = RenderRegion(0, 44100);  // 1 second at 44.1kHz
        return config;
    }
    
    bool wait_for_job_completion(uint32_t job_id, int timeout_seconds = 30) {
        auto start_time = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(timeout_seconds)) {
            if (render_engine->is_job_completed(job_id)) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        return false;
    }
    
    bool file_exists_and_has_content(const std::string& file_path) {
        if (!std::filesystem::exists(file_path)) {
            return false;
        }
        
        auto file_size = std::filesystem::file_size(file_path);
        return file_size > 44;  // Minimum size for WAV header + some data
    }
    
    std::unique_ptr<RenderEngine> render_engine;
    std::shared_ptr<MockMixerEngine> mock_mixer;
    std::filesystem::path test_output_dir;
};

// Test 1: RenderEngine initialization and shutdown
TEST_F(RenderSystemTest, RenderEngineInitialization) {
    EXPECT_TRUE(render_engine->is_initialized());
    
    // Test engine statistics
    auto stats = render_engine->get_engine_statistics();
    EXPECT_EQ(stats.total_jobs_processed, 0);
    EXPECT_EQ(stats.active_jobs, 0);
    EXPECT_EQ(stats.failed_jobs, 0);
    
    // Test supported formats
    auto formats = render_engine->get_supported_formats();
    EXPECT_GT(formats.size(), 0);
    
    EXPECT_TRUE(render_engine->is_format_supported(AudioFormat::WAV_PCM_24));
    EXPECT_TRUE(render_engine->is_format_supported(AudioFormat::AIFF_PCM_16));
    
    // Test format info
    auto format_info = render_engine->get_format_info(AudioFormat::WAV_PCM_24);
    EXPECT_TRUE(format_info.is_ok());
}

// Test 2: Basic master mix render
TEST_F(RenderSystemTest, BasicMasterMixRender) {
    auto config = create_basic_config();
    
    // Submit render job
    auto job_result = render_engine->submit_render_job(config);
    ASSERT_TRUE(job_result.is_ok());
    
    uint32_t job_id = job_result.unwrap();
    EXPECT_GT(job_id, 0);
    
    // Wait for completion
    ASSERT_TRUE(wait_for_job_completion(job_id));
    
    // Check job result
    auto result = render_engine->get_render_result(job_id);
    ASSERT_TRUE(result.is_ok());
    
    auto render_result = result.unwrap();
    EXPECT_TRUE(render_result.success);
    EXPECT_FALSE(render_result.output_file_path.empty());
    EXPECT_GT(render_result.total_render_time_seconds, 0.0);
    
    // Verify output file exists
    EXPECT_TRUE(file_exists_and_has_content(render_result.output_file_path));
}

// Test 3: Stems rendering
TEST_F(RenderSystemTest, StemsRender) {
    auto config = create_basic_config();
    config.target.type = RenderTarget::STEMS;
    config.normalize_stems = true;
    config.separate_directories = true;
    
    // Submit stems render job
    auto job_result = render_engine->submit_render_job(config);
    ASSERT_TRUE(job_result.is_ok());
    
    uint32_t job_id = job_result.unwrap();
    
    // Wait for completion
    ASSERT_TRUE(wait_for_job_completion(job_id));
    
    // Check result
    auto result = render_engine->get_render_result(job_id);
    ASSERT_TRUE(result.is_ok());
    
    auto render_result = result.unwrap();
    EXPECT_TRUE(render_result.success);
    EXPECT_GT(render_result.stem_file_paths.size(), 0);
    
    // Verify all stem files exist
    for (const auto& stem_path : render_result.stem_file_paths) {
        EXPECT_TRUE(file_exists_and_has_content(stem_path));
    }
}

// Test 4: Different audio formats
TEST_F(RenderSystemTest, AudioFormatsRender) {
    std::vector<AudioFormat> formats_to_test = {
        AudioFormat::WAV_PCM_16,
        AudioFormat::WAV_PCM_24,
        AudioFormat::WAV_FLOAT_32,
        AudioFormat::AIFF_PCM_16,
        AudioFormat::AIFF_PCM_24
    };
    
    for (auto format : formats_to_test) {
        auto config = create_basic_config();
        config.audio_format = format;
        config.filename_template = "test_" + AudioFormatUtils::get_format_name(format) + "_{timestamp}";
        
        auto job_result = render_engine->submit_render_job(config);
        ASSERT_TRUE(job_result.is_ok()) << "Failed to submit job for format: " 
                                        << static_cast<int>(format);
        
        uint32_t job_id = job_result.unwrap();
        ASSERT_TRUE(wait_for_job_completion(job_id)) << "Job timeout for format: " 
                                                     << static_cast<int>(format);
        
        auto result = render_engine->get_render_result(job_id);
        ASSERT_TRUE(result.is_ok());
        
        auto render_result = result.unwrap();
        EXPECT_TRUE(render_result.success) << "Render failed for format: " 
                                           << static_cast<int>(format);
        EXPECT_TRUE(file_exists_and_has_content(render_result.output_file_path));
        
        // Verify file extension
        std::string expected_ext = AudioFormatUtils::get_file_extension(format);
        EXPECT_TRUE(render_result.output_file_path.ends_with(expected_ext));
    }
}

// Test 5: Loudness normalization
TEST_F(RenderSystemTest, LoudnessNormalization) {
    auto config = create_basic_config();
    config.processing.loudness_standard = LoudnessStandard::EBU_R128_23;
    config.processing.true_peak_limiting = true;
    config.processing.max_true_peak_dbfs = -1.0;
    
    auto job_result = render_engine->submit_render_job(config);
    ASSERT_TRUE(job_result.is_ok());
    
    uint32_t job_id = job_result.unwrap();
    ASSERT_TRUE(wait_for_job_completion(job_id));
    
    auto result = render_engine->get_render_result(job_id);
    ASSERT_TRUE(result.is_ok());
    
    auto render_result = result.unwrap();
    EXPECT_TRUE(render_result.success);
    EXPECT_TRUE(file_exists_and_has_content(render_result.output_file_path));
    
    // Check that analysis was performed
    EXPECT_GT(render_result.analysis.integrated_lufs, -50.0);  // Should have meaningful LUFS value
    EXPECT_LT(render_result.analysis.true_peak_dbfs, 0.0);     // Should be below 0 dBFS
}

// Test 6: Render progress monitoring
TEST_F(RenderSystemTest, RenderProgressMonitoring) {
    auto config = create_basic_config();
    config.region = RenderRegion(0, 44100 * 5);  // 5 seconds for longer render
    
    bool progress_received = false;
    bool completion_received = false;
    
    render_engine->set_progress_callback([&](const RenderProgress& progress) {
        progress_received = true;
        EXPECT_GE(progress.progress_percent, 0.0);
        EXPECT_LE(progress.progress_percent, 100.0);
        EXPECT_FALSE(progress.current_operation.empty());
    });
    
    render_engine->set_completion_callback([&](const RenderResult& result) {
        completion_received = true;
        EXPECT_TRUE(result.success);
    });
    
    auto job_result = render_engine->submit_render_job(config);
    ASSERT_TRUE(job_result.is_ok());
    
    uint32_t job_id = job_result.unwrap();
    ASSERT_TRUE(wait_for_job_completion(job_id));
    
    EXPECT_TRUE(progress_received);
    EXPECT_TRUE(completion_received);
}

// Test 7: Job cancellation
TEST_F(RenderSystemTest, JobCancellation) {
    auto config = create_basic_config();
    config.region = RenderRegion(0, 44100 * 10);  // 10 seconds for cancellation test
    
    auto job_result = render_engine->submit_render_job(config);
    ASSERT_TRUE(job_result.is_ok());
    
    uint32_t job_id = job_result.unwrap();
    
    // Wait a bit then cancel
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto cancel_result = render_engine->cancel_render_job(job_id);
    EXPECT_TRUE(cancel_result.is_ok());
    
    // Wait for job to complete (should be cancelled)
    ASSERT_TRUE(wait_for_job_completion(job_id, 10));
    
    auto result = render_engine->get_render_result(job_id);
    ASSERT_TRUE(result.is_ok());
    
    auto render_result = result.unwrap();
    EXPECT_FALSE(render_result.success);
    EXPECT_FALSE(render_result.error_message.empty());
}

// Test 8: Render quality settings
TEST_F(RenderSystemTest, RenderQualitySettings) {
    std::vector<RenderQuality> qualities = {
        RenderQuality::DRAFT,
        RenderQuality::STANDARD,
        RenderQuality::HIGH_QUALITY,
        RenderQuality::MASTERING
    };
    
    for (auto quality : qualities) {
        auto config = create_basic_config();
        config.quality = quality;
        config.filename_template = "quality_test_{timestamp}";
        
        auto job_result = render_engine->submit_render_job(config);
        ASSERT_TRUE(job_result.is_ok());
        
        uint32_t job_id = job_result.unwrap();
        ASSERT_TRUE(wait_for_job_completion(job_id));
        
        auto result = render_engine->get_render_result(job_id);
        ASSERT_TRUE(result.is_ok());
        
        auto render_result = result.unwrap();
        EXPECT_TRUE(render_result.success);
        EXPECT_TRUE(file_exists_and_has_content(render_result.output_file_path));
    }
}

// Test 9: Render presets
TEST_F(RenderSystemTest, RenderPresets) {
    auto presets = render_engine->get_builtin_presets();
    EXPECT_GT(presets.size(), 0);
    
    // Find and test a preset
    const RenderEngine::RenderPreset* high_quality_preset = nullptr;
    for (const auto& preset : presets) {
        if (preset.name == "High Quality Master") {
            high_quality_preset = &preset;
            break;
        }
    }
    
    ASSERT_NE(high_quality_preset, nullptr);
    
    // Use the preset configuration
    auto config = high_quality_preset->config;
    config.output_path = test_output_dir.string();
    config.region = RenderRegion(0, 44100);  // 1 second
    
    auto job_result = render_engine->submit_render_job(config);
    ASSERT_TRUE(job_result.is_ok());
    
    uint32_t job_id = job_result.unwrap();
    ASSERT_TRUE(wait_for_job_completion(job_id));
    
    auto result = render_engine->get_render_result(job_id);
    ASSERT_TRUE(result.is_ok());
    
    auto render_result = result.unwrap();
    EXPECT_TRUE(render_result.success);
    EXPECT_TRUE(file_exists_and_has_content(render_result.output_file_path));
}

// Test 10: WAV file writer functionality
TEST_F(RenderSystemTest, WAVFileWriterFunctionality) {
    WAVFileWriter writer;
    
    std::string test_file = (test_output_dir / "wav_writer_test.wav").string();
    
    // Open for writing
    auto open_result = writer.open(test_file, 2, 44100, AudioFormat::WAV_PCM_24);
    ASSERT_TRUE(open_result.is_ok());
    
    // Create test data
    std::vector<std::vector<double>> test_data(2);
    test_data[0].resize(1024);
    test_data[1].resize(1024);
    
    // Generate sine wave test data
    for (size_t i = 0; i < 1024; ++i) {
        double phase = 2.0 * M_PI * i / 1024.0;
        test_data[0][i] = 0.5 * std::sin(phase);
        test_data[1][i] = 0.5 * std::cos(phase);
    }
    
    // Write samples
    auto write_result = writer.write_samples(test_data, 1024);
    EXPECT_TRUE(write_result.is_ok());
    EXPECT_EQ(writer.get_samples_written(), 1024);
    
    // Close file
    auto close_result = writer.close();
    EXPECT_TRUE(close_result.is_ok());
    
    // Verify file exists and has expected size
    EXPECT_TRUE(std::filesystem::exists(test_file));
    auto file_size = std::filesystem::file_size(test_file);
    EXPECT_GT(file_size, 44);  // WAV header + data
    
    // Expected size: 44 bytes header + (1024 samples * 2 channels * 3 bytes/sample)
    EXPECT_EQ(file_size, 44 + (1024 * 2 * 3));
}

// Test 11: AIFF file writer functionality
TEST_F(RenderSystemTest, AIFFFileWriterFunctionality) {
    AIFFFileWriter writer;
    
    std::string test_file = (test_output_dir / "aiff_writer_test.aiff").string();
    
    // Open for writing
    auto open_result = writer.open(test_file, 2, 44100, AudioFormat::AIFF_PCM_16);
    ASSERT_TRUE(open_result.is_ok());
    
    // Create test data
    std::vector<std::vector<double>> test_data(2);
    test_data[0].resize(512);
    test_data[1].resize(512);
    
    // Generate test data
    for (size_t i = 0; i < 512; ++i) {
        double phase = 2.0 * M_PI * i / 512.0;
        test_data[0][i] = 0.3 * std::sin(phase);
        test_data[1][i] = 0.3 * std::sin(phase + M_PI / 4);  // Phase shifted
    }
    
    // Write samples
    auto write_result = writer.write_samples(test_data, 512);
    EXPECT_TRUE(write_result.is_ok());
    EXPECT_EQ(writer.get_samples_written(), 512);
    
    // Close file
    auto close_result = writer.close();
    EXPECT_TRUE(close_result.is_ok());
    
    // Verify file exists
    EXPECT_TRUE(std::filesystem::exists(test_file));
    auto file_size = std::filesystem::file_size(test_file);
    EXPECT_GT(file_size, 46);  // AIFF header + data
}

// Test 12: Audio format utilities
TEST_F(RenderSystemTest, AudioFormatUtilities) {
    // Test file extensions
    EXPECT_EQ(AudioFormatUtils::get_file_extension(AudioFormat::WAV_PCM_24), ".wav");
    EXPECT_EQ(AudioFormatUtils::get_file_extension(AudioFormat::AIFF_PCM_16), ".aiff");
    EXPECT_EQ(AudioFormatUtils::get_file_extension(AudioFormat::FLAC_24), ".flac");
    EXPECT_EQ(AudioFormatUtils::get_file_extension(AudioFormat::MP3_320), ".mp3");
    
    // Test format names
    EXPECT_FALSE(AudioFormatUtils::get_format_name(AudioFormat::WAV_PCM_24).empty());
    EXPECT_FALSE(AudioFormatUtils::get_format_name(AudioFormat::AIFF_FLOAT_32).empty());
    
    // Test bit depths
    EXPECT_EQ(AudioFormatUtils::get_bit_depth(AudioFormat::WAV_PCM_16), 16);
    EXPECT_EQ(AudioFormatUtils::get_bit_depth(AudioFormat::WAV_PCM_24), 24);
    EXPECT_EQ(AudioFormatUtils::get_bit_depth(AudioFormat::WAV_PCM_32), 32);
    
    // Test lossy format detection
    EXPECT_FALSE(AudioFormatUtils::is_lossy_format(AudioFormat::WAV_PCM_24));
    EXPECT_FALSE(AudioFormatUtils::is_lossy_format(AudioFormat::FLAC_16));
    EXPECT_TRUE(AudioFormatUtils::is_lossy_format(AudioFormat::MP3_320));
    EXPECT_TRUE(AudioFormatUtils::is_lossy_format(AudioFormat::AAC_256));
}

// Test 13: Filename template processor
TEST_F(RenderSystemTest, FilenameTemplateProcessor) {
    // Test basic template processing
    std::map<std::string, std::string> variables = {
        {"project", "TestProject"},
        {"track_name", "Master"},
        {"timestamp", "20241201_120000"},
        {"format", "WAV 24-bit PCM"}
    };
    
    std::string template_str = "{project}_{track_name}_{timestamp}";
    std::string result = FilenameTemplateProcessor::process_template(template_str, variables);
    EXPECT_EQ(result, "TestProject_Master_20241201_120000");
    
    // Test default variables creation
    auto default_vars = FilenameTemplateProcessor::create_default_variables(
        "MyProject", "Vocals", AudioFormat::WAV_PCM_24);
    
    EXPECT_EQ(default_vars["project"], "MyProject");
    EXPECT_EQ(default_vars["track_name"], "Vocals");
    EXPECT_FALSE(default_vars["timestamp"].empty());
    
    // Test filename sanitization
    std::string unsafe_name = "Track<1>: \"Test/File*Name\"";
    std::string safe_name = FilenameTemplateProcessor::sanitize_filename(unsafe_name);
    EXPECT_EQ(safe_name, "Track_1__ _Test_File_Name_");
    
    // Test timestamp generation
    std::string timestamp1 = FilenameTemplateProcessor::generate_timestamp_string();
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    std::string timestamp2 = FilenameTemplateProcessor::generate_timestamp_string();
    
    EXPECT_NE(timestamp1, timestamp2);
    EXPECT_EQ(timestamp1.length(), 15);  // YYYYMMDD_HHMMSS
}

// Test 14: Multiple concurrent renders
TEST_F(RenderSystemTest, MultipleConcurrentRenders) {
    const int num_jobs = 4;
    std::vector<uint32_t> job_ids;
    
    // Submit multiple jobs
    for (int i = 0; i < num_jobs; ++i) {
        auto config = create_basic_config();
        config.filename_template = "concurrent_test_" + std::to_string(i) + "_{timestamp}";
        
        auto job_result = render_engine->submit_render_job(config);
        ASSERT_TRUE(job_result.is_ok());
        job_ids.push_back(job_result.unwrap());
    }
    
    // Wait for all jobs to complete
    for (uint32_t job_id : job_ids) {
        ASSERT_TRUE(wait_for_job_completion(job_id));
        
        auto result = render_engine->get_render_result(job_id);
        ASSERT_TRUE(result.is_ok());
        
        auto render_result = result.unwrap();
        EXPECT_TRUE(render_result.success);
        EXPECT_TRUE(file_exists_and_has_content(render_result.output_file_path));
    }
    
    // Check engine statistics
    auto stats = render_engine->get_engine_statistics();
    EXPECT_EQ(stats.total_jobs_processed, num_jobs);
    EXPECT_EQ(stats.failed_jobs, 0);
}

// Test 15: Performance test - large render
TEST_F(RenderSystemTest, PerformanceLargeRender) {
    auto config = create_basic_config();
    config.region = RenderRegion(0, 44100 * 30);  // 30 seconds of audio
    config.filename_template = "performance_test_{timestamp}";
    
    auto start_time = std::chrono::steady_clock::now();
    
    auto job_result = render_engine->submit_render_job(config);
    ASSERT_TRUE(job_result.is_ok());
    
    uint32_t job_id = job_result.unwrap();
    ASSERT_TRUE(wait_for_job_completion(job_id, 60));  // 60 second timeout
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    auto result = render_engine->get_render_result(job_id);
    ASSERT_TRUE(result.is_ok());
    
    auto render_result = result.unwrap();
    EXPECT_TRUE(render_result.success);
    EXPECT_TRUE(file_exists_and_has_content(render_result.output_file_path));
    
    // Performance check - should render faster than real-time for offline mode
    double render_speed = 30.0 / render_result.total_render_time_seconds;  // Speed factor
    EXPECT_GT(render_speed, 1.0);  // Should be faster than real-time
    
    std::cout << "Performance test: 30s audio rendered in " 
              << render_result.total_render_time_seconds << "s "
              << "(Speed factor: " << render_speed << "x)" << std::endl;
}

} // namespace mixmind

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}