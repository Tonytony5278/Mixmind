#include "UIComponents.h"
#include <imgui_internal.h>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace mixmind::ui {

// DAW Colors initialization
ImVec4 DAWColors::Track = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
ImVec4 DAWColors::TrackSelected = ImVec4(0.4f, 0.6f, 0.8f, 1.0f);
ImVec4 DAWColors::TrackMuted = ImVec4(0.5f, 0.3f, 0.3f, 1.0f);
ImVec4 DAWColors::TrackSolo = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
ImVec4 DAWColors::TrackRecordArmed = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);

ImVec4 DAWColors::MIDINoteDefault = ImVec4(0.4f, 0.7f, 0.9f, 1.0f);
ImVec4 DAWColors::MIDINoteSelected = ImVec4(0.9f, 0.7f, 0.4f, 1.0f);
ImVec4 DAWColors::MIDINotePlaying = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
ImVec4 DAWColors::MIDINoteVelocity[4] = {
    ImVec4(0.3f, 0.5f, 0.7f, 1.0f), // Low
    ImVec4(0.4f, 0.6f, 0.8f, 1.0f), // Med-Low
    ImVec4(0.5f, 0.7f, 0.9f, 1.0f), // Med-High
    ImVec4(0.6f, 0.8f, 1.0f, 1.0f)  // High
};

ImVec4 DAWColors::WaveformNormal = ImVec4(0.2f, 0.8f, 0.4f, 1.0f);
ImVec4 DAWColors::WaveformSelected = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
ImVec4 DAWColors::WaveformClipped = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);

ImVec4 DAWColors::Timeline = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
ImVec4 DAWColors::TimelineMarkers = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
ImVec4 DAWColors::Playhead = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
ImVec4 DAWColors::LoopRegion = ImVec4(0.3f, 0.7f, 0.3f, 0.3f);

ImVec4 DAWColors::MeterGreen = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
ImVec4 DAWColors::MeterYellow = ImVec4(0.9f, 0.9f, 0.2f, 1.0f);
ImVec4 DAWColors::MeterRed = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
ImVec4 DAWColors::MeterClip = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
ImVec4 DAWColors::MeterBackground = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

ImVec4 DAWColors::TransportPlay = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
ImVec4 DAWColors::TransportRecord = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
ImVec4 DAWColors::TransportStop = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
ImVec4 DAWColors::TransportPause = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);

void DAWColors::InitializeFromTheme() {
    if (!g_Theme) return;
    
    const auto& colors = g_Theme->getColors();
    
    Track = colors.trackArea;
    TrackSelected = colors.headerActive;
    MIDINoteDefault = colors.midiNotes;
    MIDINoteSelected = colors.midiNotesSelected;
    WaveformNormal = colors.waveform;
    WaveformSelected = colors.waveformPeak;
    
    MeterGreen = colors.meterGreen;
    MeterYellow = colors.meterYellow;
    MeterRed = colors.meterRed;
    MeterBackground = colors.meterBackground;
    
    TransportPlay = colors.playButton;
    TransportRecord = colors.recordButton;
    TransportStop = colors.stopButton;
}

// Utility functions
ImVec4 UIComponents::adjustBrightness(const ImVec4& color, float factor) {
    return ImVec4(
        std::clamp(color.x * factor, 0.0f, 1.0f),
        std::clamp(color.y * factor, 0.0f, 1.0f),
        std::clamp(color.z * factor, 0.0f, 1.0f),
        color.w
    );
}

ImVec4 UIComponents::blendColors(const ImVec4& a, const ImVec4& b, float factor) {
    factor = std::clamp(factor, 0.0f, 1.0f);
    return ImVec4(
        a.x + (b.x - a.x) * factor,
        a.y + (b.y - a.y) * factor,
        a.z + (b.z - a.z) * factor,
        a.w + (b.w - a.w) * factor
    );
}

