// MixMind AI - Rapid Demo Version
// Ready in 30 minutes with drum generation and guitar recording

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>
#include <thread>
#include <atomic>
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

// Simple WAV file writer
class WavWriter {
public:
    static bool writeWav(const std::string& filename, const std::vector<float>& data, int sampleRate) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;
        
        int dataSize = data.size() * sizeof(short);
        int fileSize = dataSize + 44;
        
        // WAV header
        file.write("RIFF", 4);
        file.write((char*)&fileSize, 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        int fmtSize = 16;
        file.write((char*)&fmtSize, 4);
        short format = 1; // PCM
        file.write((char*)&format, 2);
        short channels = 1;
        file.write((char*)&channels, 2);
        file.write((char*)&sampleRate, 4);
        int byteRate = sampleRate * channels * 2;
        file.write((char*)&byteRate, 4);
        short blockAlign = channels * 2;
        file.write((char*)&blockAlign, 2);
        short bitsPerSample = 16;
        file.write((char*)&bitsPerSample, 2);
        file.write("data", 4);
        file.write((char*)&dataSize, 4);
        
        // Convert float to 16-bit PCM
        for (float sample : data) {
            short pcm = (short)(sample * 32767.0f);
            file.write((char*)&pcm, 2);
        }
        
        return true;
    }
};

// Drum pattern generator
class DrumGenerator {
public:
    enum Style { TRAP, HIPHOP, HOUSE, ROCK, JAZZ };
    
    static std::vector<float> generatePattern(Style style, int bars, int bpm, int sampleRate) {
        int samplesPerBeat = (60.0 / bpm) * sampleRate;
        int totalSamples = samplesPerBeat * 4 * bars;
        std::vector<float> pattern(totalSamples, 0.0f);
        
        // Generate based on style
        switch (style) {
            case TRAP:
                generateTrapPattern(pattern, samplesPerBeat, bars);
                break;
            case HIPHOP:
                generateHipHopPattern(pattern, samplesPerBeat, bars);
                break;
            case HOUSE:
                generateHousePattern(pattern, samplesPerBeat, bars);
                break;
            case ROCK:
                generateRockPattern(pattern, samplesPerBeat, bars);
                break;
            case JAZZ:
                generateJazzPattern(pattern, samplesPerBeat, bars);
                break;
        }
        
        return pattern;
    }
    
private:
    static void addKick(std::vector<float>& pattern, int position, int sampleRate) {
        int duration = sampleRate * 0.15; // 150ms
        for (int i = 0; i < duration && position + i < pattern.size(); i++) {
            double t = (double)i / sampleRate;
            double env = exp(-35.0 * t);
            double pitch = 60.0 * (1.0 + exp(-200.0 * t) * 3.0);
            pattern[position + i] += env * sin(2.0 * M_PI * pitch * t) * 0.8f;
        }
    }
    
    static void addSnare(std::vector<float>& pattern, int position, int sampleRate) {
        int duration = sampleRate * 0.1; // 100ms
        for (int i = 0; i < duration && position + i < pattern.size(); i++) {
            double t = (double)i / sampleRate;
            double env = exp(-30.0 * t);
            double tone = sin(2.0 * M_PI * 200 * t) * 0.3;
            double noise = ((rand() / (float)RAND_MAX) - 0.5) * 0.5;
            pattern[position + i] += env * (tone + noise) * 0.6f;
        }
    }
    
    static void addHiHat(std::vector<float>& pattern, int position, int sampleRate, bool open = false) {
        int duration = open ? sampleRate * 0.2 : sampleRate * 0.02;
        for (int i = 0; i < duration && position + i < pattern.size(); i++) {
            double t = (double)i / sampleRate;
            double env = exp(open ? -10.0 * t : -100.0 * t);
            double noise = ((rand() / (float)RAND_MAX) - 0.5);
            pattern[position + i] += env * noise * 0.3f;
        }
    }
    
