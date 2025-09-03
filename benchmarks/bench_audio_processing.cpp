#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>
#include <algorithm>

// Mock audio processing functions for benchmarking
namespace mixmind::audio {
    
    using AudioBuffer = std::vector<float>;
    using AudioFrame = std::vector<AudioBuffer>;  // Multi-channel frame
    
    // Simple gain processing
    void applyGain(AudioBuffer& buffer, float gain) {
        for (float& sample : buffer) {
            sample *= gain;
        }
    }
    
    // Basic FIR filter implementation
    class SimpleFilter {
        std::vector<float> coeffs_;
        std::vector<float> delay_line_;
        size_t delay_index_ = 0;
        
    public:
        SimpleFilter(const std::vector<float>& coeffs) : coeffs_(coeffs) {
            delay_line_.resize(coeffs.size(), 0.0f);
        }
        
        float process(float input) {
            delay_line_[delay_index_] = input;
            
            float output = 0.0f;
            size_t idx = delay_index_;
            for (float coeff : coeffs_) {
                output += coeff * delay_line_[idx];
                idx = (idx == 0) ? delay_line_.size() - 1 : idx - 1;
            }
            
            delay_index_ = (delay_index_ + 1) % delay_line_.size();
            return output;
        }
        
        void processBlock(AudioBuffer& buffer) {
            for (float& sample : buffer) {
                sample = process(sample);
            }
        }
    };
    
    // Simple reverb simulation (delay + feedback)
    class SimpleReverb {
        std::vector<float> delay_buffer_;
        size_t write_pos_ = 0;
        size_t delay_samples_;
        float feedback_;
        float wet_level_;
        
    public:
        SimpleReverb(size_t delay_samples, float feedback = 0.3f, float wet_level = 0.2f)
            : delay_samples_(delay_samples), feedback_(feedback), wet_level_(wet_level) {
            delay_buffer_.resize(delay_samples, 0.0f);
        }
        
        float process(float input) {
            size_t read_pos = (write_pos_ + delay_buffer_.size() - delay_samples_) % delay_buffer_.size();
            float delayed = delay_buffer_[read_pos];
            
            delay_buffer_[write_pos_] = input + delayed * feedback_;
            write_pos_ = (write_pos_ + 1) % delay_buffer_.size();
            
            return input + delayed * wet_level_;
        }
        
        void processBlock(AudioBuffer& buffer) {
            for (float& sample : buffer) {
                sample = process(sample);
            }
        }
    };
    
    // Generate test audio data
    AudioBuffer generateSineWave(size_t samples, float frequency, float sample_rate = 44100.0f) {
        AudioBuffer buffer(samples);
        for (size_t i = 0; i < samples; ++i) {
            buffer[i] = std::sin(2.0f * M_PI * frequency * i / sample_rate);
        }
        return buffer;
    }
    
    AudioBuffer generateNoise(size_t samples, float amplitude = 0.1f) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        
        AudioBuffer buffer(samples);
        for (float& sample : buffer) {
            sample = dist(gen) * amplitude;
        }
        return buffer;
    }
    
    // Mix multiple audio buffers
    AudioBuffer mixBuffers(const std::vector<AudioBuffer>& buffers) {
        if (buffers.empty()) return {};
        
        AudioBuffer result(buffers[0].size(), 0.0f);
        for (const auto& buffer : buffers) {
            for (size_t i = 0; i < std::min(result.size(), buffer.size()); ++i) {
                result[i] += buffer[i];
            }
        }
        
        // Normalize to prevent clipping
        float max_val = *std::max_element(result.begin(), result.end(),
            [](float a, float b) { return std::abs(a) < std::abs(b); });
        
        if (max_val > 1.0f) {
            float scale = 1.0f / max_val;
            applyGain(result, scale);
        }
        
        return result;
    }
}

using namespace mixmind::audio;