bool UIComponents::StyledButton(const char* label, ButtonStyle style, const ImVec2& size) {
    ImVec4 buttonColor, hoverColor, activeColor;
    
    switch (style) {
        case ButtonStyle::Primary:
            buttonColor = THEME_COLOR(button);
            hoverColor = THEME_COLOR(buttonHovered);
            activeColor = THEME_COLOR(buttonActive);
            break;
        case ButtonStyle::Success:
            buttonColor = THEME_COLOR(success);
            hoverColor = adjustBrightness(THEME_COLOR(success), 1.2f);
            activeColor = adjustBrightness(THEME_COLOR(success), 0.8f);
            break;
        case ButtonStyle::Warning:
            buttonColor = THEME_COLOR(warning);
            hoverColor = adjustBrightness(THEME_COLOR(warning), 1.2f);
            activeColor = adjustBrightness(THEME_COLOR(warning), 0.8f);
            break;
        case ButtonStyle::Danger:
            buttonColor = THEME_COLOR(error);
            hoverColor = adjustBrightness(THEME_COLOR(error), 1.2f);
            activeColor = adjustBrightness(THEME_COLOR(error), 0.8f);
            break;
        case ButtonStyle::Transport:
            buttonColor = DAWColors::TransportPlay;
            hoverColor = adjustBrightness(DAWColors::TransportPlay, 1.2f);
            activeColor = adjustBrightness(DAWColors::TransportPlay, 0.8f);
            break;
        default:
            buttonColor = THEME_COLOR(frameBg);
            hoverColor = THEME_COLOR(frameBgHovered);
            activeColor = THEME_COLOR(frameBgActive);
            break;
    }
    
    ScopedStyleColor colors(
        ImGuiCol_Button, buttonColor,
        ImGuiCol_ButtonHovered, hoverColor
    );
    
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    bool result = ImGui::Button(label, size);
    ImGui::PopStyleColor();
    
    return result;
}

bool UIComponents::IconButton(const char* icon, const char* tooltip, ButtonStyle style, const ImVec2& size) {
    bool result = StyledButton(icon, style, size);
    
    if (tooltip && ImGui::IsItemHovered()) {
        ShowTooltip(tooltip);
    }
    
    return result;
}

bool UIComponents::VerticalSlider(const char* label, float* value, float min_val, float max_val,
                                 const ImVec2& size, const char* format, const SliderStyle& style) {
    ImGui::PushID(label);
    
    // Custom vertical slider with professional styling
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        ImGui::PopID();
        return false;
    }
    
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& gui_style = g.Style;
    const ImGuiID id = window->GetID("");
    
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
    const ImRect total_bb = frame_bb;
    
    ImGui::ItemSize(total_bb, gui_style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id)) {
        ImGui::PopID();
        return false;
    }
    
    // Handle input
    bool hovered = ImGui::ItemHoverable(frame_bb, id);
    bool temp_input_allowed = true;
    bool temp_input_start = false;
    
    if (temp_input_allowed) {
        temp_input_start = (hovered && g.IO.MouseClicked[0]);
    }
    
    if (temp_input_start) {
        ImGui::SetActiveID(id, window);
        ImGui::SetFocusID(id, window);
        ImGui::FocusWindow(window);
    }
    
    // Draw slider
    ImDrawList* draw_list = window->DrawList;
    
    // Background track
    const float track_left = frame_bb.Min.x + size.x * 0.5f - style.trackHeight * 0.5f;
    const float track_right = track_left + style.trackHeight;
    const ImRect track_bb(ImVec2(track_left, frame_bb.Min.y + style.knobRadius), 
                         ImVec2(track_right, frame_bb.Max.y - style.knobRadius));
    
    draw_list->AddRectFilled(track_bb.Min, track_bb.Max, colorToImU32(style.trackColor), 
                            style.trackHeight * 0.5f);
    
    // Value fill
    const float value_normalized = (*value - min_val) / (max_val - min_val);
    const float fill_height = (track_bb.Max.y - track_bb.Min.y) * value_normalized;
    const ImRect fill_bb(track_bb.Min, 
                        ImVec2(track_bb.Max.x, track_bb.Max.y - fill_height));
    
    if (fill_height > 0) {
        draw_list->AddRectFilled(ImVec2(track_bb.Min.x, track_bb.Max.y - fill_height), 
                                track_bb.Max, colorToImU32(style.fillColor), 
                                style.trackHeight * 0.5f);
    }
    
    // Knob
    const float knob_y = track_bb.Max.y - fill_height;
    const ImVec2 knob_center(frame_bb.Min.x + size.x * 0.5f, knob_y);
    
    ImU32 knob_color = colorToImU32(hovered ? adjustBrightness(style.knobColor, 1.2f) : style.knobColor);
    draw_list->AddCircleFilled(knob_center, style.knobRadius, knob_color);
    draw_list->AddCircle(knob_center, style.knobRadius, colorToImU32(adjustBrightness(style.knobColor, 0.7f)), 0, 1.5f);
    
    // Handle dragging
    bool value_changed = false;
    if (ImGui::IsItemActive()) {
        const float mouse_delta = g.IO.MouseDelta.y;
        if (mouse_delta != 0.0f) {
            const float delta = -mouse_delta / (track_bb.Max.y - track_bb.Min.y);
            *value = std::clamp(*value + delta * (max_val - min_val), min_val, max_val);
            value_changed = true;
        }
    }
    
    // Label and value display
    if (style.showLabels && label && strlen(label) > 0) {
        const ImVec2 label_pos(frame_bb.Min.x, frame_bb.Max.y + 2);
        ImGui::SetCursorScreenPos(label_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, style.textColor);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    }
    
    if (style.showValue) {
        char value_text[32];
        snprintf(value_text, sizeof(value_text), format, *value);
        const ImVec2 text_size = ImGui::CalcTextSize(value_text);
        const ImVec2 value_pos(frame_bb.Min.x + (size.x - text_size.x) * 0.5f, frame_bb.Min.y - text_size.y - 2);
        ImGui::SetCursorScreenPos(value_pos);
        ImGui::PushStyleColor(ImGuiCol_Text, style.textColor);
        ImGui::Text("%s", value_text);
        ImGui::PopStyleColor();
    }
    
    ImGui::PopID();
    return value_changed;
}

