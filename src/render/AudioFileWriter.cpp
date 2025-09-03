#include "RenderEngine.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace mixmind {

// WAV File Writer Implementation

WAVFileWriter::~WAVFileWriter() {
    if (m_file_stream && m_file_stream->is_open()) {
        close();
    }
}

Result<bool> WAVFileWriter::open(const std::string& file_path, uint32_t channels, 
                                uint32_t sample_rate, AudioFormat format) {
    if (channels == 0 || channels > 32) {
        return Error("Invalid channel count: " + std::to_string(channels));
    }
    
    if (sample_rate < 8000 || sample_rate > 192000) {
        return Error("Invalid sample rate: " + std::to_string(sample_rate));
    }
    
    // Ensure output directory exists
    std::filesystem::path path(file_path);
    std::filesystem::create_directories(path.parent_path());
    
    m_file_path = file_path;
    m_channels = channels;
    m_sample_rate = sample_rate;
    m_format = format;
    m_samples_written = 0;
    
    // Determine bytes per sample
    switch (format) {
        case AudioFormat::WAV_PCM_16:
            m_bytes_per_sample = 2;
            break;
        case AudioFormat::WAV_PCM_24:
            m_bytes_per_sample = 3;
            break;
        case AudioFormat::WAV_PCM_32:
            m_bytes_per_sample = 4;
            break;
        case AudioFormat::WAV_FLOAT_32:
            m_bytes_per_sample = 4;
            break;
        default:
            return Error("Unsupported WAV format");
    }
    
    // Open file for binary writing
    m_file_stream = std::make_unique<std::ofstream>(file_path, std::ios::binary);
    if (!m_file_stream->is_open()) {
        return Error("Failed to open file for writing: " + file_path);
    }
    
    // Write WAV header (will be updated later with correct sizes)
    write_wav_header();
    
    return Ok(true);
}

void WAVFileWriter::write_wav_header() {
    // WAV file header structure
    struct WAVHeader {
        // RIFF header
        char riff_id[4] = {'R', 'I', 'F', 'F'};
        uint32_t file_size = 0;  // Will be updated later
        char wave_id[4] = {'W', 'A', 'V', 'E'};
        
        // Format chunk
        char fmt_id[4] = {'f', 'm', 't', ' '};
        uint32_t fmt_size = 16;  // PCM format chunk size
        uint16_t audio_format = 1;  // 1 = PCM, 3 = IEEE float
        uint16_t num_channels = 0;
        uint32_t sample_rate = 0;
        uint32_t byte_rate = 0;
        uint16_t block_align = 0;
        uint16_t bits_per_sample = 0;
        
        // Data chunk header
        char data_id[4] = {'d', 'a', 't', 'a'};
        uint32_t data_size = 0;  // Will be updated later
    };
    
    WAVHeader header;
    header.num_channels = static_cast<uint16_t>(m_channels);
    header.sample_rate = m_sample_rate;
    header.bits_per_sample = static_cast<uint16_t>(m_bytes_per_sample * 8);
    header.block_align = static_cast<uint16_t>(m_channels * m_bytes_per_sample);
    header.byte_rate = m_sample_rate * header.block_align;
    
    // Set format code
    if (m_format == AudioFormat::WAV_FLOAT_32) {
        header.audio_format = 3;  // IEEE float
    } else {
        header.audio_format = 1;  // PCM
    }
    
    m_file_stream->write(reinterpret_cast<const char*>(&header), sizeof(header));
}

Result<bool> WAVFileWriter::write_samples(const std::vector<std::vector<double>>& channel_data, 
                                         uint32_t num_samples) {
    if (channel_data.size() != m_channels) {
        return Error("Channel count mismatch");
    }
    
    if (!m_file_stream || !m_file_stream->is_open()) {
        return Error("File not open for writing");
    }
    
    // Write samples based on format
    switch (m_format) {
        case AudioFormat::WAV_PCM_16:
            write_samples_typed<int16_t>(channel_data, num_samples);
            break;
        case AudioFormat::WAV_PCM_24:
            // 24-bit is special case - no native type
            for (uint32_t sample = 0; sample < num_samples; ++sample) {
                for (uint32_t ch = 0; ch < m_channels; ++ch) {
                    double sample_value = std::clamp(channel_data[ch][sample], -1.0, 1.0);
                    int32_t int_value = static_cast<int32_t>(sample_value * 8388607.0);  // 2^23 - 1
                    
                    // Write 24-bit little-endian
                    char bytes[3];
                    bytes[0] = static_cast<char>(int_value & 0xFF);
                    bytes[1] = static_cast<char>((int_value >> 8) & 0xFF);
                    bytes[2] = static_cast<char>((int_value >> 16) & 0xFF);
                    m_file_stream->write(bytes, 3);
                }
            }
            break;
        case AudioFormat::WAV_PCM_32:
            write_samples_typed<int32_t>(channel_data, num_samples);
            break;
        case AudioFormat::WAV_FLOAT_32:
            write_samples_typed<float>(channel_data, num_samples);
            break;
        default:
            return Error("Unsupported format for writing");
    }
    
    m_samples_written += num_samples;
    return Ok(true);
}

