#include "MainWindow.h"
#include "../audio/RealtimeAudioEngine.h"
#include "../ai/OpenAIIntegration.h"
#include "../ai/VoiceControl.h"
#include "../ai/StyleTransfer.h"
#include "../ai/MusicGenerator.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <GL/gl3w.h>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace mixmind::ui {

// ============================================================================
// MainWindow - Professional DAW Interface
// ============================================================================

class MainWindow::Impl {
public:
    // Window and rendering
    GLFWwindow* window_ = nullptr;
    int windowWidth_ = 1920;
    int windowHeight_ = 1080;
    bool isFullscreen_ = false;
    
    // Core systems
    audio::RealtimeAudioEngine* audioEngine_;
    ai::AudioIntelligenceEngine* aiEngine_;
    ai::VoiceController* voiceController_;
    ai::StyleTransferEngine* styleEngine_;
    ai::AICompositionEngine* compositionEngine_;
    
    // UI state
    bool showDemo_ = false;
    bool showAudioSettings_ = false;
    bool showAIPanel_ = true;
    bool showVoiceControl_ = true;
    bool showStyleTransfer_ = true;
    bool showComposer_ = true;
    bool showMixer_ = true;
    bool showTransport_ = true;
    bool showAnalyzer_ = true;
    
    // Theme and styling
    UITheme currentTheme_ = UITheme::PROFESSIONAL_DARK;
    float uiScale_ = 1.0f;
    
    // Audio system state
    bool audioEngineRunning_ = false;
    audio::AudioStats lastAudioStats_;
    
    // AI system state
    bool aiInitialized_ = false;
    std::string lastAIResponse_;
    bool voiceControlActive_ = false;
    
    // Transport controls
    bool isPlaying_ = false;
    bool isRecording_ = false;
    float playbackPosition_ = 0.0f;
    float projectLength_ = 240.0f; // 4 minutes default
    
    // Mixer state
    struct ChannelStrip {
        std::string name = "Channel";
        float volume = 0.75f;
        float pan = 0.0f;
        bool muted = false;
        bool solo = false;
        bool armed = false;
        float vuLevel = 0.0f;
    };
    std::vector<ChannelStrip> mixerChannels_;
    
    // AI panels state
    std::string aiPrompt_;
    std::string aiResponse_;
    std::string voiceStatus_;
    std::string styleTransferSource_;
    std::string styleTransferTarget_;
    