void UIComponents::LevelMeter(const char* label, float level_db, float peak_db,
                             const ImVec2& size, const MeterStyle& style) {
    ImGui::PushID(label);
    
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        ImGui::PopID();
        return;
    }
    
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(frame_bb);
    if (!ImGui::ItemAdd(frame_bb, 0)) {
        ImGui::PopID();
        return;
    }
    
    ImDrawList* draw_list = window->DrawList;
    
    // Background
    draw_list->AddRectFilled(frame_bb.Min, frame_bb.Max, colorToImU32(style.backgroundColor), 2.0f);
    
    // Calculate levels
    const float meter_min = -60.0f;  // dB
    const float meter_max = 6.0f;    // dB
    
    const float level_normalized = std::clamp((level_db - meter_min) / (meter_max - meter_min), 0.0f, 1.0f);
    const float peak_normalized = std::clamp((peak_db - meter_min) / (meter_max - meter_min), 0.0f, 1.0f);
    
    // Level segments
    const float segment_count = (frame_bb.Max.y - frame_bb.Min.y) / 3.0f;
    const float level_height = (frame_bb.Max.y - frame_bb.Min.y) * level_normalized;
    
    for (float y = frame_bb.Max.y - 3; y > frame_bb.Max.y - level_height; y -= 3) {
        float segment_pos = (frame_bb.Max.y - y) / (frame_bb.Max.y - frame_bb.Min.y);
        ImVec4 segment_color;
        
        if (segment_pos <= (style.greenThreshold - meter_min) / (meter_max - meter_min)) {
            segment_color = style.greenColor;
        } else if (segment_pos <= (style.yellowThreshold - meter_min) / (meter_max - meter_min)) {
            segment_color = style.yellowColor;
        } else if (segment_pos <= (style.redThreshold - meter_min) / (meter_max - meter_min)) {
            segment_color = style.redColor;
        } else {
            segment_color = style.clipColor;
        }
        
        const ImRect segment_bb(ImVec2(frame_bb.Min.x + 1, y), 
                               ImVec2(frame_bb.Max.x - 1, y + 2));
        draw_list->AddRectFilled(segment_bb.Min, segment_bb.Max, colorToImU32(segment_color));
    }
    
    // Peak hold line
    if (style.showPeakHold && peak_db > meter_min) {
        const float peak_y = frame_bb.Max.y - (frame_bb.Max.y - frame_bb.Min.y) * peak_normalized;
        draw_list->AddLine(ImVec2(frame_bb.Min.x, peak_y), 
                          ImVec2(frame_bb.Max.x, peak_y), 
                          colorToImU32(ImVec4(1.0f, 1.0f, 1.0f, 0.8f)), 1.0f);
    }
    
    // Scale marks
    if (style.showScale) {
        const std::vector<float> scale_marks = {0, -6, -12, -18, -24, -30};
        for (float db : scale_marks) {
            if (db >= meter_min && db <= meter_max) {
                const float mark_normalized = (db - meter_min) / (meter_max - meter_min);
                const float mark_y = frame_bb.Max.y - (frame_bb.Max.y - frame_bb.Min.y) * mark_normalized;
                
                draw_list->AddLine(ImVec2(frame_bb.Max.x, mark_y),
                                  ImVec2(frame_bb.Max.x + 4, mark_y),
                                  colorToImU32(ImVec4(0.8f, 0.8f, 0.8f, 0.6f)));
            }
        }
    }
    
    ImGui::PopID();
}

