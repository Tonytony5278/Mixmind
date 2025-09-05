#include "PianoRollUI.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace mixmind::ui {

PianoRollUI::PianoRollUI() {
    // Initialize velocity colors (0-127 range)
    noteStyle_.velocityColors.resize(128);
    for (int i = 0; i < 128; ++i) {
        float intensity = i / 127.0f;
        // Create gradient from blue (low velocity) to red (high velocity)
        noteStyle_.velocityColors[i] = ImVec4(
            0.2f + intensity * 0.7f,  // Red increases
            0.2f + intensity * 0.5f,  // Green moderate
            0.9f - intensity * 0.7f,  // Blue decreases
            1.0f
        );
    }
    
    applyTheme();
}

void PianoRollUI::render(const ImVec2& size) {
    if (!editor_) return;
    
    // Main piano roll window with professional styling
    ScopedStyleColor pianoRollColors(
        ImGuiCol_WindowBg, THEME_COLOR(childBg),
        ImGuiCol_ChildBg, THEME_COLOR(trackArea)
    );
    
    ScopedStyleVar pianoRollStyle(
        ImGuiStyleVar_WindowPadding, ImVec2(0, 0)
    );
    
    ImGui::BeginChild("PianoRoll", size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Calculate layout regions
    const ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    const float pianoKeyWidth = viewState_.showPianoKeys ? PIANO_KEY_WIDTH : 0;
    const float timelineHeight = viewState_.showTimeline ? TIMELINE_HEIGHT : 0;
    const float velocityLaneHeight = viewState_.showVelocityLane ? VELOCITY_LANE_HEIGHT : 0;
    
    // Main canvas area
    const ImVec2 canvasSize(
        contentRegion.x - pianoKeyWidth - SCROLLBAR_SIZE,
        contentRegion.y - timelineHeight - velocityLaneHeight - SCROLLBAR_SIZE
    );
    
    // Begin main canvas rendering
    beginCanvas(canvasSize);
    
    // Render layers in order (back to front)
    renderBackground();
    if (viewState_.showGrid) renderGrid();
    renderNotes();
    if (hasLoopRegion_) renderLoopRegion();
    renderPlayhead();
    if (isSelecting_) renderSelection();
    if (showNotePreview_) renderNotePreview();
    
    // Handle user interaction
    handleMouseInput();
    handleKeyboardInput();
    
    endCanvas();
    
    // Render UI elements around canvas
    if (viewState_.showPianoKeys) {
        ImGui::SameLine(0, 0);
        renderPianoKeys();
    }
    
    if (viewState_.showTimeline) {
        renderTimeline();
    }
    
    if (viewState_.showVelocityLane) {
        renderVelocityLane();
    }
    
    ImGui::EndChild();
    
    // Update performance optimization
    updateVisibleNotes();
}

void PianoRollUI::beginCanvas(const ImVec2& size) {
    canvasPos_ = ImGui::GetCursorScreenPos();
    canvasSize_ = size;
    drawList_ = ImGui::GetWindowDrawList();
    
    // Create invisible button for interaction
    ImGui::InvisibleButton("PianoRollCanvas", size);
    
    // Clip drawing to canvas bounds
    drawList_->PushClipRect(canvasPos_, canvasPos_ + canvasSize_);
}

void PianoRollUI::endCanvas() {
    if (drawList_) {
        drawList_->PopClipRect();
        drawList_ = nullptr;
    }
}

void PianoRollUI::renderBackground() {
    if (!drawList_) return;
    
    // Fill background with track area color
    drawList_->AddRectFilled(
        canvasPos_,
        canvasPos_ + canvasSize_,
        UIComponents::colorToImU32(THEME_COLOR(trackArea))
    );
}

void PianoRollUI::renderGrid() {
    if (!drawList_ || !viewState_.showGrid) return;
    
    // Calculate grid lines
    auto verticalLines = calculateGridLines();
    auto horizontalLines = calculateNoteLines();
    
    // Draw vertical grid lines (time)
    for (double beat : verticalLines) {
        float x = beatToPixel(beat);
        
        if (x < canvasPos_.x || x > canvasPos_.x + canvasSize_.x) continue;
        
        // Determine line type and color
        ImVec4 lineColor;
        float thickness;
        
        if (fmod(beat, 4.0) == 0.0) {
            // Bar lines
            lineColor = gridStyle_.barGridColor;
            thickness = gridStyle_.barGridThickness;
        } else if (fmod(beat, 1.0) == 0.0) {
            // Beat lines
            lineColor = gridStyle_.beatGridColor;
            thickness = gridStyle_.beatGridThickness;
        } else {
            // Subdivision lines
            lineColor = gridStyle_.minorGridColor;
            thickness = gridStyle_.minorGridThickness;
        }
        
        drawList_->AddLine(
            ImVec2(x, canvasPos_.y),
            ImVec2(x, canvasPos_.y + canvasSize_.y),
            UIComponents::colorToImU32(lineColor),
            thickness
        );
    }
    
    // Draw horizontal grid lines (notes)
    for (int note : horizontalLines) {
        float y = noteToPixel(note);
        
        if (y < canvasPos_.y || y > canvasPos_.y + canvasSize_.y) continue;
        
        // Different style for C notes (octave markers)
        ImVec4 lineColor = (note % 12 == 0) ? gridStyle_.majorGridColor : gridStyle_.minorGridColor;
        float thickness = (note % 12 == 0) ? gridStyle_.majorGridThickness : gridStyle_.minorGridThickness;
        
        drawList_->AddLine(
            ImVec2(canvasPos_.x, y),
            ImVec2(canvasPos_.x + canvasSize_.x, y),
            UIComponents::colorToImU32(lineColor),
            thickness
        );
    }
}

void PianoRollUI::renderNotes() {
    if (!drawList_ || !editor_ || !editor_->get_clip()) return;
    
    auto clip = editor_->get_clip();
    const auto& notes = clip->get_notes();
    
    for (auto& note : notes) {
        if (!isNoteVisible(&note)) continue;
        
        // Calculate note bounds
        const float startX = beatToPixel(note.start_time_beats);
        const float endX = beatToPixel(note.start_time_beats + note.length_beats);
        const float noteY = noteToPixel(note.note_number);
        
        const ImRect noteBounds(
            ImVec2(startX, noteY),
            ImVec2(std::max(endX, startX + MIN_NOTE_WIDTH), noteY + DEFAULT_PIXELS_PER_SEMITONE)
        );
        
        // Skip if completely outside canvas
        if (noteBounds.Max.x < canvasPos_.x || noteBounds.Min.x > canvasPos_.x + canvasSize_.x ||
            noteBounds.Max.y < canvasPos_.y || noteBounds.Min.y > canvasPos_.y + canvasSize_.y) {
            continue;
        }
        
        // Get note color based on current color scheme
        ImVec4 noteColor = getNoteColor(const_cast<MIDINote*>(&note));
        
        // Draw note
        drawNote(const_cast<MIDINote*>(&note), noteBounds, noteColor);
        
        // Draw velocity bar if enabled
        if (noteStyle_.showVelocityBars) {
            drawNoteVelocityBar(const_cast<MIDINote*>(&note), noteBounds);
        }
    }
}

void PianoRollUI::renderPlayhead() {
    if (!drawList_) return;
    
    const float playheadX = beatToPixel(playheadBeat_);
    
    // Only draw if visible
    if (playheadX >= canvasPos_.x && playheadX <= canvasPos_.x + canvasSize_.x) {
        drawList_->AddLine(
            ImVec2(playheadX, canvasPos_.y),
            ImVec2(playheadX, canvasPos_.y + canvasSize_.y),
            UIComponents::colorToImU32(THEME_COLOR(error)), // Red playhead
            3.0f
        );
        
        // Playhead triangle at top
        const float triangleSize = 8.0f;
        drawList_->AddTriangleFilled(
            ImVec2(playheadX, canvasPos_.y),
            ImVec2(playheadX - triangleSize * 0.5f, canvasPos_.y + triangleSize),
            ImVec2(playheadX + triangleSize * 0.5f, canvasPos_.y + triangleSize),
            UIComponents::colorToImU32(THEME_COLOR(error))
        );
    }
}

void PianoRollUI::renderLoopRegion() {
    if (!drawList_ || !hasLoopRegion_) return;
    
    const float loopStartX = beatToPixel(loopStartBeat_);
    const float loopEndX = beatToPixel(loopEndBeat_);
    
    // Loop region background
    const ImRect loopRect(
        ImVec2(std::max(loopStartX, canvasPos_.x), canvasPos_.y),
        ImVec2(std::min(loopEndX, canvasPos_.x + canvasSize_.x), canvasPos_.y + canvasSize_.y)
    );
    
    if (loopRect.GetWidth() > 0) {
        drawList_->AddRectFilled(
            loopRect.Min,
            loopRect.Max,
            UIComponents::colorToImU32(ImVec4(0.3f, 0.7f, 0.3f, 0.1f))
        );
        
        // Loop boundaries
        if (loopStartX >= canvasPos_.x && loopStartX <= canvasPos_.x + canvasSize_.x) {
            drawList_->AddLine(
                ImVec2(loopStartX, canvasPos_.y),
                ImVec2(loopStartX, canvasPos_.y + canvasSize_.y),
                UIComponents::colorToImU32(ImVec4(0.3f, 0.7f, 0.3f, 0.8f)),
                2.0f
            );
        }
        
        if (loopEndX >= canvasPos_.x && loopEndX <= canvasPos_.x + canvasSize_.x) {
            drawList_->AddLine(
                ImVec2(loopEndX, canvasPos_.y),
                ImVec2(loopEndX, canvasPos_.y + canvasSize_.y),
                UIComponents::colorToImU32(ImVec4(0.3f, 0.7f, 0.3f, 0.8f)),
                2.0f
            );
        }
    }
}

void PianoRollUI::renderPianoKeys() {
    if (!drawList_) return;
    
    const ImVec2 keysPos(canvasPos_.x + canvasSize_.x, canvasPos_.y);
    const ImVec2 keysSize(PIANO_KEY_WIDTH, canvasSize_.y);
    
    // Background
    drawList_->AddRectFilled(
        keysPos,
        keysPos + keysSize,
        UIComponents::colorToImU32(THEME_COLOR(frameBg))
    );
    
    // Draw piano keys
    for (int note = viewState_.visibleLowNote; note <= viewState_.visibleHighNote; ++note) {
        const float keyY = noteToPixel(note);
        const float keyHeight = DEFAULT_PIXELS_PER_SEMITONE;
        
        const bool isBlack = isBlackKey(note);
        const ImVec4 keyColor = getPianoKeyColor(note, hoveredNote_ && hoveredNote_->note_number == note);
        
        const ImRect keyRect(
            ImVec2(keysPos.x, keyY),
            ImVec2(keysPos.x + (isBlack ? keysSize.x * 0.6f : keysSize.x), keyY + keyHeight)
        );
        
        // Draw key
        drawList_->AddRectFilled(
            keyRect.Min,
            keyRect.Max,
            UIComponents::colorToImU32(keyColor),
            2.0f
        );
        
        // Key border
        drawList_->AddRect(
            keyRect.Min,
            keyRect.Max,
            UIComponents::colorToImU32(THEME_COLOR(border)),
            2.0f,
            0,
            1.0f
        );
        
        // Note name (only for white keys and if there's space)
        if (!isBlack && keyHeight > 16.0f && viewState_.showNoteNames) {
            const std::string noteName = getNoteNameFromMIDI(note);
            const ImVec2 textSize = ImGui::CalcTextSize(noteName.c_str());
            const ImVec2 textPos(
                keyRect.Min.x + 4,
                keyRect.Min.y + (keyHeight - textSize.y) * 0.5f
            );
            
            drawList_->AddText(
                textPos,
                UIComponents::colorToImU32(THEME_COLOR(text)),
                noteName.c_str()
            );
        }
    }
}

void PianoRollUI::handleMouseInput() {
    if (!ImGui::IsItemActive() && !ImGui::IsItemHovered()) return;
    
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;
    
    // Convert mouse position to beat/note coordinates
    auto [beat, midiNote] = pixelToBeat(mousePos);
    
    // Update hovered note
    hoveredNote_ = findNoteAtPosition(mousePos);
    
    // Handle different tools
    switch (currentTool_) {
        case Tool::Select:
            handleSelectTool();
            break;
        case Tool::Draw:
            handleDrawTool();
            break;
        case Tool::Erase:
            handleEraseTool();
            break;
        case Tool::Trim:
            handleTrimTool();
            break;
        case Tool::Split:
            handleSplitTool();
            break;
        case Tool::Velocity:
            handleVelocityTool();
            break;
    }
    
    // Handle zoom with mouse wheel
    if (io.MouseWheel != 0.0f && ImGui::IsItemHovered()) {
        if (io.KeyCtrl) {
            // Horizontal zoom
            const float zoomFactor = io.MouseWheel > 0 ? 1.1f : 0.9f;
            viewState_.horizontalZoom = std::clamp(viewState_.horizontalZoom * zoomFactor, 
                                                  static_cast<float>(MIN_ZOOM), 
                                                  static_cast<float>(MAX_ZOOM));
        } else if (io.KeyShift) {
            // Vertical zoom
            const float zoomFactor = io.MouseWheel > 0 ? 1.1f : 0.9f;
            viewState_.verticalZoom = std::clamp(viewState_.verticalZoom * zoomFactor, 
                                                static_cast<float>(MIN_ZOOM), 
                                                static_cast<float>(MAX_ZOOM));
        } else {
            // Vertical scroll
            viewState_.scrollY = std::clamp(viewState_.scrollY - static_cast<int>(io.MouseWheel * 3), 
                                           0, 127);
        }
    }
}

void PianoRollUI::handleDrawTool() {
    const ImGuiIO& io = ImGui::GetIO();
    
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto [beat, midiNote] = pixelToBeat(io.MousePos);
        
        if (midiNote >= 0 && midiNote <= 127) {
            // Snap to grid if enabled
            const double snappedBeat = viewState_.snapToGrid ? snapBeatToGrid(beat) : beat;
            
            // Add new note
            if (editor_) {
                const auto& defaultProps = editor_->get_default_note_properties();
                editor_->draw_note_at_position(snappedBeat, midiNote, defaultProps.length_beats, defaultProps.velocity);
                
                if (onNoteAdded) {
                    onNoteAdded(snappedBeat, midiNote, defaultProps.velocity);
                }
            }
        }
    }
}

