#include "MixMindMainWindow.h"
#include "../core/logging.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <GL/gl3w.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace mixmind::ui {

// ============================================================================
// MixMindMainWindow Implementation
// ============================================================================

class MixMindMainWindow::Impl {
public:
    // Window and rendering
    GLFWwindow* window = nullptr;
    bool initialized = false;
    bool shouldClose = false;
    
    // Engine references
    std::shared_ptr<audio::RealtimeAudioEngine> audioEngine;
    std::shared_ptr<automation::ParameterAutomationManager> automationManager;
    std::shared_ptr<performance::PerformanceMonitor> performanceMonitor;
    std::shared_ptr<services::RealOpenAIService> aiService;
    
    // UI Panels
    std::unique_ptr<TransportBar> transportBar;
    std::unique_ptr<MixerPanel> mixerPanel;
    std::unique_ptr<PluginRackPanel> pluginRack;
    std::unique_ptr<AutomationEditor> automationEditor;
    std::unique_ptr<AIAssistantPanel> aiAssistant;
    std::unique_ptr<PerformanceMonitorPanel> performancePanel;
    
    // UI State
    bool showDemoWindow = false;
    bool showPluginBrowser = false;
    bool showAudioSettings = false;
    bool showPerformanceMonitor = true;
    bool showAIAssistant = true;
    bool showAutomationEditor = true;
    bool showMixer = true;
    bool showPluginRack = true;
    
    // Theme and layout
    ImGuiID dockspaceId = 0;
    bool firstFrame = true;
    
    // Real-time data
    struct AudioLevels {
        float peakL = -96.0f;
        float peakR = -96.0f;
        float rmsL = -96.0f;
        float rmsR = -96.0f;
        std::vector<float> spectrum;
    } audioLevels;
    
    performance::SystemMetrics lastSystemMetrics;
    performance::AudioEngineMetrics lastAudioMetrics;
    std::vector<performance::PluginMetrics> lastPluginMetrics;
};

MixMindMainWindow::MixMindMainWindow() : pImpl_(std::make_unique<Impl>()) {
    // Initialize UI panels
    pImpl_->transportBar = std::make_unique<TransportBar>();
    pImpl_->mixerPanel = std::make_unique<MixerPanel>();
    pImpl_->pluginRack = std::make_unique<PluginRackPanel>();
    pImpl_->automationEditor = std::make_unique<AutomationEditor>();
    pImpl_->aiAssistant = std::make_unique<AIAssistantPanel>();
    pImpl_->performancePanel = std::make_unique<PerformanceMonitorPanel>();
}

MixMindMainWindow::~MixMindMainWindow() {
    shutdown();
}

mixmind::core::Result<void> MixMindMainWindow::initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        MIXMIND_LOG_ERROR("Failed to initialize GLFW");
        return mixmind::core::Result<void>::failure("Failed to initialize GLFW");
    }
    
    // Configure OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x MSAA
    
    // Create window
    pImpl_->window = glfwCreateWindow(1920, 1080, "MixMind AI - Professional DAW", nullptr, nullptr);
    if (!pImpl_->window) {
        MIXMIND_LOG_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return mixmind::core::Result<void>::failure("Failed to create window");
    }
    
    glfwMakeContextCurrent(pImpl_->window);
    glfwSwapInterval(1); // Enable vsync
    
    // Initialize OpenGL loader
    if (gl3wInit() != 0) {
        MIXMIND_LOG_ERROR("Failed to initialize OpenGL loader");
        glfwDestroyWindow(pImpl_->window);
        glfwTerminate();
        return mixmind::core::Result<void>::failure("Failed to initialize OpenGL");
    }
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(pImpl_->window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Setup professional theme
    setupDarkTheme();
    setupProfessionalColors();
    
    // Load fonts
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", 16.0f);
    io.Fonts->AddFontFromFileTTF("assets/fonts/RobotoMono-Regular.ttf", 14.0f);
    
    pImpl_->initialized = true;
    MIXMIND_LOG_INFO("MixMind main window initialized successfully");
    
    return mixmind::core::Result<void>::success();
}

