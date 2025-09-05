# CLAUDE CODE - COMPLETE DAW IMPLEMENTATION TASKS

## 🔥 CRITICAL PATH TO COMPLETE DAW

### PHASE 1: GET EVERYTHING COMPILING (30 minutes)

#### Task 1.1: Fix Dependencies
Location: `CMakeLists.txt`

```cmake
# LINE 90 - Fix Tracktion Engine version
CHANGE:
GIT_TAG develop
TO:
GIT_TAG v1.2.0

# LINE 150-160 - Fix library linking
CHANGE:
target_link_libraries(mixmind_services PUBLIC SoundTouch)
TO:
target_link_libraries(mixmind_services PUBLIC soundtouch::SoundTouch)

CHANGE:
target_link_libraries(mixmind_services PUBLIC tag)
TO:
target_link_libraries(mixmind_services PUBLIC TagLib::tag)

CHANGE:
target_link_libraries(mixmind_services PUBLIC kissfft)
TO:
target_link_libraries(mixmind_services PUBLIC kissfft::kissfft)
```

#### Task 1.2: Add PortAudio for Real-time Audio
Location: `CMakeLists.txt` (after line 200)

```cmake
# Add PortAudio for real-time audio I/O
FetchContent_Declare(
    portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG v19.7.0
    SOURCE_DIR ${DEPS_DIR}/portaudio
)
FetchContent_MakeAvailable(portaudio)
```

#### Task 1.3: Build and Test
```bash
cd C:\Users\antoi\Desktop\reaper-ai-pilot
mkdir build_complete
cmake -S . -B build_complete -G "Visual Studio 16 2019" -A x64 -DMIXMIND_LEVEL_FULL=ON
cmake --build build_complete --config Release
```

---

### PHASE 2: IMPLEMENT REAL-TIME AUDIO ENGINE (45 minutes)

#### Task 2.1: Create Complete Audio Engine
Location: `src/audio/RealtimeAudioEngine.cpp` (CREATE NEW)

```cpp
#include "RealtimeAudioEngine.h"
#include <portaudio.h>
#include <atomic>
#include <vector>
#include <memory>

namespace mixmind::audio {

class RealtimeAudioEngine::Impl {
private:
    PaStream* stream = nullptr;
    std::atomic<bool> isRunning{false};
    std::atomic<float> masterVolume{1.0f};
    
    // Audio buffers
    std::vector<std::unique_ptr<AudioTrack>> tracks;
    std::unique_ptr<MasterBus> masterBus;
    
    // VST3 host
    std::unique_ptr<VST3Host> vstHost;
    
    // AI processor
    std::unique_ptr<AIAudioProcessor> aiProcessor;
    
public:
    Result<void> initialize() {
        // Initialize PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            return Error("Failed to initialize audio: " + std::string(Pa_GetErrorText(err)));
        }
        
        // Open audio stream with low latency
        PaStreamParameters outputParameters;
        outputParameters.device = Pa_GetDefaultOutputDevice();
        outputParameters.channelCount = 2;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = 0.003; // 3ms latency
        outputParameters.hostApiSpecificStreamInfo = nullptr;
        
        err = Pa_OpenStream(
            &stream,
            nullptr, // No input for now
            &outputParameters,
            44100,   // Sample rate
            64,      // Buffer size (ultra-low latency)
            paClipOff,
            audioCallback,
            this
        );
        
        if (err != paNoError) {
            Pa_Terminate();
            return Error("Failed to open audio stream");
        }
        
        // Initialize VST3 host
        vstHost = std::make_unique<VST3Host>();
        vstHost->scanPlugins();
        
        // Initialize AI processor
        aiProcessor = std::make_unique<AIAudioProcessor>();
        aiProcessor->initialize();
        
        // Create master bus with AI analysis
        masterBus = std::make_unique<MasterBus>();
        masterBus->addAnalyzer(aiProcessor.get());
        
        return Ok();
    }
    
    Result<void> start() {
        PaError err = Pa_StartStream(stream);
        if (err != paNoError) {
            return Error("Failed to start audio stream");
        }
        isRunning = true;
        return Ok();
    }
    
    // Real-time audio callback
    static int audioCallback(const void* input, void* output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData) {
        auto* engine = static_cast<Impl*>(userData);
        float* out = static_cast<float*>(output);
        
        // Clear output buffer
        std::memset(out, 0, frameCount * 2 * sizeof(float));
        
        // Mix all tracks
        for (auto& track : engine->tracks) {
            if (track->isActive()) {
                track->process(out, frameCount);
            }
        }
        
        // Process through master bus (includes AI analysis)
        engine->masterBus->process(out, frameCount);
        
        // Apply master volume
        float vol = engine->masterVolume.load();
        for (size_t i = 0; i < frameCount * 2; ++i) {
            out[i] *= vol;
        }
        
        // AI real-time analysis
        engine->aiProcessor->analyzeRealtime(out, frameCount);
        
        return paContinue;
    }
    
    // VST3 plugin loading
    Result<void> loadVST3Plugin(const std::string& pluginPath, int trackIndex) {
        auto plugin = vstHost->loadPlugin(pluginPath);
        if (!plugin) {
            return Error("Failed to load VST3 plugin");
        }
        
        if (trackIndex < tracks.size()) {
            tracks[trackIndex]->addPlugin(std::move(plugin));
        }
        
        return Ok();
    }
    
    // AI-driven mixing
    void applyAIMixingSuggestion(const MixingSuggestion& suggestion) {
        // Apply AI suggestion in real-time
        if (suggestion.type == MixingSuggestion::EQ_ADJUSTMENT) {
            // Apply EQ changes
            for (auto& [param, value] : suggestion.parameters) {
                masterBus->setEQParameter(param, value);
            }
        } else if (suggestion.type == MixingSuggestion::COMPRESSION_SETTING) {
            masterBus->setCompressorSettings(suggestion.parameters);
        }
    }
};

RealtimeAudioEngine::RealtimeAudioEngine() : pImpl(std::make_unique<Impl>()) {}
RealtimeAudioEngine::~RealtimeAudioEngine() = default;

Result<void> RealtimeAudioEngine::initialize() { return pImpl->initialize(); }
Result<void> RealtimeAudioEngine::start() { return pImpl->start(); }

} // namespace mixmind::audio
```