void PianoRollUI::handleEraseTool() {
    const ImGuiIO& io = ImGui::GetIO();
    
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || 
        (ImGui::IsMouseDown(ImGuiMouseButton_Left) && hoveredNote_)) {
        
        if (hoveredNote_ && editor_) {
            editor_->erase_note_at_position(hoveredNote_->start_time_beats, hoveredNote_->note_number);
            
            if (onNoteDeleted) {
                onNoteDeleted(hoveredNote_);
            }
        }
    }
}

void PianoRollUI::handleSelectTool() {
    const ImGuiIO& io = ImGui::GetIO();
    
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (hoveredNote_) {
            // Select/deselect note
            if (!io.KeyShift && !io.KeyCtrl) {
                selectNone();
            }
            
            if (auto clip = editor_->get_clip()) {
                // Toggle selection state
                // Note: This would need proper selection management in the actual implementation
                if (onNoteSelected) {
                    onNoteSelected(hoveredNote_);
                }
            }
        } else {
            // Start selection rectangle
            isSelecting_ = true;
            selectionStart_ = io.MousePos;
        }
    }
    
    if (isSelecting_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        selectionEnd_ = io.MousePos;
    }
    
    if (isSelecting_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        // Complete selection
        const ImRect selectionRect(selectionStart_, selectionEnd_);
        const auto [startBeat, startNote] = pixelToBeat(selectionRect.Min);
        const auto [endBeat, endNote] = pixelToBeat(selectionRect.Max);
        
        selectInRegion(std::min(startBeat, endBeat), std::max(startBeat, endBeat),
                      std::min(startNote, endNote), std::max(startNote, endNote));
        
        isSelecting_ = false;
    }
}

