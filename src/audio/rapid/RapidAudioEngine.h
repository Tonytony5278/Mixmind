#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mixmind::rapid {

    // Simplified audio buffer for rapid development
    class AudioBuffer {
    public:
        AudioBuffer(int numSamples = 1024, int numChannels = 2) 
            : numSamples_(numSamples), numChannels_(numChannels) {
            data_.resize(numSamples * numChannels, 0.0f);
        }
        
        float* getWritePointer(int channel) { 
            return data_.data() + (channel * numSamples_); 
        }
        
        const float* getReadPointer(int channel) const { 
            return data_.data() + (channel * numSamples_); 
        }
        
        int getNumSamples() const { return numSamples_; }
        int getNumChannels() const { return numChannels_; }
        
        void clear() { std::fill(data_.begin(), data_.end(), 0.0f); }
        
        float getRMSLevel() const {
            float sum = 0.0f;
            for (float sample : data_) {
                sum += sample * sample;
            }
            return std::sqrt(sum / data_.size());
        }
        
        float getPeakLevel() const {
            float peak = 0.0f;
            for (float sample : data_) {
                peak = std::max(peak, std::abs(sample));
            }
            return peak;
        }
        
    private:
        std::vector<float> data_;
        int numSamples_, numChannels_;
    };

    // Audio device abstraction for rapid prototyping
    class AudioDevice {
    public:
        struct Config {
            int sampleRate = 44100;
            int bufferSize = 512;
            int inputChannels = 2;
            int outputChannels = 2;
        };
        
        using AudioCallback = std::function<void(AudioBuffer& input, AudioBuffer& output)>;
        
        virtual ~AudioDevice() = default;
        
        virtual bool initialize(const Config& config) = 0;
        virtual bool start() = 0;
        virtual bool stop() = 0;
        virtual void setCallback(AudioCallback callback) = 0;
        
        virtual std::vector<std::string> getAvailableDevices() = 0;
        virtual bool setActiveDevice(const std::string& deviceName) = 0;
        
        Config getConfig() const { return config_; }
        
    protected:
        Config config_;
        AudioCallback audioCallback_;
    };

    // Simple file-based audio device for testing (no real hardware)
    class FileAudioDevice : public AudioDevice {
    public:
        bool initialize(const Config& config) override {
            config_ = config;
            inputBuffer_ = std::make_unique<AudioBuffer>(config.bufferSize, config.inputChannels);
            outputBuffer_ = std::make_unique<AudioBuffer>(config.bufferSize, config.outputChannels);
            return true;
        }
        
        bool start() override {
            isRunning_ = true;
            return true;
        }
        
        bool stop() override {
            isRunning_ = false;
            return true;
        }
        
        void setCallback(AudioCallback callback) override {
            audioCallback_ = callback;
        }
        
        std::vector<std::string> getAvailableDevices() override {
            return {"File Audio Device (Test)"};
        }
        
        bool setActiveDevice(const std::string& deviceName) override {
            return deviceName == "File Audio Device (Test)";
        }
        
        // Simulate processing a block of audio
        void processBlock() {
            if (isRunning_ && audioCallback_) {
                inputBuffer_->clear();
                outputBuffer_->clear();
                audioCallback_(*inputBuffer_, *outputBuffer_);
            }
        }
        
    private:
        bool isRunning_ = false;
        std::unique_ptr<AudioBuffer> inputBuffer_;
        std::unique_ptr<AudioBuffer> outputBuffer_;
    };

    // Rapid audio engine for quick prototyping
    class RapidAudioEngine {
    public:
        RapidAudioEngine() {
            // Use file device by default for rapid development
            audioDevice_ = std::make_unique<FileAudioDevice>();
        }
        
        bool initialize(int sampleRate = 44100, int bufferSize = 512) {
            AudioDevice::Config config;
            config.sampleRate = sampleRate;
            config.bufferSize = bufferSize;
            
            return audioDevice_->initialize(config);
        }
        
        bool start() {
            return audioDevice_->start();
        }
        
        bool stop() {
            return audioDevice_->stop();
        }
        
        void setAudioCallback(AudioDevice::AudioCallback callback) {
            audioDevice_->setCallback(callback);
        }
        
        // Quick test - process a single block
        void processTestBlock() {
            if (auto* fileDevice = dynamic_cast<FileAudioDevice*>(audioDevice_.get())) {
                fileDevice->processBlock();
            }
        }
        
        AudioDevice* getDevice() { return audioDevice_.get(); }
        
    private:
        std::unique_ptr<AudioDevice> audioDevice_;
    };

    // Quick audio effect interface for prototyping
    class AudioEffect {
    public:
        virtual ~AudioEffect() = default;
        virtual void process(AudioBuffer& buffer) = 0;
        virtual void reset() {}
        
        void setParameter(const std::string& name, float value) {
            parameters_[name] = value;
        }
        
        float getParameter(const std::string& name) const {
            auto it = parameters_.find(name);
            return it != parameters_.end() ? it->second : 0.0f;
        }
        
    protected:
        std::map<std::string, float> parameters_;
    };

    // Simple gain effect for testing
    class GainEffect : public AudioEffect {
    public:
        GainEffect() {
            parameters_["gain"] = 1.0f;
        }
        
        void process(AudioBuffer& buffer) override {
            float gain = getParameter("gain");
            
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                float* channelData = buffer.getWritePointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i) {
                    channelData[i] *= gain;
                }
            }
        }
    };

    // Utility functions for rapid development
    void generateTestTone(AudioBuffer& buffer, float frequency, float amplitude = 0.5f);
    bool validateAudioBuffer(const AudioBuffer& buffer);

} // namespace mixmind::rapid