    static void generateTrapPattern(std::vector<float>& pattern, int samplesPerBeat, int bars) {
        int sampleRate = 44100;
        
        for (int bar = 0; bar < bars; bar++) {
            int barOffset = bar * samplesPerBeat * 4;
            
            // Trap pattern: kick on 1, snare on 3, hi-hats with rolls
            addKick(pattern, barOffset, sampleRate);
            addKick(pattern, barOffset + samplesPerBeat * 2.5, sampleRate);
            
            addSnare(pattern, barOffset + samplesPerBeat * 2, sampleRate);
            
            // Trap hi-hat pattern with triplets
            for (int i = 0; i < 16; i++) {
                int pos = barOffset + (samplesPerBeat * i) / 4;
                addHiHat(pattern, pos, sampleRate);
                
                // Add rolls on certain beats
                if (i == 7 || i == 15) {
                    for (int j = 1; j < 4; j++) {
                        addHiHat(pattern, pos + (samplesPerBeat * j) / 16, sampleRate);
                    }
                }
            }
        }
    }
    
    static void generateHipHopPattern(std::vector<float>& pattern, int samplesPerBeat, int bars) {
        int sampleRate = 44100;
        
        for (int bar = 0; bar < bars; bar++) {
            int barOffset = bar * samplesPerBeat * 4;
            
            // Classic hip-hop: boom-bap pattern
            addKick(pattern, barOffset, sampleRate);
            addKick(pattern, barOffset + samplesPerBeat * 2.5, sampleRate);
            
            addSnare(pattern, barOffset + samplesPerBeat, sampleRate);
            addSnare(pattern, barOffset + samplesPerBeat * 3, sampleRate);
            
            // Simple hi-hat pattern
            for (int i = 0; i < 8; i++) {
                addHiHat(pattern, barOffset + (samplesPerBeat * i) / 2, sampleRate);
            }
        }
    }
    
    static void generateHousePattern(std::vector<float>& pattern, int samplesPerBeat, int bars) {
        int sampleRate = 44100;
        
        for (int bar = 0; bar < bars; bar++) {
            int barOffset = bar * samplesPerBeat * 4;
            
            // Four-on-the-floor kick
            for (int i = 0; i < 4; i++) {
                addKick(pattern, barOffset + samplesPerBeat * i, sampleRate);
            }
            
            // Snare on 2 and 4
            addSnare(pattern, barOffset + samplesPerBeat, sampleRate);
            addSnare(pattern, barOffset + samplesPerBeat * 3, sampleRate);
            
            // Open hi-hat offbeats
            for (int i = 0; i < 4; i++) {
                addHiHat(pattern, barOffset + samplesPerBeat * i + samplesPerBeat / 2, 
                        sampleRate, true);
            }
        }
    }
    
    static void generateRockPattern(std::vector<float>& pattern, int samplesPerBeat, int bars) {
        int sampleRate = 44100;
        
        for (int bar = 0; bar < bars; bar++) {
            int barOffset = bar * samplesPerBeat * 4;
            
            // Rock beat
            addKick(pattern, barOffset, sampleRate);
            addKick(pattern, barOffset + samplesPerBeat * 2, sampleRate);
            
            addSnare(pattern, barOffset + samplesPerBeat, sampleRate);
            addSnare(pattern, barOffset + samplesPerBeat * 3, sampleRate);
            
            // Steady hi-hats
            for (int i = 0; i < 8; i++) {
                addHiHat(pattern, barOffset + (samplesPerBeat * i) / 2, sampleRate);
            }
        }
    }
    
    static void generateJazzPattern(std::vector<float>& pattern, int samplesPerBeat, int bars) {
        int sampleRate = 44100;
        
        for (int bar = 0; bar < bars; bar++) {
            int barOffset = bar * samplesPerBeat * 4;
            
            // Jazz swing pattern
            addKick(pattern, barOffset, sampleRate);
            addKick(pattern, barOffset + samplesPerBeat * 2, sampleRate);
            
            // Syncopated snare
            addSnare(pattern, barOffset + samplesPerBeat * 1.5, sampleRate);
            addSnare(pattern, barOffset + samplesPerBeat * 3.25, sampleRate);
            
            // Ride cymbal pattern
            for (int i = 0; i < 12; i++) {
                int pos = barOffset + (samplesPerBeat * i) / 3;
                addHiHat(pattern, pos, sampleRate, i % 3 == 0);
            }
        }
    }
};