void MixMindMainWindow::shutdown() {
    if (pImpl_->initialized) {
        saveLayout();
        
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        if (pImpl_->window) {
            glfwDestroyWindow(pImpl_->window);
            pImpl_->window = nullptr;
        }
        glfwTerminate();
        
        pImpl_->initialized = false;
        MIXMIND_LOG_INFO("MixMind main window shutdown complete");
    }
}

bool MixMindMainWindow::isInitialized() const {
    return pImpl_->initialized;
}

void MixMindMainWindow::render() {
    if (!pImpl_->initialized) return;
    
    // Poll events
    glfwPollEvents();
    
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Setup docking
    setupDocking();
    
    // Render main menu bar
    renderMainMenuBar();
    
    // Render all panels
    if (pImpl_->showMixer) {
        if (ImGui::Begin("Mixer", &pImpl_->showMixer)) {
            pImpl_->mixerPanel->render();
        }
        ImGui::End();
    }
    
    if (pImpl_->showPluginRack) {
        if (ImGui::Begin("Plugin Rack", &pImpl_->showPluginRack)) {
            pImpl_->pluginRack->render();
        }
        ImGui::End();
    }
    
    if (pImpl_->showAutomationEditor) {
        if (ImGui::Begin("Automation", &pImpl_->showAutomationEditor)) {
            pImpl_->automationEditor->render();
        }
        ImGui::End();
    }
    
    if (pImpl_->showAIAssistant) {
        if (ImGui::Begin("AI Assistant", &pImpl_->showAIAssistant)) {
            pImpl_->aiAssistant->render();
        }
        ImGui::End();
    }
    
    if (pImpl_->showPerformanceMonitor) {
        if (ImGui::Begin("Performance", &pImpl_->showPerformanceMonitor)) {
            pImpl_->performancePanel->render();
        }
        ImGui::End();
    }
    
    // Transport bar (always visible)
    if (ImGui::Begin("Transport", nullptr, 
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
        pImpl_->transportBar->render();
    }
    ImGui::End();
    
    // Render status bar
    renderStatusBar();
    
    // Optional windows
    if (pImpl_->showAudioSettings) {
        renderAudioSettings();
    }
    
    if (pImpl_->showPluginBrowser) {
        renderPluginBrowser();
    }
    
    // Demo window for development
    if (pImpl_->showDemoWindow) {
        ImGui::ShowDemoWindow(&pImpl_->showDemoWindow);
    }
    
    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(pImpl_->window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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
    
    glfwSwapBuffers(pImpl_->window);
}

void MixMindMainWindow::handleEvents() {
    pImpl_->shouldClose = glfwWindowShouldClose(pImpl_->window);
}

bool MixMindMainWindow::shouldClose() const {
    return pImpl_->shouldClose;
}

void MixMindMainWindow::setAudioEngine(std::shared_ptr<audio::RealtimeAudioEngine> engine) {
    pImpl_->audioEngine = engine;
}

void MixMindMainWindow::setAutomationManager(std::shared_ptr<automation::ParameterAutomationManager> automation) {
    pImpl_->automationManager = automation;
}

void MixMindMainWindow::setPerformanceMonitor(std::shared_ptr<performance::PerformanceMonitor> monitor) {
    pImpl_->performanceMonitor = monitor;
    
    // Set up performance callbacks
    if (monitor) {
        monitor->setSystemMetricsCallback([this](const performance::SystemMetrics& metrics) {
            pImpl_->lastSystemMetrics = metrics;
        });
        
        monitor->setAudioMetricsCallback([this](const performance::AudioEngineMetrics& metrics) {
            pImpl_->lastAudioMetrics = metrics;
        });
        
        monitor->setPluginMetricsCallback([this](const std::vector<performance::PluginMetrics>& metrics) {
            pImpl_->lastPluginMetrics = metrics;
        });
    }
}

void MixMindMainWindow::setOpenAIService(std::shared_ptr<services::RealOpenAIService> aiService) {
    pImpl_->aiService = aiService;
}

void MixMindMainWindow::renderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project", "Ctrl+N")) {
                // Handle new project
            }
            if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
                // Handle open project
            }
            if (ImGui::MenuItem("Save Project", "Ctrl+S")) {
                // Handle save project
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import Audio", "Ctrl+I")) {
                // Handle import audio
            }
            if (ImGui::MenuItem("Export Audio", "Ctrl+E")) {
                // Handle export audio
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                pImpl_->shouldClose = true;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                // Handle undo
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                // Handle redo
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                // Handle cut
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                // Handle copy
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                // Handle paste
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Audio")) {
            if (ImGui::MenuItem("Audio Settings", "F4")) {
                pImpl_->showAudioSettings = true;
            }
            if (ImGui::MenuItem("Plugin Browser", "F5")) {
                pImpl_->showPluginBrowser = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Latency Test")) {
                // Handle latency test
            }
            if (ImGui::MenuItem("Performance Test")) {
                // Handle performance test
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Mixer", "F1", &pImpl_->showMixer);
            ImGui::MenuItem("Plugin Rack", "F2", &pImpl_->showPluginRack);
            ImGui::MenuItem("Automation", "F3", &pImpl_->showAutomationEditor);
            ImGui::MenuItem("AI Assistant", "F6", &pImpl_->showAIAssistant);
            ImGui::MenuItem("Performance Monitor", "F7", &pImpl_->showPerformanceMonitor);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                // Handle reset layout
            }
            if (ImGui::MenuItem("Save Layout")) {
                saveLayout();
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("AI")) {
            if (ImGui::MenuItem("Analyze Project")) {
                // Handle AI project analysis
            }
            if (ImGui::MenuItem("Generate Melody")) {
                // Handle AI melody generation
            }
            if (ImGui::MenuItem("Suggest Plugins")) {
                // Handle AI plugin suggestions
            }
            ImGui::Separator();
            if (ImGui::MenuItem("AI Settings")) {
                // Handle AI settings
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("User Manual", "F1")) {
                // Handle user manual
            }
            if (ImGui::MenuItem("Keyboard Shortcuts", "F12")) {
                // Handle shortcuts
            }
            ImGui::Separator();
            ImGui::MenuItem("Demo Window", nullptr, &pImpl_->showDemoWindow);
            ImGui::Separator();
            if (ImGui::MenuItem("About MixMind AI")) {
                // Handle about dialog
            }
            ImGui::EndMenu();
        }
        
        // Performance indicator in menu bar
        if (pImpl_->performanceMonitor) {
            ImGui::SameLine(ImGui::GetWindowWidth() - 200);
            ImGui::Text("CPU: %.1f%% | MEM: %.1f%% | Latency: %.1fms", 
                       pImpl_->lastSystemMetrics.cpuUsagePercent,
                       pImpl_->lastSystemMetrics.memoryUsagePercent,
                       pImpl_->lastAudioMetrics.roundTripLatencyMs);
        }
        
        ImGui::EndMainMenuBar();
    }
}

