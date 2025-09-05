#include "Theme.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace mixmind::ui {

// Global theme instance
Theme* g_Theme = nullptr;

Theme::Theme() : currentStyle_(Style::LogicProDark) {
    // Initialize with Logic Pro Dark theme as default
    initializeLogicProDark();
    g_Theme = this;
}

void Theme::setStyle(Style style) {
    if (currentStyle_ == style) return;
    
    currentStyle_ = style;
    
    switch (style) {
        case Style::LogicProDark:
            initializeLogicProDark();
            break;
        case Style::AbletonDark:
            initializeAbletonDark();
            break;
        case Style::ProToolsDark:
            initializeProToolsDark();
            break;
        case Style::CubaseDark:
            initializeCubaseDark();
            break;
        case Style::StudioOneDark:
            initializeStudioOneDark();
            break;
        case Style::Custom:
            // Keep current configuration
            break;
    }
    
    apply();
}

void Theme::apply() {
    applyColorPalette();
    applyStyleConfig();
    loadFonts();
}

void Theme::reset() {
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle(); // Reset to default
    setStyle(currentStyle_); // Reapply current theme
}

void Theme::applyColorPalette() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    style.Colors[ImGuiCol_Text] = colors_.text;
    style.Colors[ImGuiCol_TextDisabled] = colors_.textDisabled;
    style.Colors[ImGuiCol_WindowBg] = colors_.windowBg;
    style.Colors[ImGuiCol_ChildBg] = colors_.childBg;
    style.Colors[ImGuiCol_PopupBg] = colors_.popupBg;
    style.Colors[ImGuiCol_Border] = colors_.border;
    style.Colors[ImGuiCol_BorderShadow] = colors_.borderShadow;
    style.Colors[ImGuiCol_FrameBg] = colors_.frameBg;
    style.Colors[ImGuiCol_FrameBgHovered] = colors_.frameBgHovered;
    style.Colors[ImGuiCol_FrameBgActive] = colors_.frameBgActive;
    style.Colors[ImGuiCol_TitleBg] = colors_.titleBg;
    style.Colors[ImGuiCol_TitleBgActive] = colors_.titleBgActive;
    style.Colors[ImGuiCol_TitleBgCollapsed] = colors_.titleBgCollapsed;
    style.Colors[ImGuiCol_MenuBarBg] = colors_.menuBarBg;
    style.Colors[ImGuiCol_ScrollbarBg] = colors_.scrollbarBg;
    style.Colors[ImGuiCol_ScrollbarGrab] = colors_.scrollbarGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = colors_.scrollbarGrabHovered;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = colors_.scrollbarGrabActive;
    style.Colors[ImGuiCol_CheckMark] = colors_.text;
    style.Colors[ImGuiCol_SliderGrab] = colors_.button;
    style.Colors[ImGuiCol_SliderGrabActive] = colors_.buttonActive;
    style.Colors[ImGuiCol_Button] = colors_.button;
    style.Colors[ImGuiCol_ButtonHovered] = colors_.buttonHovered;
    style.Colors[ImGuiCol_ButtonActive] = colors_.buttonActive;
    style.Colors[ImGuiCol_Header] = colors_.header;
    style.Colors[ImGuiCol_HeaderHovered] = colors_.headerHovered;
    style.Colors[ImGuiCol_HeaderActive] = colors_.headerActive;
    style.Colors[ImGuiCol_Separator] = colors_.separator;
    style.Colors[ImGuiCol_SeparatorHovered] = colors_.separatorHovered;
    style.Colors[ImGuiCol_SeparatorActive] = colors_.separatorActive;
    style.Colors[ImGuiCol_ResizeGrip] = colors_.resizeGrip;
    style.Colors[ImGuiCol_ResizeGripHovered] = colors_.resizeGripHovered;
    style.Colors[ImGuiCol_ResizeGripActive] = colors_.resizeGripActive;
    style.Colors[ImGuiCol_Tab] = colors_.tab;
    style.Colors[ImGuiCol_TabHovered] = colors_.tabHovered;
    style.Colors[ImGuiCol_TabActive] = colors_.tabActive;
    style.Colors[ImGuiCol_TabUnfocused] = colors_.tabUnfocused;
    style.Colors[ImGuiCol_TabUnfocusedActive] = colors_.tabUnfocusedActive;
    style.Colors[ImGuiCol_PlotLines] = colors_.plotLines;
    style.Colors[ImGuiCol_PlotLinesHovered] = colors_.plotLinesHovered;
    style.Colors[ImGuiCol_PlotHistogram] = colors_.plotHistogram;
    style.Colors[ImGuiCol_PlotHistogramHovered] = colors_.plotHistogramHovered;
    style.Colors[ImGuiCol_TableHeaderBg] = colors_.tableHeaderBg;
    style.Colors[ImGuiCol_TableBorderStrong] = colors_.tableBorderStrong;
    style.Colors[ImGuiCol_TableBorderLight] = colors_.tableBorderLight;
    style.Colors[ImGuiCol_TableRowBg] = colors_.tableRowBg;
    style.Colors[ImGuiCol_TableRowBgAlt] = colors_.tableRowBgAlt;
    style.Colors[ImGuiCol_TextSelectedBg] = colors_.textSelectedBg;
    style.Colors[ImGuiCol_DragDropTarget] = colors_.buttonHovered;
    style.Colors[ImGuiCol_NavHighlight] = colors_.buttonHovered;
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.9f, 0.2f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.8f, 0.8f, 0.9f, 0.35f);
}

