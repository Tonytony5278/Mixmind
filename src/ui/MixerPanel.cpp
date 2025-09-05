#include "MixMindMainWindow.h"
#include "../core/logging.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace mixmind::ui {

// ============================================================================
// MixerPanel Implementation
// ============================================================================

class MixerPanel::Impl {
public:
    std::vector<ChannelStrip> channels;
    
    // Callbacks
    VolumeCallback volumeCallback;
    PanCallback panCallback;
    MuteCallback muteCallback;
    SoloCallback soloCallback;
    EQCallback eqCallback;
    
    // UI State
    int selectedChannel = -1;
    bool showEQ = true;
    bool showSends = false;
    
    float dbToSlider(float db) const {
        // Convert dB to 0-1 slider value (with proper curve)
        return std::pow(10.0f, db / 20.0f);
    }
    
    float sliderToDB(float slider) const {
        // Convert 0-1 slider to dB
        if (slider <= 0.0f) return -96.0f;
        return 20.0f * std::log10(slider);
    }
    
    void renderChannelStrip(int index, ChannelStrip& strip) {
        ImGui::PushID(index);
        
        // Channel header
        if (ImGui::CollapsingHeader(strip.channelName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            
            // Solo/Mute buttons
            ImGui::PushStyleColor(ImGuiCol_Button, strip.solo ? 
                                 ImVec4(1.0f, 0.8f, 0.0f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("S", ImVec2(25, 25))) {
                strip.solo = !strip.solo;
                if (soloCallback) {
                    soloCallback(index, strip.solo);
                }
            }
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, strip.mute ? 
                                 ImVec4(0.8f, 0.2f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("M", ImVec2(25, 25))) {
                strip.mute = !strip.mute;
                if (muteCallback) {
                    muteCallback(index, strip.mute);
                }
            }
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, strip.record ? 
                                 ImVec4(0.8f, 0.2f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("R", ImVec2(25, 25))) {
                strip.record = !strip.record;
            }
            ImGui::PopStyleColor();
            
            // Level meters
            renderLevelMeter(strip);
            
            // Volume fader
            ImGui::Text("Vol");
            ImGui::PushItemWidth(60);
            float volumeSlider = dbToSlider(strip.volume + 96.0f); // Offset for 0-1 range
            if (ImGui::VSliderFloat("##Volume", ImVec2(60, 150), &volumeSlider, 0.0f, 1.0f, "")) {
                strip.volume = sliderToDB(volumeSlider) - 96.0f;
                if (volumeCallback) {
                    volumeCallback(index, strip.volume);
                }
            }
            ImGui::PopItemWidth();
            
            // Volume text display
            ImGui::Text("%.1f dB", strip.volume);
            
            // Pan knob
            ImGui::Text("Pan");
            ImGui::PushItemWidth(80);
            if (ImGui::SliderFloat("##Pan", &strip.pan, -1.0f, 1.0f, "%.2f")) {
                if (panCallback) {
                    panCallback(index, strip.pan);
                }
            }
            ImGui::PopItemWidth();
            
            // EQ Section
            if (showEQ && !strip.eqBands.empty()) {
                ImGui::Separator();
                ImGui::Text("EQ");
                
                for (size_t i = 0; i < strip.eqBands.size(); ++i) {
                    renderEQBand(index, static_cast<int>(i), strip.eqBands[i]);
                }
            }
            
            // Sends Section
            if (showSends && !strip.sendLevels.empty()) {
                ImGui::Separator();
                ImGui::Text("Sends");
                
                int sendIndex = 0;
                for (auto& [sendName, level] : strip.sendLevels) {
                    ImGui::Text("%s", sendName.c_str());
                    ImGui::PushItemWidth(80);
                    ImGui::SliderFloat(("##Send" + std::to_string(sendIndex)).c_str(), 
                                      &level, 0.0f, 1.0f, "%.2f");
                    ImGui::PopItemWidth();
                    sendIndex++;
                }
            }
        }
        
        ImGui::PopID();
    }
    
    void renderLevelMeter(const ChannelStrip& strip) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImVec2(40, 100);
        
        // Background
        drawList->AddRectFilled(canvasPos, 
                               ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                               IM_COL32(20, 20, 20, 255));
        
        // Convert dB to pixel height
        auto dbToPixels = [&](float db) -> float {
            if (db <= -96.0f) return 0.0f;
            float normalized = (db + 96.0f) / 96.0f; // -96 to 0 dB -> 0 to 1
            return normalized * canvasSize.y;
        };
        
        // Left channel (peak)
        float leftHeight = dbToPixels(strip.peakLevelL);
        if (leftHeight > 0) {
            ImU32 leftColor = strip.peakLevelL > -6.0f ? IM_COL32(255, 100, 100, 255) : 
                             strip.peakLevelL > -18.0f ? IM_COL32(255, 255, 100, 255) : 
                             IM_COL32(100, 255, 100, 255);
            
            drawList->AddRectFilled(ImVec2(canvasPos.x + 2, canvasPos.y + canvasSize.y - leftHeight),
                                   ImVec2(canvasPos.x + 18, canvasPos.y + canvasSize.y - 2),
                                   leftColor);
        }
        
        // Right channel (peak)
        float rightHeight = dbToPixels(strip.peakLevelR);
        if (rightHeight > 0) {
            ImU32 rightColor = strip.peakLevelR > -6.0f ? IM_COL32(255, 100, 100, 255) : 
                              strip.peakLevelR > -18.0f ? IM_COL32(255, 255, 100, 255) : 
                              IM_COL32(100, 255, 100, 255);
            
            drawList->AddRectFilled(ImVec2(canvasPos.x + 22, canvasPos.y + canvasSize.y - rightHeight),
                                   ImVec2(canvasPos.x + 38, canvasPos.y + canvasSize.y - 2),
                                   rightColor);
        }
        
        // Grid lines
        for (int db = -60; db <= 0; db += 12) {
            float y = canvasPos.y + canvasSize.y - dbToPixels(static_cast<float>(db));
            drawList->AddLine(ImVec2(canvasPos.x, y), 
                             ImVec2(canvasPos.x + canvasSize.x, y),
                             IM_COL32(60, 60, 60, 255));
        }
        
        // Border
        drawList->AddRect(canvasPos, 
                         ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                         IM_COL32(100, 100, 100, 255));
        
        // dB labels
        ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + canvasSize.x + 5, canvasPos.y));
        ImGui::BeginGroup();
        ImGui::Text("0");
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15);
        ImGui::Text("-12");
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15);
        ImGui::Text("-24");
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15);
        ImGui::Text("-48");
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15);
        ImGui::Text("-∞");
        ImGui::EndGroup();
        
        ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y + 5));
        ImGui::Dummy(canvasSize);
    }
    
    void renderEQBand(int channelIndex, int bandIndex, ChannelStrip::EQBand& band) {
        ImGui::PushID(bandIndex);
        
        // EQ Type
        const char* eqTypes[] = {"HPF", "Low Shelf", "Bell", "High Shelf", "LPF"};
        int currentType = static_cast<int>(band.type);
        if (ImGui::Combo("Type", &currentType, eqTypes, IM_ARRAYSIZE(eqTypes))) {
            band.type = static_cast<ChannelStrip::EQBand::Type>(currentType);
            if (eqCallback) {
                eqCallback(channelIndex, bandIndex, band);
            }
        }
        
        // Frequency
        ImGui::PushItemWidth(80);
        if (ImGui::DragFloat("Freq", &band.frequency, 10.0f, 20.0f, 20000.0f, "%.0f Hz")) {
            if (eqCallback) {
                eqCallback(channelIndex, bandIndex, band);
            }
        }
        
        // Gain (for applicable types)
        if (band.type == ChannelStrip::EQBand::LOWSHELF || 
            band.type == ChannelStrip::EQBand::BELL || 
            band.type == ChannelStrip::EQBand::HIGHSHELF) {
            
            if (ImGui::DragFloat("Gain", &band.gain, 0.1f, -24.0f, 24.0f, "%.1f dB")) {
                if (eqCallback) {
                    eqCallback(channelIndex, bandIndex, band);
                }
            }
        }
        
        // Q/Width (for applicable types)
        if (band.type == ChannelStrip::EQBand::BELL || 
            band.type == ChannelStrip::EQBand::HIGHPASS || 
            band.type == ChannelStrip::EQBand::LOWPASS) {
            
            if (ImGui::DragFloat("Q", &band.q, 0.01f, 0.1f, 20.0f, "%.2f")) {
                if (eqCallback) {
                    eqCallback(channelIndex, bandIndex, band);
                }
            }
        }
        
        ImGui::PopItemWidth();
        
        // Enable/Disable
        if (ImGui::Checkbox("Enable", &band.enabled)) {
            if (eqCallback) {
                eqCallback(channelIndex, bandIndex, band);
            }
        }
        
        ImGui::PopID();
        ImGui::Separator();
    }
};

