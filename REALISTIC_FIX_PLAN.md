# Realistic Fix Plan - From Zero to Working DAW

## Phase 1: Get Audio I/O Working (Days 1-3)

### Task 1.1: Simple PortAudio Implementation
**File:** `src/audio/SimpleAudioEngine.cpp`

```cpp
#include <portaudio.h>
#include <vector>
#include <iostream>

class SimpleAudioEngine {
private:
    PaStream* stream = nullptr;
    
public:
    bool initialize() {
        if (Pa_Initialize() != paNoError) {
            std::cerr << "Failed to initialize PortAudio\n";
            return false;
        }
        
        PaStreamParameters outputParameters;
        outputParameters.device = Pa_GetDefaultOutputDevice();
        if (outputParameters.device == paNoDevice) {
            std::cerr << "No default output device.\n";
            return false;
        }
        
        outputParameters.channelCount = 2;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = 0.05; // 50ms is realistic
        outputParameters.hostApiSpecificStreamInfo = nullptr;
        
        // Open stream
        PaError err = Pa_OpenStream(
            &stream,
            nullptr, // no input
            &outputParameters,
            44100, // sample rate
            256,   // frames per buffer - realistic size
            paClipOff,
            audioCallback,
            this
        );
        
        if (err != paNoError) {
            std::cerr << "Failed to open stream: " << Pa_GetErrorText(err) << "\n";
            return false;
        }
        
        return true;
    }
    
    bool start() {
        return Pa_StartStream(stream) == paNoError;
    }
    
    static int audioCallback(const void* input, void* output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData) {
        float* out = (float*)output;
        
        // Generate simple sine wave for testing
        static float phase = 0.0f;
        for (unsigned long i = 0; i < frameCount; i++) {
            float sample = sin(phase) * 0.1f; // quiet
            *out++ = sample; // left
            *out++ = sample; // right
            phase += 0.02f;
            if (phase > 2 * M_PI) phase -= 2 * M_PI;
        }
        
        return paContinue;
    }
};
```

### Task 1.2: Build Just This
```batch
# Simple build command
cl /EHsc src/audio/SimpleAudioEngine.cpp /I include /link portaudio_x64.lib
```

## Phase 2: Simple File Operations (Days 4-5)

### Task 2.1: WAV File Writer That Works
```cpp
class SimpleWavWriter {
public:
    static bool write(const std::string& filename, 
                     const std::vector<float>& data, 
                     int sampleRate) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;
        
        // WAV header
        int dataSize = data.size() * 2; // 16-bit samples
        int fileSize = dataSize + 36;
        
        file.write("RIFF", 4);
        file.write((char*)&fileSize, 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        int fmtSize = 16;
        file.write((char*)&fmtSize, 4);
        short format = 1;
        file.write((char*)&format, 2);
        short channels = 1;
        file.write((char*)&channels, 2);
        file.write((char*)&sampleRate, 4);
        int byteRate = sampleRate * 2;
        file.write((char*)&byteRate, 4);
        short blockAlign = 2;
        file.write((char*)&blockAlign, 2);
        short bitsPerSample = 16;
        file.write((char*)&bitsPerSample, 2);
        file.write("data", 4);
        file.write((char*)&dataSize, 4);
        
        // Convert and write samples
        for (float sample : data) {
            short pcm = (short)(sample * 32767.0f);
            file.write((char*)&pcm, 2);
        }
        
        return true;
    }
};
```

## Phase 3: Basic Recording (Days 6-7)

### Task 3.1: Simple Recorder
```cpp
class SimpleRecorder {
private:
    std::vector<float> recordBuffer;
    bool isRecording = false;
    
public:
    void startRecording() {
        recordBuffer.clear();
        isRecording = true;
    }
    
    void stopRecording() {
        isRecording = false;
    }
    
    void processAudio(const float* input, int frameCount) {
        if (isRecording && input) {
            for (int i = 0; i < frameCount; i++) {
                recordBuffer.push_back(input[i]);
            }
        }
    }
    
    bool saveRecording(const std::string& filename) {
        return SimpleWavWriter::write(filename, recordBuffer, 44100);
    }
};
```

## Phase 4: Basic MIDI (Days 8-10)

### Task 4.1: Simple MIDI Note
```cpp
struct SimpleMidiNote {
    int pitch;      // 0-127
    int velocity;   // 0-127  
    float startTime; // in seconds
    float duration;  // in seconds
};

class SimpleMidiTrack {
private:
    std::vector<SimpleMidiNote> notes;
    
public:
    void addNote(int pitch, int velocity, float start, float duration) {
        notes.push_back({pitch, velocity, start, duration});
    }
    
    void clear() {
        notes.clear();
    }
    
    // Render to audio
    std::vector<float> renderToAudio(int sampleRate,