// Coordinate conversion methods
ImVec2 PianoRollUI::beatToPixel(double beat, int midiNote) const {
    return ImVec2(beatToPixel(beat), noteToPixel(midiNote));
}

std::pair<double, int> PianoRollUI::pixelToBeat(const ImVec2& pixel) const {
    return {pixelToBeat(pixel.x), pixelToNote(pixel.y)};
}

double PianoRollUI::pixelToBeat(float x) const {
    return viewState_.scrollX + (x - canvasPos_.x) / (DEFAULT_PIXELS_PER_BEAT * viewState_.horizontalZoom);
}

int PianoRollUI::pixelToNote(float y) const {
    return static_cast<int>(viewState_.scrollY - (y - canvasPos_.y) / (DEFAULT_PIXELS_PER_SEMITONE * viewState_.verticalZoom));
}

float PianoRollUI::beatToPixel(double beat) const {
    return canvasPos_.x + (beat - viewState_.scrollX) * DEFAULT_PIXELS_PER_BEAT * viewState_.horizontalZoom;
}

float PianoRollUI::noteToPixel(int note) const {
    return canvasPos_.y + (viewState_.scrollY - note) * DEFAULT_PIXELS_PER_SEMITONE * viewState_.verticalZoom;
}

// Helper methods
ImVec4 PianoRollUI::getNoteColor(MIDINote* note) const {
    if (!note) return noteStyle_.defaultColor;
    
    if (isNoteSelected(note)) {
        return noteStyle_.selectedColor;
    }
    
    switch (viewState_.colorScheme) {
        case ViewState::ColorScheme::Velocity:
            return getVelocityColor(note->velocity);
        case ViewState::ColorScheme::Channel:
            return noteStyle_.defaultColor; // TODO: Implement channel colors
        case ViewState::ColorScheme::Pitch:
            return noteStyle_.defaultColor; // TODO: Implement pitch colors
        default:
            return noteStyle_.defaultColor;
    }
}

