#include "TEUtils.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <sstream>
#include <iomanip>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TETypeConverter Implementation
// ============================================================================

te::BeatPosition TETypeConverter::samplesToBeats(core::TimestampSamples samples, double sampleRate, double bpm) {
    double seconds = static_cast<double>(samples) / sampleRate;
    double beats = (seconds * bpm) / 60.0;
    return te::BeatPosition::fromBeats(beats);
}

core::TimestampSamples TETypeConverter::beatsToSamples(te::BeatPosition beats, double sampleRate, double bpm) {
    double beatValue = beats.inBeats();
    double seconds = (beatValue * 60.0) / bpm;
    return static_cast<core::TimestampSamples>(seconds * sampleRate);
}

te::TimePosition TETypeConverter::secondsToTime(core::TimestampSeconds seconds) {
    return te::TimePosition::fromSeconds(seconds);
}

core::TimestampSeconds TETypeConverter::timeToSeconds(te::TimePosition time) {
    return time.inSeconds();
}

core::TimestampSeconds TETypeConverter::samplesToSeconds(core::TimestampSamples samples, core::SampleRate sampleRate) {
    return static_cast<double>(samples) / static_cast<double>(sampleRate);
}

core::TimestampSamples TETypeConverter::secondsToSamples(core::TimestampSeconds seconds, core::SampleRate sampleRate) {
    return static_cast<core::TimestampSamples>(seconds * static_cast<double>(sampleRate));
}

juce::AudioFormat* TETypeConverter::getJuceAudioFormat(core::AudioFormat format) {
    static juce::AudioFormatManager formatManager;
    static bool initialized = false;
    
    if (!initialized) {
        formatManager.registerBasicFormats();
        initialized = true;
    }
    
    switch (format) {
        case core::AudioFormat::WAV:
            return formatManager.findFormatForFileExtension("wav");
        case core::AudioFormat::FLAC:
            return formatManager.findFormatForFileExtension("flac");
        case core::AudioFormat::MP3:
            return formatManager.findFormatForFileExtension("mp3");
        case core::AudioFormat::AIFF:
            return formatManager.findFormatForFileExtension("aiff");
        case core::AudioFormat::OGG:
            return formatManager.findFormatForFileExtension("ogg");
        default:
            return nullptr;
    }
}

std::optional<core::AudioFormat> TETypeConverter::getAudioFormat(const juce::AudioFormat* format) {
    if (!format) return std::nullopt;
    
    juce::String formatName = format->getFormatName().toLowerCase();
    
    if (formatName.contains("wav")) return core::AudioFormat::WAV;
    if (formatName.contains("flac")) return core::AudioFormat::FLAC;
    if (formatName.contains("mp3")) return core::AudioFormat::MP3;
    if (formatName.contains("aiff")) return core::AudioFormat::AIFF;
    if (formatName.contains("ogg")) return core::AudioFormat::OGG;
    
    return std::nullopt;
}

double TETypeConverter::sampleRateToDouble(core::SampleRate sampleRate) {
    return static_cast<double>(sampleRate);
}

core::SampleRate TETypeConverter::doubleToSampleRate(double sampleRate) {
    return static_cast<core::SampleRate>(sampleRate);
}

juce::AudioBuffer<float> TETypeConverter::convertToJuceBuffer(const core::FloatAudioBuffer& buffer) {
    juce::AudioBuffer<float> juceBuffer(buffer.channels.size(), 
                                       buffer.channels.empty() ? 0 : buffer.channels[0].size());
    
    for (size_t channel = 0; channel < buffer.channels.size(); ++channel) {
        const auto& sourceChannel = buffer.channels[channel];
        auto* destChannel = juceBuffer.getWritePointer(static_cast<int>(channel));
        
        std::copy(sourceChannel.begin(), sourceChannel.end(), destChannel);
    }
    
    return juceBuffer;
}

core::FloatAudioBuffer TETypeConverter::convertFromJuceBuffer(const juce::AudioBuffer<float>& buffer) {
    core::FloatAudioBuffer result;
    result.channels.resize(buffer.getNumChannels());
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        const float* sourceData = buffer.getReadPointer(channel);
        auto& destChannel = result.channels[channel];
        destChannel.assign(sourceData, sourceData + buffer.getNumSamples());
    }
    
    return result;
}

