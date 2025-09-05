#pragma once

#include <memory>

namespace mixmind::ui {

// ============================================================================
// UI Theme System
// ============================================================================

enum class UITheme {
    PROFESSIONAL_DARK,      // Modern dark theme (default)
    PROFESSIONAL_LIGHT,     // Clean light theme
    STUDIO_CLASSIC          // Hardware-inspired vintage theme
};

// ============================================================================
// MainWindow - Professional DAW Interface
// ============================================================================

class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    
    // Non-copyable, non-movable
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(MainWindow&&) = delete;
    
    // Window lifecycle
    bool initialize();
    void run();        // Main application loop
    void cleanup();    // Cleanup resources
    
    // Window management
    void setTitle(const std::string& title);
    void setSize(int width, int height);
    void setFullscreen(bool fullscreen);
    
    // Theme management
    void setTheme(UITheme theme);
    UITheme getCurrentTheme() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// UI Panel Interfaces (for extensibility)
// ============================================================================

class UIPanel {
public:
    virtual ~UIPanel() = default;
    virtual void render() = 0;
    virtual const char* getName() const = 0;
    virtual bool isVisible() const = 0;
    virtual void setVisible(bool visible) = 0;
};

// Transport controls panel
class TransportPanel : public UIPanel {
public:
    void render() override;
    const char* getName() const override { return "Transport"; }
    bool isVisible() const override;
    void setVisible(bool visible) override;
    
private:
    bool visible_ = true;
};

// Mixer console panel
class MixerPanel : public UIPanel {
public:
    void render() override;
    const char* getName() const override { return "Mixer"; }
    bool isVisible() const override;
    void setVisible(bool visible) override;
    
private:
    bool visible_ = true;
};

// AI assistant panel
class AIPanel : public UIPanel {
public:
    void render() override;
    const char* getName() const override { return "AI Assistant"; }
    bool isVisible() const override;
    void setVisible(bool visible) override;
    
private:
    bool visible_ = true;
};

// Voice control panel
class VoiceControlPanel : public UIPanel {
public:
    void render() override;
    const char* getName() const override { return "Voice Control"; }
    bool isVisible() const override;
    void setVisible(bool visible) override;
    
private:
    bool visible_ = true;
};

// Style transfer panel
class StyleTransferPanel : public UIPanel {
public:
    void render() override;
    const char* getName() const override { return "Style Transfer"; }
    bool isVisible() const override;
    void setVisible(bool visible) override;
    
private:
    bool visible_ = true;
};

// AI composer panel
class ComposerPanel : public UIPanel {
public:
    void render() override;
    const char* getName() const override { return "AI Composer"; }
    bool isVisible() const override;
    void setVisible(bool visible) override;
    
private:
    bool visible_ = true;
};

// Audio analyzer panel
class AnalyzerPanel : public UIPanel {
public:
    void render() override;
    const char* getName() const override { return "Audio Analyzer"; }
    bool isVisible() const override;
    void setVisible(bool visible) override;
    
private:
    bool visible_ = true;
};

// ============================================================================
// UI Utilities
// ============================================================================

namespace utils {
    // Convert colors for ImGui
    struct Color {
        float r, g, b, a;
        
        Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
        
        // Convert to ImVec4
        operator ImVec4() const { return ImVec4(r, g, b, a); }
        
        // Predefined colors
        static const Color BLACK;
        static const Color WHITE;
        static const Color RED;
        static const Color GREEN;
        static const Color BLUE;
        static const Color YELLOW;
        static const Color CYAN;
        static const Color MAGENTA;
        static const Color ORANGE;
        static const Color PURPLE;
        static const Color GRAY;
        static const Color DARK_GRAY;
        static const Color LIGHT_GRAY;
    };
    
    // UI measurement utilities
    float dpToPx(float dp);                    // Device-independent pixels to pixels
    float spToPx(float sp);                    // Scale-independent pixels to pixels
    ImVec2 scaleVec2(const ImVec2& vec);      // Scale vector by UI scale factor
    
    // Widget helpers
    bool ColoredButton(const char* label, const Color& color, const ImVec2& size = ImVec2(0, 0));
    bool VUMeter(const char* label, float level, const ImVec2& size = ImVec2(20, 100));
    bool Knob(const char* label, float* value, float min, float max, const ImVec2& size = ImVec2(50, 50));
    bool Fader(const char* label, float* value, float min, float max, const ImVec2& size = ImVec2(30, 150));
    
    // Layout helpers
    void BeginColumns(int count, const char* id = nullptr, bool border = true);
    void NextColumn();
    void EndColumns();
    
    void BeginHorizontal();
    void EndHorizontal();
    
    void BeginVertical();
    void EndVertical();
    
    // Text utilities
    void TextCentered(const char* text);
    void TextColored(const Color& color, const char* text);
    void TextWithIcon(const char* icon, const char* text);
    
    // Tooltip system
    void SetTooltip(const char* text);
    void SetTooltipDelayed(const char* text, float delay = 0.5f);
}

} // namespace mixmind::ui