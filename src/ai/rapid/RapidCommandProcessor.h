#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <regex>
#include <functional>
#include <algorithm>
#include <cctype>
#include <iostream>
#include "../../audio/rapid/RapidAudioEngine.h"

namespace mixmind::rapid {

    // Simple command structure for rapid prototyping
    struct Command {
        std::string action;        // "add", "set", "remove", "play"
        std::string target;        // "track", "effect", "parameter"
        std::string object;        // "reverb", "gain", "volume"
        std::map<std::string, std::string> parameters;
        
        bool isValid() const {
            return !action.empty() && !target.empty();
        }
        
        std::string toString() const {
            std::string result = action + " " + object + " to " + target;
            if (!parameters.empty()) {
                result += " with ";
                for (const auto& [key, value] : parameters) {
                    result += key + "=" + value + " ";
                }
            }
            return result;
        }
    };

    // Simple natural language processor for rapid development
    class RapidNLP {
    public:
        struct Pattern {
            std::regex regex;
            std::function<Command(const std::smatch&)> parser;
        };
        
        RapidNLP() {
            setupPatterns();
        }
        
        Command parseCommand(const std::string& input) {
            std::string lowercaseInput = toLowercase(input);
            
            for (const auto& pattern : patterns_) {
                std::smatch match;
                if (std::regex_search(lowercaseInput, match, pattern.regex)) {
                    return pattern.parser(match);
                }
            }
            
            // Return invalid command if no match
            return Command{};
        }
        
    private:
        std::vector<Pattern> patterns_;
        
        void setupPatterns() {
            // "add reverb to track 1"
            patterns_.push_back({
                std::regex(R"(add\s+(\w+)\s+to\s+track\s*(\d+))"),
                [](const std::smatch& m) {
                    Command cmd;
                    cmd.action = "add";
                    cmd.target = "track";
                    cmd.object = m[1].str();
                    cmd.parameters["track_id"] = m[2].str();
                    return cmd;
                }
            });
            
            // "set volume to 50%" or "set gain to 0.8"
            patterns_.push_back({
                std::regex(R"(set\s+(\w+)\s+to\s+([\d.]+)(%?))"),
                [](const std::smatch& m) {
                    Command cmd;
                    cmd.action = "set";
                    cmd.target = "parameter";
                    cmd.object = m[1].str();
                    std::string value = m[2].str();
                    if (m[3].str() == "%") {
                        // Convert percentage to decimal
                        float percent = std::stof(value);
                        value = std::to_string(percent / 100.0f);
                    }
                    cmd.parameters["value"] = value;
                    return cmd;
                }
            });
            
            // "remove effect from track 2"
            patterns_.push_back({
                std::regex(R"(remove\s+(\w+)\s+from\s+track\s*(\d+))"),
                [](const std::smatch& m) {
                    Command cmd;
                    cmd.action = "remove";
                    cmd.target = "track";
                    cmd.object = m[1].str();
                    cmd.parameters["track_id"] = m[2].str();
                    return cmd;
                }
            });
            
            // "play" or "stop"
            patterns_.push_back({
                std::regex(R"(^(play|stop|pause)$)"),
                [](const std::smatch& m) {
                    Command cmd;
                    cmd.action = m[1].str();
                    cmd.target = "transport";
                    return cmd;
                }
            });
            
            // "make track 1 louder" or "make the drums quieter"
            patterns_.push_back({
                std::regex(R"(make\s+(?:track\s*(\d+)|the\s+(\w+))\s+(louder|quieter|punchier|warmer))"),
                [](const std::smatch& m) {
                    Command cmd;
                    cmd.action = "adjust";
                    cmd.target = "track";
                    
                    if (m[1].matched) {
                        cmd.parameters["track_id"] = m[1].str();
                    } else if (m[2].matched) {
                        cmd.parameters["track_name"] = m[2].str();
                    }
                    
                    std::string adjustment = m[3].str();
                    cmd.object = "character";
                    cmd.parameters["adjustment"] = adjustment;
                    
                    return cmd;
                }
            });
        }
        