---

### PHASE 3: IMPLEMENT COMPLETE AI SYSTEM (1 hour)

#### Task 3.1: OpenAI Integration
Location: `src/ai/OpenAIIntegration.cpp` (CREATE NEW)

```cpp
#include "OpenAIIntegration.h"
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace mixmind::ai {

class OpenAIIntegration::Impl {
private:
    httplib::Client client{"https://api.openai.com"};
    std::string apiKey;
    
public:
    void setAPIKey(const std::string& key) {
        apiKey = key;
    }
    
    // GPT-4 for natural language understanding
    AsyncResult<AIResponse> processNaturalLanguage(const std::string& input) {
        return async([=]() -> Result<AIResponse> {
            json request = {
                {"model", "gpt-4-turbo-preview"},
                {"messages", {
                    {{"role", "system"}, {"content", 
                        "You are MixMind AI, a professional music production assistant. "
                        "You control a DAW and help users create music. "
                        "Interpret user commands and return structured JSON responses."}},
                    {{"role", "user"}, {"content", input}}
                }},
                {"response_format", {{"type", "json_object"}}},
                {"temperature", 0.3}
            };
            
            httplib::Headers headers = {
                {"Authorization", "Bearer " + apiKey},
                {"Content-Type", "application/json"}
            };
            
            auto res = client.Post("/v1/chat/completions", 
                                 headers, 
                                 request.dump(), 
                                 "application/json");
            
            if (res && res->status == 200) {
                auto response_json = json::parse(res->body);
                auto content = response_json["choices"][0]["message"]["content"];
                
                // Parse AI response into structured format
                auto ai_response = parseAIResponse(json::parse(content));
                return Ok(ai_response);
            }
            
            return Error("OpenAI API request failed");
        });
    }
    
    // Whisper for voice recognition
    AsyncResult<std::string> transcribeAudio(const AudioBuffer& audio) {
        return async([=]() -> Result<std::string> {
            // Convert audio to WAV format
            auto wavData = audioToWav(audio);
            
            httplib::MultipartFormDataItems items = {
                {"file", wavData, "audio.wav", "audio/wav"},
                {"model", "whisper-1"},
                {"language", "en"}
            };
            
            httplib::Headers headers = {
                {"Authorization", "Bearer " + apiKey}
            };
            
            auto res = client.Post("/v1/audio/transcriptions", headers, items);
            
            if (res && res->status == 200) {
                auto response_json = json::parse(res->body);
                return Ok(response_json["text"]);
            }
            
            return Error("Voice transcription failed");
        });
    }
    
    // DALL-E for album art generation
    AsyncResult<std::string> generateAlbumArt(const std::string& prompt) {
        return async([=]() -> Result<std::string> {
            json request = {
                {"model", "dall-e-3"},
                {"prompt", "Album cover art: " + prompt},
                {"n", 1},
                {"size", "1024x1024"},
                {"quality", "hd"}
            };
            
            httplib::Headers headers = {
                {"Authorization", "Bearer " + apiKey},
                {"Content-Type", "application/json"}
            };
            
            auto res = client.Post("/v1/images/generations",
                                 headers,
                                 request.dump(),
                                 "application/json");
            
            if (res && res->status == 200) {
                auto response_json = json::parse(res->body);
                return Ok(response_json["data"][0]["url"]);
            }
            
            return Error("Album art generation failed");
        });
    }
    
private:
    AIResponse parseAIResponse(const json& ai_json) {
        AIResponse response;
        
        // Parse intent
        if (ai_json.contains("action")) {
            response.actions.push_back(ai_json["action"]);
        }
        
        if (ai_json.contains("parameters")) {
            for (auto& [key, value] : ai_json["parameters"].items()) {
                response.parameters[key] = value;
            }
        }
        
        if (ai_json.contains("response")) {
            response.text = ai_json["response"];
        }
        
        if (ai_json.contains("suggestions")) {
            for (const auto& suggestion : ai_json["suggestions"]) {
                response.suggestions.push_back(suggestion);
            }
        }
        
        return response;
    }
};

} // namespace mixmind::ai
```

