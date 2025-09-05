#pragma once

#include <imgui.h>
#include <string>
#include <unordered_map>

namespace mixmind::ui {

// Professional DAW Theme Manager
class Theme {
public:
    enum class Style {
        LogicProDark,
        AbletonDark,
        ProToolsDark,
        CubaseDark,
        StudioOneDark,
        Custom
    };
    
    // Color palette for professional DAW themes
    struct ColorPalette {
        // Window colors
        ImVec4 windowBg;
        ImVec4 childBg;
        ImVec4 popupBg;
        ImVec4 border;
        ImVec4 borderShadow;
        
        // Frame colors
        ImVec4 frameBg;
        ImVec4 frameBgHovered;
        ImVec4 frameBgActive;
        
        // Title colors
        ImVec4 titleBg;
        ImVec4 titleBgActive;
        ImVec4 titleBgCollapsed;
        
        // Menu colors
        ImVec4 menuBarBg;
        ImVec4 menuBg;
        
        // Scrollbar colors
        ImVec4 scrollbarBg;
        ImVec4 scrollbarGrab;
        ImVec4 scrollbarGrabHovered;
        ImVec4 scrollbarGrabActive;
        
        // Button colors
        ImVec4 button;
        ImVec4 buttonHovered;
        ImVec4 buttonActive;
        
        // Header colors
        ImVec4 header;
        ImVec4 headerHovered;
        ImVec4 headerActive;
        
        // Separator
        ImVec4 separator;
        ImVec4 separatorHovered;
        ImVec4 separatorActive;
        
        // Resize grip
        ImVec4 resizeGrip;
        ImVec4 resizeGripHovered;
        ImVec4 resizeGripActive;
        
        // Tab colors
        ImVec4 tab;
        ImVec4 tabHovered;
        ImVec4 tabActive;
        ImVec4 tabUnfocused;
        ImVec4 tabUnfocusedActive;
        
        // Plotting colors
        ImVec4 plotLines;
        ImVec4 plotLinesHovered;
        ImVec4 plotHistogram;
        ImVec4 plotHistogramHovered;
        
        // Table colors
        ImVec4 tableHeaderBg;
        ImVec4 tableBorderStrong;
        ImVec4 tableBorderLight;
        ImVec4 tableRowBg;
        ImVec4 tableRowBgAlt;
        
        // Text colors
        ImVec4 text;
        ImVec4 textDisabled;
        ImVec4 textSelectedBg;
        
        // Special DAW colors
        ImVec4 trackArea;
        ImVec4 mixerArea;
        ImVec4 pianoRollKeys;
        ImVec4 pianoRollKeysBlack;
        ImVec4 pianoRollGrid;
        ImVec4 midiNotes;
        ImVec4 midiNotesSelected;
        ImVec4 waveform;
        ImVec4 waveformPeak;
        
        // Status colors
        ImVec4 success;
        ImVec4 warning;
        ImVec4 error;
        ImVec4 info;
        
        // Transport colors
        ImVec4 playButton;
        ImVec4 recordButton;
        ImVec4 stopButton;
        
        // Level meters
        ImVec4 meterGreen;
        ImVec4 meterYellow;
        ImVec4 meterRed;
        ImVec4 meterBackground;
    };
    
    // Font configuration
    struct FontConfig {
        std::string regularFontPath = "fonts/Inter-Regular.ttf";
        std::string boldFontPath = "fonts/Inter-Bold.ttf";
        std::string monoFontPath = "fonts/JetBrainsMono-Regular.ttf";
        
        float fontSize = 14.0f;
        float iconFontSize = 16.0f;
        float titleFontSize = 18.0f;
        
        bool enableAntiAliasing = true;
        bool enableSubPixelAA = true;
    };
    
    // Style configuration
    struct StyleConfig {
        float windowRounding = 8.0f;
        float childRounding = 4.0f;
        float frameRounding = 4.0f;
        float popupRounding = 6.0f;
        float scrollbarRounding = 12.0f;
        float grabRounding = 4.0f;
        float tabRounding = 4.0f;
        
        float windowBorderSize = 1.0f;
        float childBorderSize = 0.0f;
        float popupBorderSize = 1.0f;
        float frameBorderSize = 0.0f;
        
        ImVec2 windowPadding = ImVec2(12, 8);
        ImVec2 framePadding = ImVec2(8, 4);
        ImVec2 cellPadding = ImVec2(6, 3);
        ImVec2 itemSpacing = ImVec2(8, 4);
        ImVec2 itemInnerSpacing = ImVec2(4, 4);
        ImVec2 touchExtraPadding = ImVec2(0, 0);
        
        float indentSpacing = 20.0f;
        float scrollbarSize = 16.0f;
        float grabMinSize = 12.0f;
        
