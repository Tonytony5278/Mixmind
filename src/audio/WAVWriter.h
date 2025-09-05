#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>
#include "../core/types.h"

namespace mixmind::audio {

/**
 * Simple WAV file writer for audio export
 * Supports 16-bit and 32-bit PCM formats
 */
class WAVWriter {
public:
    enum class BitDepth {
        Bit16 = 16,
        Bit32 = 32
    };

    struct WAVHeader {
        // RIFF header
        char chunkID[4];           // "RIFF"
        uint32_t chunkSize;        // File size - 8 bytes
        char format[4];            // "WAVE"
        
        // Format sub-chunk
        char subchunk1ID[4];       // "fmt "
        uint32_t subchunk1Size;    // 16 for PCM
        uint16_t audioFormat;      // 1 for PCM
        uint16_t numChannels;      // Mono = 1, Stereo = 2
        uint32_t sampleRate;       // Sample rate
        uint32_t byteRate;         // Bytes per second
        uint16_t blockAlign;       // Bytes per sample frame
        uint16_t bitsPerSample;    // Bits per sample
        
        // Data sub-chunk
        char subchunk2ID[4];       // "data"
        uint32_t subchunk2Size;    // Data size in bytes
    };

    WAVWriter() = default;
    ~WAVWriter() = default;

    /**
     * Write floating point audio buffer to WAV file
     */
    bool writeWAV(const std::string& filename, 
                  const FloatAudioBuffer& buffer, 
                  int sampleRate,
                  BitDepth bitDepth = BitDepth::Bit16);

    /**
     * Write interleaved float array to WAV file
     */
    bool writeWAV(const std::string& filename,
                  const float* data,
                  int numChannels,
                  int numSamples,
                  int sampleRate,
                  BitDepth bitDepth = BitDepth::Bit16);

    /**
     * Get last error message
     */
    const std::string& getLastError() const { return lastError_; }

    /**
     * Utility: Convert float samples to 16-bit PCM
     */
    static std::vector<int16_t> floatTo16Bit(const float* data, size_t numSamples);

    /**
     * Utility: Convert float samples to 32-bit PCM  
     */
    static std::vector<int32_t> floatTo32Bit(const float* data, size_t numSamples);

    /**
     * Create a simple test tone (for validation)
     */
    static FloatAudioBuffer generateTestTone(int sampleRate, double frequency, double duration, int channels = 2);

private:
    std::string lastError_;

    WAVHeader createHeader(int numChannels, int numSamples, int sampleRate, BitDepth bitDepth);
    bool writeHeader(std::ofstream& file, const WAVHeader& header);
    bool writeData16Bit(std::ofstream& file, const float* data, int numChannels, int numSamples);
    bool writeData32Bit(std::ofstream& file, const float* data, int numChannels, int numSamples);
};

} // namespace mixmind::audio