#### Task 3.2: Voice Control Implementation
Location: `src/ai/VoiceControl.cpp` (CREATE NEW)

```cpp
#include "VoiceControl.h"
#include <thread>
#include <Windows.h>
#include <mmsystem.h>
#include <atomic>

namespace mixmind::ai {

class VoiceControl::Impl {
private:
    std::atomic<bool> isListening{false};
    std::unique_ptr<OpenAIIntegration> openAI;
    std::function<void(const std::string&)> onCommand;
    std::thread listeningThread;
    
    // Audio recording for voice
    HWAVEIN hWaveIn;
    std::vector<WAVEHDR> waveHeaders;
    std::vector<std::vector<short>> buffers;
    std::vector<short> recordedData;
    
public:
    void initialize(OpenAIIntegration* ai) {
        openAI.reset(ai);
    }
    
    void startListening() {
        if (isListening) return;
        
        isListening = true;
        listeningThread = std::thread([this]() {
            listenForVoiceCommands();
        });
    }
    
    void stopListening() {
        isListening = false;
        if (listeningThread.joinable()) {
            listeningThread.join();
        }
    }
    
private:
    void listenForVoiceCommands() {
        // Initialize Windows audio input
        WAVEFORMATEX waveFormat;
        waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        waveFormat.nChannels = 1;
        waveFormat.nSamplesPerSec = 16000; // 16kHz for voice
        waveFormat.wBitsPerSample = 16;
        waveFormat.nBlockAlign = 2;
        waveFormat.nAvgBytesPerSec = 32000;
        waveFormat.cbSize = 0;
        
        waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat,
                  (DWORD_PTR)waveInProc, (DWORD_PTR)this,
                  CALLBACK_FUNCTION);
        
        // Prepare buffers
        buffers.resize(4, std::vector<short>(1600)); // 100ms buffers
        waveHeaders.resize(4);
        
        for (int i = 0; i < 4; i++) {
            waveHeaders[i].lpData = (LPSTR)buffers[i].data();
            waveHeaders[i].dwBufferLength = buffers[i].size() * sizeof(short);
            waveHeaders[i].dwFlags = 0;
            waveInPrepareHeader(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
            waveInAddBuffer(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
        }
        
        waveInStart(hWaveIn);
        
        // Voice activity detection and command processing
        while (isListening) {
            if (detectVoiceActivity()) {
                // Record until silence
                auto audioData = recordVoiceCommand();
                
                // Convert to AudioBuffer
                AudioBuffer buffer(1, audioData.size());
                for (size_t i = 0; i < audioData.size(); i++) {
                    buffer.setSample(i, 0, audioData[i] / 32768.0f);
                }
                
                // Transcribe with Whisper
                auto transcription = openAI->transcribeAudio(buffer).get();
                if (transcription.isSuccess()) {
                    std::string command = transcription.getValue();
                    
                    // Check for wake word
                    if (command.find("Hey MixMind") != std::string::npos ||
                        command.find("OK MixMind") != std::string::npos) {
                        
                        // Process command
                        processVoiceCommand(command);
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        waveInStop(hWaveIn);
        waveInClose(hWaveIn);
    }
    
    bool detectVoiceActivity() {
        // Simple energy-based VAD
        double energy = 0;
        for (const auto& sample : recordedData) {
            energy += sample * sample;
        }
        energy /= recordedData.size();
        
        return energy > 1000000; // Threshold for voice
    }
    
    std::vector<short> recordVoiceCommand() {
        std::vector<short> command;
        int silenceCount = 0;
        
        while (silenceCount < 10) { // 1 second of silence
            // Record 100ms chunks
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (detectVoiceActivity()) {
                silenceCount = 0;
                command.insert(command.end(), recordedData.begin(), recordedData.end());
            } else {
                silenceCount++;
            }
            
            if (command.size() > 16000 * 10) { // Max 10 seconds
                break;
            }
        }
        
        return command;
    }
    
    void processVoiceCommand(const std::string& command) {
        if (onCommand) {
            onCommand(command);
        }
    }
    
    static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                   DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
        if (uMsg == WIM_DATA) {
            auto* vc = reinterpret_cast<Impl*>(dwInstance);
            WAVEHDR* hdr = (WAVEHDR*)dwParam1;
            
            short* data = (short*)hdr->lpData;
            int samples = hdr->dwBytesRecorded / sizeof(short);
            
            vc->recordedData.clear();
            vc->recordedData.insert(vc->recordedData.end(), data, data + samples);
            
            waveInAddBuffer(hwi, hdr, sizeof(WAVEHDR));
        }
    }
};

} // namespace mixmind::ai
```