void MixMindMainWindow::renderStatusBar() {
    // Status bar at bottom of window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - 25));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 25));
    
    if (ImGui::Begin("StatusBar", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        
        // Audio engine status
        if (pImpl_->audioEngine) {
            ImGui::Text("Audio: %s", pImpl_->audioEngine->isRunning() ? "Running" : "Stopped");
            ImGui::SameLine();
            ImGui::Text("| Device: %s", "Default Device"); // TODO: Get actual device name
        } else {
            ImGui::Text("Audio: Not Connected");
        }
        
        ImGui::SameLine();
        
        // Sample rate and buffer size
        ImGui::Text("| SR: %.0f Hz | Buffer: %d samples", 
                   pImpl_->lastAudioMetrics.sampleRate,
                   pImpl_->lastAudioMetrics.bufferSize);
        
        // AI Service status
        ImGui::SameLine();
        if (pImpl_->aiService) {
            ImGui::Text("| AI: Connected");
        } else {
            ImGui::Text("| AI: Offline");
        }
        
        // Memory usage
        ImGui::SameLine();
        ImGui::Text("| RAM: %zu MB", pImpl_->lastSystemMetrics.usedMemoryMB);
        
    }
    ImGui::End();
}

void MixMindMainWindow::setupDocking() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    pImpl_->dockspaceId = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(pImpl_->dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    
    // First-time layout setup
    if (pImpl_->firstFrame) {
        pImpl_->firstFrame = false;
        
        ImGui::DockBuilderRemoveNode(pImpl_->dockspaceId);
        ImGui::DockBuilderAddNode(pImpl_->dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(pImpl_->dockspaceId, viewport->Size);
        
        // Split the dockspace
        auto dock_id_left = ImGui::DockBuilderSplitNode(pImpl_->dockspaceId, ImGuiDir_Left, 0.25f, nullptr, &pImpl_->dockspaceId);
        auto dock_id_right = ImGui::DockBuilderSplitNode(pImpl_->dockspaceId, ImGuiDir_Right, 0.25f, nullptr, &pImpl_->dockspaceId);
        auto dock_id_bottom = ImGui::DockBuilderSplitNode(pImpl_->dockspaceId, ImGuiDir_Down, 0.3f, nullptr, &pImpl_->dockspaceId);
        
        // Dock windows to specific areas
        ImGui::DockBuilderDockWindow("Mixer", dock_id_left);
        ImGui::DockBuilderDockWindow("Plugin Rack", dock_id_right);
        ImGui::DockBuilderDockWindow("Automation", dock_id_bottom);
        ImGui::DockBuilderDockWindow("AI Assistant", dock_id_right);
        ImGui::DockBuilderDockWindow("Performance", dock_id_bottom);
        
        ImGui::DockBuilderFinish(pImpl_->dockspaceId);
        
        loadLayout();
    }
    
    ImGui::End();
}

void MixMindMainWindow::saveLayout() {
    // TODO: Implement layout saving to file
    MIXMIND_LOG_INFO("Layout saved");
}

void MixMindMainWindow::loadLayout() {
    // TODO: Implement layout loading from file
    MIXMIND_LOG_INFO("Layout loaded");
}

void MixMindMainWindow::setupDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Spacing and sizing
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.ScrollbarSize = 16;
    style.GrabMinSize = 10;
    
    // Rounding
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    
    // Borders
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
}

void MixMindMainWindow::setupProfessionalColors() {
    ImVec4* colors = ImGui::GetStyle().Colors;
    
    // Professional dark theme colors
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
    colors[ImGuiCol_Header]                 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.29f, 0.30f, 0.31f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.08f, 0.08f, 0.09f, 0.83f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.33f, 0.34f, 0.36f, 0.83f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.11f, 0.64f, 0.92f, 0.78f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

void MixMindMainWindow::renderPluginBrowser() {
    if (ImGui::Begin("Plugin Browser", &pImpl_->showPluginBrowser)) {
        ImGui::Text("Available Plugins");
        ImGui::Separator();
        
        // Search box
        static char searchBuffer[256] = "";
        ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
        
        // Plugin categories
        static int selectedCategory = 0;
        const char* categories[] = {"All", "Instruments", "Effects", "Dynamics", "EQ", "Reverb", "Delay"};
        ImGui::Combo("Category", &selectedCategory, categories, IM_ARRAYSIZE(categories));
        
        // Plugin list
        ImGui::BeginChild("PluginList", ImVec2(0, -30));
        
        // Mock plugin data - in real implementation, get from plugin scanner
        std::vector<std::string> plugins = {
            "Serum - Wavetable Synthesizer",
            "FabFilter Pro-Q 3 - Equalizer",
            "Valhalla VintageVerb - Reverb",
            "Soundtoys EchoBoy - Delay",
            "Native Instruments Massive X - Synthesizer"
        };
        
        for (const auto& plugin : plugins) {
            if (ImGui::Selectable(plugin.c_str())) {
                // Handle plugin selection
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Double-click to load plugin");
            }
        }
        
        ImGui::EndChild();
        
        // Buttons
        if (ImGui::Button("Load Plugin")) {
            // Handle load plugin
        }
        ImGui::SameLine();
        if (ImGui::Button("Rescan Plugins")) {
            // Handle rescan plugins
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            pImpl_->showPluginBrowser = false;
        }
    }
    ImGui::End();
}

void MixMindMainWindow::renderAudioSettings() {
    if (ImGui::Begin("Audio Settings", &pImpl_->showAudioSettings)) {
        ImGui::Text("Audio Device Configuration");
        ImGui::Separator();
        
        // Driver selection
        static int currentDriver = 0;
        const char* drivers[] = {"DirectSound", "WASAPI", "ASIO"};
        if (ImGui::Combo("Audio Driver", &currentDriver, drivers, IM_ARRAYSIZE(drivers))) {
            // Handle driver change
        }
        
        // Input device
        static int currentInputDevice = 0;
        const char* inputDevices[] = {"Default Input", "Microphone (Built-in)", "Line In"};
        if (ImGui::Combo("Input Device", &currentInputDevice, inputDevices, IM_ARRAYSIZE(inputDevices))) {
            // Handle input device change
        }
        
        // Output device  
        static int currentOutputDevice = 0;
        const char* outputDevices[] = {"Default Output", "Speakers (Built-in)", "Headphones"};
        if (ImGui::Combo("Output Device", &currentOutputDevice, outputDevices, IM_ARRAYSIZE(outputDevices))) {
            // Handle output device change
        }
        
        // Sample rate
        static int currentSampleRate = 1;
        const char* sampleRates[] = {"44100 Hz", "48000 Hz", "88200 Hz", "96000 Hz"};
        if (ImGui::Combo("Sample Rate", &currentSampleRate, sampleRates, IM_ARRAYSIZE(sampleRates))) {
            // Handle sample rate change
        }
        
        // Buffer size
        static int currentBufferSize = 2;
        const char* bufferSizes[] = {"64 samples", "128 samples", "256 samples", "512 samples", "1024 samples"};
        if (ImGui::Combo("Buffer Size", &currentBufferSize, bufferSizes, IM_ARRAYSIZE(bufferSizes))) {
            // Handle buffer size change
        }
        
        ImGui::Separator();
        
        // Current status
        ImGui::Text("Current Status:");
        ImGui::BulletText("Sample Rate: %.0f Hz", pImpl_->lastAudioMetrics.sampleRate);
        ImGui::BulletText("Buffer Size: %d samples", pImpl_->lastAudioMetrics.bufferSize);
        ImGui::BulletText("Input Latency: %.1f ms", pImpl_->lastAudioMetrics.inputLatencyMs);
        ImGui::BulletText("Output Latency: %.1f ms", pImpl_->lastAudioMetrics.outputLatencyMs);
        ImGui::BulletText("Total Latency: %.1f ms", pImpl_->lastAudioMetrics.roundTripLatencyMs);
        
        ImGui::Separator();
        
        // Control buttons
        if (ImGui::Button("Apply Changes")) {
            // Apply audio settings
        }
        ImGui::SameLine();
        if (ImGui::Button("Test Audio")) {
            // Test audio configuration
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            pImpl_->showAudioSettings = false;
        }
    }
    ImGui::End();
}

// Additional render methods would be implemented here...
// renderTransportControls(), renderMixerPanel(), etc.

} // namespace mixmind::ui