template<typename SampleType>
void WAVFileWriter::write_samples_typed(const std::vector<std::vector<double>>& channel_data, uint32_t num_samples) {
    for (uint32_t sample = 0; sample < num_samples; ++sample) {
        for (uint32_t ch = 0; ch < m_channels; ++ch) {
            double sample_value = std::clamp(channel_data[ch][sample], -1.0, 1.0);
            
            SampleType output_sample;
            if constexpr (std::is_same_v<SampleType, float>) {
                output_sample = static_cast<float>(sample_value);
            } else {
                // Integer conversion
                constexpr double max_value = static_cast<double>(std::numeric_limits<SampleType>::max());
                output_sample = static_cast<SampleType>(sample_value * max_value);
            }
            
            m_file_stream->write(reinterpret_cast<const char*>(&output_sample), sizeof(SampleType));
        }
    }
}

Result<bool> WAVFileWriter::write_metadata(const RenderJobConfig::Metadata& metadata) {
    // WAV metadata is typically stored in INFO chunk or BWF chunks
    // For simplicity, we'll skip metadata implementation for now
    return Ok(true);
}

Result<bool> WAVFileWriter::close() {
    if (!m_file_stream || !m_file_stream->is_open()) {
        return Ok(true);  // Already closed
    }
    
    // Update header with correct file sizes
    update_wav_header();
    
    m_file_stream->close();
    return Ok(true);
}

void WAVFileWriter::update_wav_header() {
    if (!m_file_stream) return;
    
    // Calculate file sizes
    uint64_t data_size = m_samples_written * m_channels * m_bytes_per_sample;
    uint64_t file_size = data_size + 36;  // Header size is 44 bytes, file size excludes first 8 bytes
    
    // Update RIFF file size
    m_file_stream->seekp(4);
    uint32_t file_size_32 = static_cast<uint32_t>(std::min(file_size, static_cast<uint64_t>(UINT32_MAX)));
    m_file_stream->write(reinterpret_cast<const char*>(&file_size_32), sizeof(uint32_t));
    
    // Update data chunk size
    m_file_stream->seekp(40);
    uint32_t data_size_32 = static_cast<uint32_t>(std::min(data_size, static_cast<uint64_t>(UINT32_MAX)));
    m_file_stream->write(reinterpret_cast<const char*>(&data_size_32), sizeof(uint32_t));
    
    // Return to end of file
    m_file_stream->seekp(0, std::ios::end);
}

uint64_t WAVFileWriter::get_file_size_bytes() const {
    if (!m_file_stream) return 0;
    
    return 44 + (m_samples_written * m_channels * m_bytes_per_sample);  // Header + data
}

// AIFF File Writer Implementation

AIFFFileWriter::~AIFFFileWriter() {
    if (m_file_stream && m_file_stream->is_open()) {
        close();
    }
}

Result<bool> AIFFFileWriter::open(const std::string& file_path, uint32_t channels, 
                                 uint32_t sample_rate, AudioFormat format) {
    if (channels == 0 || channels > 32) {
        return Error("Invalid channel count: " + std::to_string(channels));
    }
    
    if (sample_rate < 8000 || sample_rate > 192000) {
        return Error("Invalid sample rate: " + std::to_string(sample_rate));
    }
    
    // Ensure output directory exists
    std::filesystem::path path(file_path);
    std::filesystem::create_directories(path.parent_path());
    
    m_file_path = file_path;
    m_channels = channels;
    m_sample_rate = sample_rate;
    m_format = format;
    m_samples_written = 0;
    
    // Determine bytes per sample
    switch (format) {
        case AudioFormat::AIFF_PCM_16:
            m_bytes_per_sample = 2;
            break;
        case AudioFormat::AIFF_PCM_24:
            m_bytes_per_sample = 3;
            break;
        case AudioFormat::AIFF_FLOAT_32:
            m_bytes_per_sample = 4;
            break;
        default:
            return Error("Unsupported AIFF format");
    }
    
    // Open file for binary writing
    m_file_stream = std::make_unique<std::ofstream>(file_path, std::ios::binary);
    if (!m_file_stream->is_open()) {
        return Error("Failed to open file for writing: " + file_path);
    }
    
    // Write AIFF header (will be updated later with correct sizes)
    write_aiff_header();
    
    return Ok(true);
}