bool UIComponents::TransportControls(TransportState& state, const ImVec2& size) {
    ImGui::PushID("Transport");
    
    bool changed = false;
    const float button_size = size.y * 0.8f;
    const float spacing = 8.0f;
    
    ImGui::BeginChild("TransportControls", size, true);
    
    // Center the buttons
    const float total_width = button_size * 5 + spacing * 4;
    const float start_x = (size.x - total_width) * 0.5f;
    ImGui::SetCursorPosX(start_x);
    
    // Rewind
    if (IconButton("⏮", "Rewind", ButtonStyle::Secondary, ImVec2(button_size, button_size))) {
        state.position = 0.0;
        changed = true;
    }
    ImGui::SameLine(0, spacing);
    
    // Play/Pause
    const char* play_icon = state.playing ? "⏸" : "▶";
    const char* play_tooltip = state.playing ? "Pause" : "Play";
    ButtonStyle play_style = state.playing ? ButtonStyle::Warning : ButtonStyle::Success;
    
    if (IconButton(play_icon, play_tooltip, play_style, ImVec2(button_size, button_size))) {
        state.playing = !state.playing;
        changed = true;
    }
    ImGui::SameLine(0, spacing);
    
    // Stop
    if (IconButton("⏹", "Stop", ButtonStyle::Secondary, ImVec2(button_size, button_size))) {
        state.playing = false;
        state.position = 0.0;
        changed = true;
    }
    ImGui::SameLine(0, spacing);
    
    // Record
    ButtonStyle record_style = state.recording ? ButtonStyle::Danger : ButtonStyle::Secondary;
    if (IconButton("⏺", "Record", record_style, ImVec2(button_size, button_size))) {
        state.recording = !state.recording;
        changed = true;
    }
    ImGui::SameLine(0, spacing);
    
    // Loop
    ButtonStyle loop_style = state.looping ? ButtonStyle::Success : ButtonStyle::Secondary;
    if (IconButton("🔄", "Loop", loop_style, ImVec2(button_size, button_size))) {
        state.looping = !state.looping;
        changed = true;
    }
    
    // Time display
    ImGui::SetCursorPosY(button_size + 8);
    ImGui::SetCursorPosX(start_x);
    
    std::string time_str = formatTime(state.position);
    ImGui::Text("%s", time_str.c_str());
    
    ImGui::SameLine(0, 20);
    
    // Tempo display
    ImGui::Text("♩ %.1f", state.tempo);
    
    ImGui::EndChild();
    
    ImGui::PopID();
    return changed;
}

bool UIComponents::ChannelStrip(ChannelStripState& state, const ImVec2& size) {
    ImGui::PushID(state.channel_number);
    
    bool changed = false;
    const float button_width = size.x - 16;
    const float slider_height = size.y * 0.5f;
    
    ImGui::BeginChild("ChannelStrip", size, true);
    
    // Channel name and number
    ImGui::SetCursorPosX(4);
    ImGui::Text("%d", state.channel_number);
    ImGui::SetCursorPosX(4);
    ImGui::PushItemWidth(button_width);
    if (ImGui::InputText("##name", &state.name)) {
        changed = true;
    }
    ImGui::PopItemWidth();
    
    ImGui::Spacing();
    
    // Gain fader
    ImGui::SetCursorPosX(8);
    if (VerticalSlider("Gain", &state.gain, -60.0f, 12.0f, 
                      ImVec2(size.x - 16, slider_height), "%.1fdB")) {
        changed = true;
    }
    
    ImGui::Spacing();
    
    // Pan knob
    ImGui::SetCursorPosX((size.x - 50) * 0.5f);
    if (Knob("Pan", &state.pan, -1.0f, 1.0f, "%.2f")) {
        changed = true;
    }
    
    ImGui::Spacing();
    
    // Control buttons
    ImGui::SetCursorPosX(4);
    ButtonStyle mute_style = state.mute ? ButtonStyle::Warning : ButtonStyle::Secondary;
    if (StyledButton("M", mute_style, ImVec2(button_width * 0.3f, 25))) {
        state.mute = !state.mute;
        changed = true;
    }
    
    ImGui::SameLine();
    ButtonStyle solo_style = state.solo ? ButtonStyle::Success : ButtonStyle::Secondary;
    if (StyledButton("S", solo_style, ImVec2(button_width * 0.3f, 25))) {
        state.solo = !state.solo;
        changed = true;
    }
    
    ImGui::SameLine();
    ButtonStyle rec_style = state.record_arm ? ButtonStyle::Danger : ButtonStyle::Secondary;
    if (StyledButton("R", rec_style, ImVec2(button_width * 0.3f, 25))) {
        state.record_arm = !state.record_arm;
        changed = true;
    }
    
    ImGui::Spacing();
    
    // Level meters
    const float meter_width = (size.x - 20) * 0.5f;
    ImGui::SetCursorPosX(4);
    LevelMeter("L", state.level_l, -100.0f, ImVec2(meter_width, 100));
    ImGui::SameLine();
    LevelMeter("R", state.level_r, -100.0f, ImVec2(meter_width, 100));
    
    ImGui::EndChild();
    
    ImGui::PopID();
    return changed;
}

