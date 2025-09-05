#pragma once

#include "Theme.h"
#include <imgui.h>
#include <string>
#include <functional>
#include <vector>

namespace mixmind::ui {

// Professional UI Components for DAW Applications
class UIComponents {
public:
    // Color utilities
    static ImVec4 adjustBrightness(const ImVec4& color, float factor);
    static ImVec4 blendColors(const ImVec4& a, const ImVec4& b, float factor);
    
    // Professional button styles
    enum class ButtonStyle {
        Primary,
        Secondary,
        Success,
        Warning,
        Danger,
        Transport,
        Mute,
        Solo,
        Record,
        Custom
    };
    
    static bool StyledButton(const char* label, ButtonStyle style = ButtonStyle::Primary, 
                            const ImVec2& size = ImVec2(0, 0));
    static bool IconButton(const char* icon, const char* tooltip = nullptr, 
                          ButtonStyle style = ButtonStyle::Secondary, 
                          const ImVec2& size = ImVec2(30, 30));
    
    // Professional sliders with DAW styling
    struct SliderStyle {
        ImVec4 trackColor;
        ImVec4 fillColor;
        ImVec4 knobColor;
        ImVec4 textColor;
        float trackHeight = 4.0f;
        float knobRadius = 8.0f;
        bool showValue = true;
        bool showLabels = true;
    };
    
    static bool VerticalSlider(const char* label, float* value, float min_val, float max_val,
                              const ImVec2& size = ImVec2(40, 160),
                              const char* format = "%.1f",
                              const SliderStyle& style = getDefaultSliderStyle());
    
    static bool HorizontalSlider(const char* label, float* value, float min_val, float max_val,
                                const ImVec2& size = ImVec2(200, 30),
                                const char* format = "%.1f",
                                const SliderStyle& style = getDefaultSliderStyle());
    
    // Level meters for audio
    struct MeterStyle {
        ImVec4 backgroundColor;
        ImVec4 greenColor;
        ImVec4 yellowColor;
        ImVec4 redColor;
        ImVec4 clipColor;
        float greenThreshold = -18.0f;  // dB
        float yellowThreshold = -6.0f;  // dB
        float redThreshold = -3.0f;     // dB
        bool showScale = true;
        bool showPeakHold = true;
    };
    
    static void LevelMeter(const char* label, float level_db, float peak_db = -100.0f,
                          const ImVec2& size = ImVec2(20, 200),
                          const MeterStyle& style = getDefaultMeterStyle());
    
    static void StereoLevelMeter(const char* label, float left_db, float right_db,
                                float left_peak_db = -100.0f, float right_peak_db = -100.0f,
                                const ImVec2& size = ImVec2(40, 200),
                                const MeterStyle& style = getDefaultMeterStyle());
    
    // Professional knobs/rotary controls
    struct KnobStyle {
        ImVec4 baseColor;
        ImVec4 valueColor;
        ImVec4 textColor;
        float radius = 25.0f;
        float lineThickness = 3.0f;
        float sensitivity = 1.0f;
        bool showValue = true;
        bool bipolar = false;  // -1 to 1 instead of 0 to 1
    };
    
    static bool Knob(const char* label, float* value, float min_val, float max_val,
                    const char* format = "%.1f",
                    const KnobStyle& style = getDefaultKnobStyle());
    
    // Transport controls
    struct TransportState {
        bool playing = false;
        bool recording = false;
        bool looping = false;
        bool metronome = false;
        double position = 0.0;  // In seconds
        double length = 240.0;  // In seconds
        double tempo = 120.0;   // BPM
    };
    
    static bool TransportControls(TransportState& state, const ImVec2& size = ImVec2(300, 60));
    
    // Time display and scrub bar
    static bool TimeDisplay(double time_seconds, const char* format = "mm:ss.ms",
                           const ImVec2& size = ImVec2(100, 30));
    
    static bool ScrubBar(double* position, double length, 
                        const ImVec2& size = ImVec2(400, 20));
    
    // Professional panels and sections
    static void BeginPanel(const char* title, const ImVec2& size = ImVec2(0, 0),
                          bool* open = nullptr);
    static void EndPanel();
    
    static bool BeginSection(const char* title, bool* open = nullptr,
                            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen);
    static void EndSection();
    
    // Channel strip components
    struct ChannelStripState {
        float gain = 0.0f;       // dB
        float pan = 0.0f;        // -1.0 to 1.0
        bool mute = false;
        bool solo = false;
        bool record_arm = false;
        float level_l = -100.0f; // dB
        float level_r = -100.0f; // dB
        std::string name = "Track";
        int channel_number = 1;
    };
    
    static bool ChannelStrip(ChannelStripState& state, 
                            const ImVec2& size = ImVec2(80, 400));
    
