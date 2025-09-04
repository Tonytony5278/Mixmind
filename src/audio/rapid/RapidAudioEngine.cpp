#include "RapidAudioEngine.h"
#include <iostream>
#include <cmath>

namespace mixmind::rapid {

// Implementation is header-only for rapid development
// This file exists to ensure the header compiles correctly

void generateTestTone(AudioBuffer& buffer, float frequency, float amplitude) {
    const int sampleRate = 44100; // Assume standard rate for testing
    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* channelData = buffer.getWritePointer(ch);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            float sample = amplitude * std::sin(2.0f * M_PI * frequency * i / sampleRate);
            channelData[i] = sample;
        }
    }
}

bool validateAudioBuffer(const AudioBuffer& buffer) {
    // Check for NaN or infinite values
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        const float* channelData = buffer.getReadPointer(ch);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            if (!std::isfinite(channelData[i])) {
                return false;
            }
        }
    }
    
    return true;
}

} // namespace mixmind::rapid