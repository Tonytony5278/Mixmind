#include "MixMindMainWindow.h"
#include "../core/logging.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace mixmind::ui {

// ============================================================================
// PerformanceMonitorPanel Implementation
// ============================================================================

class PerformanceMonitorPanel::Impl {
public:
    // Metrics data
    performance::SystemMetrics currentSystemMetrics;
    performance::AudioEngineMetrics currentAudioMetrics;
    std::vector<performance::PluginMetrics> currentPluginMetrics;
    
    // Historical data for graphs
    std::vector<float> cpuHistory;
    std::vector<float> memoryHistory;
    std::vector<float> latencyHistory;
    std::vector<float> xrunHistory;
    
    // Display settings
    std::chrono::milliseconds updateInterval{100};
    std::chrono::seconds historyDuration{60};
    float cpuAlertThreshold = 80.0f;
    float memoryAlertThreshold = 80.0f;
    float latencyAlertThreshold = 20.0f;
    
    // UI state
    bool showSystemMetrics = true;
    bool showAudioMetrics = true;
    bool showPluginMetrics = true;
    bool showGraphs = true;
    bool showOptimizations = true;
    int selectedPluginIndex = -1;
    
    static constexpr size_t MAX_HISTORY_SAMPLES = 600; // 10 minutes at 100ms intervals
    
    void updateHistory() {
        // Update CPU history
        cpuHistory.push_back(currentSystemMetrics.cpuUsagePercent);
        if (cpuHistory.size() > MAX_HISTORY_SAMPLES) {
            cpuHistory.erase(cpuHistory.begin());
        }
        
        // Update memory history
        memoryHistory.push_back(currentSystemMetrics.memoryUsagePercent);
        if (memoryHistory.size() > MAX_HISTORY_SAMPLES) {
            memoryHistory.erase(memoryHistory.begin());
        }
        
        // Update latency history
        latencyHistory.push_back(static_cast<float>(currentAudioMetrics.roundTripLatencyMs));
        if (latencyHistory.size() > MAX_HISTORY_SAMPLES) {
            latencyHistory.erase(latencyHistory.begin());
        }
        
        // Update XRUN history
        xrunHistory.push_back(static_cast<float>(currentAudioMetrics.xrunCount));
        if (xrunHistory.size() > MAX_HISTORY_SAMPLES) {
            xrunHistory.erase(xrunHistory.begin());
        }
    }
    