// Benchmark test cases
TEST_CASE("Audio Processing Benchmarks", "[benchmark][audio]") {
    
    // Standard audio block sizes for benchmarking
    constexpr size_t SMALL_BLOCK = 64;    // Low latency
    constexpr size_t MEDIUM_BLOCK = 512;  // Typical DAW block size
    constexpr size_t LARGE_BLOCK = 2048;  // High throughput
    
    SECTION("Gain processing performance") {
        auto testBuffer = generateSineWave(MEDIUM_BLOCK, 440.0f);
        
        BENCHMARK("Gain - Small block (64 samples)") {
            auto buffer = generateSineWave(SMALL_BLOCK, 440.0f);
            applyGain(buffer, 0.8f);
            return buffer[0];  // Prevent optimization
        };
        
        BENCHMARK("Gain - Medium block (512 samples)") {
            auto buffer = generateSineWave(MEDIUM_BLOCK, 440.0f);
            applyGain(buffer, 0.8f);
            return buffer[0];
        };
        
        BENCHMARK("Gain - Large block (2048 samples)") {
            auto buffer = generateSineWave(LARGE_BLOCK, 440.0f);
            applyGain(buffer, 0.8f);
            return buffer[0];
        };
    }
    
    SECTION("Filter processing performance") {
        // Simple low-pass filter coefficients
        std::vector<float> lpf_coeffs = {0.1f, 0.2f, 0.4f, 0.2f, 0.1f};
        SimpleFilter filter(lpf_coeffs);
        
        BENCHMARK("FIR Filter - Small block") {
            auto buffer = generateNoise(SMALL_BLOCK);
            filter.processBlock(buffer);
            return buffer[0];
        };
        
        BENCHMARK("FIR Filter - Medium block") {
            auto buffer = generateNoise(MEDIUM_BLOCK);
            filter.processBlock(buffer);
            return buffer[0];
        };
        
        BENCHMARK("FIR Filter - Large block") {
            auto buffer = generateNoise(LARGE_BLOCK);
            filter.processBlock(buffer);
            return buffer[0];
        };
    }
    
    SECTION("Reverb processing performance") {
        SimpleReverb reverb(1024);  // ~23ms delay at 44.1kHz
        
        BENCHMARK("Reverb - Small block") {
            auto buffer = generateSineWave(SMALL_BLOCK, 1000.0f);
            reverb.processBlock(buffer);
            return buffer[0];
        };
        
        BENCHMARK("Reverb - Medium block") {
            auto buffer = generateSineWave(MEDIUM_BLOCK, 1000.0f);
            reverb.processBlock(buffer);
            return buffer[0];
        };
        
        BENCHMARK("Reverb - Large block") {
            auto buffer = generateSineWave(LARGE_BLOCK, 1000.0f);
            reverb.processBlock(buffer);
            return buffer[0];
        };
    }
    
    SECTION("Buffer mixing performance") {
        // Create multiple audio sources to mix
        auto createSources = [](size_t blockSize) {
            return std::vector<AudioBuffer>{
                generateSineWave(blockSize, 220.0f),    // A3
                generateSineWave(blockSize, 440.0f),    // A4
                generateSineWave(blockSize, 880.0f),    // A5
                generateNoise(blockSize, 0.1f),         // Background noise
            };
        };
        
        BENCHMARK("Mix 4 sources - Small block") {
            auto sources = createSources(SMALL_BLOCK);
            auto result = mixBuffers(sources);
            return result[0];
        };
        
        BENCHMARK("Mix 4 sources - Medium block") {
            auto sources = createSources(MEDIUM_BLOCK);
            auto result = mixBuffers(sources);
            return result[0];
        };
        
        BENCHMARK("Mix 4 sources - Large block") {
            auto sources = createSources(LARGE_BLOCK);
            auto result = mixBuffers(sources);
            return result[0];
        };
    }
    
    SECTION("Memory allocation benchmarks") {
        BENCHMARK("Vector allocation - Small") {
            AudioBuffer buffer(SMALL_BLOCK);
            std::fill(buffer.begin(), buffer.end(), 0.5f);
            return buffer.size();
        };
        
        BENCHMARK("Vector allocation - Medium") {
            AudioBuffer buffer(MEDIUM_BLOCK);
            std::fill(buffer.begin(), buffer.end(), 0.5f);
            return buffer.size();
        };
        
        BENCHMARK("Vector allocation - Large") {
            AudioBuffer buffer(LARGE_BLOCK);
            std::fill(buffer.begin(), buffer.end(), 0.5f);
            return buffer.size();
        };
        
        BENCHMARK("Pre-allocated buffer reuse") {
            static AudioBuffer reusable_buffer(MEDIUM_BLOCK);
            std::fill(reusable_buffer.begin(), reusable_buffer.end(), 0.7f);
            return reusable_buffer.size();
        };
    }
    
    SECTION("Multi-channel processing") {
        auto createStereoData = [](size_t samples) {
            return AudioFrame{
                generateSineWave(samples, 440.0f),  // Left channel
                generateSineWave(samples, 880.0f)   // Right channel
            };
        };
        
        BENCHMARK("Stereo gain processing") {
            auto stereo = createStereoData(MEDIUM_BLOCK);
            applyGain(stereo[0], 0.8f);  // Left
            applyGain(stereo[1], 0.8f);  // Right
            return stereo[0][0];
        };
        
        BENCHMARK("Stereo filtering") {
            auto stereo = createStereoData(MEDIUM_BLOCK);
            std::vector<float> coeffs = {0.25f, 0.5f, 0.25f};
            SimpleFilter leftFilter(coeffs), rightFilter(coeffs);
            
            leftFilter.processBlock(stereo[0]);
            rightFilter.processBlock(stereo[1]);
            return stereo[0][0];
        };
    }
}