ImVec4 PianoRollUI::getVelocityColor(int velocity) const {
    velocity = std::clamp(velocity, 0, 127);
    return noteStyle_.velocityColors[velocity];
}

bool PianoRollUI::isNoteSelected(MIDINote* note) const {
    return std::find(selectedNotes_.begin(), selectedNotes_.end(), note) != selectedNotes_.end();
}

void PianoRollUI::drawNote(MIDINote* note, const ImRect& bounds, const ImVec4& color) {
    if (!drawList_ || !note) return;
    
    // Note body
    drawList_->AddRectFilled(
        bounds.Min,
        bounds.Max,
        UIComponents::colorToImU32(color),
        noteStyle_.cornerRounding
    );
    
    // Note border (for selected notes)
    if (isNoteSelected(note)) {
        drawList_->AddRect(
            bounds.Min,
            bounds.Max,
            UIComponents::colorToImU32(noteStyle_.selectedColor),
            noteStyle_.cornerRounding,
            0,
            noteStyle_.borderThickness * 2.0f
        );
    }
}

std::vector<double> PianoRollUI::calculateGridLines() const {
    std::vector<double> lines;
    
    const double resolution = getGridResolution();
    const double startBeat = std::floor(viewState_.visibleStartBeat / resolution) * resolution;
    const double endBeat = viewState_.visibleEndBeat;
    
    for (double beat = startBeat; beat <= endBeat; beat += resolution) {
        lines.push_back(beat);
    }
    
    return lines;
}