    bool initialize() {
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "❌ Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        // Create window
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        window_ = glfwCreateWindow(windowWidth_, windowHeight_, "MixMind AI - Professional DAW", nullptr, nullptr);
        if (!window_) {
            std::cerr << "❌ Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1); // Enable vsync
        
        // Initialize OpenGL
        if (gl3wInit() != 0) {
            std::cerr << "❌ Failed to initialize OpenGL" << std::endl;
            return false;
        }
        
        // Setup Dear ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        
        // Setup Dear ImGui style
        setupTheme(currentTheme_);
        
        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        // Get core systems
        audioEngine_ = &audio::getGlobalAudioEngine();
        aiEngine_ = &ai::getGlobalAIEngine();
        voiceController_ = &ai::getGlobalVoiceController();
        styleEngine_ = &ai::getGlobalStyleEngine();
        compositionEngine_ = &ai::getGlobalCompositionEngine();
        
        // Initialize mixer channels
        initializeMixer();
        
        std::cout << "🖥️ MixMind AI Professional DAW Interface initialized" << std::endl;
        return true;
    }
    
    void setupTheme(UITheme theme) {
        ImGuiStyle& style = ImGui::GetStyle();
        
        switch (theme) {
            case UITheme::PROFESSIONAL_DARK:
                setupProfessionalDarkTheme(style);
                break;
            case UITheme::PROFESSIONAL_LIGHT:
                setupProfessionalLightTheme(style);
                break;
            case UITheme::STUDIO_CLASSIC:
                setupStudioClassicTheme(style);
                break;
        }
    }
    
    void setupProfessionalDarkTheme(ImGuiStyle& style) {
        // Professional dark theme inspired by modern DAWs
        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 4);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.IndentSpacing = 20.0f;
        style.ScrollbarSize = 16.0f;
        style.GrabMinSize = 12.0f;
        
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        
        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 4.0f;
        
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.60f, 0.80f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.60f, 0.80f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.60f, 0.80f, 1.00f);
    }
    
    void setupProfessionalLightTheme(ImGuiStyle& style) {
        // Professional light theme
        ImGui::StyleColorsLight();
        // Additional light theme customizations would go here
    }
    
    void setupStudioClassicTheme(ImGuiStyle& style) {
        // Classic studio hardware-inspired theme
        setupProfessionalDarkTheme(style); // Start with dark theme
        
        // Add more vintage/hardware-inspired colors
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f); // Amber
        colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.80f, 0.60f, 0.00f, 1.00f);
    }
    
    void initializeMixer() {
        mixerChannels_.clear();
        
        // Create default mixer channels
        std::vector<std::string> channelNames = {
            "Master", "Track 1", "Track 2", "Track 3", "Track 4", 
            "Track 5", "Track 6", "Track 7", "Track 8"
        };
        
        for (const auto& name : channelNames) {
            ChannelStrip channel;
            channel.name = name;
            if (name == "Master") {
                channel.volume = 0.85f;
            }
            mixerChannels_.push_back(channel);
        }
    }
    
    void run() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            
            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Update system states
            updateSystemStates();
            
            // Create the main dockspace
            createDockSpace();
            
            // Render main menu
            renderMainMenu();
            
            // Render all panels
            if (showTransport_) renderTransportPanel();
            if (showMixer_) renderMixerPanel();
            if (showAIPanel_) renderAIPanel();
            if (showVoiceControl_) renderVoiceControlPanel();
            if (showStyleTransfer_) renderStyleTransferPanel();
            if (showComposer_) renderComposerPanel();
            if (showAnalyzer_) renderAnalyzerPanel();
            
            // Show audio settings if requested
            if (showAudioSettings_) renderAudioSettingsDialog();
            
            // Show ImGui demo if requested
            if (showDemo_) ImGui::ShowDemoWindow(&showDemo_);
            
            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window_, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.12f, 0.12f, 0.14f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            // Update and Render additional Platform Windows
            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                GLFWwindow* backup_current_context = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(backup_current_context);
            }
            
            glfwSwapBuffers(window_);
        }
    }
    
    void updateSystemStates() {
        // Update audio engine state
        if (audioEngine_) {
            audioEngineRunning_ = audioEngine_->isRunning();
            lastAudioStats_ = audioEngine_->getStats();
        }
        
        // Update voice control state
        if (voiceController_) {
            voiceControlActive_ = voiceController_->isListening();
            
            if (voiceControlActive_) {
                voiceStatus_ = "🎤 Listening...";
            } else {
                voiceStatus_ = "🔇 Voice Control Off";
            }
        }
        
        // Update playback position (mock)
        if (isPlaying_) {
            playbackPosition_ += ImGui::GetIO().DeltaTime;
            if (playbackPosition_ >= projectLength_) {
                playbackPosition_ = projectLength_;
                isPlaying_ = false;
            }
        }
        
        // Update VU meters (mock)
        for (auto& channel : mixerChannels_) {
            if (!channel.muted && audioEngineRunning_) {
                // Mock VU meter animation
                channel.vuLevel = 0.3f + 0.4f * sin(glfwGetTime() * 4.0 + (&channel - &mixerChannels_[0]));
                channel.vuLevel = std::max(0.0f, std::min(1.0f, channel.vuLevel));
            } else {
                channel.vuLevel *= 0.95f; // Decay
            }
        }
    }
    
    void createDockSpace() {
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | 
                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        } else {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }
        
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;
        
        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);
        
        if (!opt_padding)
            ImGui::PopStyleVar();
        if (opt_fullscreen)
            ImGui::PopStyleVar(2);
        
        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        
        ImGui::End();
    }
    
    void renderMainMenu() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Project", "Ctrl+N")) {
                    // New project logic
                }
                if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
                    // Open project logic  
                }
                if (ImGui::MenuItem("Save Project", "Ctrl+S")) {
                    // Save project logic
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Export Audio", "Ctrl+E")) {
                    // Export audio logic
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    glfwSetWindowShouldClose(window_, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
                if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
                if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Transport", nullptr, &showTransport_);
                ImGui::MenuItem("Mixer", nullptr, &showMixer_);
                ImGui::MenuItem("AI Panel", nullptr, &showAIPanel_);
                ImGui::MenuItem("Voice Control", nullptr, &showVoiceControl_);
                ImGui::MenuItem("Style Transfer", nullptr, &showStyleTransfer_);
                ImGui::MenuItem("AI Composer", nullptr, &showComposer_);
                ImGui::MenuItem("Analyzer", nullptr, &showAnalyzer_);
                ImGui::Separator();
                if (ImGui::BeginMenu("Theme")) {
                    if (ImGui::MenuItem("Professional Dark", nullptr, currentTheme_ == UITheme::PROFESSIONAL_DARK)) {
                        currentTheme_ = UITheme::PROFESSIONAL_DARK;
                        setupTheme(currentTheme_);
                    }
                    if (ImGui::MenuItem("Professional Light", nullptr, currentTheme_ == UITheme::PROFESSIONAL_LIGHT)) {
                        currentTheme_ = UITheme::PROFESSIONAL_LIGHT;
                        setupTheme(currentTheme_);
                    }
                    if (ImGui::MenuItem("Studio Classic", nullptr, currentTheme_ == UITheme::STUDIO_CLASSIC)) {
                        currentTheme_ = UITheme::STUDIO_CLASSIC;
                        setupTheme(currentTheme_);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Audio")) {
                if (ImGui::MenuItem("Audio Settings")) {
                    showAudioSettings_ = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem(audioEngineRunning_ ? "Stop Audio Engine" : "Start Audio Engine")) {
                    if (audioEngineRunning_) {
                        audioEngine_->stop();
                    } else {
                        audioEngine_->start();
                    }
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("AI")) {
                if (ImGui::MenuItem("Initialize AI Engine")) {
                    // AI initialization logic
                }
                ImGui::Separator();
                if (ImGui::MenuItem(voiceControlActive_ ? "Stop Voice Control" : "Start Voice Control")) {
                    if (voiceControlActive_) {
                        voiceController_->stopListening();
                    } else {
                        voiceController_->startListening();
                    }
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About MixMind AI")) {
                    // Show about dialog
                }
                if (ImGui::MenuItem("Show Demo Window")) {
                    showDemo_ = true;
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMainMenuBar();
        }
    }
    
    void renderTransportPanel() {
        if (ImGui::Begin("Transport", &showTransport_)) {
            // Transport controls
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
            
            // Play/Pause button
            ImVec4 playColor = isPlaying_ ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, playColor);
            if (ImGui::Button(isPlaying_ ? "⏸️ Pause" : "▶️ Play", ImVec2(100, 40))) {
                isPlaying_ = !isPlaying_;
                if (isPlaying_ && !audioEngineRunning_) {
                    audioEngine_->start();
                }
            }
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            
            // Stop button
            if (ImGui::Button("⏹️ Stop", ImVec2(80, 40))) {
                isPlaying_ = false;
                isRecording_ = false;
                playbackPosition_ = 0.0f;
            }
            
            ImGui::SameLine();
            
            // Record button
            ImVec4 recColor = isRecording_ ? ImVec4(0.9f, 0.1f, 0.1f, 1.0f) : ImVec4(0.5f, 0.1f, 0.1f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, recColor);
            if (ImGui::Button("⏺️ Record", ImVec2(100, 40))) {
                isRecording_ = !isRecording_;
                if (isRecording_ && !audioEngineRunning_) {
                    audioEngine_->start();
                }
            }
            ImGui::PopStyleColor();
            
            ImGui::PopStyleVar();
            
            ImGui::Separator();
            
            // Position display and scrubber
            int minutes = static_cast<int>(playbackPosition_ / 60);
            int seconds = static_cast<int>(playbackPosition_) % 60;
            int centiseconds = static_cast<int>((playbackPosition_ - floor(playbackPosition_)) * 100);
            
            ImGui::Text("Position: %02d:%02d.%02d", minutes, seconds, centiseconds);
            
            // Timeline scrubber
            ImGui::PushItemWidth(-1);
            if (ImGui::SliderFloat("##Timeline", &playbackPosition_, 0.0f, projectLength_, "")) {
                // Scrubbing logic
            }
            ImGui::PopItemWidth();
            
            // Tempo and time signature
            ImGui::Columns(2, nullptr, false);
            
            static int tempo = 120;
            ImGui::Text("Tempo");
            ImGui::InputInt("BPM", &tempo, 1, 10);
            
            ImGui::NextColumn();
            
            static int timeSigNum = 4, timeSigDen = 4;
            ImGui::Text("Time Signature");
            ImGui::PushItemWidth(50);
            ImGui::InputInt("##TimeSigNum", &timeSigNum, 0, 0);
            ImGui::SameLine();
            ImGui::Text("/");
            ImGui::SameLine();
            ImGui::InputInt("##TimeSigDen", &timeSigDen, 0, 0);
            ImGui::PopItemWidth();
            
            ImGui::Columns(1);
        }
        ImGui::End();
    }
    
    void renderMixerPanel() {
        if (ImGui::Begin("Mixer", &showMixer_)) {
            ImGui::Columns(static_cast<int>(mixerChannels_.size()), nullptr, false);
            
            for (size_t i = 0; i < mixerChannels_.size(); ++i) {
                auto& channel = mixerChannels_[i];
                
                ImGui::PushID(static_cast<int>(i));
                
                // Channel name
                ImGui::Text("%s", channel.name.c_str());
                
                // VU Meter
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImVec2 size(20, 100);
                
                // VU meter background
                drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                                       IM_COL32(40, 40, 40, 255));
                
                // VU meter level
                float levelHeight = channel.vuLevel * size.y;
                ImU32 levelColor = channel.vuLevel > 0.8f ? IM_COL32(255, 100, 100, 255) :
                                  channel.vuLevel > 0.6f ? IM_COL32(255, 255, 100, 255) :
                                  IM_COL32(100, 255, 100, 255);
                
                drawList->AddRectFilled(ImVec2(pos.x, pos.y + size.y - levelHeight),
                                       ImVec2(pos.x + size.x, pos.y + size.y),
                                       levelColor);
                
                ImGui::Dummy(size);
                
                // Volume fader
                ImGui::PushItemWidth(30);
                ImGui::VSliderFloat("##Vol", ImVec2(30, 150), &channel.volume, 0.0f, 1.0f, "");
                ImGui::PopItemWidth();
                
                // Pan knob
                ImGui::PushItemWidth(50);
                ImGui::SliderFloat("##Pan", &channel.pan, -1.0f, 1.0f, "%.2f");
                ImGui::PopItemWidth();
                
                // Buttons
                if (ImGui::Button(channel.muted ? "MUTE" : "mute", ImVec2(50, 25))) {
                    channel.muted = !channel.muted;
                }
                
                if (ImGui::Button(channel.solo ? "SOLO" : "solo", ImVec2(50, 25))) {
                    channel.solo = !channel.solo;
                }
                
                if (i > 0) { // Not master channel
                    if (ImGui::Button(channel.armed ? "ARM" : "arm", ImVec2(50, 25))) {
                        channel.armed = !channel.armed;
                    }
                }
                
                ImGui::PopID();
                
                if (i < mixerChannels_.size() - 1) {
                    ImGui::NextColumn();
                }
            }
            
            ImGui::Columns(1);
        }
        ImGui::End();
    }
    
    void renderAIPanel() {
        if (ImGui::Begin("AI Assistant", &showAIPanel_)) {
            ImGui::Text("🤖 MixMind AI - Your Creative Assistant");
            ImGui::Separator();
            
            // AI prompt input
            ImGui::Text("Ask AI for help:");
            ImGui::PushItemWidth(-50);
            ImGui::InputTextMultiline("##AIPrompt", &aiPrompt_, ImVec2(0, 80));
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::Button("Send", ImVec2(40, 80))) {
                // Send AI request
                sendAIRequest();
            }
            
            ImGui::Separator();
            
            // AI response display
            if (!aiResponse_.empty()) {
                ImGui::Text("AI Response:");
                ImGui::BeginChild("AIResponse", ImVec2(0, 200), true);
                ImGui::TextWrapped("%s", aiResponse_.c_str());
                ImGui::EndChild();
            }
            
            ImGui::Separator();
            
            // Quick AI actions
            ImGui::Text("Quick Actions:");
            if (ImGui::Button("Analyze Current Track")) {
                aiPrompt_ = "Please analyze the current track and suggest improvements";
                sendAIRequest();
            }
            ImGui::SameLine();
            if (ImGui::Button("Suggest Chord Progression")) {
                aiPrompt_ = "Generate a creative chord progression for a modern pop song";
                sendAIRequest();
            }
            
            if (ImGui::Button("Mixing Advice")) {
                aiPrompt_ = "Give me professional mixing advice for this genre";
                sendAIRequest();
            }
            ImGui::SameLine();
            if (ImGui::Button("Creative Ideas")) {
                aiPrompt_ = "Suggest some creative production techniques";
                sendAIRequest();
            }
        }
        ImGui::End();
    }
    
    void sendAIRequest() {
        if (aiPrompt_.empty() || !aiEngine_) return;
        
        aiResponse_ = "🤖 Thinking... Please wait.";
        
        // Send async AI request
        ai::ChatRequest request;
        request.model = "gpt-4";
        request.temperature = 0.7f;
        
        ai::ChatMessage userMsg;
        userMsg.role = "user";
        userMsg.content = aiPrompt_;
        
        request.messages = {userMsg};
        
        // In a real implementation, this would be handled asynchronously
        // For now, just simulate a response
        aiResponse_ = "🎵 AI Response: Great question! For modern production, I recommend:\n\n"
                     "1. Use subtle compression to glue your mix together\n"
                     "2. Apply high-frequency enhancement to vocals\n"
                     "3. Consider parallel processing for drums\n"
                     "4. Layer your basslines for more punch\n\n"
                     "Would you like me to elaborate on any of these techniques?";
    }
    
    void renderVoiceControlPanel() {
        if (ImGui::Begin("Voice Control", &showVoiceControl_)) {
            ImGui::Text("🎤 Voice Control System");
            ImGui::Separator();
            
            // Voice control status
            ImVec4 statusColor = voiceControlActive_ ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(statusColor, "%s", voiceStatus_.c_str());
            
            // Voice control toggle
            if (ImGui::Button(voiceControlActive_ ? "Stop Listening" : "Start Listening", ImVec2(150, 40))) {
                if (voiceControlActive_) {
                    voiceController_->stopListening();
                } else {
                    voiceController_->startListening();
                }
            }
            
            ImGui::Separator();
            
            // Voice commands help
            ImGui::Text("Example Commands:");
            ImGui::BulletText("\"Play\" / \"Pause\" / \"Stop\"");
            ImGui::BulletText("\"Set volume to 75 percent\"");
            ImGui::BulletText("\"Mute track 3\"");
            ImGui::BulletText("\"Add reverb to vocals\"");
            ImGui::BulletText("\"Analyze this track\"");
            ImGui::BulletText("\"How can I make this sound better?\"");
            
            ImGui::Separator();
            
            // Recent voice commands (mock)
            ImGui::Text("Recent Commands:");
            ImGui::BeginChild("VoiceHistory", ImVec2(0, 100), true);
            ImGui::Text("🎤 \"Play the track\"");
            ImGui::Text("🎤 \"Set volume to 80\"");
            ImGui::Text("🎤 \"Add some reverb\"");
            ImGui::EndChild();
        }
        ImGui::End();
    }
    
    void renderStyleTransferPanel() {
        if (ImGui::Begin("Style Transfer", &showStyleTransfer_)) {
            ImGui::Text("🎨 AI Style Transfer");
            ImGui::Separator();
            
            // Source description
            ImGui::Text("Describe your source audio:");
            ImGui::InputTextMultiline("##StyleSource", &styleTransferSource_, ImVec2(-1, 60));
            
            // Target style selection
            ImGui::Text("Target Style:");
            static int selectedStyle = 0;
            const char* styles[] = {"Jazz", "Electronic", "Rock", "Classical", "Hip Hop"};
            ImGui::Combo("##TargetStyle", &selectedStyle, styles, IM_ARRAYSIZE(styles));
            styleTransferTarget_ = styles[selectedStyle];
            
            // Intensity control
            static float intensity = 0.7f;
            ImGui::Text("Transfer Intensity:");
            ImGui::SliderFloat("##Intensity", &intensity, 0.0f, 1.0f, "%.2f");
            
            // Transfer button
            bool isProcessing = styleEngine_ && styleEngine_->isProcessing();
            if (isProcessing) {
                ImGui::Button("Processing...", ImVec2(150, 40));
            } else {
                if (ImGui::Button("Transfer Style", ImVec2(150, 40))) {
                    if (styleEngine_ && !styleTransferSource_.empty()) {
                        // Trigger style transfer
                        auto result = styleEngine_->transferStyle(styleTransferSource_, styleTransferTarget_, intensity);
                        // Handle result asynchronously
                    }
                }
            }
            
            ImGui::Separator();
            
            // Style examples
            ImGui::Text("Style Examples:");
            if (ImGui::Button("Rock → Jazz")) {
                styleTransferSource_ = "Energetic rock song with distorted guitars and heavy drums";
                selectedStyle = 0; // Jazz
            }
            ImGui::SameLine();
            if (ImGui::Button("Acoustic → Electronic")) {
                styleTransferSource_ = "Gentle acoustic guitar and vocals";
                selectedStyle = 1; // Electronic
            }
        }
        ImGui::End();
    }
    
    void renderComposerPanel() {
        if (ImGui::Begin("AI Composer", &showComposer_)) {
            ImGui::Text("🎵 AI Music Generator");
            ImGui::Separator();
            
            // Composition parameters
            static char title[128] = "My AI Composition";
            ImGui::InputText("Title", title, sizeof(title));
            
            static int genreIndex = 0;
            const char* genres[] = {"Pop", "Rock", "Electronic", "Jazz", "Classical", "Hip Hop", "Ambient"};
            ImGui::Combo("Genre", &genreIndex, genres, IM_ARRAYSIZE(genres));
            
            static int keyIndex = 0;
            const char* keys[] = {"C Major", "G Major", "D Major", "A Major", "E Major", "B Major",
                                 "A Minor", "E Minor", "B Minor", "F# Minor", "C# Minor"};
            ImGui::Combo("Key", &keyIndex, keys, IM_ARRAYSIZE(keys));
            
            static int tempo = 120;
            ImGui::InputInt("Tempo (BPM)", &tempo, 1, 10);
            
            // Creative controls
            ImGui::Separator();
            ImGui::Text("Creative Controls:");
            
            static float creativity = 0.7f;
            ImGui::SliderFloat("Creativity", &creativity, 0.0f, 1.0f, "%.2f");
            
            static float complexity = 0.5f;
            ImGui::SliderFloat("Complexity", &complexity, 0.0f, 1.0f, "%.2f");
            
            static float energy = 0.6f;
            ImGui::SliderFloat("Energy", &energy, 0.0f, 1.0f, "%.2f");
            
            // Generate button
            bool isGenerating = compositionEngine_ && compositionEngine_->isGenerating();
            if (isGenerating) {
                ImGui::Button("Generating...", ImVec2(200, 40));
            } else {
                if (ImGui::Button("Generate Composition", ImVec2(200, 40))) {
                    generateComposition(title, genres[genreIndex], keys[keyIndex], tempo, creativity, complexity, energy);
                }
            }
            
            ImGui::Separator();
            
            // Quick presets
            ImGui::Text("Quick Presets:");
            if (ImGui::Button("Pop Ballad")) {
                strcpy_s(title, "Heartfelt Ballad");
                genreIndex = 0; // Pop
                keyIndex = 6;   // A Minor
                tempo = 75;
                creativity = 0.5f;
                complexity = 0.3f;
                energy = 0.4f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Electronic Dance")) {
                strcpy_s(title, "Dance Floor Anthem");
                genreIndex = 2; // Electronic
                keyIndex = 0;   // C Major
                tempo = 128;
                creativity = 0.8f;
                complexity = 0.7f;
                energy = 0.9f;
            }
            
            if (ImGui::Button("Jazz Standard")) {
                strcpy_s(title, "Midnight Blue");
                genreIndex = 3; // Jazz
                keyIndex = 1;   // G Major
                tempo = 120;
                creativity = 0.9f;
                complexity = 0.8f;
                energy = 0.6f;
            }
        }
        ImGui::End();
    }
    
    void generateComposition(const char* title, const char* genre, const char* key, int tempo, 
                           float creativity, float complexity, float energy) {
        if (!compositionEngine_) return;
        
        ai::GenerationRequest request;
        request.title = title;
        request.genre = genre;
        request.key = key;
        request.tempo = tempo;
        request.creativity = creativity;
        request.complexity = complexity;
        request.energy = energy;
        request.duration = 180; // 3 minutes
        request.useAI = true;
        
        // Set creativity levels in the engine
        compositionEngine_->setCreativityLevel(creativity);
        compositionEngine_->setComplexityLevel(complexity);
        
        // Generate composition (async)
        auto result = compositionEngine_->generateComposition(request);
        
        // In a real implementation, handle the result asynchronously
        std::cout << "🎵 Generating composition: " << title << " (" << genre << ")" << std::endl;
    }
    
    void renderAnalyzerPanel() {
        if (ImGui::Begin("Audio Analyzer", &showAnalyzer_)) {
            ImGui::Text("📊 Real-time Audio Analysis");
            ImGui::Separator();
            
            // Audio stats display
            if (audioEngineRunning_) {
                ImGui::Text("📈 Audio Engine Status: RUNNING");
                ImGui::Text("🔊 Sample Rate: %.0f Hz", lastAudioStats_.sampleRate);
                ImGui::Text("📊 Buffer Size: %lu samples", lastAudioStats_.framesPerBuffer);
                ImGui::Text("⚡ CPU Load: %.1f%%", lastAudioStats_.cpuLoad * 100.0f);
                ImGui::Text("🎯 Input Latency: %.1f ms", lastAudioStats_.inputLatency * 1000.0);
                ImGui::Text("🎯 Output Latency: %.1f ms", lastAudioStats_.outputLatency * 1000.0);
                ImGui::Text("⚠️ Xruns: %ld", lastAudioStats_.xrunCount);
            } else {
                ImGui::Text("📈 Audio Engine Status: STOPPED");
            }
            
            ImGui::Separator();
            
            // Mock spectrum analyzer
            ImGui::Text("Frequency Spectrum:");
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
            ImVec2 canvas_size = ImGui::GetContentRegionAvail();
            canvas_size.y = 150;
            
            drawList->AddRectFilled(canvas_pos, 
                                   ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                                   IM_COL32(30, 30, 35, 255));
            
            // Draw spectrum bars (mock data)
            int numBars = 64;
            float barWidth = canvas_size.x / numBars;
            
            for (int i = 0; i < numBars; ++i) {
                float height = (0.3f + 0.7f * sin(glfwGetTime() * 2.0 + i * 0.1)) * canvas_size.y;
                ImU32 color = IM_COL32(
                    static_cast<ImU8>(100 + i * 2),
                    static_cast<ImU8>(255 - i * 3),
                    static_cast<ImU8>(150 + sin(i * 0.1) * 50),
                    200
                );
                
                drawList->AddRectFilled(
                    ImVec2(canvas_pos.x + i * barWidth, canvas_pos.y + canvas_size.y - height),
                    ImVec2(canvas_pos.x + (i + 1) * barWidth - 1, canvas_pos.y + canvas_size.y),
                    color
                );
            }
            
            ImGui::Dummy(canvas_size);
            
            // Mock LUFS meter
            ImGui::Separator();
            ImGui::Text("LUFS Meter:");
            static float lufs = -23.0f;
            lufs += (sin(glfwGetTime() * 0.5) - lufs) * 0.1f; // Mock LUFS variation
            
            ImGui::ProgressBar((lufs + 60.0f) / 60.0f, ImVec2(-1, 20), "");
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::Text("%.1f LUFS", lufs);
        }
        ImGui::End();
    }
    
    void renderAudioSettingsDialog() {
        if (ImGui::Begin("Audio Settings", &showAudioSettings_)) {
            ImGui::Text("⚙️ Audio Device Configuration");
            ImGui::Separator();
            
            // Sample rate selection
            static int sampleRateIndex = 2; // 48kHz default
            const char* sampleRates[] = {"22050 Hz", "44100 Hz", "48000 Hz", "88200 Hz", "96000 Hz"};
            ImGui::Combo("Sample Rate", &sampleRateIndex, sampleRates, IM_ARRAYSIZE(sampleRates));
            
            // Buffer size selection
            static int bufferSizeIndex = 3; // 512 samples default
            const char* bufferSizes[] = {"64", "128", "256", "512", "1024", "2048"};
            ImGui::Combo("Buffer Size", &bufferSizeIndex, bufferSizes, IM_ARRAYSIZE(bufferSizes));
            
            // Input/Output device selection (mock)
            static int inputDevice = 0;
            static int outputDevice = 0;
            const char* devices[] = {"Default Device", "Built-in Audio", "USB Audio Interface", "ASIO Driver"};
            
            ImGui::Combo("Input Device", &inputDevice, devices, IM_ARRAYSIZE(devices));
            ImGui::Combo("Output Device", &outputDevice, devices, IM_ARRAYSIZE(devices));
            
            ImGui::Separator();
            
            // Apply and Close buttons
            if (ImGui::Button("Apply Settings", ImVec2(120, 30))) {
                // Apply audio settings
                applyAudioSettings(sampleRateIndex, bufferSizeIndex, inputDevice, outputDevice);
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(80, 30))) {
                showAudioSettings_ = false;
            }
        }
        ImGui::End();
    }
    
    void applyAudioSettings(int sampleRateIndex, int bufferSizeIndex, int inputDevice, int outputDevice) {
        // Convert indices to actual values
        const int sampleRates[] = {22050, 44100, 48000, 88200, 96000};
        const int bufferSizes[] = {64, 128, 256, 512, 1024, 2048};
        
        audio::AudioConfig config;
        config.sampleRate = sampleRates[sampleRateIndex];
        config.framesPerBuffer = bufferSizes[bufferSizeIndex];
        config.inputDevice = inputDevice;
        config.outputDevice = outputDevice;
        
        // Restart audio engine with new settings
        if (audioEngine_) {
            bool wasRunning = audioEngine_->isRunning();
            audioEngine_->stop();
            audioEngine_->initialize(config);
            if (wasRunning) {
                audioEngine_->start();
            }
        }
        
        std::cout << "🔧 Audio settings applied: " << config.sampleRate << "Hz, " 
                  << config.framesPerBuffer << " samples" << std::endl;
    }
    
    void cleanup() {
        if (window_) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
    }
};

// ============================================================================
// MainWindow Public Interface
// ============================================================================

MainWindow::MainWindow() : pImpl_(std::make_unique<Impl>()) {}
MainWindow::~MainWindow() = default;

bool MainWindow::initialize() {
    return pImpl_->initialize();
}

void MainWindow::run() {
    pImpl_->run();
}

void MainWindow::cleanup() {
    pImpl_->cleanup();
}

} // namespace mixmind::ui