te::MidiMessageSequence TETypeConverter::convertToTEMidiSequence(const core::MidiBuffer& buffer) {
    te::MidiMessageSequence sequence;
    
    for (const auto& message : buffer.messages) {
        // Convert our MIDI message to JUCE MidiMessage
        juce::MidiMessage juceMessage;
        
        switch (message.type) {
            case core::MidiMessage::Type::NoteOn:
                juceMessage = juce::MidiMessage::noteOn(
                    message.channel, 
                    message.data1, 
                    static_cast<float>(message.data2) / 127.0f
                );
                break;
            case core::MidiMessage::Type::NoteOff:
                juceMessage = juce::MidiMessage::noteOff(message.channel, message.data1);
                break;
            case core::MidiMessage::Type::ControlChange:
                juceMessage = juce::MidiMessage::controllerEvent(message.channel, message.data1, message.data2);
                break;
            case core::MidiMessage::Type::ProgramChange:
                juceMessage = juce::MidiMessage::programChange(message.channel, message.data1);
                break;
            case core::MidiMessage::Type::PitchBend:
                juceMessage = juce::MidiMessage::pitchWheel(message.channel, (message.data2 << 7) | message.data1);
                break;
            default:
                continue;  // Skip unknown message types
        }
        
        sequence.addEvent(juceMessage, message.timestamp);
    }
    
    return sequence;
}

core::MidiBuffer TETypeConverter::convertFromTEMidiSequence(const te::MidiMessageSequence& sequence) {
    core::MidiBuffer buffer;
    buffer.messages.reserve(sequence.getNumEvents());
    
    for (int i = 0; i < sequence.getNumEvents(); ++i) {
        const auto* event = sequence.getEventPointer(i);
        if (!event) continue;
        
        const juce::MidiMessage& juceMessage = event->message;
        core::MidiMessage message;
        
        message.timestamp = event->getTimeStamp();
        message.channel = juceMessage.getChannel();
        
        if (juceMessage.isNoteOn()) {
            message.type = core::MidiMessage::Type::NoteOn;
            message.data1 = juceMessage.getNoteNumber();
            message.data2 = static_cast<uint8_t>(juceMessage.getVelocity() * 127.0f);
        } else if (juceMessage.isNoteOff()) {
            message.type = core::MidiMessage::Type::NoteOff;
            message.data1 = juceMessage.getNoteNumber();
            message.data2 = 0;
        } else if (juceMessage.isController()) {
            message.type = core::MidiMessage::Type::ControlChange;
            message.data1 = juceMessage.getControllerNumber();
            message.data2 = juceMessage.getControllerValue();
        } else if (juceMessage.isProgramChange()) {
            message.type = core::MidiMessage::Type::ProgramChange;
            message.data1 = juceMessage.getProgramChangeNumber();
            message.data2 = 0;
        } else if (juceMessage.isPitchWheel()) {
            message.type = core::MidiMessage::Type::PitchBend;
            int pitchValue = juceMessage.getPitchWheelValue();
            message.data1 = pitchValue & 0x7F;
            message.data2 = (pitchValue >> 7) & 0x7F;
        } else {
            continue;  // Skip unknown message types
        }
        
        buffer.messages.push_back(message);
    }
    
    return buffer;
}

core::PluginType TETypeConverter::convertPluginType(te::Plugin::Type teType) {
    switch (teType) {
        case te::Plugin::Type::vst:
            return core::PluginType::VST2;
        case te::Plugin::Type::vst3:
            return core::PluginType::VST3;
        case te::Plugin::Type::audioUnit:
            return core::PluginType::AudioUnit;
        default:
            return core::PluginType::Unknown;
    }
}

te::Plugin::Type TETypeConverter::convertToTEPluginType(core::PluginType type) {
    switch (type) {
        case core::PluginType::VST2:
            return te::Plugin::Type::vst;
        case core::PluginType::VST3:
            return te::Plugin::Type::vst3;
        case core::PluginType::AudioUnit:
            return te::Plugin::Type::audioUnit;
        default:
            return te::Plugin::Type::vst;  // Default fallback
    }
}

core::PluginCategory TETypeConverter::convertPluginCategory(const juce::String& teCategory) {
    juce::String category = teCategory.toLowerCase();
    
    if (category.contains("synth") || category.contains("instrument")) 
        return core::PluginCategory::Instrument;
    if (category.contains("effect")) 
        return core::PluginCategory::Effect;
    if (category.contains("reverb")) 
        return core::PluginCategory::Reverb;
    if (category.contains("delay")) 
        return core::PluginCategory::Delay;
    if (category.contains("distortion")) 
        return core::PluginCategory::Distortion;
    if (category.contains("dynamics") || category.contains("compressor")) 
        return core::PluginCategory::Dynamics;
    if (category.contains("eq") || category.contains("filter")) 
        return core::PluginCategory::EQ;
    if (category.contains("modulation")) 
        return core::PluginCategory::Modulation;
    if (category.contains("utility")) 
        return core::PluginCategory::Utility;
    
    return core::PluginCategory::Other;
}