void Theme::applyStyleConfig() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    style.WindowRounding = styleConfig_.windowRounding;
    style.ChildRounding = styleConfig_.childRounding;
    style.FrameRounding = styleConfig_.frameRounding;
    style.PopupRounding = styleConfig_.popupRounding;
    style.ScrollbarRounding = styleConfig_.scrollbarRounding;
    style.GrabRounding = styleConfig_.grabRounding;
    style.TabRounding = styleConfig_.tabRounding;
    
    style.WindowBorderSize = styleConfig_.windowBorderSize;
    style.ChildBorderSize = styleConfig_.childBorderSize;
    style.PopupBorderSize = styleConfig_.popupBorderSize;
    style.FrameBorderSize = styleConfig_.frameBorderSize;
    
    style.WindowPadding = styleConfig_.windowPadding;
    style.FramePadding = styleConfig_.framePadding;
    style.CellPadding = styleConfig_.cellPadding;
    style.ItemSpacing = styleConfig_.itemSpacing;
    style.ItemInnerSpacing = styleConfig_.itemInnerSpacing;
    style.TouchExtraPadding = styleConfig_.touchExtraPadding;
    
    style.IndentSpacing = styleConfig_.indentSpacing;
    style.ScrollbarSize = styleConfig_.scrollbarSize;
    style.GrabMinSize = styleConfig_.grabMinSize;
    
    // Enable anti-aliasing for smooth curves
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;
    
    // Professional window flags
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ColorButtonPosition = ImGuiDir_Right;
}

void Theme::loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Clear existing fonts (except the default one)
    if (io.Fonts->Fonts.Size > 1) {
        io.Fonts->Clear();
    }
    
    // Load custom fonts if available
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 3;
    fontConfig.OversampleV = 3;
    fontConfig.PixelSnapH = true;
    
    // Try to load custom fonts, fall back to default if not available
    if (!fonts_.regularFontPath.empty()) {
        ImFont* regularFont = io.Fonts->AddFontFromFileTTF(
            fonts_.regularFontPath.c_str(), 
            fonts_.fontSize, 
            &fontConfig
        );
        
        if (regularFont) {
            io.FontDefault = regularFont;
        }
    }
    
    // Build font atlas
    io.Fonts->Build();
}

void Theme::initializeLogicProDark() {
    colors_ = createLogicProDarkPalette();
    
    // Professional spacing and rounding
    styleConfig_.windowRounding = 8.0f;
    styleConfig_.childRounding = 4.0f;
    styleConfig_.frameRounding = 4.0f;
    styleConfig_.scrollbarRounding = 12.0f;
    styleConfig_.grabRounding = 4.0f;
    styleConfig_.tabRounding = 4.0f;
    
    styleConfig_.windowPadding = ImVec2(12, 8);
    styleConfig_.framePadding = ImVec2(8, 4);
    styleConfig_.itemSpacing = ImVec2(8, 4);
    styleConfig_.itemInnerSpacing = ImVec2(4, 4);
    
    styleConfig_.windowBorderSize = 1.0f;
    styleConfig_.childBorderSize = 0.0f;
    styleConfig_.frameBorderSize = 0.0f;
    
    // DAW specific measurements
    styleConfig_.trackHeight = 80.0f;
    styleConfig_.mixerChannelWidth = 60.0f;
    styleConfig_.pianoKeyWidth = 100.0f;
    styleConfig_.timelineHeight = 40.0f;
    styleConfig_.transportHeight = 60.0f;
}

