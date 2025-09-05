#include <iostream>
#include <memory>
#include "../audio/generators/AudioGenerator.h"
#include "../audio/WAVWriter.h"
#include "../core/types.h"

using namespace mixmind;
using namespace mixmind::audio;

/**
 * MixMind AI - Real Audio Generation Demo
 * 
 * This demonstrates the actual working audio engine by:
 * 1. Generating real drum sounds (kick, snare, hihat)
 * 2. Generating real bass sounds (sub bass synth)
 * 3. Mixing them together
 * 4. Writing a real WAV file you can play
 */

int main() {
    std::cout << "\n=== MixMind AI - Real Audio Generation Demo ===\n\n";

    // Audio configuration
    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 2;
    const double TEMPO = 120.0;
    const int BARS = 8;
    
    std::cout << "Generating 8-bar demo at 120 BPM...\n";
    std::cout << "Sample Rate: " << SAMPLE_RATE << " Hz\n";
    std::cout << "Channels: " << CHANNELS << " (stereo)\n\n";

    try {
        // Create generator parameters
        GeneratorParams params;
        params.sampleRate = SAMPLE_RATE;
        params.channels = CHANNELS;
        params.tempo = TEMPO;
        params.bars = BARS;
        params.beatsPerBar = 4;
        params.volume = 0.7f;

        // Calculate total length
        double totalDuration = (60.0 / TEMPO) * 4.0 * BARS;  // 4 beats per bar
        int totalSamples = static_cast<int>(totalDuration * SAMPLE_RATE);
        std::cout << "Total duration: " << totalDuration << " seconds\n";
        std::cout << "Total samples: " << totalSamples << "\n\n";

        // Create master mix buffer
        FloatAudioBuffer masterMix(CHANNELS, totalSamples);
        masterMix.clear();

        // === DRUM GENERATION ===
        std::cout << "1. Generating drums...\n";
        
        // Create drum generator
        DrumGenerator::DrumParams drumParams;
        drumParams.velocity = 0.8f;
        drumParams.drumType = DrumGenerator::DrumType::Kick;
        
        auto drumGen = GeneratorFactory::createDrumGenerator(drumParams);
        
        // Generate kick drum pattern
        std::cout << "   - Generating kick pattern...\n";
        auto kickBuffer = drumGen->generate(params);
        
        // Mix kick into master
        for (int sample = 0; sample < totalSamples && sample < kickBuffer.getNumSamples(); ++sample) {
            for (int ch = 0; ch < CHANNELS; ++ch) {
                float existing = masterMix.getSample(sample, ch);
                float kick = kickBuffer.getSample(sample, ch);
                masterMix.setSample(sample, ch, existing + kick * 0.6f);  // Mix at 60%
            }
        }

        // Generate snare
        drumParams.drumType = DrumGenerator::DrumType::Snare;
        drumParams.velocity = 0.7f;
        auto snareGen = GeneratorFactory::createDrumGenerator(drumParams);
        
        std::cout << "   - Generating snare pattern...\n";
        auto snareBuffer = snareGen->generate(params);
        
        // Mix snare into master
        for (int sample = 0; sample < totalSamples && sample < snareBuffer.getNumSamples(); ++sample) {
            for (int ch = 0; ch < CHANNELS; ++ch) {
                float existing = masterMix.getSample(sample, ch);
                float snare = snareBuffer.getSample(sample, ch);
                masterMix.setSample(sample, ch, existing + snare * 0.5f);  // Mix at 50%
            }
        }

        // Generate hi-hat
        drumParams.drumType = DrumGenerator::DrumType::HiHat;
        drumParams.velocity = 0.4f;
        auto hihatGen = GeneratorFactory::createDrumGenerator(drumParams);
        
        std::cout << "   - Generating hi-hat pattern...\n";
        auto hihatBuffer = hihatGen->generate(params);
        
        // Mix hi-hat into master
        for (int sample = 0; sample < totalSamples && sample < hihatBuffer.getNumSamples(); ++sample) {
            for (int ch = 0; ch < CHANNELS; ++ch) {
                float existing = masterMix.getSample(sample, ch);
                float hihat = hihatBuffer.getSample(sample, ch);
                masterMix.setSample(sample, ch, existing + hihat * 0.3f);  // Mix at 30%
            }
        }

        // === BASS GENERATION ===
        std::cout << "\n2. Generating bass...\n";
        
        BassGenerator::BassParams bassParams;
        bassParams.bassType = BassGenerator::BassType::SynthBass;
        bassParams.rootNote = 36;  // C2
        bassParams.velocity = 0.7f;
        
        auto bassGen = GeneratorFactory::createBassGenerator(bassParams);
        
        std::cout << "   - Generating synth bass line...\n";
        auto bassBuffer = bassGen->generate(params);
        
        // Mix bass into master
        for (int sample = 0; sample < totalSamples && sample < bassBuffer.getNumSamples(); ++sample) {
            for (int ch = 0; ch < CHANNELS; ++ch) {
                float existing = masterMix.getSample(sample, ch);
                float bass = bassBuffer.getSample(sample, ch);
                masterMix.setSample(sample, ch, existing + bass * 0.4f);  // Mix at 40%
            }
        }

        // === AUDIO ANALYSIS ===
        std::cout << "\n3. Analyzing generated audio...\n";
        
        float peakLevel = 0.0f;
        float rmsLevel = 0.0f;
        float sumSquares = 0.0f;
        
        for (int sample = 0; sample < totalSamples; ++sample) {
            for (int ch = 0; ch < CHANNELS; ++ch) {
                float sampleValue = masterMix.getSample(sample, ch);
                peakLevel = std::max(peakLevel, std::abs(sampleValue));
                sumSquares += sampleValue * sampleValue;
            }
        }
        
        rmsLevel = std::sqrt(sumSquares / (totalSamples * CHANNELS));
        float rmsDb = 20.0f * std::log10(rmsLevel);
        float peakDb = 20.0f * std::log10(peakLevel);
        
        std::cout << "   - Peak level: " << peakDb << " dBFS\n";
        std::cout << "   - RMS level: " << rmsDb << " dBFS\n";
        std::cout << "   - Dynamic range: " << (peakDb - rmsDb) << " dB\n";

        // === WAV FILE EXPORT ===
        std::cout << "\n4. Exporting to WAV file...\n";
        
        WAVWriter writer;
        std::string filename = "mixmind_demo_8bars_120bpm.wav";
        
        bool success = writer.writeWAV(filename, masterMix, SAMPLE_RATE, WAVWriter::BitDepth::Bit16);
        
        if (success) {
            std::cout << "✅ SUCCESS: Audio exported to '" << filename << "'\n";
            std::cout << "\n=== DEMO COMPLETE ===\n";
            std::cout << "You can now play '" << filename << "' in any audio player!\n";
            std::cout << "This proves MixMind AI generates REAL AUDIO, not just architecture.\n\n";
        } else {
            std::cout << "❌ FAILED to export WAV: " << writer.getLastError() << "\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}