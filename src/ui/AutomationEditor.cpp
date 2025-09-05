#include "MixMindMainWindow.h"
#include "../core/logging.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace mixmind::ui {

// ============================================================================
// AutomationEditor Implementation
// ============================================================================

class AutomationEditor::Impl {
public:
    std::vector<AutomationLaneView> lanes;
    
    // View parameters
    double viewStartTime = 0.0;
    double viewEndTime = 60.0; // 60 seconds default
    float horizontalZoom = 1.0f;
    float verticalZoom = 1.0f;
    
    // Edit state
    EditMode currentEditMode = EditMode::SELECT;
    int selectedLane = -1;
    int selectedPoint = -1;
    bool isDraggingPoint = false;
    bool isDraggingView = false;
    ImVec2 dragStartPos;
    
    // Callbacks
    AddPointCallback addPointCallback;
    RemovePointCallback removePointCallback;
    MovePointCallback movePointCallback;
    SelectPointCallback selectPointCallback;
    
    // UI constants
    static constexpr float TIMELINE_HEIGHT = 30.0f;
    static constexpr float LANE_MIN_HEIGHT = 50.0f;
    static constexpr float LANE_MAX_HEIGHT = 200.0f;
    
    void renderTimeline() {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, TIMELINE_HEIGHT);
        
        // Timeline background
        drawList->AddRectFilled(canvasPos, 
                               ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                               IM_COL32(30, 30, 30, 255));
        
        // Time markers
        double timeRange = viewEndTime - viewStartTime;
        double pixelsPerSecond = canvasSize.x / timeRange;
        
        // Major grid (seconds)
        for (double time = std::ceil(viewStartTime); time <= viewEndTime; time += 1.0) {
            float x = canvasPos.x + static_cast<float>((time - viewStartTime) * pixelsPerSecond);
            
            // Grid line
            drawList->AddLine(ImVec2(x, canvasPos.y), 
                             ImVec2(x, canvasPos.y + canvasSize.y),
                             IM_COL32(60, 60, 60, 255));
            
            // Time label
            if (x >= canvasPos.x && x <= canvasPos.x + canvasSize.x - 40) {
                int minutes = static_cast<int>(time) / 60;
                int seconds = static_cast<int>(time) % 60;
                
                drawList->AddText(ImVec2(x + 2, canvasPos.y + 5), 
                                 IM_COL32(200, 200, 200, 255),
                                 ImGui::GetIO().Fonts->Fonts[0],
                                 12.0f,
                                 (std::to_string(minutes) + ":" + 
                                  (seconds < 10 ? "0" : "") + std::to_string(seconds)).c_str());
            }
        }
        