void Theme::initializeAbletonDark() {
    colors_ = createAbletonDarkPalette();
    
    // Ableton's more minimal styling
    styleConfig_.windowRounding = 0.0f;
    styleConfig_.childRounding = 0.0f;
    styleConfig_.frameRounding = 2.0f;
    styleConfig_.scrollbarRounding = 0.0f;
    styleConfig_.grabRounding = 2.0f;
    styleConfig_.tabRounding = 0.0f;
    
    styleConfig_.windowPadding = ImVec2(8, 6);
    styleConfig_.framePadding = ImVec2(6, 3);
    styleConfig_.itemSpacing = ImVec2(6, 3);
}

void Theme::initializeProToolsDark() {
    colors_ = createProToolsDarkPalette();
    
    // Pro Tools professional look
    styleConfig_.windowRounding = 4.0f;
    styleConfig_.frameRounding = 2.0f;
    styleConfig_.scrollbarRounding = 8.0f;
    
    styleConfig_.windowPadding = ImVec2(10, 6);
    styleConfig_.framePadding = ImVec2(7, 3);
}

void Theme::initializeCubaseDark() {
    colors_ = createCubaseDarkPalette();
    
    // Cubase modern styling
    styleConfig_.windowRounding = 6.0f;
    styleConfig_.frameRounding = 3.0f;
    styleConfig_.scrollbarRounding = 10.0f;
}

void Theme::initializeStudioOneDark() {
    colors_ = createStudioOneDarkPalette();
    
    // Studio One sleek design
    styleConfig_.windowRounding = 8.0f;
    styleConfig_.frameRounding = 4.0f;
    styleConfig_.scrollbarRounding = 12.0f;
}