    void renderSystemMetrics() {
        if (!showSystemMetrics) return;
        
        if (ImGui::CollapsingHeader("System Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            // CPU metrics
            ImGui::Text("CPU Usage:");
            ImGui::SameLine();
            renderProgressBar(currentSystemMetrics.cpuUsagePercent / 100.0f, 
                            "%.1f%%", currentSystemMetrics.cpuUsagePercent,
                            cpuAlertThreshold / 100.0f);
            
            ImGui::Text("  Audio Thread: %.1f%%", currentSystemMetrics.audioThreadCpuPercent);
            ImGui::Text("  UI Thread: %.1f%%", currentSystemMetrics.uiThreadCpuPercent);
            ImGui::Text("  Active Cores: %d / %d", currentSystemMetrics.activeCoreCount, 
                       currentSystemMetrics.totalCoreCount);
            
            ImGui::Separator();
            
            // Memory metrics
            ImGui::Text("Memory Usage:");
            ImGui::SameLine();
            renderProgressBar(currentSystemMetrics.memoryUsagePercent / 100.0f,
                            "%.1f%%", currentSystemMetrics.memoryUsagePercent,
                            memoryAlertThreshold / 100.0f);
            
            ImGui::Text("  Used: %zu MB / %zu MB", currentSystemMetrics.usedMemoryMB,
                       currentSystemMetrics.totalMemoryMB);
            ImGui::Text("  Available: %zu MB", currentSystemMetrics.availableMemoryMB);
            ImGui::Text("  Audio Buffers: %zu MB", currentSystemMetrics.audioBufferMemoryMB);
            ImGui::Text("  Plugin Memory: %zu MB", currentSystemMetrics.pluginMemoryMB);
            
            ImGui::Separator();
            
            // Disk I/O
            ImGui::Text("Disk I/O:");
            ImGui::Text("  Read: %.1f MB/s", currentSystemMetrics.diskReadMBps);
            ImGui::Text("  Write: %.1f MB/s", currentSystemMetrics.diskWriteMBps);
            ImGui::Text("  Queue Depth: %zu", currentSystemMetrics.diskQueueDepth);
            ImGui::Text("  Latency: %.1f ms", currentSystemMetrics.diskLatencyMs);
            
            // Additional metrics
            if (currentSystemMetrics.networkLatencyMs > 0) {
                ImGui::Separator();
                ImGui::Text("Network:");
                ImGui::Text("  Latency: %.1f ms", currentSystemMetrics.networkLatencyMs);
                ImGui::Text("  Bandwidth: %.1f Mbps", currentSystemMetrics.networkBandwidthMbps);
            }
            
            if (currentSystemMetrics.gpuUsagePercent > 0) {
                ImGui::Separator();
                ImGui::Text("GPU Usage: %.1f%%", currentSystemMetrics.gpuUsagePercent);
                ImGui::Text("GPU Memory: %zu MB", currentSystemMetrics.gpuMemoryMB);
            }
        }
    }
    
    void renderAudioEngineMetrics() {
        if (!showAudioMetrics) return;
        
        if (ImGui::CollapsingHeader("Audio Engine Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            // CPU Load
            ImGui::Text("Audio CPU Load:");
            ImGui::SameLine();
            renderProgressBar(currentAudioMetrics.currentCpuLoad / 100.0f,
                            "%.1f%%", currentAudioMetrics.currentCpuLoad,
                            0.8f); // 80% warning threshold
            
            ImGui::Text("  Average: %.1f%%", currentAudioMetrics.averageCpuLoad);
            ImGui::Text("  Peak: %.1f%%", currentAudioMetrics.peakCpuLoad);
            ImGui::Text("  Headroom: %.1f%%", currentAudioMetrics.headroomPercent);
            
            ImGui::Separator();
            
            // Timing
            ImGui::Text("Latency:");
            ImGui::Text("  Input: %.1f ms", currentAudioMetrics.inputLatencyMs);
            ImGui::Text("  Output: %.1f ms", currentAudioMetrics.outputLatencyMs);
            ImGui::Text("  Round-trip: %.1f ms", currentAudioMetrics.roundTripLatencyMs);
            ImGui::Text("  Jitter: %.3f ms", currentAudioMetrics.jitter);
            
            ImGui::Separator();
            
            // Buffer info
            ImGui::Text("Audio Configuration:");
            ImGui::Text("  Sample Rate: %.0f Hz", currentAudioMetrics.sampleRate);
            ImGui::Text("  Buffer Size: %d samples", currentAudioMetrics.bufferSize);
            ImGui::Text("  Channels: %d in / %d out", currentAudioMetrics.inputChannels,
                       currentAudioMetrics.outputChannels);
            
            ImGui::Separator();
            
            // Performance stats
            ImGui::Text("Performance:");
            ImGui::Text("  Buffers Processed: %d", currentAudioMetrics.buffersProcessed);
            ImGui::Text("  Buffers Dropped: %d", currentAudioMetrics.buffersDropped);
            
            // XRUN indicator
            if (currentAudioMetrics.xrunCount > 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("  XRUNs: %d", currentAudioMetrics.xrunCount);
                ImGui::PopStyleColor();
            } else {
                ImGui::Text("  XRUNs: %d", currentAudioMetrics.xrunCount);
            }
            
            ImGui::Separator();
            
            // Device info
            ImGui::Text("Audio Device:");
            ImGui::Text("  Driver: %s", currentAudioMetrics.audioDriver.c_str());
            ImGui::Text("  Input: %s", currentAudioMetrics.inputDevice.c_str());
            ImGui::Text("  Output: %s", currentAudioMetrics.outputDevice.c_str());
            ImGui::Text("  Mode: %s", currentAudioMetrics.exclusiveMode ? "Exclusive" : "Shared");
            ImGui::Text("  Status: %s", currentAudioMetrics.deviceStatus.c_str());
        }
    }
    
    void renderPluginPerformance() {
        if (!showPluginMetrics) return;
        
        if (ImGui::CollapsingHeader("Plugin Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            if (currentPluginMetrics.empty()) {
                ImGui::Text("No plugins loaded");
                return;
            }
            
            // Plugin list
            if (ImGui::BeginChild("PluginList", ImVec2(0, 200))) {
                
                for (size_t i = 0; i < currentPluginMetrics.size(); ++i) {
                    const auto& plugin = currentPluginMetrics[i];
                    
                    ImGui::PushID(static_cast<int>(i));
                    
                    bool isSelected = (selectedPluginIndex == static_cast<int>(i));
                    if (ImGui::Selectable(plugin.pluginName.c_str(), isSelected)) {
                        selectedPluginIndex = static_cast<int>(i);
                    }
                    
                    ImGui::SameLine(200);
                    ImGui::Text("%.1f%%", plugin.cpuUsagePercent);
                    
                    ImGui::SameLine(250);
                    ImGui::Text("%zu MB", plugin.memoryUsageMB);
                    
                    ImGui::SameLine(300);
                    if (plugin.latencyMs > 0) {
                        ImGui::Text("%.1f ms", plugin.latencyMs);
                    } else {
                        ImGui::Text("0 ms");
                    }
                    
                    // Status indicators
                    ImGui::SameLine(350);
                    if (!plugin.isActive) {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Inactive");
                    } else if (plugin.isBypassed) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Bypassed");
                    } else if (plugin.processingErrors > 0) {
                        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Errors");
                    } else {
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "OK");
                    }
                    
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            
            // Selected plugin details
            if (selectedPluginIndex >= 0 && selectedPluginIndex < static_cast<int>(currentPluginMetrics.size())) {
                ImGui::Separator();
                const auto& plugin = currentPluginMetrics[selectedPluginIndex];
                
                ImGui::Text("Selected Plugin: %s", plugin.pluginName.c_str());
                ImGui::Text("Manufacturer: %s", plugin.manufacturer.c_str());
                ImGui::Text("Format: %s", plugin.format.c_str());
                ImGui::Separator();
                
                ImGui::Text("CPU Usage: %.1f%% (avg: %.1f%%, peak: %.1f%%)", 
                           plugin.cpuUsagePercent, plugin.averageCpuUsage, plugin.peakCpuUsage);
                ImGui::Text("Processing Time: %.1f µs", plugin.processingTimeUs);
                ImGui::Text("Memory: %zu MB (peak: %zu MB)", plugin.memoryUsageMB, plugin.peakMemoryUsageMB);
                ImGui::Text("Latency: %d samples (%.1f ms)", plugin.latencySamples, plugin.latencyMs);
                
                ImGui::Separator();
                ImGui::Text("Buffers: %d processed, %d skipped", plugin.buffersProcessed, plugin.buffersSkipped);
                if (plugin.processingErrors > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 
                                      "Errors: %d", plugin.processingErrors);
                }
            }
        }
    }
    
    void renderPerformanceGraphs() {
        if (!showGraphs) return;
        
        if (ImGui::CollapsingHeader("Performance Graphs", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            // CPU Usage Graph
            if (!cpuHistory.empty()) {
                ImGui::Text("CPU Usage History");
                ImGui::PlotLines("##CPUGraph", cpuHistory.data(), static_cast<int>(cpuHistory.size()),
                               0, nullptr, 0.0f, 100.0f, ImVec2(0, 80));
            }
            
            // Memory Usage Graph
            if (!memoryHistory.empty()) {
                ImGui::Text("Memory Usage History");
                ImGui::PlotLines("##MemoryGraph", memoryHistory.data(), static_cast<int>(memoryHistory.size()),
                               0, nullptr, 0.0f, 100.0f, ImVec2(0, 80));
            }
            
            // Latency Graph
            if (!latencyHistory.empty()) {
                ImGui::Text("Audio Latency History (ms)");
                float maxLatency = *std::max_element(latencyHistory.begin(), latencyHistory.end());
                ImGui::PlotLines("##LatencyGraph", latencyHistory.data(), static_cast<int>(latencyHistory.size()),
                               0, nullptr, 0.0f, maxLatency * 1.1f, ImVec2(0, 80));
            }
            
            // XRUN Graph
            if (!xrunHistory.empty()) {
                ImGui::Text("Audio Dropouts (XRUNs)");
                float maxXruns = *std::max_element(xrunHistory.begin(), xrunHistory.end());
                ImGui::PlotLines("##XRunGraph", xrunHistory.data(), static_cast<int>(xrunHistory.size()),
                               0, nullptr, 0.0f, std::max(maxXruns * 1.1f, 10.0f), ImVec2(0, 80));
            }
        }
    }
    
    void renderOptimizationSuggestions() {
        if (!showOptimizations) return;
        
        if (ImGui::CollapsingHeader("Optimization Suggestions", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            // Generate suggestions based on current metrics
            std::vector<std::string> suggestions;
            
            if (currentSystemMetrics.cpuUsagePercent > cpuAlertThreshold) {
                suggestions.push_back("High CPU usage detected. Consider increasing buffer size.");
                suggestions.push_back("Close unnecessary applications to free up CPU resources.");
            }
            
            if (currentSystemMetrics.memoryUsagePercent > memoryAlertThreshold) {
                suggestions.push_back("High memory usage detected. Consider unloading unused plugins.");
                suggestions.push_back("Check for memory leaks in loaded plugins.");
            }
            
            if (currentAudioMetrics.roundTripLatencyMs > latencyAlertThreshold) {
                suggestions.push_back("High audio latency detected. Try reducing buffer size.");
                suggestions.push_back("Consider using ASIO driver for lower latency.");
            }
            
            if (currentAudioMetrics.xrunCount > 0) {
                suggestions.push_back("Audio dropouts detected. Increase buffer size or optimize CPU usage.");
                suggestions.push_back("Check for competing processes using audio resources.");
            }
            
            // CPU per-plugin analysis
            for (const auto& plugin : currentPluginMetrics) {
                if (plugin.cpuUsagePercent > 20.0f) {
                    suggestions.push_back("Plugin '" + plugin.pluginName + 
                                        "' is using high CPU (" + 
                                        std::to_string(static_cast<int>(plugin.cpuUsagePercent)) + 
                                        "%). Consider bypassing when not needed.");
                }
            }
            
            if (suggestions.empty()) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), 
                                  "✓ System performance is optimal");
            } else {
                for (size_t i = 0; i < suggestions.size(); ++i) {
                    ImGui::BulletText("%s", suggestions[i].c_str());
                }
            }
        }
    }
    