---

### PHASE 4: IMPLEMENT STYLE TRANSFER & AI GENERATION (1 hour)

#### Task 4.1: Style Transfer Engine
Location: `src/ai/StyleTransfer.cpp` (CREATE NEW)

```cpp
#include "StyleTransfer.h"
#include <tensorflow/c/c_api.h>

namespace mixmind::ai {

class StyleTransfer::Impl {
private:
    TF_Session* session = nullptr;
    TF_Graph* graph = nullptr;
    
    struct StyleProfile {
        std::string name;
        std::vector<float> eq_curve;
        std::vector<float> compression_settings;
        std::vector<float> spatial_characteristics;
        std::vector<std::string> typical_plugins;
    };
    
    std::map<std::string, StyleProfile> styleProfiles;
    
public:
    void initialize() {
        // Load pre-trained style transfer model
        loadModel("models/style_transfer.pb");
        
        // Define style profiles
        initializeStyleProfiles();
    }
    
    Result<AudioBuffer> transferStyle(const AudioBuffer& input, const std::string& targetStyle) {
        auto styleIter = styleProfiles.find(targetStyle);
        if (styleIter == styleProfiles.end()) {
            return Error("Unknown style: " + targetStyle);
        }
        
        const auto& profile = styleIter->second;
        AudioBuffer output = input;
        
        // Apply EQ curve matching
        applyEQMatching(output, profile.eq_curve);
        
        // Apply dynamics matching
        applyDynamicsMatching(output, profile.compression_settings);
        
        // Apply spatial characteristics
        applySpatialProcessing(output, profile.spatial_characteristics);
        
        // If we have a neural model, use it for fine details
        if (session) {
            output = processWithNeuralModel(output, profile);
        }
        
        return Ok(output);
    }
    
private:
    void initializeStyleProfiles() {
        // Daft Punk style
        styleProfiles["Daft Punk"] = {
            "Daft Punk",
            {0, 2, 4, 3, 1, -1, -2, 1, 3, 2}, // EQ boost in lows and highs
            {4.0f, 0.005f, 0.1f, -6.0f}, // Heavy compression
            {0.8f, 0.3f, 0.5f}, // Wide stereo, moderate reverb
            {"Vocoder", "Sidechain Compressor", "Bit Crusher", "Filter"}
        };
        
        // Skrillex style
        styleProfiles["Skrillex"] = {
            "Skrillex",
            {3, 1, -2, -1, 2, 4, 3, 2, 1, 0}, // Mid-scoop, hyped highs
            {8.0f, 0.001f, 0.05f, -3.0f}, // Extreme compression
            {1.0f, 0.1f, 0.2f}, // Very wide, dry
            {"OTT", "Serum", "Massive", "Distortion", "Multiband Compressor"}
        };
        
        // Hans Zimmer style
        styleProfiles["Hans Zimmer"] = {
            "Hans Zimmer",
            {1, 2, 2, 1, 0, 0, 1, 2, 3, 4}, // Warm, full spectrum
            {2.0f, 0.01f, 0.3f, -12.0f}, // Gentle compression
            {0.9f, 0.7f, 0.8f}, // Wide with epic reverb
            {"Orchestral Reverb", "Tape Saturation", "Ensemble", "Strings"}
        };
        
        // Billie Eilish style
        styleProfiles["Billie Eilish"] = {
            "Billie Eilish",
            {4, 3, 1, 0, -1, -2, 0, 1, 0, -1}, // Intimate lows, controlled highs
            {3.0f, 0.002f, 0.15f, -9.0f}, // Moderate compression
            {0.3f, 0.4f, 0.3f}, // Intimate, close
            {"Auto-Tune", "Reverb", "Delay", "Saturation"}
        };
    }
    
    void applyEQMatching(AudioBuffer& buffer, const std::vector<float>& targetCurve) {
        // FFT-based EQ matching
        auto spectrum = fft(buffer);
        
        // Apply target EQ curve
        for (size_t i = 0; i < spectrum.size() && i < targetCurve.size(); ++i) {
            float gain = std::pow(10, targetCurve[i] / 20.0f);
            spectrum[i] *= gain;
        }
        
        // Inverse FFT
        buffer = ifft(spectrum);
    }
    
    void applyDynamicsMatching(AudioBuffer& buffer, const std::vector<float>& settings) {
        // Extract dynamics parameters
        float ratio = settings[0];
        float attack = settings[1];
        float release = settings[2];
        float threshold = settings[3];
        
        // Apply compression
        Compressor comp;
        comp.setRatio(ratio);
        comp.setAttack(attack);
        comp.setRelease(release);
        comp.setThreshold(threshold);
        comp.process(buffer);
    }
    
    void applySpatialProcessing(AudioBuffer& buffer, const std::vector<float>& spatial) {
        float width = spatial[0];
        float reverb = spatial[1];
        float depth = spatial[2];
        
        // Stereo widening
        StereoWidener widener;
        widener.setWidth(width);
        widener.process(buffer);
        
        // Reverb
        if (reverb > 0) {
            Reverb reverb_processor;
            reverb_processor.setMix(reverb);
            reverb_processor.setSize(depth);
            reverb_processor.process(buffer);
        }
    }
};

} // namespace mixmind::ai
```