Theme::ColorPalette Theme::createLogicProDarkPalette() {
    ColorPalette palette;
    
    // Logic Pro X dark theme colors
    palette.windowBg = hexToImVec4(0x2D2D2D);
    palette.childBg = hexToImVec4(0x262626);
    palette.popupBg = hexToImVec4(0x2D2D2D);
    palette.border = hexToImVec4(0x1A1A1A);
    palette.borderShadow = hexToImVec4(0x000000, 0.0f);
    
    palette.frameBg = hexToImVec4(0x363636);
    palette.frameBgHovered = hexToImVec4(0x404040);
    palette.frameBgActive = hexToImVec4(0x4A4A4A);
    
    palette.titleBg = hexToImVec4(0x1E1E1E);
    palette.titleBgActive = hexToImVec4(0x2D2D2D);
    palette.titleBgCollapsed = hexToImVec4(0x1E1E1E);
    
    palette.menuBarBg = hexToImVec4(0x262626);
    
    palette.scrollbarBg = hexToImVec4(0x2D2D2D);
    palette.scrollbarGrab = hexToImVec4(0x4A4A4A);
    palette.scrollbarGrabHovered = hexToImVec4(0x5A5A5A);
    palette.scrollbarGrabActive = hexToImVec4(0x6A6A6A);
    
    palette.button = hexToImVec4(0x404040);
    palette.buttonHovered = hexToImVec4(0x505050);
    palette.buttonActive = hexToImVec4(0x606060);
    
    palette.header = hexToImVec4(0x404040);
    palette.headerHovered = hexToImVec4(0x505050);
    palette.headerActive = hexToImVec4(0x606060);
    
    palette.separator = hexToImVec4(0x1A1A1A);
    palette.separatorHovered = hexToImVec4(0x606060);
    palette.separatorActive = hexToImVec4(0x808080);
    
    palette.resizeGrip = hexToImVec4(0x4A4A4A);
    palette.resizeGripHovered = hexToImVec4(0x6A6A6A);
    palette.resizeGripActive = hexToImVec4(0x8A8A8A);
    
    palette.tab = hexToImVec4(0x363636);
    palette.tabHovered = hexToImVec4(0x505050);
    palette.tabActive = hexToImVec4(0x4A4A4A);
    palette.tabUnfocused = hexToImVec4(0x2D2D2D);
    palette.tabUnfocusedActive = hexToImVec4(0x363636);
    
    palette.plotLines = hexToImVec4(0x9C9C9C);
    palette.plotLinesHovered = hexToImVec4(0xFFFFFF);
    palette.plotHistogram = hexToImVec4(0x9C9C9C);
    palette.plotHistogramHovered = hexToImVec4(0xFFFFFF);
    
    palette.tableHeaderBg = hexToImVec4(0x404040);
    palette.tableBorderStrong = hexToImVec4(0x606060);
    palette.tableBorderLight = hexToImVec4(0x404040);
    palette.tableRowBg = hexToImVec4(0x000000, 0.0f);
    palette.tableRowBgAlt = hexToImVec4(0x404040, 0.1f);
    
    palette.text = hexToImVec4(0xE6E6E6);
    palette.textDisabled = hexToImVec4(0x808080);
    palette.textSelectedBg = hexToImVec4(0x4A4A4A);
    
    // DAW specific colors
    palette.trackArea = hexToImVec4(0x2A2A2A);
    palette.mixerArea = hexToImVec4(0x262626);
    palette.pianoRollKeys = hexToImVec4(0xEEEEEE);
    palette.pianoRollKeysBlack = hexToImVec4(0x1A1A1A);
    palette.pianoRollGrid = hexToImVec4(0x404040);
    palette.midiNotes = hexToImVec4(0x6B9BD2);
    palette.midiNotesSelected = hexToImVec4(0x8AB6E8);
    palette.waveform = hexToImVec4(0x7DB46C);
    palette.waveformPeak = hexToImVec4(0x9FD487);
    
    // Status colors
    palette.success = hexToImVec4(0x5CB85C);
    palette.warning = hexToImVec4(0xF0AD4E);
    palette.error = hexToImVec4(0xD9534F);
    palette.info = hexToImVec4(0x5BC0DE);
    
    // Transport colors
    palette.playButton = hexToImVec4(0x5CB85C);
    palette.recordButton = hexToImVec4(0xD9534F);
    palette.stopButton = hexToImVec4(0x6C757D);
    
    // Level meters
    palette.meterGreen = hexToImVec4(0x28A745);
    palette.meterYellow = hexToImVec4(0xFFC107);
    palette.meterRed = hexToImVec4(0xDC3545);
    palette.meterBackground = hexToImVec4(0x1A1A1A);
    
    return palette;
}

Theme::ColorPalette Theme::createAbletonDarkPalette() {
    ColorPalette palette = createLogicProDarkPalette();
    
    // Ableton Live specific adjustments
    palette.windowBg = hexToImVec4(0x1E1E1E);
    palette.frameBg = hexToImVec4(0x2A2A2A);
    palette.button = hexToImVec4(0x3A3A3A);
    palette.midiNotes = hexToImVec4(0xFF6B35); // Ableton orange
    
    return palette;
}

Theme::ColorPalette Theme::createProToolsDarkPalette() {
    ColorPalette palette = createLogicProDarkPalette();
    
    // Pro Tools specific adjustments
    palette.windowBg = hexToImVec4(0x2C2C2C);
    palette.frameBg = hexToImVec4(0x383838);
    palette.midiNotes = hexToImVec4(0x4A90E2); // Pro Tools blue
    
    return palette;
}

Theme::ColorPalette Theme::createCubaseDarkPalette() {
    ColorPalette palette = createLogicProDarkPalette();
    
    // Cubase specific adjustments
    palette.windowBg = hexToImVec4(0x2E2E2E);
    palette.frameBg = hexToImVec4(0x3C3C3C);
    palette.midiNotes = hexToImVec4(0xE85D00); // Cubase orange
    
    return palette;
}

Theme::ColorPalette Theme::createStudioOneDarkPalette() {
    ColorPalette palette = createLogicProDarkPalette();
    
    // Studio One specific adjustments
    palette.windowBg = hexToImVec4(0x282828);
    palette.frameBg = hexToImVec4(0x353535);
    palette.midiNotes = hexToImVec4(0x4FB3D9); // Studio One blue
    
    return palette;
}