    void renderAlerts() {
        // Show critical alerts at the top
        std::vector<std::string> alerts;
        
        if (currentSystemMetrics.cpuUsagePercent > cpuAlertThreshold) {
            alerts.push_back("HIGH CPU USAGE: " + 
                           std::to_string(static_cast<int>(currentSystemMetrics.cpuUsagePercent)) + "%");
        }
        
        if (currentSystemMetrics.memoryUsagePercent > memoryAlertThreshold) {
            alerts.push_back("HIGH MEMORY USAGE: " + 
                           std::to_string(static_cast<int>(currentSystemMetrics.memoryUsagePercent)) + "%");
        }
        
        if (currentAudioMetrics.xrunCount > 0) {
            alerts.push_back("AUDIO DROPOUTS: " + std::to_string(currentAudioMetrics.xrunCount) + " XRUNs");
        }
        
        if (!alerts.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::CollapsingHeader("⚠️ Performance Alerts", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& alert : alerts) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", alert.c_str());
                }
            }
            ImGui::PopStyleColor();
            ImGui::Separator();
        }
    }
    
    void renderProgressBar(float fraction, const char* format, float value, float alertThreshold) {
        ImVec4 color = fraction > alertThreshold ? 
                      ImVec4(1.0f, 0.2f, 0.2f, 1.0f) :  // Red for high values
                      fraction > alertThreshold * 0.8f ?
                      ImVec4(1.0f, 1.0f, 0.2f, 1.0f) :  // Yellow for medium values
                      ImVec4(0.2f, 1.0f, 0.2f, 1.0f);   // Green for low values
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
        ImGui::ProgressBar(fraction, ImVec2(-1, 0), 
                          (std::string(format) + " " + std::to_string(static_cast<int>(value))).c_str());
        ImGui::PopStyleColor();
    }
};