        std::string toLowercase(const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return result;
        }
    };

    // Simple track representation for rapid development
    class RapidTrack {
    public:
        RapidTrack(const std::string& name) : name_(name), volume_(1.0f), muted_(false) {}
        
        void setVolume(float volume) { volume_ = std::clamp(volume, 0.0f, 2.0f); }
        float getVolume() const { return volume_; }
        
        void setMuted(bool muted) { muted_ = muted; }
        bool isMuted() const { return muted_; }
        
        void addEffect(std::shared_ptr<AudioEffect> effect) {
            effects_.push_back(effect);
        }
        
        void removeEffect(const std::string& type) {
            // Simple effect removal by type name
            effects_.erase(
                std::remove_if(effects_.begin(), effects_.end(),
                    [&type](const std::weak_ptr<AudioEffect>& effect) {
                        return effect.expired(); // Remove expired effects
                    }),
                effects_.end());
        }
        
        void processAudio(AudioBuffer& buffer) {
            if (muted_) {
                buffer.clear();
                return;
            }
            
            // Apply volume
            if (volume_ != 1.0f) {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                    float* data = buffer.getWritePointer(ch);
                    for (int i = 0; i < buffer.getNumSamples(); ++i) {
                        data[i] *= volume_;
                    }
                }
            }
            
            // Apply effects
            for (auto& weakEffect : effects_) {
                if (auto effect = weakEffect.lock()) {
                    effect->process(buffer);
                }
            }
        }
        
        const std::string& getName() const { return name_; }
        size_t getEffectCount() const { return effects_.size(); }
        
