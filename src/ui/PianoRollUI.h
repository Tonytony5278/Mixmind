#pragma once

#include "UIComponents.h"
#include "Theme.h"
#include "../ui/PianoRollEditor.h"
#include <imgui.h>
#include <vector>
#include <memory>
#include <functional>

namespace mixmind::ui {

// Professional Piano Roll UI with Logic Pro styling
class PianoRollUI {
public:
    struct ViewState {
        // Zoom and scroll
        float horizontalZoom = 1.0f;    // Pixels per beat
        float verticalZoom = 1.0f;      // Pixels per semitone
        double scrollX = 0.0;           // Horizontal scroll position (beats)
        int scrollY = 60;               // Vertical scroll position (MIDI note number)
        
        // Visible range
        double visibleStartBeat = 0.0;
        double visibleEndBeat = 16.0;
        int visibleLowNote = 36;        // C2
        int visibleHighNote = 96;       // C7
        
        // Grid settings
        bool showGrid = true;
        bool snapToGrid = true;
        GridSnap gridResolution = GridSnap::SIXTEENTH;
        
        // Visual options
        bool showNoteNames = true;
        bool showVelocityColors = true;
        bool showNotePreview = true;
        bool showPianoKeys = true;
        bool showTimeline = true;
        bool showVelocityLane = false;
        
        // Color scheme
        enum class ColorScheme {
            Default,
            Velocity,
            Channel,
            Pitch,
            Custom
        } colorScheme = ColorScheme::Velocity;
    };
    
    struct NoteStyle {
        ImVec4 defaultColor = ImVec4(0.4f, 0.7f, 0.9f, 1.0f);
        ImVec4 selectedColor = ImVec4(0.9f, 0.7f, 0.4f, 1.0f);
        ImVec4 playingColor = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
        ImVec4 mutedColor = ImVec4(0.5f, 0.5f, 0.5f, 0.6f);
        
        // Velocity-based colors (127 levels)
        std::vector<ImVec4> velocityColors;
        
        float cornerRounding = 4.0f;
        float borderThickness = 1.0f;
        bool showVelocityBars = true;
        bool showNoteText = false;
    };
    
    struct GridStyle {
        ImVec4 majorGridColor = ImVec4(0.4f, 0.4f, 0.4f, 0.8f);
        ImVec4 minorGridColor = ImVec4(0.3f, 0.3f, 0.3f, 0.4f);
        ImVec4 beatGridColor = ImVec4(0.5f, 0.5f, 0.5f, 0.6f);
        ImVec4 barGridColor = ImVec4(0.6f, 0.6f, 0.6f, 0.8f);
        
        float majorGridThickness = 1.0f;
        float minorGridThickness = 0.5f;
        float beatGridThickness = 1.0f;
        float barGridThickness = 2.0f;
    };
    
    PianoRollUI();
    ~PianoRollUI() = default;
    
    // Main rendering
    void render(const ImVec2& size = ImVec2(1200, 600));
    
    // Editor integration
    void setEditor(std::shared_ptr<PianoRollEditor> editor) { editor_ = editor; }
    std::shared_ptr<PianoRollEditor> getEditor() const { return editor_; }
    
    // View control
    ViewState& getViewState() { return viewState_; }
    const ViewState& getViewState() const { return viewState_; }
    
    void setZoom(float horizontal, float vertical);
    void zoomIn(float factor = 1.2f);
    void zoomOut(float factor = 0.8f);
    void zoomToFit();
    void zoomToSelection();
    
    void scrollTo(double beat, int midiNote);
    void scrollToBeat(double beat);
    void scrollToNote(int midiNote);
    
    // Style customization
    NoteStyle& getNoteStyle() { return noteStyle_; }
    const NoteStyle& getNoteStyle() const { return noteStyle_; }
    
    GridStyle& getGridStyle() { return gridStyle_; }
    const GridStyle& getGridStyle() const { return gridStyle_; }
    
    // Interaction callbacks
    std::function<void(double beat, int note, int velocity)> onNoteAdded;
    std::function<void(MIDINote*)> onNoteSelected;
    std::function<void(MIDINote*)> onNoteDeleted;
    std::function<void(MIDINote*, double newStart, double newLength)> onNoteMoved;
    std::function<void(MIDINote*, int newVelocity)> onVelocityChanged;
    
    // Tool state
    enum class Tool {
        Select,
        Draw,
        Erase,
        Trim,
        Split,
        Velocity
    } currentTool_ = Tool::Draw;
    
    void setTool(Tool tool) { currentTool_ = tool; }
    Tool getTool() const { return currentTool_; }
    
    // Playback visualization
    void setPlayheadPosition(double beat);
    void setLoopRegion(double startBeat, double endBeat);
    void clearLoopRegion();
    
    // Selection
    void selectAll();
    void selectNone();
    void selectInRegion(double startBeat, double endBeat, int lowNote, int highNote);
    void invertSelection();
    
    // Clipboard operations
    void copySelection();
    void cutSelection();
    void paste(double atBeat = -1.0); // -1 = at playhead
    
private:
    std::shared_ptr<PianoRollEditor> editor_;
    ViewState viewState_;
    NoteStyle noteStyle_;
    GridStyle gridStyle_;
    
    // UI state
    ImVec2 canvasPos_;
    ImVec2 canvasSize_;
    ImDrawList* drawList_ = nullptr;
    