void AIFFFileWriter::write_aiff_header() {
    // AIFF is big-endian format
    auto write_be_uint32 = [this](uint32_t value) {
        char bytes[4];
        bytes[0] = static_cast<char>((value >> 24) & 0xFF);
        bytes[1] = static_cast<char>((value >> 16) & 0xFF);
        bytes[2] = static_cast<char>((value >> 8) & 0xFF);
        bytes[3] = static_cast<char>(value & 0xFF);
        m_file_stream->write(bytes, 4);
    };
    
    auto write_be_uint16 = [this](uint16_t value) {
        char bytes[2];
        bytes[0] = static_cast<char>((value >> 8) & 0xFF);
        bytes[1] = static_cast<char>(value & 0xFF);
        m_file_stream->write(bytes, 2);
    };
    
    // FORM chunk
    m_file_stream->write("FORM", 4);
    write_be_uint32(0);  // File size - will be updated later
    m_file_stream->write("AIFF", 4);
    
    // Common chunk
    m_file_stream->write("COMM", 4);
    write_be_uint32(18);  // Common chunk size
    write_be_uint16(static_cast<uint16_t>(m_channels));
    write_be_uint32(0);  // Number of sample frames - will be updated later
    write_be_uint16(static_cast<uint16_t>(m_bytes_per_sample * 8));  // Bits per sample
    
    // Sample rate as IEEE 754 80-bit extended precision
    uint8_t sample_rate_bytes[10];
    write_ieee_extended(static_cast<double>(m_sample_rate), sample_rate_bytes);
    m_file_stream->write(reinterpret_cast<const char*>(sample_rate_bytes), 10);
    
    // Sound data chunk header
    m_file_stream->write("SSND", 4);
    write_be_uint32(8);  // Sound data chunk size - will be updated later
    write_be_uint32(0);  // Offset
    write_be_uint32(0);  // Block size
}

void AIFFFileWriter::write_ieee_extended(double value, uint8_t* bytes) {
    // Convert double to IEEE 754 80-bit extended precision
    // This is a simplified implementation
    if (value == 0.0) {
        std::memset(bytes, 0, 10);
        return;
    }
    
    bool negative = value < 0.0;
    if (negative) value = -value;
    
    // Extract exponent and mantissa
    int exponent;
    double mantissa = std::frexp(value, &exponent);
    
    // Adjust for IEEE extended format
    exponent += 16382;  // Bias for extended precision
    
    // Pack into bytes (big-endian)
    bytes[0] = negative ? 0x80 : 0x00;
    bytes[0] |= (exponent >> 8) & 0x7F;
    bytes[1] = exponent & 0xFF;
    
    // Convert mantissa to 64-bit integer
    uint64_t mantissa_int = static_cast<uint64_t>((mantissa - 0.5) * (1ULL << 63)) | (1ULL << 63);
    
    for (int i = 0; i < 8; ++i) {
        bytes[9 - i] = static_cast<uint8_t>(mantissa_int & 0xFF);
        mantissa_int >>= 8;
    }
}