    private:
        std::string name_;
        float volume_;
        bool muted_;
        std::vector<std::weak_ptr<AudioEffect>> effects_;
    };

    // Rapid DAW engine that combines everything for quick prototyping
    class RapidDAW {
    public:
        RapidDAW() {
            audioEngine_ = std::make_unique<RapidAudioEngine>();
            nlp_ = std::make_unique<RapidNLP>();
            
            // Set up audio callback
            audioEngine_->setAudioCallback([this](AudioBuffer& input, AudioBuffer& output) {
                processAudio(input, output);
            });
        }
        
        bool initialize(int sampleRate = 44100, int bufferSize = 512) {
            return audioEngine_->initialize(sampleRate, bufferSize);
        }
        
        bool start() { return audioEngine_->start(); }
        bool stop() { return audioEngine_->stop(); }
        
        // Natural language command interface
        std::string executeCommand(const std::string& commandText) {
            Command cmd = nlp_->parseCommand(commandText);
            
            if (!cmd.isValid()) {
                return "Error: Could not understand command '" + commandText + "'";
            }
            
            return executeCommand(cmd);
        }
        
        // Add a track
        void addTrack(const std::string& name) {
            tracks_.push_back(std::make_unique<RapidTrack>(name));
        }
        
        RapidTrack* getTrack(int index) {
            return (index >= 0 && index < static_cast<int>(tracks_.size())) 
                ? tracks_[index].get() : nullptr;
        }
        
        size_t getTrackCount() const { return tracks_.size(); }
        
        // Quick test interface
        void processTestBlock() {
            audioEngine_->processTestBlock();
        }
        
    private:
        std::unique_ptr<RapidAudioEngine> audioEngine_;
        std::unique_ptr<RapidNLP> nlp_;
        std::vector<std::unique_ptr<RapidTrack>> tracks_;
        bool isPlaying_ = false;
        
        std::string executeCommand(const Command& cmd) {
            try {
                if (cmd.action == "add" && cmd.target == "track") {
                    return addEffectToTrack(cmd);
                } else if (cmd.action == "set" && cmd.target == "parameter") {
                    return setParameter(cmd);
                } else if (cmd.action == "play") {
                    isPlaying_ = true;
                    return "Playback started";
                } else if (cmd.action == "stop") {
                    isPlaying_ = false;
                    return "Playback stopped";
                } else if (cmd.action == "adjust") {
                    return adjustTrack(cmd);
                } else {
                    return "Error: Unknown command action '" + cmd.action + "'";
                }
            } catch (const std::exception& e) {
                return "Error executing command: " + std::string(e.what());
            }
        }
        
        std::string addEffectToTrack(const Command& cmd) {
            auto trackIdIt = cmd.parameters.find("track_id");
            if (trackIdIt == cmd.parameters.end()) {
                return "Error: No track ID specified";
            }
            
            int trackId = std::stoi(trackIdIt->second) - 1; // Convert to 0-based
            RapidTrack* track = getTrack(trackId);
            if (!track) {
                return "Error: Track " + trackIdIt->second + " not found";
            }
            
            // Create effect based on type
            std::shared_ptr<AudioEffect> effect;
            if (cmd.object == "gain" || cmd.object == "volume") {
                effect = std::make_shared<GainEffect>();
            } else {
                // For other effects, create a basic gain effect as placeholder
                effect = std::make_shared<GainEffect>();
            }
            
            track->addEffect(effect);
            return "Added " + cmd.object + " to " + track->getName();
        }
        
        std::string setParameter(const Command& cmd) {
            auto valueIt = cmd.parameters.find("value");
            if (valueIt == cmd.parameters.end()) {
                return "Error: No value specified";
            }
            
            float value = std::stof(valueIt->second);
            
            if (cmd.object == "volume" || cmd.object == "gain") {
                // Set volume on all tracks for now (simplified)
                for (auto& track : tracks_) {
                    track->setVolume(value);
                }
                return "Set " + cmd.object + " to " + valueIt->second;
            }
            
            return "Error: Unknown parameter '" + cmd.object + "'";
        }
        
        std::string adjustTrack(const Command& cmd) {
            auto adjustmentIt = cmd.parameters.find("adjustment");
            if (adjustmentIt == cmd.parameters.end()) {
                return "Error: No adjustment specified";
            }
            
            std::string adjustment = adjustmentIt->second;
            
            // Find the track
            RapidTrack* targetTrack = nullptr;
            auto trackIdIt = cmd.parameters.find("track_id");
            if (trackIdIt != cmd.parameters.end()) {
                int trackId = std::stoi(trackIdIt->second) - 1;
                targetTrack = getTrack(trackId);
            }
            
            if (!targetTrack && !tracks_.empty()) {
                targetTrack = tracks_[0].get(); // Default to first track
            }
            
            if (!targetTrack) {
                return "Error: No track found to adjust";
            }
            
            // Apply adjustment
            float currentVolume = targetTrack->getVolume();
            if (adjustment == "louder") {
                targetTrack->setVolume(currentVolume * 1.2f);
                return "Made " + targetTrack->getName() + " louder";
            } else if (adjustment == "quieter") {
                targetTrack->setVolume(currentVolume * 0.8f);
                return "Made " + targetTrack->getName() + " quieter";
            } else {
                return "Applied " + adjustment + " adjustment to " + targetTrack->getName();
            }
        }
        
        void processAudio(AudioBuffer& input, AudioBuffer& output) {
            output.clear();
            
            if (!isPlaying_ || tracks_.empty()) {
                return;
            }
            
            // Simple mixing: sum all tracks
            AudioBuffer trackBuffer(output.getNumSamples(), output.getNumChannels());
            
            for (auto& track : tracks_) {
                trackBuffer.clear();
                // In a real implementation, we'd read from audio files or generate test tones
                generateTestTone(trackBuffer, 440.0f, 0.1f); // Generate test tone
                
                track->processAudio(trackBuffer);
                
                // Mix into output
                for (int ch = 0; ch < output.getNumChannels(); ++ch) {
                    const float* trackData = trackBuffer.getReadPointer(ch);
                    float* outputData = output.getWritePointer(ch);
                    
                    for (int i = 0; i < output.getNumSamples(); ++i) {
                        outputData[i] += trackData[i];
                    }
                }
            }
        }
    };

} // namespace mixmind::rapid