    // Interaction state
    bool isDragging_ = false;
    bool isSelecting_ = false;
    bool isDrawingNote_ = false;
    ImVec2 dragStartPos_;
    ImVec2 selectionStart_;
    ImVec2 selectionEnd_;
    
    MIDINote* hoveredNote_ = nullptr;
    MIDINote* draggedNote_ = nullptr;
    std::vector<MIDINote*> selectedNotes_;
    
    // Playback state
    double playheadBeat_ = 0.0;
    bool hasLoopRegion_ = false;
    double loopStartBeat_ = 0.0;
    double loopEndBeat_ = 4.0;
    
    // Preview state
    bool showNotePreview_ = false;
    double previewBeat_ = 0.0;
    int previewNote_ = 60;
    
    // Performance optimization
    struct VisibleNote {
        MIDINote* note;
        ImRect bounds;
        bool isVisible;
    };
    std::vector<VisibleNote> visibleNotes_;
    
    // Rendering methods
    void beginCanvas(const ImVec2& size);
    void endCanvas();
    
    void renderBackground();
    void renderGrid();
    void renderPianoKeys();
    void renderTimeline();
    void renderNotes();
    void renderPlayhead();
    void renderLoopRegion();
    void renderSelection();
    void renderNotePreview();
    void renderVelocityLane();
    
    // Interaction methods
    void handleMouseInput();
    void handleKeyboardInput();
    void handleToolInput();
    
    void handleSelectTool();
    void handleDrawTool();
    void handleEraseTool();
    void handleTrimTool();
    void handleSplitTool();
    void handleVelocityTool();
    
    // Coordinate conversion
    ImVec2 beatToPixel(double beat, int midiNote) const;
    std::pair<double, int> pixelToBeat(const ImVec2& pixel) const;
    
    double pixelToBeat(float x) const;
    int pixelToNote(float y) const;
    float beatToPixel(double beat) const;
    float noteToPixel(int note) const;
    
    // Grid snapping
    double snapBeatToGrid(double beat) const;
    double getGridResolution() const;
    
    // Note management
    void updateVisibleNotes();
    MIDINote* findNoteAtPosition(const ImVec2& pos, float tolerance = 5.0f);
    std::vector<MIDINote*> findNotesInRegion(const ImRect& region);
    
    // Visual helpers
    ImVec4 getNoteColor(MIDINote* note) const;
    ImVec4 getVelocityColor(int velocity) const;
    bool isNoteSelected(MIDINote* note) const;
    void drawNote(MIDINote* note, const ImRect& bounds, const ImVec4& color);
    void drawNoteVelocityBar(MIDINote* note, const ImRect& bounds);
    
    // Grid calculations
    std::vector<double> calculateGridLines() const;
    std::vector<int> calculateNoteLines() const;
    
    // Piano key helpers
    bool isBlackKey(int midiNote) const;
    std::string getNoteNameFromMIDI(int midiNote) const;
    ImVec4 getPianoKeyColor(int midiNote, bool isPressed = false) const;
    
    // Theme integration
    void applyTheme();
    void updateColorsFromTheme();
    
    // Optimization
    bool isNoteVisible(MIDINote* note) const;
    void cullNonVisibleNotes();
    
    // Professional styling constants
    static constexpr float MIN_NOTE_WIDTH = 8.0f;
    static constexpr float MIN_NOTE_HEIGHT = 12.0f;
    static constexpr float PIANO_KEY_WIDTH = 120.0f;
    static constexpr float TIMELINE_HEIGHT = 40.0f;
    static constexpr float VELOCITY_LANE_HEIGHT = 100.0f;
    static constexpr float SCROLLBAR_SIZE = 16.0f;
    
    static constexpr double MIN_ZOOM = 0.1;
    static constexpr double MAX_ZOOM = 10.0;
    static constexpr double DEFAULT_PIXELS_PER_BEAT = 60.0;
    static constexpr float DEFAULT_PIXELS_PER_SEMITONE = 16.0f;
};

// Piano Roll toolbar with professional DAW styling
class PianoRollToolbar {
public:
    PianoRollToolbar(PianoRollUI* pianoRoll);
    
    void render();
    
    // Tool selection
    void setTool(PianoRollUI::Tool tool);
    PianoRollUI::Tool getCurrentTool() const;
    
    // Grid controls
    void setGridSnap(GridSnap snap);
    GridSnap getGridSnap() const;
    
    // Quantization
    void showQuantizeDialog();
    void quantizeSelection(float strength = 1.0f);
    
private:
    PianoRollUI* pianoRoll_;
    
    // UI state
    bool showQuantizeDialog_ = false;
    float quantizeStrength_ = 1.0f;
    MIDIClip::QuantizeResolution quantizeResolution_ = MIDIClip::QuantizeResolution::SIXTEENTH;
    
    // Rendering helpers
    void renderToolButtons();
    void renderGridControls();
    void renderZoomControls();
    void renderPlaybackControls();
    void renderQuantizeControls();
    
    bool toolButton(const char* icon, const char* tooltip, PianoRollUI::Tool tool, 
                   const ImVec2& size = ImVec2(40, 40));
    bool gridSnapButton(const char* label, GridSnap snap);
};

} // namespace mixmind::ui