#### Task 4.2: AI Music Generator
Location: `src/ai/MusicGenerator.cpp` (CREATE NEW)

```cpp
namespace mixmind::ai {

class MusicGenerator {
public:
    // Generate complete drum pattern with AI
    AudioBuffer generateDrumPattern(const std::string& style, int bars, int bpm) {
        DrumPattern pattern;
        
        if (style == "trap") {
            pattern = generateTrapPattern(bars, bpm);
        } else if (style == "house") {
            pattern = generateHousePattern(bars, bpm);
        } else if (style == "dnb") {
            pattern = generateDnBPattern(bars, bpm);
        }
        
        return renderDrumPattern(pattern, bpm);
    }
    
    // Generate chord progression
    std::vector<Chord> generateChordProgression(const std::string& key, const std::string& mood) {
        std::vector<Chord> progression;
        
        if (mood == "happy") {
            progression = {{"C", "maj"}, {"G", "maj"}, {"Am", "min"}, {"F", "maj"}};
        } else if (mood == "sad") {
            progression = {{"Am", "min"}, {"F", "maj"}, {"C", "maj"}, {"G", "maj"}};
        } else if (mood == "epic") {
            progression = {{"Cm", "min"}, {"Ab", "maj"}, {"Eb", "maj"}, {"Bb", "maj"}};
        }
        
        return progression;
    }
    
    // Generate bass line that follows chord progression
    MidiSequence generateBassLine(const std::vector<Chord>& chords, const std::string& style) {
        MidiSequence sequence;
        
        for (const auto& chord : chords) {
            if (style == "walking") {
                addWalkingBassPattern(sequence, chord);
            } else if (style == "808") {
                add808Pattern(sequence, chord);
            } else if (style == "synth") {
                addSynthBassPattern(sequence, chord);
            }
        }
        
        return sequence;
    }
    
    // Generate melody using AI
    MidiSequence generateMelody(const std::vector<Chord>& chords, const std::string& style) {
        // Use Magenta or custom AI model for melody generation
        MelodyGenerator ai_generator;
        ai_generator.setChordProgression(chords);
        ai_generator.setStyle(style);
        
        return ai_generator.generate();
    }
};

} // namespace mixmind::ai
```