Result<bool> AIFFFileWriter::write_samples(const std::vector<std::vector<double>>& channel_data, 
                                          uint32_t num_samples) {
    if (channel_data.size() != m_channels) {
        return Error("Channel count mismatch");
    }
    
    if (!m_file_stream || !m_file_stream->is_open()) {
        return Error("File not open for writing");
    }
    
    // AIFF uses big-endian byte order
    for (uint32_t sample = 0; sample < num_samples; ++sample) {
        for (uint32_t ch = 0; ch < m_channels; ++ch) {
            double sample_value = std::clamp(channel_data[ch][sample], -1.0, 1.0);
            
            if (m_format == AudioFormat::AIFF_FLOAT_32) {
                // Write 32-bit float big-endian
                float float_value = static_cast<float>(sample_value);
                uint32_t int_value = *reinterpret_cast<uint32_t*>(&float_value);
                
                char bytes[4];
                bytes[0] = static_cast<char>((int_value >> 24) & 0xFF);
                bytes[1] = static_cast<char>((int_value >> 16) & 0xFF);
                bytes[2] = static_cast<char>((int_value >> 8) & 0xFF);
                bytes[3] = static_cast<char>(int_value & 0xFF);
                m_file_stream->write(bytes, 4);
            } else if (m_format == AudioFormat::AIFF_PCM_16) {
                // Write 16-bit PCM big-endian
                int16_t int_value = static_cast<int16_t>(sample_value * 32767.0);
                
                char bytes[2];
                bytes[0] = static_cast<char>((int_value >> 8) & 0xFF);
                bytes[1] = static_cast<char>(int_value & 0xFF);
                m_file_stream->write(bytes, 2);
            } else if (m_format == AudioFormat::AIFF_PCM_24) {
                // Write 24-bit PCM big-endian
                int32_t int_value = static_cast<int32_t>(sample_value * 8388607.0);
                
                char bytes[3];
                bytes[0] = static_cast<char>((int_value >> 16) & 0xFF);
                bytes[1] = static_cast<char>((int_value >> 8) & 0xFF);
                bytes[2] = static_cast<char>(int_value & 0xFF);
                m_file_stream->write(bytes, 3);
            }
        }
    }
    
    m_samples_written += num_samples;
    return Ok(true);
}

Result<bool> AIFFFileWriter::write_metadata(const RenderJobConfig::Metadata& metadata) {
    // AIFF metadata can be stored in various chunks (NAME, AUTH, COMT, etc.)
    // For simplicity, we'll skip metadata implementation for now
    return Ok(true);
}

Result<bool> AIFFFileWriter::close() {
    if (!m_file_stream || !m_file_stream->is_open()) {
        return Ok(true);  // Already closed
    }
    
    // Update header with correct file sizes
    update_aiff_header();
    
    m_file_stream->close();
    return Ok(true);
}

void AIFFFileWriter::update_aiff_header() {
    if (!m_file_stream) return;
    
    auto write_be_uint32 = [this](std::streampos pos, uint32_t value) {
        m_file_stream->seekp(pos);
        char bytes[4];
        bytes[0] = static_cast<char>((value >> 24) & 0xFF);
        bytes[1] = static_cast<char>((value >> 16) & 0xFF);
        bytes[2] = static_cast<char>((value >> 8) & 0xFF);
        bytes[3] = static_cast<char>(value & 0xFF);
        m_file_stream->write(bytes, 4);
    };
    
    // Calculate sizes
    uint64_t data_size = m_samples_written * m_channels * m_bytes_per_sample;
    uint64_t sound_chunk_size = data_size + 8;  // Data + offset + block size
    uint64_t file_size = sound_chunk_size + 18 + 8;  // Sound chunk + COMM chunk + headers
    
    // Update FORM chunk size
    write_be_uint32(4, static_cast<uint32_t>(std::min(file_size, static_cast<uint64_t>(UINT32_MAX))));
    
    // Update number of sample frames in COMM chunk
    write_be_uint32(22, static_cast<uint32_t>(m_samples_written));
    
    // Update SSND chunk size
    write_be_uint32(38, static_cast<uint32_t>(std::min(sound_chunk_size, static_cast<uint64_t>(UINT32_MAX))));
    
    // Return to end of file
    m_file_stream->seekp(0, std::ios::end);
}

uint64_t AIFFFileWriter::get_file_size_bytes() const {
    if (!m_file_stream) return 0;
    
    return 46 + (m_samples_written * m_channels * m_bytes_per_sample);  // Header + data
}

// Audio Format Utility Functions Implementation

std::string AudioFormatUtils::get_file_extension(AudioFormat format) {
    switch (format) {
        case AudioFormat::WAV_PCM_16:
        case AudioFormat::WAV_PCM_24:
        case AudioFormat::WAV_PCM_32:
        case AudioFormat::WAV_FLOAT_32:
            return ".wav";
            
        case AudioFormat::AIFF_PCM_16:
        case AudioFormat::AIFF_PCM_24:
        case AudioFormat::AIFF_FLOAT_32:
            return ".aiff";
            
        case AudioFormat::FLAC_16:
        case AudioFormat::FLAC_24:
            return ".flac";
            
        case AudioFormat::MP3_128:
        case AudioFormat::MP3_192:
        case AudioFormat::MP3_320:
            return ".mp3";
            
        case AudioFormat::OGG_VORBIS_Q6:
            return ".ogg";
            
        case AudioFormat::AAC_128:
        case AudioFormat::AAC_256:
            return ".aac";
            
        default:
            return ".wav";
    }
}