PerformanceMonitorPanel::PerformanceMonitorPanel() : pImpl_(std::make_unique<Impl>()) {}
PerformanceMonitorPanel::~PerformanceMonitorPanel() = default;

void PerformanceMonitorPanel::render() {
    ImGui::Text("Performance Monitor");
    ImGui::Separator();
    
    // Update historical data
    pImpl_->updateHistory();
    
    // Show alerts first
    pImpl_->renderAlerts();
    
    // Toggle buttons for different sections
    if (ImGui::Button("System")) pImpl_->showSystemMetrics = !pImpl_->showSystemMetrics;
    ImGui::SameLine();
    if (ImGui::Button("Audio")) pImpl_->showAudioMetrics = !pImpl_->showAudioMetrics;
    ImGui::SameLine();
    if (ImGui::Button("Plugins")) pImpl_->showPluginMetrics = !pImpl_->showPluginMetrics;
    ImGui::SameLine();
    if (ImGui::Button("Graphs")) pImpl_->showGraphs = !pImpl_->showGraphs;
    ImGui::SameLine();
    if (ImGui::Button("Tips")) pImpl_->showOptimizations = !pImpl_->showOptimizations;
    
    ImGui::Separator();
    
    // Render performance sections
    ImGui::BeginChild("PerformanceContent", ImVec2(0, 0), false);
    
    pImpl_->renderSystemMetrics();
    pImpl_->renderAudioEngineMetrics();
    pImpl_->renderPluginPerformance();
    pImpl_->renderPerformanceGraphs();
    pImpl_->renderOptimizationSuggestions();
    
    ImGui::EndChild();
}

void PerformanceMonitorPanel::updateMetrics(const performance::SystemMetrics& system,
                                           const performance::AudioEngineMetrics& audio) {
    pImpl_->currentSystemMetrics = system;
    pImpl_->currentAudioMetrics = audio;
}

void PerformanceMonitorPanel::updatePluginMetrics(const std::vector<performance::PluginMetrics>& plugins) {
    pImpl_->currentPluginMetrics = plugins;
}

void PerformanceMonitorPanel::setUpdateInterval(std::chrono::milliseconds interval) {
    pImpl_->updateInterval = interval;
}

void PerformanceMonitorPanel::setHistoryDuration(std::chrono::seconds duration) {
    pImpl_->historyDuration = duration;
}

void PerformanceMonitorPanel::setAlertThresholds(float cpuThreshold, float memoryThreshold, float latencyThreshold) {
    pImpl_->cpuAlertThreshold = cpuThreshold;
    pImpl_->memoryAlertThreshold = memoryThreshold;
    pImpl_->latencyAlertThreshold = latencyThreshold;
}

} // namespace mixmind::ui