// Utility functions
ImVec4 Theme::adjustBrightness(const ImVec4& color, float factor) {
    return ImVec4(
        std::clamp(color.x * factor, 0.0f, 1.0f),
        std::clamp(color.y * factor, 0.0f, 1.0f),
        std::clamp(color.z * factor, 0.0f, 1.0f),
        color.w
    );
}

ImVec4 Theme::adjustSaturation(const ImVec4& color, float factor) {
    float h, s, v;
    rgbToHsv(color, h, s, v);
    s = std::clamp(s * factor, 0.0f, 1.0f);
    return hsvToRgb(h, s, v, color.w);
}

ImVec4 Theme::adjustAlpha(const ImVec4& color, float alpha) {
    return ImVec4(color.x, color.y, color.z, alpha);
}

ImVec4 Theme::blendColors(const ImVec4& a, const ImVec4& b, float factor) {
    factor = std::clamp(factor, 0.0f, 1.0f);
    return ImVec4(
        a.x + (b.x - a.x) * factor,
        a.y + (b.y - a.y) * factor,
        a.z + (b.z - a.z) * factor,
        a.w + (b.w - a.w) * factor
    );
}

ImVec4 Theme::hexToImVec4(uint32_t hex, float alpha) {
    return ImVec4(
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8) & 0xFF) / 255.0f,
        (hex & 0xFF) / 255.0f,
        alpha
    );
}

uint32_t Theme::imVec4ToHex(const ImVec4& color) {
    uint32_t r = static_cast<uint32_t>(color.x * 255.0f) & 0xFF;
    uint32_t g = static_cast<uint32_t>(color.y * 255.0f) & 0xFF;
    uint32_t b = static_cast<uint32_t>(color.z * 255.0f) & 0xFF;
    return (r << 16) | (g << 8) | b;
}

ImVec4 Theme::hsvToRgb(float h, float s, float v, float a) {
    ImVec4 result;
    ImGui::ColorConvertHSVtoRGB(h, s, v, result.x, result.y, result.z);
    result.w = a;
    return result;
}

void Theme::rgbToHsv(const ImVec4& rgb, float& h, float& s, float& v) {
    ImGui::ColorConvertRGBtoHSV(rgb.x, rgb.y, rgb.z, h, s, v);
}

void Theme::updateAnimations(float deltaTime) {
    for (auto& [name, anim] : animations_) {
        if (anim.active) {
            anim.elapsed += deltaTime;
            if (anim.elapsed >= anim.duration) {
                anim.active = false;
                anim.elapsed = anim.duration;
            }
        }
    }
}

bool Theme::validateTheme() const {
    // Basic validation - check that key colors are not completely transparent
    if (colors_.text.w == 0.0f || colors_.windowBg.w == 0.0f) {
        return false;
    }
    
    // Check for reasonable contrast
    float textLuminance = 0.299f * colors_.text.x + 0.587f * colors_.text.y + 0.114f * colors_.text.z;
    float bgLuminance = 0.299f * colors_.windowBg.x + 0.587f * colors_.windowBg.y + 0.114f * colors_.windowBg.z;
    
    float contrast = std::abs(textLuminance - bgLuminance);
    return contrast > 0.3f; // Minimum contrast ratio
}

void Theme::printThemeInfo() const {
    std::cout << "Current Theme: ";
    switch (currentStyle_) {
        case Style::LogicProDark: std::cout << "Logic Pro Dark\n"; break;
        case Style::AbletonDark: std::cout << "Ableton Dark\n"; break;
        case Style::ProToolsDark: std::cout << "Pro Tools Dark\n"; break;
        case Style::CubaseDark: std::cout << "Cubase Dark\n"; break;
        case Style::StudioOneDark: std::cout << "Studio One Dark\n"; break;
        case Style::Custom: std::cout << "Custom\n"; break;
    }
    
    std::cout << "Theme is " << (validateTheme() ? "valid" : "invalid") << "\n";
    std::cout << "Window rounding: " << styleConfig_.windowRounding << "\n";
    std::cout << "Frame rounding: " << styleConfig_.frameRounding << "\n";
}

} // namespace mixmind::ui