std::string AudioFormatUtils::get_format_name(AudioFormat format) {
    switch (format) {
        case AudioFormat::WAV_PCM_16: return "WAV 16-bit PCM";
        case AudioFormat::WAV_PCM_24: return "WAV 24-bit PCM";
        case AudioFormat::WAV_PCM_32: return "WAV 32-bit PCM";
        case AudioFormat::WAV_FLOAT_32: return "WAV 32-bit Float";
        case AudioFormat::AIFF_PCM_16: return "AIFF 16-bit PCM";
        case AudioFormat::AIFF_PCM_24: return "AIFF 24-bit PCM";
        case AudioFormat::AIFF_FLOAT_32: return "AIFF 32-bit Float";
        case AudioFormat::FLAC_16: return "FLAC 16-bit Lossless";
        case AudioFormat::FLAC_24: return "FLAC 24-bit Lossless";
        case AudioFormat::MP3_128: return "MP3 128 kbps";
        case AudioFormat::MP3_192: return "MP3 192 kbps";
        case AudioFormat::MP3_320: return "MP3 320 kbps";
        case AudioFormat::OGG_VORBIS_Q6: return "Ogg Vorbis Quality 6";
        case AudioFormat::AAC_128: return "AAC 128 kbps";
        case AudioFormat::AAC_256: return "AAC 256 kbps";
        default: return "Unknown Format";
    }
}

uint32_t AudioFormatUtils::get_bit_depth(AudioFormat format) {
    switch (format) {
        case AudioFormat::WAV_PCM_16:
        case AudioFormat::AIFF_PCM_16:
        case AudioFormat::FLAC_16:
            return 16;
            
        case AudioFormat::WAV_PCM_24:
        case AudioFormat::AIFF_PCM_24:
        case AudioFormat::FLAC_24:
            return 24;
            
        case AudioFormat::WAV_PCM_32:
        case AudioFormat::WAV_FLOAT_32:
        case AudioFormat::AIFF_FLOAT_32:
            return 32;
            
        default:
            return 16;  // Lossy formats
    }
}

bool AudioFormatUtils::is_lossy_format(AudioFormat format) {
    switch (format) {
        case AudioFormat::MP3_128:
        case AudioFormat::MP3_192:
        case AudioFormat::MP3_320:
        case AudioFormat::OGG_VORBIS_Q6:
        case AudioFormat::AAC_128:
        case AudioFormat::AAC_256:
            return true;
        default:
            return false;
    }
}

// Filename Template Processor Implementation

std::string FilenameTemplateProcessor::process_template(const std::string& template_str,
                                                       const std::map<std::string, std::string>& variables) {
    std::string result = template_str;
    
    for (const auto& [key, value] : variables) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    return sanitize_filename(result);
}

std::map<std::string, std::string> FilenameTemplateProcessor::create_default_variables(
    const std::string& project_name,
    const std::string& track_name,
    AudioFormat format) {
    
    std::map<std::string, std::string> variables;
    variables["project"] = project_name.empty() ? "Project" : project_name;
    variables["track_name"] = track_name.empty() ? "Master" : track_name;
    variables["timestamp"] = generate_timestamp_string();
    variables["format"] = AudioFormatUtils::get_format_name(format);
    variables["date"] = generate_timestamp_string().substr(0, 8);  // YYYYMMDD
    
    return variables;
}

std::string FilenameTemplateProcessor::generate_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string FilenameTemplateProcessor::sanitize_filename(const std::string& filename) {
    std::string result = filename;
    
    // Replace invalid filename characters with underscores
    const std::string invalid_chars = "<>:\"/\\|?*";
    for (char invalid_char : invalid_chars) {
        std::replace(result.begin(), result.end(), invalid_char, '_');
    }
    
    // Remove leading/trailing whitespace and dots
    result.erase(0, result.find_first_not_of(" \t."));
    result.erase(result.find_last_not_of(" \t.") + 1);
    
    // Ensure filename isn't empty
    if (result.empty()) {
        result = "untitled";
    }
    
    return result;
}

} // namespace mixmind