---

### PHASE 5: COMPLETE UI WITH DEAR IMGUI (45 minutes)

#### Task 5.1: Main DAW Window
Location: `src/ui/MainWindow.cpp` (CREATE NEW)

```cpp
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

namespace mixmind::ui {

class MainWindow {
private:
    GLFWwindow* window;
    RealtimeAudioEngine* audioEngine;
    AIAssistant* aiAssistant;
    VoiceControl* voiceControl;
    
public:
    void initialize() {
        glfwInit();
        window = glfwCreateWindow(1920, 1080, "MixMind AI - Professional DAW", NULL, NULL);
        glfwMakeContextCurrent(window);
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        
        // Apply dark professional theme
        applyProTheme();
        
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }
    
    void render() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Main docking space
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        
        renderMenuBar();
        renderTransport();
        renderTrackView();
        renderMixer();
        renderAIAssistant();
        renderPianoRoll();
        renderPluginWindows();
        renderStyleTransferPanel();
        
        // Voice control indicator
        if (voiceControl->isListening()) {
            renderVoiceIndicator();
        }
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    
private:
    void renderAIAssistant() {
        ImGui::Begin("AI Assistant", nullptr, ImGuiWindowFlags_NoCollapse);
        
        // AI status with animated indicator
        static float pulse = 0.0f;
        pulse += ImGui::GetIO().DeltaTime * 2.0f;
        float alpha = (sin(pulse) + 1.0f) * 0.5f;
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, alpha));
        ImGui::Text("🤖 AI Online - Listening...");
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        
        // Chat history with syntax highlighting
        ImGui::BeginChild("ChatHistory", ImVec2(0, -100), true);
        for (const auto& msg : chatHistory) {
            if (msg.isAI) {
                // AI message with special formatting
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                ImGui::TextWrapped("AI: %s", msg.text.c_str());
                
                // Show action buttons for suggestions
                if (!msg.suggestions.empty()) {
                    ImGui::Indent();
                    for (const auto& suggestion : msg.suggestions) {
                        if (ImGui::Button(suggestion.c_str())) {
                            executeAISuggestion(suggestion);
                        }
                        ImGui::SameLine();
                    }
                    ImGui::Unindent();
                }
            } else {
                // User message
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::TextWrapped("You: %s", msg.text.c_str());
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        
        // Proactive suggestions panel
        if (!proactiveSuggestions.empty()) {
            ImGui::Separator();
            ImGui::Text("💡 AI Suggestions:");
            
            for (auto& suggestion : proactiveSuggestions) {
                ImVec4 color = getPriorityColor(suggestion.priority);
                ImGui::PushStyleColor(ImGuiCol_Header, color);
                
                if (ImGui::CollapsingHeader(suggestion.title.c_str())) {
                    ImGui::Text("%s", suggestion.description.c_str());
                    
                    if (ImGui::Button("Apply")) {
                        applySuggestion(suggestion);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Dismiss")) {
                        dismissSuggestion(suggestion);
                    }
                }
                
                ImGui::PopStyleColor();
            }
        }
        
        // Natural language input
        static char inputBuffer[512] = "";
        ImGui::Text("Command:");
        ImGui::PushItemWidth(-80);
        if (ImGui::InputTextWithHint("##ai_input", 
                                    "Ask me anything or describe what you want...",
                                    inputBuffer, sizeof(inputBuffer),
                                    ImGuiInputTextFlags_EnterReturnsTrue)) {
            processAICommand(inputBuffer);
            strcpy(inputBuffer, "");
        }
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        if (ImGui::Button("Send")) {
            processAICommand(inputBuffer);
            strcpy(inputBuffer, "");
        }
        
        // Quick AI actions
        ImGui::Separator();
        if (ImGui::Button("🎙️ Voice Control")) {
            toggleVoiceControl();
        }
        ImGui::SameLine();
        if (ImGui::Button("🎨 Style Transfer")) {
            openStyleTransferPanel();
        }
        ImGui::SameLine();
        if (ImGui::Button("🎵 Generate")) {
            openGeneratorPanel();
        }
        ImGui::SameLine();
        if (ImGui::Button("📊 Analyze")) {
            analyzeCurrentProject();
        }
        ImGui::SameLine();
        if (ImGui::Button("🎚️ AI Mix")) {
            startAIMixing();
        }
        
        ImGui::End();
    }
    
    void renderStyleTransferPanel() {
        if (showStyleTransfer) {
            ImGui::Begin("Style Transfer", &showStyleTransfer);
            
            ImGui::Text("Transform your track's style:");
            
            static int selectedStyle = 0;
            const char* styles[] = {
                "Daft Punk", "Skrillex", "Hans Zimmer", "Billie Eilish",
                "The Weeknd", "Flume", "Deadmau5", "Swedish House Mafia",
                "Tame Impala", "Travis Scott", "Custom..."
            };
            
            ImGui::Combo("Target Style", &selectedStyle, styles, IM_ARRAYSIZE(styles));
            
            // Style intensity
            static float intensity = 0.7f;
            ImGui::SliderFloat("Transfer Intensity", &intensity, 0.0f, 1.0f);
            
            // Preview
            if (ImGui::Button("Preview")) {
                previewStyleTransfer(styles[selectedStyle], intensity);
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply")) {
                applyStyleTransfer(styles[selectedStyle], intensity);
            }
            
            // Show style characteristics
            ImGui::Separator();
            ImGui::Text("Style Characteristics:");
            ImGui::BulletText("EQ Profile: Boosted lows and highs");
            ImGui::BulletText("Compression: Heavy sidechain");
            ImGui::BulletText("Effects: Vocoder, filters, distortion");
            ImGui::BulletText("Spatial: Wide stereo image");
            
            ImGui::End();
        }
    }
    
    void renderVoiceIndicator() {
        // Floating voice indicator
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowSize(ImVec2(200, 60));
        ImGui::Begin("Voice", nullptr, 
                    ImGuiWindowFlags_NoTitleBar | 
                    ImGuiWindowFlags_NoResize | 
                    ImGuiWindowFlags_NoMove);
        
        // Animated listening indicator
        static float wave = 0.0f;
        wave += ImGui::GetIO().DeltaTime * 3.0f;
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        
        // Draw waveform animation
        for (int i = 0; i < 20; i++) {
            float height = sin(wave + i * 0.5f) * 15.0f;
            draw_list->AddRectFilled(
                ImVec2(p.x + i * 10, p.y + 30 - height),
                ImVec2(p.x + i * 10 + 8, p.y + 30 + height),
                IM_COL32(100, 200, 255, 200)
            );
        }
        
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::Text("🎤 Listening for commands...");
        
        ImGui::End();
    }
    
    void processAICommand(const std::string& command) {
        // Send to AI for processing
        auto response = aiAssistant->processNaturalLanguage(command);
        
        // Add to chat history
        chatHistory.push_back({false, command});
        chatHistory.push_back({true, response.text, response.suggestions});
        
        // Execute actions
        for (const auto& action : response.actions) {
            executeAction(action, response.parameters);
        }
    }
};

} // namespace mixmind::ui
```

---

## TELL CLAUDE CODE:

"This is the COMPLETE implementation guide. Start with PHASE 1 to get everything compiling, then implement each phase in order. The user wants the FULL DAW with all AI features working - voice control, style transfer, AI mastering, proactive suggestions. No demos or shortcuts. Each code block shows exactly what to create or modify. Test after each phase to ensure it's working before moving to the next. The goal is a professional DAW that rivals Ableton/Logic but with AI superpowers."

**Critical paths:**
1. Fix CMakeLists.txt dependencies FIRST
2. Implement real-time audio with PortAudio
3. Add OpenAI integration for natural language
4. Implement voice control with Windows audio
5. Complete UI with Dear ImGui

**The user can provide their OpenAI API key in a .env file for the AI features to work fully.**