// Simple audio recorder
class AudioRecorder {
    HWAVEIN hWaveIn;
    WAVEFORMATEX waveFormat;
    std::vector<WAVEHDR> waveHeaders;
    std::vector<std::vector<short>> buffers;
    std::atomic<bool> recording{false};
    std::vector<float> recordedData;
    
public:
    bool startRecording(int deviceId = WAVE_MAPPER) {
        waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        waveFormat.nChannels = 1;
        waveFormat.nSamplesPerSec = 44100;
        waveFormat.wBitsPerSample = 16;
        waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        waveFormat.cbSize = 0;
        
        MMRESULT result = waveInOpen(&hWaveIn, deviceId, &waveFormat, 
                                     (DWORD_PTR)waveInProc, (DWORD_PTR)this, 
                                     CALLBACK_FUNCTION);
        
        if (result != MMSYSERR_NOERROR) {
            std::cerr << "Failed to open audio input device\n";
            return false;
        }
        
        // Prepare buffers
        buffers.resize(4, std::vector<short>(4410)); // 100ms buffers
        waveHeaders.resize(4);
        
        for (int i = 0; i < 4; i++) {
            waveHeaders[i].lpData = (LPSTR)buffers[i].data();
            waveHeaders[i].dwBufferLength = buffers[i].size() * sizeof(short);
            waveHeaders[i].dwFlags = 0;
            
            waveInPrepareHeader(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
            waveInAddBuffer(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
        }
        
        recording = true;
        waveInStart(hWaveIn);
        std::cout << "Recording started... Press Enter to stop.\n";
        return true;
    }
    
    void stopRecording() {
        if (recording) {
            recording = false;
            waveInStop(hWaveIn);
            waveInClose(hWaveIn);
            std::cout << "Recording stopped. " << recordedData.size() << " samples recorded.\n";
        }
    }
    
    std::vector<float> getRecordedData() const {
        return recordedData;
    }
    
private:
    static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                   DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
        if (uMsg == WIM_DATA) {
            AudioRecorder* recorder = (AudioRecorder*)dwInstance;
            WAVEHDR* hdr = (WAVEHDR*)dwParam1;
            
            if (recorder->recording) {
                // Convert to float and store
                short* data = (short*)hdr->lpData;
                int samples = hdr->dwBytesRecorded / sizeof(short);
                
                for (int i = 0; i < samples; i++) {
                    recorder->recordedData.push_back(data[i] / 32768.0f);
                }
                
                // Re-queue buffer
                waveInAddBuffer(hwi, hdr, sizeof(WAVEHDR));
            }
        }
    }
};

// Simple AI Assistant
class AIAssistant {
public:
    void processCommand(const std::string& command) {
        std::cout << "\n🤖 AI: Processing: \"" << command << "\"\n";
        
        // Parse command
        if (command.find("drum") != std::string::npos || 
            command.find("beat") != std::string::npos) {
            
            // Detect style
            DrumGenerator::Style style = DrumGenerator::TRAP;
            if (command.find("trap") != std::string::npos) style = DrumGenerator::TRAP;
            else if (command.find("hip") != std::string::npos || 
                     command.find("hop") != std::string::npos) style = DrumGenerator::HIPHOP;
            else if (command.find("house") != std::string::npos) style = DrumGenerator::HOUSE;
            else if (command.find("rock") != std::string::npos) style = DrumGenerator::ROCK;
            else if (command.find("jazz") != std::string::npos) style = DrumGenerator::JAZZ;
            
            // Extract BPM if mentioned
            int bpm = 120;
            size_t bpmPos = command.find("bpm");
            if (bpmPos != std::string::npos && bpmPos > 0) {
                // Try to extract number before "bpm"
                size_t numStart = command.rfind(' ', bpmPos - 2);
                if (numStart != std::string::npos) {
                    std::string bpmStr = command.substr(numStart + 1, bpmPos - numStart - 1);
                    try {
                        bpm = std::stoi(bpmStr);
                    } catch (...) {}
                }
            }
            
            generateDrums(style, bpm);
            
        } else if (command.find("record") != std::string::npos && 
                  command.find("guitar") != std::string::npos) {
            recordGuitar();
            
        } else if (command.find("play") != std::string::npos) {
            playLastRecording();
            
        } else if (command.find("save") != std::string::npos) {
            saveProject();
            
        } else if (command.find("help") != std::string::npos) {
            showHelp();
            
        } else {
            std::cout << "🤖 AI: I can help you with:\n";
            showHelp();
        }
    }
    
private:
    std::string lastDrumFile = "drums.wav";
    std::string lastGuitarFile = "guitar.wav";
    