TEST_CASE("Real-time Performance Tests", "[benchmark][realtime]") {
    // Tests that simulate real-time constraints
    
    SECTION("Real-time block processing simulation") {
        constexpr size_t BLOCK_SIZE = 128;  // Common low-latency block size
        constexpr double SAMPLE_RATE = 44100.0;
        constexpr double BLOCK_TIME_MS = (BLOCK_SIZE / SAMPLE_RATE) * 1000.0;  // ~2.9ms
        
        auto processBlock = [](AudioBuffer& buffer) {
            // Simulate typical DAW processing chain
            applyGain(buffer, 0.8f);  // Volume adjustment
            
            SimpleFilter hpf({-0.1f, 0.0f, 0.9f, 0.0f, -0.1f});  // High-pass
            hpf.processBlock(buffer);
            
            SimpleReverb reverb(64, 0.2f, 0.15f);  // Short reverb
            reverb.processBlock(buffer);
        };
        
        BENCHMARK("RT Block Processing Chain") {
            auto buffer = generateNoise(BLOCK_SIZE);
            
            auto start = std::chrono::high_resolution_clock::now();
            processBlock(buffer);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
            
            // Assert we're well under the real-time deadline
            REQUIRE(duration_ms < BLOCK_TIME_MS * 0.5);  // Use < 50% of available time
            
            return buffer[0];
        };
    }
    
    SECTION("CPU cache efficiency tests") {
        // Test memory access patterns for cache efficiency
        
        BENCHMARK("Sequential memory access") {
            AudioBuffer buffer(4096);
            for (size_t i = 0; i < buffer.size(); ++i) {
                buffer[i] = std::sin(i * 0.1f);
            }
            return buffer.back();
        };
        
        BENCHMARK("Strided memory access") {
            AudioBuffer buffer(4096);
            for (size_t i = 0; i < buffer.size(); i += 8) {  // Every 8th sample
                buffer[i] = std::sin(i * 0.1f);
            }
            return buffer[buffer.size() - 8];
        };
        
        BENCHMARK("Random memory access") {
            AudioBuffer buffer(4096);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dist(0, buffer.size() - 1);
            
            for (int i = 0; i < 512; ++i) {  // 512 random accesses
                size_t idx = dist(gen);
                buffer[idx] = std::sin(i * 0.1f);
            }
            return buffer[0];
        };
    }
}