MixerPanel::MixerPanel() : pImpl_(std::make_unique<Impl>()) {
    // Initialize with default channels
    setChannelCount(8);
}

MixerPanel::~MixerPanel() = default;

void MixerPanel::render() {
    // Mixer controls
    ImGui::Text("Mixer Console");
    ImGui::Separator();
    
    // Global controls
    if (ImGui::Button("Show EQ")) {
        pImpl_->showEQ = !pImpl_->showEQ;
    }
    ImGui::SameLine();
    if (ImGui::Button("Show Sends")) {
        pImpl_->showSends = !pImpl_->showSends;
    }
    
    ImGui::Separator();
    
    // Channel strips in columns
    ImGui::BeginChild("ChannelStrips", ImVec2(0, 0), true);
    
    ImGui::Columns(static_cast<int>(pImpl_->channels.size()), "Channels", true);
    
    for (size_t i = 0; i < pImpl_->channels.size(); ++i) {
        pImpl_->renderChannelStrip(static_cast<int>(i), pImpl_->channels[i]);
        ImGui::NextColumn();
    }
    
    ImGui::Columns(1);
    ImGui::EndChild();
}

void MixerPanel::setChannelCount(int count) {
    pImpl_->channels.resize(count);
    
    // Initialize default channel strips
    for (int i = 0; i < count; ++i) {
        auto& channel = pImpl_->channels[i];
        channel.channelId = "channel_" + std::to_string(i);
        channel.channelName = "Track " + std::to_string(i + 1);
        channel.volume = 0.0f; // 0 dB
        channel.pan = 0.0f;
        
        // Initialize EQ bands
        channel.eqBands.resize(4);
        channel.eqBands[0] = {true, 80.0f, 0.0f, 0.7f, ChannelStrip::EQBand::HIGHPASS};
        channel.eqBands[1] = {true, 200.0f, 0.0f, 1.0f, ChannelStrip::EQBand::LOWSHELF};
        channel.eqBands[2] = {true, 2000.0f, 0.0f, 1.0f, ChannelStrip::EQBand::BELL};
        channel.eqBands[3] = {true, 8000.0f, 0.0f, 0.7f, ChannelStrip::EQBand::HIGHSHELF};
        
        // Initialize sends
        channel.sendLevels["Reverb"] = 0.0f;
        channel.sendLevels["Delay"] = 0.0f;
    }
}

void MixerPanel::updateChannelStrip(int channelIndex, const ChannelStrip& strip) {
    if (channelIndex >= 0 && channelIndex < static_cast<int>(pImpl_->channels.size())) {
        pImpl_->channels[channelIndex] = strip;
    }
}

void MixerPanel::setVolumeCallback(VolumeCallback callback) {
    pImpl_->volumeCallback = callback;
}

void MixerPanel::setPanCallback(PanCallback callback) {
    pImpl_->panCallback = callback;
}

void MixerPanel::setMuteCallback(MuteCallback callback) {
    pImpl_->muteCallback = callback;
}

void MixerPanel::setSoloCallback(SoloCallback callback) {
    pImpl_->soloCallback = callback;
}

void MixerPanel::setEQCallback(EQCallback callback) {
    pImpl_->eqCallback = callback;
}

} // namespace mixmind::ui