        // Border
        drawList->AddRect(canvasPos, 
                         ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                         IM_COL32(100, 100, 100, 255));
        
        ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y));
        ImGui::Dummy(canvasSize);
    }
    
    void renderAutomationLane(const AutomationLaneView& lane, int laneIndex) {
        ImGui::PushID(laneIndex);
        
        // Lane header
        if (ImGui::CollapsingHeader((lane.parameterName + " (" + lane.targetName + ")").c_str(), 
                                   ImGuiTreeNodeFlags_DefaultOpen)) {
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, lane.height);
            
            // Lane background
            ImU32 bgColor = (selectedLane == laneIndex) ? 
                           IM_COL32(40, 40, 50, 255) : IM_COL32(25, 25, 25, 255);
            drawList->AddRectFilled(canvasPos, 
                                   ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                                   bgColor);
            
            // Grid lines
            if (lane.showGrid) {
                renderGrid(canvasPos, canvasSize);
            }
            
            // Automation curve
            renderAutomationCurve(lane, canvasPos, canvasSize);
            
            // Automation points
            if (lane.showPoints) {
                renderAutomationPoints(lane, laneIndex, canvasPos, canvasSize);
            }
            
            // Handle mouse input for this lane
            handleLaneInput(laneIndex, canvasPos, canvasSize);
            
            // Lane border
            drawList->AddRect(canvasPos, 
                             ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                             IM_COL32(60, 60, 60, 255));
            
            ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y));
            ImGui::Dummy(canvasSize);
        }
        
        ImGui::PopID();
    }
    
    void renderGrid(ImVec2 canvasPos, ImVec2 canvasSize) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        double timeRange = viewEndTime - viewStartTime;
        double pixelsPerSecond = canvasSize.x / timeRange;
        
        // Vertical grid lines (time)
        for (double time = std::ceil(viewStartTime); time <= viewEndTime; time += 1.0) {
            float x = canvasPos.x + static_cast<float>((time - viewStartTime) * pixelsPerSecond);
            drawList->AddLine(ImVec2(x, canvasPos.y), 
                             ImVec2(x, canvasPos.y + canvasSize.y),
                             IM_COL32(40, 40, 40, 255));
        }
        
        // Horizontal grid lines (value)
        for (int i = 1; i <= 4; ++i) {
            float y = canvasPos.y + (canvasSize.y * i / 5);
            drawList->AddLine(ImVec2(canvasPos.x, y), 
                             ImVec2(canvasPos.x + canvasSize.x, y),
                             IM_COL32(40, 40, 40, 255));
        }
    }
    
    void renderAutomationCurve(const AutomationLaneView& lane, ImVec2 canvasPos, ImVec2 canvasSize) {
        // Mock automation data - in real implementation, get from automation manager
        std::vector<ImVec2> curvePoints;
        
        // Sample the automation curve
        double timeRange = viewEndTime - viewStartTime;
        double pixelsPerSecond = canvasSize.x / timeRange;
        
        for (float x = 0; x <= canvasSize.x; x += 2.0f) {
            double time = viewStartTime + (x / pixelsPerSecond);
            
            // Mock automation curve (sine wave for demo)
            float value = 0.5f + 0.3f * std::sin(time * 0.5f);
            value = std::clamp(value, lane.minValue, lane.maxValue);
            
            float normalizedValue = (value - lane.minValue) / (lane.maxValue - lane.minValue);
            float y = canvasPos.y + canvasSize.y - (normalizedValue * canvasSize.y);
            
            curvePoints.emplace_back(canvasPos.x + x, y);
        }
        
        // Draw the curve
        if (curvePoints.size() >= 2) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (size_t i = 1; i < curvePoints.size(); ++i) {
                drawList->AddLine(curvePoints[i-1], curvePoints[i], lane.color, lane.lineWidth);
            }
        }
    }
    
    void renderAutomationPoints(const AutomationLaneView& lane, int laneIndex, ImVec2 canvasPos, ImVec2 canvasSize) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Mock automation points - in real implementation, get from automation manager
        std::vector<std::pair<double, float>> points = {
            {5.0, 0.2f},
            {15.0, 0.8f},
            {30.0, 0.3f},
            {45.0, 0.7f}
        };
        
        double timeRange = viewEndTime - viewStartTime;
        double pixelsPerSecond = canvasSize.x / timeRange;
        
        for (size_t i = 0; i < points.size(); ++i) {
            double time = points[i].first;
            float value = points[i].second;
            
            // Skip points outside view
            if (time < viewStartTime || time > viewEndTime) continue;
            
            float x = canvasPos.x + static_cast<float>((time - viewStartTime) * pixelsPerSecond);
            float normalizedValue = (value - lane.minValue) / (lane.maxValue - lane.minValue);
            float y = canvasPos.y + canvasSize.y - (normalizedValue * canvasSize.y);
            
            // Point appearance
            bool isSelected = (selectedLane == laneIndex && selectedPoint == static_cast<int>(i));
            float radius = isSelected ? 6.0f : 4.0f;
            ImU32 pointColor = isSelected ? IM_COL32(255, 255, 100, 255) : IM_COL32(200, 200, 200, 255);
            ImU32 outlineColor = IM_COL32(60, 60, 60, 255);
            
            // Draw point
            drawList->AddCircleFilled(ImVec2(x, y), radius, pointColor);
            drawList->AddCircle(ImVec2(x, y), radius, outlineColor);
            
            // Value tooltip on hover
            if (ImGui::IsMouseHoveringRect(ImVec2(x - radius - 2, y - radius - 2),
                                          ImVec2(x + radius + 2, y + radius + 2))) {
                ImGui::SetTooltip("Time: %.2fs\nValue: %.3f %s", 
                                 time, value, lane.units.c_str());
            }
        }
    }
    
    void handleLaneInput(int laneIndex, ImVec2 canvasPos, ImVec2 canvasSize) {
        if (!ImGui::IsMouseHoveringRect(canvasPos, 
                                       ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y))) {
            return;
        }
        
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Select lane on click
        if (ImGui::IsMouseClicked(0)) {
            selectedLane = laneIndex;
        }
        
        // Add point on double-click (in PENCIL mode)
        if (currentEditMode == EditMode::PENCIL && ImGui::IsMouseDoubleClicked(0)) {
            double timeRange = viewEndTime - viewStartTime;
            double pixelsPerSecond = canvasSize.x / timeRange;
            
            double time = viewStartTime + ((mousePos.x - canvasPos.x) / pixelsPerSecond);
            float normalizedValue = 1.0f - ((mousePos.y - canvasPos.y) / canvasSize.y);
            
            const auto& lane = lanes[laneIndex];
            float value = lane.minValue + normalizedValue * (lane.maxValue - lane.minValue);
            
            if (addPointCallback) {
                addPointCallback(lane.laneId, time, value);
            }
        }
        
        // Context menu
        if (ImGui::IsMouseClicked(1)) {
            ImGui::OpenPopup("AutomationContextMenu");
        }
        
        if (ImGui::BeginPopup("AutomationContextMenu")) {
            if (ImGui::MenuItem("Add Point")) {
                // Handle add point
            }
            if (ImGui::MenuItem("Delete Point")) {
                // Handle delete point
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Lane")) {
                // Handle clear lane
            }
            ImGui::EndPopup();
        }
    }
};