juce::String TETypeConverter::convertToTEPluginCategory(core::PluginCategory category) {
    switch (category) {
        case core::PluginCategory::Instrument: return "Instrument";
        case core::PluginCategory::Effect: return "Effect";
        case core::PluginCategory::Reverb: return "Reverb";
        case core::PluginCategory::Delay: return "Delay";
        case core::PluginCategory::Distortion: return "Distortion";
        case core::PluginCategory::Dynamics: return "Dynamics";
        case core::PluginCategory::EQ: return "EQ";
        case core::PluginCategory::Modulation: return "Modulation";
        case core::PluginCategory::Utility: return "Utility";
        default: return "Other";
    }
}

juce::String TETypeConverter::toJuceString(const std::string& str) {
    return juce::String(str);
}

std::string TETypeConverter::fromJuceString(const juce::String& str) {
    return str.toStdString();
}

juce::File TETypeConverter::convertFilePath(const std::string& path) {
    return juce::File(path);
}

std::string TETypeConverter::convertFromFile(const juce::File& file) {
    return file.getFullPathName().toStdString();
}

core::TransportState TETypeConverter::convertTransportState(te::TransportControl::PlayState teState) {
    switch (teState) {
        case te::TransportControl::PlayState::playing:
            return core::TransportState::Playing;
        case te::TransportControl::PlayState::recording:
            return core::TransportState::Recording;
        case te::TransportControl::PlayState::stopped:
            return core::TransportState::Stopped;
        default:
            return core::TransportState::Stopped;
    }
}

te::TransportControl::PlayState TETypeConverter::convertToTETransportState(core::TransportState state) {
    switch (state) {
        case core::TransportState::Playing:
            return te::TransportControl::PlayState::playing;
        case core::TransportState::Recording:
            return te::TransportControl::PlayState::recording;
        case core::TransportState::Stopped:
        case core::TransportState::Paused:
        default:
            return te::TransportControl::PlayState::stopped;
    }
}

juce::Colour TETypeConverter::convertToJuceColour(const std::string& hexColor) {
    if (hexColor.empty() || hexColor[0] != '#') {
        return juce::Colours::white;
    }
    
    try {
        uint32_t colorValue = static_cast<uint32_t>(std::stoul(hexColor.substr(1), nullptr, 16));
        return juce::Colour(colorValue);
    } catch (...) {
        return juce::Colours::white;
    }
}

std::string TETypeConverter::convertFromJuceColour(juce::Colour colour) {
    std::stringstream ss;
    ss << "#" << std::hex << std::setw(6) << std::setfill('0') 
       << (colour.getARGB() & 0x00FFFFFF);
    return ss.str();
}

float TETypeConverter::normalizeParameterValue(float value, float minValue, float maxValue) {
    if (maxValue <= minValue) return 0.0f;
    return juce::jlimit(0.0f, 1.0f, (value - minValue) / (maxValue - minValue));
}

float TETypeConverter::denormalizeParameterValue(float normalizedValue, float minValue, float maxValue) {
    return minValue + (juce::jlimit(0.0f, 1.0f, normalizedValue) * (maxValue - minValue));
}

core::ErrorCode TETypeConverter::convertErrorCode(const juce::Result& result) {
    if (result.wasOk()) {
        return core::ErrorCode::Success;
    }
    
    juce::String errorMessage = result.getErrorMessage().toLowerCase();
    
    if (errorMessage.contains("file") || errorMessage.contains("path")) {
        return core::ErrorCode::FileNotFound;
    }
    if (errorMessage.contains("memory") || errorMessage.contains("allocation")) {
        return core::ErrorCode::OutOfMemory;
    }
    if (errorMessage.contains("permission") || errorMessage.contains("access")) {
        return core::ErrorCode::PermissionDenied;
    }
    if (errorMessage.contains("timeout")) {
        return core::ErrorCode::Timeout;
    }
    if (errorMessage.contains("cancel")) {
        return core::ErrorCode::OperationCancelled;
    }
    
    return core::ErrorCode::TracktionEngineError;
}

core::ErrorCode TETypeConverter::convertExceptionToErrorCode(const std::exception& e) {
    std::string message = e.what();
    std::transform(message.begin(), message.end(), message.begin(), ::tolower);
    
    if (message.find("bad_alloc") != std::string::npos) {
        return core::ErrorCode::OutOfMemory;
    }
    if (message.find("timeout") != std::string::npos) {
        return core::ErrorCode::Timeout;
    }
    if (message.find("invalid") != std::string::npos) {
        return core::ErrorCode::InvalidParameter;
    }
    
    return core::ErrorCode::UnknownError;
}

