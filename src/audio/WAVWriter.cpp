#include "WAVWriter.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mixmind::audio {

bool WAVWriter::writeWAV(const std::string& filename, 
                         const FloatAudioBuffer& buffer, 
                         int sampleRate,
                         BitDepth bitDepth) {
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) {
        lastError_ = "Empty audio buffer";
        return false;
    }

    return writeWAV(filename, 
                    buffer.getData(), 
                    buffer.getNumChannels(),
                    buffer.getNumSamples(),
                    sampleRate,
                    bitDepth);
}

bool WAVWriter::writeWAV(const std::string& filename,
                         const float* data,
                         int numChannels,
                         int numSamples,
                         int sampleRate,
                         BitDepth bitDepth) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        lastError_ = "Cannot create file: " + filename;
        return false;
    }

    // Create and write header
    WAVHeader header = createHeader(numChannels, numSamples, sampleRate, bitDepth);
    if (!writeHeader(file, header)) {
        return false;
    }

    // Write audio data
    bool success = false;
    if (bitDepth == BitDepth::Bit16) {
        success = writeData16Bit(file, data, numChannels, numSamples);
    } else {
        success = writeData32Bit(file, data, numChannels, numSamples);
    }

    file.close();
    return success;
}

WAVWriter::WAVHeader WAVWriter::createHeader(int numChannels, int numSamples, int sampleRate, BitDepth bitDepth) {
    WAVHeader header = {};

    // RIFF header
    std::memcpy(header.chunkID, "RIFF", 4);
    std::memcpy(header.format, "WAVE", 4);

    // Format sub-chunk
    std::memcpy(header.subchunk1ID, "fmt ", 4);
    header.subchunk1Size = 16;  // PCM format size
    header.audioFormat = 1;     // PCM
    header.numChannels = static_cast<uint16_t>(numChannels);
    header.sampleRate = static_cast<uint32_t>(sampleRate);
    header.bitsPerSample = static_cast<uint16_t>(bitDepth);
    header.blockAlign = static_cast<uint16_t>(numChannels * static_cast<int>(bitDepth) / 8);
    header.byteRate = sampleRate * header.blockAlign;

    // Data sub-chunk
    std::memcpy(header.subchunk2ID, "data", 4);
    header.subchunk2Size = numSamples * header.blockAlign;
    header.chunkSize = 36 + header.subchunk2Size;

    return header;
}

bool WAVWriter::writeHeader(std::ofstream& file, const WAVHeader& header) {
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!file.good()) {
        lastError_ = "Failed to write WAV header";
        return false;
    }
    return true;
}

bool WAVWriter::writeData16Bit(std::ofstream& file, const float* data, int numChannels, int numSamples) {
    auto samples16 = floatTo16Bit(data, numSamples * numChannels);
    
    file.write(reinterpret_cast<const char*>(samples16.data()), samples16.size() * sizeof(int16_t));
    if (!file.good()) {
        lastError_ = "Failed to write 16-bit audio data";
        return false;
    }
    return true;
}

bool WAVWriter::writeData32Bit(std::ofstream& file, const float* data, int numChannels, int numSamples) {
    auto samples32 = floatTo32Bit(data, numSamples * numChannels);
    
    file.write(reinterpret_cast<const char*>(samples32.data()), samples32.size() * sizeof(int32_t));
    if (!file.good()) {
        lastError_ = "Failed to write 32-bit audio data";
        return false;
    }
    return true;
}

std::vector<int16_t> WAVWriter::floatTo16Bit(const float* data, size_t numSamples) {
    std::vector<int16_t> result(numSamples);
    
    for (size_t i = 0; i < numSamples; ++i) {
        // Clamp to [-1.0, 1.0] and convert to 16-bit
        float sample = std::clamp(data[i], -1.0f, 1.0f);
        result[i] = static_cast<int16_t>(sample * 32767.0f);
    }
    
    return result;
}

std::vector<int32_t> WAVWriter::floatTo32Bit(const float* data, size_t numSamples) {
    std::vector<int32_t> result(numSamples);
    
    for (size_t i = 0; i < numSamples; ++i) {
        // Clamp to [-1.0, 1.0] and convert to 32-bit
        float sample = std::clamp(data[i], -1.0f, 1.0f);
        result[i] = static_cast<int32_t>(sample * 2147483647.0f);
    }
    
    return result;
}

FloatAudioBuffer WAVWriter::generateTestTone(int sampleRate, double frequency, double duration, int channels) {
    int numSamples = static_cast<int>(duration * sampleRate);
    FloatAudioBuffer buffer(channels, numSamples);
    
    for (int i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        float sample = static_cast<float>(0.3 * std::sin(2.0 * M_PI * frequency * t));
        
        for (int ch = 0; ch < channels; ++ch) {
            buffer.setSample(i, ch, sample);
        }
    }
    
    return buffer;
}

} // namespace mixmind::audio