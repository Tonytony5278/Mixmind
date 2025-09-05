#pragma once

#include "UIComponents.h"
#include <imgui.h>
#include <memory>

#ifdef MIXMIND_LEVEL_AUDIO
#include <JuceHeader.h>
namespace te = tracktion_engine;
#endif

namespace mixmind::ui {

// Professional DAW Transport Bar with Logic Pro styling
class TransportBar {
public:
    struct TransportState {
        bool playing = false;
        bool recording = false;
        bool looping = false;
        bool metronome = false;
        double position = 0.0;      // Current playback position in seconds
        double length = 240.0;      // Total length in seconds
        double tempo = 120.0;       // BPM
        int timeSignatureNumer = 4; // Time signature numerator
        int timeSignatureDenom = 4; // Time signature denominator
        
        // Transport mode
        enum class Mode {
            Stop,
            Play,
            Record,
            Pause
        } mode = Mode::Stop;
        
        // Position format
        enum class TimeFormat {
            Bars_Beats,
            Minutes_Seconds,
            Samples,
            Frames
        } timeFormat = TimeFormat::Bars_Beats;
    };

#ifdef MIXMIND_LEVEL_AUDIO
    TransportBar(te::Edit& edit);
#else
    TransportBar();
#endif
    ~TransportBar() = default;
    
    // Main render function
    void render(const ImVec2& size = ImVec2(800, 80));
    
    // Transport controls
    void play();
    void pause();
    void stop();
    void record();
    void togglePlayPause();
    void toggleLoop();
    void toggleMetronome();
    
    // Position control
    void setPosition(double seconds);
    void setTempo(double bpm);
    void setTimeSignature(int numerator, int denominator);
    void setLength(double seconds);
    
    // State access
    const TransportState& getState() const { return state_; }
    TransportState& getState() { return state_; }
    
    // Callbacks
    std::function<void(TransportState::Mode)> onModeChanged;
    std::function<void(double)> onPositionChanged;
    std::function<void(double)> onTempoChanged;
    std::function<void(bool)> onLoopChanged;
    std::function<void(bool)> onMetronomeChanged;
    
    // Visual feedback
    void setLevels(float leftLevel, float rightLevel);
    void setCPUUsage(float cpuPercent);
    void setBufferSize(int samples);
    void setSampleRate(int hz);
    
private:
    TransportState state_;
    
#ifdef MIXMIND_LEVEL_AUDIO
    te::Edit* edit_ = nullptr;
#endif
    
    // Animation states
    UIAnimator::AnimatedFloat playButtonGlow_;
    UIAnimator::AnimatedFloat recordButtonGlow_;
    UIAnimator::AnimatedColor playButtonColor_;
    UIAnimator::AnimatedColor recordButtonColor_;
    
    // Performance monitoring
    float leftLevel_ = -100.0f;
    float rightLevel_ = -100.0f;
    float cpuUsage_ = 0.0f;
    int bufferSize_ = 512;
    int sampleRate_ = 48000;
    
    // UI state
    bool showAdvanced_ = false;
    bool showMeters_ = true;
    bool showTempo_ = true;
    bool showTime_ = true;
    
    // Rendering methods
    void renderMainControls();
    void renderTimeDisplay();
    void renderTempoControls();
    void renderLevelMeters();
    void renderAdvancedControls();
    void renderPositionBar();
    void renderPerformanceInfo();
    
    // Internal helpers
    void updateAnimations(float deltaTime);
    void triggerModeChange(TransportState::Mode newMode);
    std::string formatPosition(double seconds) const;
    std::string formatTempo(double bpm) const;
    void handleJuceIntegration(); // Only available if JUCE is enabled
    
    // Theme integration
    void applyThemeColors();
    ImVec4 getTransportColor(TransportState::Mode mode) const;
    
    // Professional DAW styling
    struct TransportStyle {
        // Button sizes
        float mainButtonSize = 50.0f;
        float secondaryButtonSize = 35.0f;
        float miniButtonSize = 25.0f;
        
        // Colors
        ImVec4 playColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
        ImVec4 recordColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
        ImVec4 stopColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        ImVec4 loopColor = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
        
        // Glow effects
        bool enableGlow = true;
        float glowRadius = 4.0f;
        float glowIntensity = 0.8f;
        
        // Animation
        float animationSpeed = 8.0f;
        bool smoothTransitions = true;
    } style_;
    
    // Update timer for real-time display
    float updateTimer_ = 0.0f;
    const float updateInterval_ = 1.0f / 60.0f; // 60 FPS updates
};

} // namespace mixmind::ui