        // DAW specific spacing
        float trackHeight = 80.0f;
        float mixerChannelWidth = 60.0f;
        float pianoKeyWidth = 100.0f;
        float timelineHeight = 40.0f;
        float transportHeight = 60.0f;
        
        bool enableShadows = true;
        bool enableGlow = true;
        float shadowOffset = 2.0f;
        float glowRadius = 4.0f;
    };
    
    Theme();
    ~Theme() = default;
    
    // Theme management
    void setStyle(Style style);
    Style getCurrentStyle() const { return currentStyle_; }
    
    // Apply theme to ImGui
    void apply();
    void reset();
    
    // Color palette access
    const ColorPalette& getColors() const { return colors_; }
    ColorPalette& getColors() { return colors_; }
    
    // Font configuration
    const FontConfig& getFonts() const { return fonts_; }
    FontConfig& getFonts() { return fonts_; }
    
    // Style configuration
    const StyleConfig& getStyleConfig() const { return styleConfig_; }
    StyleConfig& getStyleConfig() { return styleConfig_; }
    
    // Predefined themes
    static ColorPalette createLogicProDarkPalette();
    static ColorPalette createAbletonDarkPalette();
    static ColorPalette createProToolsDarkPalette();
    static ColorPalette createCubaseDarkPalette();
    static ColorPalette createStudioOneDarkPalette();
    
    // Utility functions
    static ImVec4 adjustBrightness(const ImVec4& color, float factor);
    static ImVec4 adjustSaturation(const ImVec4& color, float factor);
    static ImVec4 adjustAlpha(const ImVec4& color, float alpha);
    static ImVec4 blendColors(const ImVec4& a, const ImVec4& b, float factor);
    
    // Color conversion utilities
    static ImVec4 hexToImVec4(uint32_t hex, float alpha = 1.0f);
    static uint32_t imVec4ToHex(const ImVec4& color);
    static ImVec4 hsvToRgb(float h, float s, float v, float a = 1.0f);
    static void rgbToHsv(const ImVec4& rgb, float& h, float& s, float& v);
    
    // Professional color scheme generators
    static std::vector<ImVec4> generateComplementaryColors(const ImVec4& base, int count = 4);
    static std::vector<ImVec4> generateAnalogousColors(const ImVec4& base, int count = 4);
    static std::vector<ImVec4> generateTriadicColors(const ImVec4& base);
    
    // Animation support
    struct AnimationState {
        ImVec4 fromColor;
        ImVec4 toColor;
        float duration;
        float elapsed;
        bool active;
    };
    
    void startColorAnimation(const std::string& colorName, const ImVec4& targetColor, float duration);
    void updateAnimations(float deltaTime);
    ImVec4 getAnimatedColor(const std::string& colorName, const ImVec4& defaultColor);
    
    // Theme validation and debugging
    bool validateTheme() const;
    void exportTheme(const std::string& filename) const;
    bool importTheme(const std::string& filename);
    void printThemeInfo() const;
    
private:
    Style currentStyle_;
    ColorPalette colors_;
    FontConfig fonts_;
    StyleConfig styleConfig_;
    
    // Animation system
    std::unordered_map<std::string, AnimationState> animations_;
    
    // Internal methods
    void applyColorPalette();
    void applyStyleConfig();
    void loadFonts();
    
    // Theme initialization
    void initializeLogicProDark();
    void initializeAbletonDark();
    void initializeProToolsDark();
    void initializeCubaseDark();
    void initializeStudioOneDark();
};

// Global theme instance
extern Theme* g_Theme;

// Convenience macros for theme colors
#define THEME_COLOR(name) (mixmind::ui::g_Theme->getColors().name)
#define THEME_STYLE(name) (mixmind::ui::g_Theme->getStyleConfig().name)

// Scoped theme color override
class ScopedStyleColor {
public:
    ScopedStyleColor(ImGuiCol idx, const ImVec4& color) : count_(1) {
        ImGui::PushStyleColor(idx, color);
    }
    
    ScopedStyleColor(ImGuiCol idx1, const ImVec4& color1, 
                     ImGuiCol idx2, const ImVec4& color2) : count_(2) {
        ImGui::PushStyleColor(idx1, color1);
        ImGui::PushStyleColor(idx2, color2);
    }
    
    ~ScopedStyleColor() {
        ImGui::PopStyleColor(count_);
    }
    
private:
    int count_;
};

// Scoped style variable override
class ScopedStyleVar {
public:
    ScopedStyleVar(ImGuiStyleVar idx, float val) : count_(1) {
        ImGui::PushStyleVar(idx, val);
    }
    
    ScopedStyleVar(ImGuiStyleVar idx, const ImVec2& val) : count_(1) {
        ImGui::PushStyleVar(idx, val);
    }
    
    ~ScopedStyleVar() {
        ImGui::PopStyleVar(count_);
    }
    
private:
    int count_;
};

} // namespace mixmind::ui