std::string TETypeConverter::createErrorMessage(const std::string& operation, const juce::String& teMessage) {
    return operation + ": " + teMessage.toStdString();
}

// ============================================================================
// TEEngineUtils Implementation
// ============================================================================

std::unique_ptr<te::Engine> TEEngineUtils::createEngine() {
    return std::make_unique<te::Engine>("MixMind", nullptr, nullptr);
}

void TEEngineUtils::optimizeEngine(te::Engine& engine) {
    // Configure engine for optimal performance
    auto& deviceManager = engine.getDeviceManager();
    
    // Set reasonable buffer sizes
    deviceManager.setDefaultBufferSize(512);
    
    // Enable multi-threading where possible
    // Note: Actual optimization depends on specific TE version and capabilities
}

std::string TEEngineUtils::getEngineVersion() {
    // Return Tracktion Engine version info
    return "Tracktion Engine (version info not available in this context)";
}

TEEngineUtils::EngineCapabilities TEEngineUtils::getEngineCapabilities(te::Engine& engine) {
    EngineCapabilities caps;
    
    // Query plugin format support
    auto& pluginManager = engine.getPluginManager();
    caps.supportsVST = pluginManager.areVSTsEnabled();
    caps.supportsVST3 = pluginManager.areVST3sEnabled();
    caps.supportsAU = pluginManager.areAUsEnabled();
    
    // Query audio format support
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    for (int i = 0; i < formatManager.getNumKnownFormats(); ++i) {
        auto* format = formatManager.getKnownFormat(i);
        if (format) {
            caps.supportedAudioFormats.push_back(format->getFormatName().toStdString());
        }
    }
    
    // Set common sample rates
    caps.supportedSampleRates = {22050, 44100, 48000, 88200, 96000, 192000};
    caps.maxChannels = 32;  // Reasonable default
    
    return caps;
}

juce::Result TEEngineUtils::setupDefaultAudioDevice(te::Engine& engine) {
    auto& deviceManager = engine.getDeviceManager();
    
    // Try to initialize with default audio device
    juce::String error = deviceManager.initialise();
    
    if (error.isEmpty()) {
        return juce::Result::ok();
    } else {
        return juce::Result::fail(error);
    }
}

TEEngineUtils::EngineStats TEEngineUtils::getEngineStats(te::Engine& engine) {
    EngineStats stats;
    
    auto& deviceManager = engine.getDeviceManager();
    
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        stats.sampleRate = device->getCurrentSampleRate();
        stats.bufferSize = device->getCurrentBufferSizeSamples();
        stats.cpuUsage = device->getCpuUsage() * 100.0;
    }
    
    // Count active projects and plugins
    // Note: These would need to be tracked by the application
    stats.activeProjects = 0;  // Placeholder
    stats.loadedPlugins = 0;   // Placeholder
    stats.memoryUsage = 0;     // Placeholder
    
    return stats;
}

// ============================================================================
// TEProgressCallback Implementation
// ============================================================================

TEProgressCallback::TEProgressCallback(core::ProgressCallback coreCallback) 
    : coreCallback_(std::move(coreCallback)) 
{
}

std::function<bool(float)> TEProgressCallback::asTECallback() {
    if (!coreCallback_) {
        return [](float) { return true; };
    }
    
    return [this](float progress) -> bool {
        core::ProgressInfo info;
        info.progress = progress;
        info.canCancel = true;
        return coreCallback_(info);
    };
}

std::function<void(float, const juce::String&)> TEProgressCallback::asJuceCallback() {
    if (!coreCallback_) {
        return [](float, const juce::String&) {};
    }
    
    return [this](float progress, const juce::String& message) {
        core::ProgressInfo info;
        info.progress = progress;
        info.message = message.toStdString();
        info.canCancel = true;
        coreCallback_(info);
    };
}

// ============================================================================
// TEThreadSafety Implementation
// ============================================================================

bool TEThreadSafety::isMessageThread() {
    return juce::MessageManager::getInstance()->isThisTheMessageThread();
}

bool TEThreadSafety::isAudioThread() {
    // This is a simplified check - actual implementation would need proper audio thread detection
    return !isMessageThread();
}

void TEThreadSafety::assertMessageThread(const std::string& operation) {
    if (!isMessageThread()) {
        throw std::runtime_error("Operation '" + operation + "' must be called from message thread");
    }
}

void TEThreadSafety::assertNotAudioThread(const std::string& operation) {
    if (isAudioThread()) {
        throw std::runtime_error("Operation '" + operation + "' cannot be called from audio thread");
    }
}

} // namespace mixmind::adapters::tracktion