AutomationEditor::AutomationEditor() : pImpl_(std::make_unique<Impl>()) {}
AutomationEditor::~AutomationEditor() = default;

void AutomationEditor::render() {
    ImGui::Text("Automation Editor");
    ImGui::Separator();
    
    // Toolbar
    if (ImGui::Button("Select")) {
        pImpl_->currentEditMode = EditMode::SELECT;
    }
    ImGui::SameLine();
    if (ImGui::Button("Pencil")) {
        pImpl_->currentEditMode = EditMode::PENCIL;
    }
    ImGui::SameLine();
    if (ImGui::Button("Line")) {
        pImpl_->currentEditMode = EditMode::LINE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Curve")) {
        pImpl_->currentEditMode = EditMode::CURVE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Erase")) {
        pImpl_->currentEditMode = EditMode::ERASE;
    }
    
    // Zoom controls
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    
    if (ImGui::Button("Zoom In")) {
        pImpl_->horizontalZoom *= 1.5f;
        double center = (pImpl_->viewStartTime + pImpl_->viewEndTime) / 2.0;
        double halfRange = (pImpl_->viewEndTime - pImpl_->viewStartTime) / 3.0;
        pImpl_->viewStartTime = center - halfRange;
        pImpl_->viewEndTime = center + halfRange;
    }
    ImGui::SameLine();
    if (ImGui::Button("Zoom Out")) {
        pImpl_->horizontalZoom /= 1.5f;
        double center = (pImpl_->viewStartTime + pImpl_->viewEndTime) / 2.0;
        double halfRange = (pImpl_->viewEndTime - pImpl_->viewStartTime) * 1.5;
        pImpl_->viewStartTime = center - halfRange;
        pImpl_->viewEndTime = center + halfRange;
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit View")) {
        pImpl_->viewStartTime = 0.0;
        pImpl_->viewEndTime = 60.0;
        pImpl_->horizontalZoom = 1.0f;
    }
    
    ImGui::Separator();
    
    // Timeline
    pImpl_->renderTimeline();
    
    // Automation lanes
    ImGui::BeginChild("AutomationLanes", ImVec2(0, 0), false);
    
    for (size_t i = 0; i < pImpl_->lanes.size(); ++i) {
        if (pImpl_->lanes[i].isVisible) {
            pImpl_->renderAutomationLane(pImpl_->lanes[i], static_cast<int>(i));
        }
    }
    
    ImGui::EndChild();
}

void AutomationEditor::setTimeRange(double startSeconds, double endSeconds) {
    pImpl_->viewStartTime = startSeconds;
    pImpl_->viewEndTime = endSeconds;
}

void AutomationEditor::setZoom(float horizontalZoom, float verticalZoom) {
    pImpl_->horizontalZoom = horizontalZoom;
    pImpl_->verticalZoom = verticalZoom;
}

void AutomationEditor::addAutomationLane(const AutomationLaneView& lane) {
    pImpl_->lanes.push_back(lane);
}

void AutomationEditor::removeAutomationLane(const std::string& laneId) {
    pImpl_->lanes.erase(
        std::remove_if(pImpl_->lanes.begin(), pImpl_->lanes.end(),
                      [&laneId](const AutomationLaneView& lane) {
                          return lane.laneId == laneId;
                      }),
        pImpl_->lanes.end());
}

void AutomationEditor::updateAutomationLane(const std::string& laneId, const AutomationLaneView& lane) {
    auto it = std::find_if(pImpl_->lanes.begin(), pImpl_->lanes.end(),
                          [&laneId](const AutomationLaneView& l) {
                              return l.laneId == laneId;
                          });
    
    if (it != pImpl_->lanes.end()) {
        *it = lane;
    }
}

void AutomationEditor::setAddPointCallback(AddPointCallback callback) {
    pImpl_->addPointCallback = callback;
}

void AutomationEditor::setRemovePointCallback(RemovePointCallback callback) {
    pImpl_->removePointCallback = callback;
}

void AutomationEditor::setMovePointCallback(MovePointCallback callback) {
    pImpl_->movePointCallback = callback;
}

void AutomationEditor::setSelectPointCallback(SelectPointCallback callback) {
    pImpl_->selectPointCallback = callback;
}

void AutomationEditor::setEditMode(EditMode mode) {
    pImpl_->currentEditMode = mode;
}

AutomationEditor::EditMode AutomationEditor::getEditMode() const {
    return pImpl_->currentEditMode;
}

} // namespace mixmind::ui