// Default style getters
UIComponents::SliderStyle UIComponents::getDefaultSliderStyle() {
    SliderStyle style;
    style.trackColor = THEME_COLOR(frameBg);
    style.fillColor = THEME_COLOR(buttonActive);
    style.knobColor = THEME_COLOR(button);
    style.textColor = THEME_COLOR(text);
    return style;
}

UIComponents::MeterStyle UIComponents::getDefaultMeterStyle() {
    MeterStyle style;
    style.backgroundColor = DAWColors::MeterBackground;
    style.greenColor = DAWColors::MeterGreen;
    style.yellowColor = DAWColors::MeterYellow;
    style.redColor = DAWColors::MeterRed;
    style.clipColor = DAWColors::MeterClip;
    return style;
}

UIComponents::KnobStyle UIComponents::getDefaultKnobStyle() {
    KnobStyle style;
    style.baseColor = THEME_COLOR(frameBg);
    style.valueColor = THEME_COLOR(buttonActive);
    style.textColor = THEME_COLOR(text);
    return style;
}

// Utility functions
std::string UIComponents::formatTime(double seconds, const char* format) {
    int minutes = static_cast<int>(seconds / 60);
    int secs = static_cast<int>(seconds) % 60;
    int ms = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000);
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << minutes << ":"
       << std::setfill('0') << std::setw(2) << secs << "."
       << std::setfill('0') << std::setw(3) << ms;
    
    return ss.str();
}

std::string UIComponents::formatFrequency(float hz) {
    if (hz >= 1000.0f) {
        return std::to_string(static_cast<int>(hz / 1000.0f)) + "kHz";
    } else {
        return std::to_string(static_cast<int>(hz)) + "Hz";
    }
}

std::string UIComponents::formatGain(float db, int precision) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << db << "dB";
    return ss.str();
}

// Internal helpers
ImU32 UIComponents::colorToImU32(const ImVec4& color) {
    return ImGui::ColorConvertFloat4ToU32(color);
}

float UIComponents::dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

float UIComponents::linearToDb(float linear) {
    return 20.0f * std::log10(std::max(linear, 1e-6f));
}

void UIComponents::ShowTooltip(const char* text, float delay) {
    if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > delay) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Animation helpers
void UIAnimator::AnimatedFloat::Update(float dt) {
    if (IsAnimating()) {
        const float diff = target - current;
        current += diff * speed * dt;
        
        if (std::abs(diff) < 0.001f) {
            current = target;
        }
    }
}

void UIAnimator::AnimatedFloat::SetTarget(float new_target) {
    target = new_target;
}

bool UIAnimator::AnimatedColor::IsAnimating() const {
    return std::abs(current.x - target.x) > 0.001f ||
           std::abs(current.y - target.y) > 0.001f ||
           std::abs(current.z - target.z) > 0.001f ||
           std::abs(current.w - target.w) > 0.001f;
}

ImVec4 UIAnimator::LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

float UIAnimator::SmoothStep(float edge0, float edge1, float x) {
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3 - 2 * x);
}

} // namespace mixmind::ui