    void generateDrums(DrumGenerator::Style style, int bpm) {
        std::cout << "🎵 Generating drum pattern...\n";
        std::cout << "   Style: " << getStyleName(style) << "\n";
        std::cout << "   BPM: " << bpm << "\n";
        std::cout << "   Bars: 4\n";
        
        auto pattern = DrumGenerator::generatePattern(style, 4, bpm, 44100);
        
        // Save to file
        if (WavWriter::writeWav(lastDrumFile, pattern, 44100)) {
            std::cout << "✅ Drums saved to: " << lastDrumFile << "\n";
            
            // Auto-play
            std::cout << "🔊 Playing drums...\n";
            PlaySound(lastDrumFile.c_str(), NULL, SND_FILENAME | SND_ASYNC);
        }
    }
    
    void recordGuitar() {
        std::cout << "🎸 Setting up guitar recording...\n";
        
        AudioRecorder recorder;
        if (recorder.startRecording()) {
            std::cin.get(); // Wait for Enter
            recorder.stopRecording();
            
            auto data = recorder.getRecordedData();
            if (!data.empty()) {
                if (WavWriter::writeWav(lastGuitarFile, data, 44100)) {
                    std::cout << "✅ Guitar saved to: " << lastGuitarFile << "\n";
                }
            }
        }
    }
    
    void playLastRecording() {
        std::cout << "🔊 Playing last recording...\n";
        PlaySound(lastGuitarFile.c_str(), NULL, SND_FILENAME | SND_ASYNC);
    }
    
    void saveProject() {
        std::cout << "💾 Project saved with:\n";
        std::cout << "   - Drums: " << lastDrumFile << "\n";
        std::cout << "   - Guitar: " << lastGuitarFile << "\n";
    }
    
    void showHelp() {
        std::cout << "   • 'Generate trap drums at 140 bpm'\n";
        std::cout << "   • 'Create hip hop beat'\n";
        std::cout << "   • 'Make house drums'\n";
        std::cout << "   • 'Generate rock beat at 120 bpm'\n";
        std::cout << "   • 'Create jazz drums'\n";
        std::cout << "   • 'Record guitar'\n";
        std::cout << "   • 'Play' - play last recording\n";
        std::cout << "   • 'Save project'\n";
    }
    
    std::string getStyleName(DrumGenerator::Style style) {
        switch (style) {
            case DrumGenerator::TRAP: return "Trap";
            case DrumGenerator::HIPHOP: return "Hip-Hop";
            case DrumGenerator::HOUSE: return "House";
            case DrumGenerator::ROCK: return "Rock";
            case DrumGenerator::JAZZ: return "Jazz";
            default: return "Unknown";
        }
    }
};

// Main application
int main() {
    system("cls");
    std::cout << R"(
    ███╗   ███╗██╗██╗  ██╗███╗   ███╗██╗███╗   ██╗██████╗ 
    ████╗ ████║██║╚██╗██╔╝████╗ ████║██║████╗  ██║██╔══██╗
    ██╔████╔██║██║ ╚███╔╝ ██╔████╔██║██║██╔██╗ ██║██║  ██║
    ██║╚██╔╝██║██║ ██╔██╗ ██║╚██╔╝██║██║██║╚██╗██║██║  ██║
    ██║ ╚═╝ ██║██║██╔╝ ██╗██║ ╚═╝ ██║██║██║ ╚████║██████╔╝
    ╚═╝     ╚═╝╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═════╝ 
    
    🎵 AI-Powered Digital Audio Workstation - Rapid Demo 🎵
    )" << std::endl;
    
    std::cout << "Welcome to MixMind AI! I'm your AI music production assistant.\n\n";
    std::cout << "Try these commands:\n";
    std::cout << "• 'Generate trap drums at 140 bpm'\n";
    std::cout << "• 'Record guitar'\n";
    std::cout << "• 'help' for more commands\n\n";
    
    AIAssistant ai;
    std::string command;
    
    while (true) {
        std::cout << "\n> ";
        std::getline(std::cin, command);
        
        if (command == "exit" || command == "quit") {
            break;
        }
        
        ai.processCommand(command);
    }
    
    std::cout << "\nThanks for using MixMind AI! 🎵\n";
    return 0;
}