    // EQ visualization
    struct EQBand {
        float frequency = 1000.0f;  // Hz
        float gain = 0.0f;          // dB
        float q = 1.0f;             // Q factor
        bool enabled = true;
        enum Type { HighPass, LowShelf, Bell, HighShelf, LowPass } type = Bell;
    };
    
    static bool EQGraph(std::vector<EQBand>& bands, 
                       const ImVec2& size = ImVec2(400, 200),
                       float min_freq = 20.0f, float max_freq = 20000.0f,
                       float min_gain = -24.0f, float max_gain = 24.0f);
    
    // Waveform display
    static void WaveformDisplay(const float* samples, size_t sample_count,
                               const ImVec2& size = ImVec2(800, 120),
                               float zoom = 1.0f, size_t offset = 0,
                               const ImVec4& waveform_color = ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
    
    // Piano roll key display
    static bool PianoKeys(int& selected_key, int octave_start = 2, int octave_count = 7,
                         const ImVec2& size = ImVec2(100, 400));
    
    // Professional tooltips
    static void ShowTooltip(const char* text, float delay = 0.5f);
    static void ShowHelpMarker(const char* desc, const char* marker = "(?)");
    
    // Status indicators
    enum class StatusType { Info, Success, Warning, Error };
    static void StatusIndicator(const char* label, StatusType type, bool active = true,
                               const ImVec2& size = ImVec2(20, 20));
    
    // Professional loading spinner
    static void LoadingSpinner(const char* label, float radius = 16.0f, 
                              float thickness = 3.0f, const ImVec4& color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
    
    // Default style getters
    static SliderStyle getDefaultSliderStyle();
    static MeterStyle getDefaultMeterStyle();
    static KnobStyle getDefaultKnobStyle();
    
    // Utility functions
    static std::string formatTime(double seconds, const char* format = "mm:ss.ms");
    static std::string formatFrequency(float hz);
    static std::string formatGain(float db, int precision = 1);
    
private:
    // Internal helpers
    static void drawCircle(ImDrawList* draw_list, const ImVec2& center, float radius, 
                          ImU32 color, int segments = 32);
    static void drawArc(ImDrawList* draw_list, const ImVec2& center, float radius,
                       float angle_start, float angle_end, ImU32 color, float thickness = 1.0f);
    static ImU32 colorToImU32(const ImVec4& color);
    static float dbToLinear(float db);
    static float linearToDb(float linear);
};

// Professional color schemes for different DAW elements
struct DAWColors {
    static ImVec4 Track;
    static ImVec4 TrackSelected;
    static ImVec4 TrackMuted;
    static ImVec4 TrackSolo;
    static ImVec4 TrackRecordArmed;
    
    static ImVec4 MIDINoteDefault;
    static ImVec4 MIDINoteSelected;
    static ImVec4 MIDINotePlaying;
    static ImVec4 MIDINoteVelocity[4]; // Low, Med-Low, Med-High, High
    
    static ImVec4 WaveformNormal;
    static ImVec4 WaveformSelected;
    static ImVec4 WaveformClipped;
    
    static ImVec4 Timeline;
    static ImVec4 TimelineMarkers;
    static ImVec4 Playhead;
    static ImVec4 LoopRegion;
    
    static ImVec4 MeterGreen;
    static ImVec4 MeterYellow;
    static ImVec4 MeterRed;
    static ImVec4 MeterClip;
    static ImVec4 MeterBackground;
    
    static ImVec4 TransportPlay;
    static ImVec4 TransportRecord;
    static ImVec4 TransportStop;
    static ImVec4 TransportPause;
    
    // Initialize colors based on current theme
    static void InitializeFromTheme();
};

// Animation helpers for smooth UI transitions
class UIAnimator {
public:
    static float SmoothStep(float edge0, float edge1, float x);
    static ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t);
    static float EaseInOut(float t);
    static float EaseIn(float t);
    static float EaseOut(float t);
    
    // Animated values
    struct AnimatedFloat {
        float current = 0.0f;
        float target = 0.0f;
        float speed = 5.0f;
        
        void Update(float dt);
        void SetTarget(float new_target);
        float Get() const { return current; }
        bool IsAnimating() const { return std::abs(current - target) > 0.001f; }
    };
    
    struct AnimatedColor {
        ImVec4 current = ImVec4(0, 0, 0, 0);
        ImVec4 target = ImVec4(0, 0, 0, 0);
        float speed = 5.0f;
        
        void Update(float dt);
        void SetTarget(const ImVec4& new_target);
        ImVec4 Get() const { return current; }
        bool IsAnimating() const;
    };
};

} // namespace mixmind::ui