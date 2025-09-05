#include "MixMindMainWindow.h"
#include "../core/logging.h"
#include <imgui.h>
#include <algorithm>

namespace mixmind::ui {

// ============================================================================
// PluginRackPanel Implementation  
// ============================================================================

class PluginRackPanel::Impl {
public:
    std::vector<PluginSlot> slots;
    
    // Callbacks
    LoadPluginCallback loadPluginCallback;
    UnloadPluginCallback unloadPluginCallback;
    BypassPluginCallback bypassPluginCallback;
    OpenEditorCallback openEditorCallback;
    CloseEditorCallback closeEditorCallback;
    MovePluginCallback movePluginCallback;
    
    // UI State
    bool showPluginBrowser = false;
    int draggedSlot = -1;
    int selectedSlot = -1;
    
    void renderPluginSlot(int index, PluginSlot& slot) {
        ImGui::PushID(index);
        
        // Slot background color based on state
        ImVec4 slotColor = slot.isLoaded ? 
                          (slot.isBypassed ? ImVec4(0.5f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 0.5f, 0.3f, 1.0f)) :
                          ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, slotColor);
        
        if (ImGui::BeginChild(("Slot" + std::to_string(index)).c_str(), 
                             ImVec2(0, 120), true, ImGuiWindowFlags_NoScrollbar)) {
            
            // Slot header
            ImGui::Text("Slot %d", index + 1);
            
            if (slot.isLoaded) {
                // Plugin name and info
                ImGui::Text("%s", slot.pluginName.c_str());
                ImGui::Text("by %s", slot.manufacturer.c_str());
                ImGui::Separator();
                
                // Plugin controls
                ImGui::PushStyleColor(ImGuiCol_Button, slot.isBypassed ? 
                                     ImVec4(0.8f, 0.4f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("Bypass")) {
                    slot.isBypassed = !slot.isBypassed;
                    if (bypassPluginCallback) {
                        bypassPluginCallback(index, slot.isBypassed);
                    }
                }
                ImGui::PopStyleColor();
                
                ImGui::SameLine();
                if (ImGui::Button("Edit")) {
                    if (!slot.editorOpen) {
                        slot.editorOpen = true;
                        if (openEditorCallback) {
                            openEditorCallback(index);
                        }
                    } else {
                        slot.editorOpen = false;
                        if (closeEditorCallback) {
                            closeEditorCallback(index);
                        }
                    }
                }
                
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    if (unloadPluginCallback) {
                        unloadPluginCallback(index);
                    }
                }
                
                // Performance info
                ImGui::Separator();
                ImGui::Text("CPU: %.1f%%", slot.cpuUsage);
                if (slot.latencySamples > 0) {
                    ImGui::Text("Latency: %d samples", slot.latencySamples);
                }
                
            } else {
                // Empty slot
                ImGui::Text("Empty");
                ImGui::Separator();
                
                if (ImGui::Button("Load Plugin", ImVec2(-1, 30))) {
                    showPluginBrowser = true;
                    selectedSlot = index;
                }
                
                ImGui::Text("Drag plugin here\nor click Load Plugin");
            }
            
            // Drag and drop handling
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLUGIN_SLOT")) {
                    int sourceSlot = *static_cast<int*>(payload->Data);
                    if (sourceSlot != index && movePluginCallback) {
                        movePluginCallback(sourceSlot, index);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            
            if (slot.isLoaded && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("PLUGIN_SLOT", &index, sizeof(int));
                ImGui::Text("Move %s", slot.pluginName.c_str());
                ImGui::EndDragDropSource();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        
        ImGui::PopID();
    }
    
    void renderPluginBrowser() {
        if (!showPluginBrowser) return;
        
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Plugin Browser", &showPluginBrowser)) {
            
            // Search and filter
            static char searchBuffer[256] = "";
            ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
            
            static int categoryFilter = 0;
            const char* categories[] = {"All", "Instruments", "Effects", "Dynamics", "EQ", "Reverb", "Delay", "Distortion"};
            ImGui::Combo("Category", &categoryFilter, categories, IM_ARRAYSIZE(categories));
            
            ImGui::Separator();
            
            // Plugin list
            if (ImGui::BeginChild("PluginList", ImVec2(0, -40))) {
                
                // Mock plugin data - in real implementation, get from plugin scanner
                struct PluginInfo {
                    std::string name;
                    std::string manufacturer;
                    std::string category;
                    std::string path;
                };
                
                std::vector<PluginInfo> availablePlugins = {
                    {"Serum", "Xfer Records", "Instruments", "C:/VSTPlugins/Serum.vst3"},
                    {"Pro-Q 3", "FabFilter", "EQ", "C:/VSTPlugins/FabFilter Pro-Q 3.vst3"},
                    {"VintageVerb", "Valhalla DSP", "Reverb", "C:/VSTPlugins/ValhallaVintageVerb.vst3"},
                    {"EchoBoy", "Soundtoys", "Delay", "C:/VSTPlugins/Soundtoys EchoBoy.vst3"},
                    {"Massive X", "Native Instruments", "Instruments", "C:/VSTPlugins/Massive X.vst3"},
                    {"Saturn 2", "FabFilter", "Distortion", "C:/VSTPlugins/FabFilter Saturn 2.vst3"},
                    {"Pro-C 2", "FabFilter", "Dynamics", "C:/VSTPlugins/FabFilter Pro-C 2.vst3"},
                    {"H-Reverb", "Waves", "Reverb", "C:/VSTPlugins/Waves H-Reverb.vst3"}
                };
                
                // Filter plugins based on search and category
                std::string searchStr = searchBuffer;
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                
                for (const auto& plugin : availablePlugins) {
                    // Category filter
                    if (categoryFilter != 0) {
                        if (plugin.category != categories[categoryFilter]) {
                            continue;
                        }
                    }
                    
                    // Search filter
                    if (!searchStr.empty()) {
                        std::string pluginName = plugin.name;
                        std::string manufacturer = plugin.manufacturer;
                        std::transform(pluginName.begin(), pluginName.end(), pluginName.begin(), ::tolower);
                        std::transform(manufacturer.begin(), manufacturer.end(), manufacturer.begin(), ::tolower);
                        
                        if (pluginName.find(searchStr) == std::string::npos && 
                            manufacturer.find(searchStr) == std::string::npos) {
                            continue;
                        }
                    }
                    
                    // Plugin entry
                    ImGui::PushID(plugin.path.c_str());
                    
                    if (ImGui::Selectable((plugin.name + " - " + plugin.manufacturer).c_str(), false, 
                                         ImGuiSelectableFlags_AllowDoubleClick)) {
                        
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            // Load plugin into selected slot
                            if (selectedSlot >= 0 && loadPluginCallback) {
                                loadPluginCallback(selectedSlot, plugin.path);
                                showPluginBrowser = false;
                            }
                        }
                    }
                    
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Category: %s\nPath: %s\nDouble-click to load", 
                                         plugin.category.c_str(), plugin.path.c_str());
                    }
                    
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            
            // Bottom buttons
            if (ImGui::Button("Load Selected")) {
                // Load selected plugin
                showPluginBrowser = false;
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Rescan Plugins")) {
                // Trigger plugin rescan
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                showPluginBrowser = false;
            }
            
        }
        ImGui::End();
    }
};

PluginRackPanel::PluginRackPanel() : pImpl_(std::make_unique<Impl>()) {
    // Initialize with 8 empty slots
    pImpl_->slots.resize(8);
    for (int i = 0; i < 8; ++i) {
        pImpl_->slots[i].slotId = "slot_" + std::to_string(i);
    }
}

PluginRackPanel::~PluginRackPanel() = default;

void PluginRackPanel::render() {
    ImGui::Text("Plugin Rack");
    ImGui::Separator();
    
    // Rack controls
    if (ImGui::Button("Load Plugin")) {
        pImpl_->showPluginBrowser = true;
        pImpl_->selectedSlot = 0; // Default to first empty slot
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) {
        for (int i = 0; i < static_cast<int>(pImpl_->slots.size()); ++i) {
            if (pImpl_->unloadPluginCallback) {
                pImpl_->unloadPluginCallback(i);
            }
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Add Slot")) {
        PluginSlot newSlot;
        newSlot.slotId = "slot_" + std::to_string(pImpl_->slots.size());
        pImpl_->slots.push_back(newSlot);
    }
    
    ImGui::Separator();
    
    // Plugin slots grid
    ImGui::BeginChild("PluginSlots", ImVec2(0, 0), false);
    
    // Arrange slots in a grid (2 columns)
    const int slotsPerRow = 2;
    ImGui::Columns(slotsPerRow, "PluginColumns", true);
    
    for (int i = 0; i < static_cast<int>(pImpl_->slots.size()); ++i) {
        pImpl_->renderPluginSlot(i, pImpl_->slots[i]);
        
        if ((i + 1) % slotsPerRow == 0) {
            ImGui::NextColumn();
        } else {
            ImGui::NextColumn();
        }
    }
    
    ImGui::Columns(1);
    ImGui::EndChild();
    
    // Plugin browser window
    pImpl_->renderPluginBrowser();
}

void PluginRackPanel::setPluginSlots(const std::vector<PluginSlot>& slots) {
    pImpl_->slots = slots;
}

void PluginRackPanel::updatePluginSlot(int slotIndex, const PluginSlot& slot) {
    if (slotIndex >= 0 && slotIndex < static_cast<int>(pImpl_->slots.size())) {
        pImpl_->slots[slotIndex] = slot;
    }
}

void PluginRackPanel::setLoadPluginCallback(LoadPluginCallback callback) {
    pImpl_->loadPluginCallback = callback;
}

void PluginRackPanel::setUnloadPluginCallback(UnloadPluginCallback callback) {
    pImpl_->unloadPluginCallback = callback;
}

void PluginRackPanel::setBypassPluginCallback(BypassPluginCallback callback) {
    pImpl_->bypassPluginCallback = callback;
}

void PluginRackPanel::setOpenEditorCallback(OpenEditorCallback callback) {
    pImpl_->openEditorCallback = callback;
}

void PluginRackPanel::setCloseEditorCallback(CloseEditorCallback callback) {
    pImpl_->closeEditorCallback = callback;
}

void PluginRackPanel::setMovePluginCallback(MovePluginCallback callback) {
    pImpl_->movePluginCallback = callback;
}

} // namespace mixmind::ui