std::vector<int> PianoRollUI::calculateNoteLines() const {
    std::vector<int> lines;
    
    for (int note = viewState_.visibleLowNote; note <= viewState_.visibleHighNote; ++note) {
        lines.push_back(note);
    }
    
    return lines;
}

double PianoRollUI::getGridResolution() const {
    switch (viewState_.gridResolution) {
        case GridSnap::QUARTER: return 1.0;
        case GridSnap::EIGHTH: return 0.5;
        case GridSnap::SIXTEENTH: return 0.25;
        case GridSnap::THIRTY_SECOND: return 0.125;
        case GridSnap::TRIPLET_EIGHTH: return 1.0 / 3.0;
        case GridSnap::TRIPLET_SIXTEENTH: return 1.0 / 6.0;
        default: return 0.25;
    }
}

bool PianoRollUI::isBlackKey(int midiNote) const {
    const int noteInOctave = midiNote % 12;
    return noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || 
           noteInOctave == 8 || noteInOctave == 10;
}

std::string PianoRollUI::getNoteNameFromMIDI(int midiNote) const {
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int octave = (midiNote / 12) - 1;
    const int noteInOctave = midiNote % 12;
    
    std::stringstream ss;
    ss << noteNames[noteInOctave] << octave;
    return ss.str();
}

void PianoRollUI::applyTheme() {
    if (!g_Theme) return;
    
    // Update grid colors from theme
    gridStyle_.majorGridColor = THEME_COLOR(separator);
    gridStyle_.minorGridColor = UIComponents::adjustBrightness(THEME_COLOR(separator), 0.5f);
    gridStyle_.beatGridColor = UIComponents::adjustBrightness(THEME_COLOR(separator), 0.8f);
    gridStyle_.barGridColor = UIComponents::adjustBrightness(THEME_COLOR(separator), 1.2f);
    
    // Update note colors from theme
    noteStyle_.defaultColor = THEME_COLOR(midiNotes);
    noteStyle_.selectedColor = THEME_COLOR(midiNotesSelected);
}

// Placeholder implementations for missing methods
void PianoRollUI::handleKeyboardInput() {}
void PianoRollUI::handleToolInput() {}
void PianoRollUI::handleTrimTool() {}
void PianoRollUI::handleSplitTool() {}
void PianoRollUI::handleVelocityTool() {}
void PianoRollUI::renderTimeline() {}
void PianoRollUI::renderVelocityLane() {}
void PianoRollUI::renderSelection() {}
void PianoRollUI::renderNotePreview() {}
void PianoRollUI::updateVisibleNotes() {}
void PianoRollUI::selectAll() {}
void PianoRollUI::selectNone() { selectedNotes_.clear(); }
void PianoRollUI::selectInRegion(double, double, int, int) {}
MIDINote* PianoRollUI::findNoteAtPosition(const ImVec2&, float) { return nullptr; }
bool PianoRollUI::isNoteVisible(MIDINote*) const { return true; }
void PianoRollUI::drawNoteVelocityBar(MIDINote*, const ImRect&) {}
ImVec4 PianoRollUI::getPianoKeyColor(int, bool) const { return THEME_COLOR(pianoRollKeys); }

} // namespace mixmind::ui