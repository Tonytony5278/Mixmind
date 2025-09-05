#include "MixMindMainWindow.h"
#include "../core/logging.h"
#include <imgui.h>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace mixmind::ui {

// ============================================================================
// AIAssistantPanel Implementation
// ============================================================================

class AIAssistantPanel::Impl {
public:
    std::vector<ChatMessage> messages;
    std::vector<AICapability> capabilities;
    bool isProcessing = false;
    
    // Callbacks
    SendMessageCallback sendMessageCallback;
    AnalyzeProjectCallback analyzeProjectCallback;
    SuggestPluginsCallback suggestPluginsCallback;
    GenerateMelodyCallback generateMelodyCallback;
    
    // UI State
    char inputBuffer[1024] = "";
    bool scrollToBottom = false;
    int selectedCapability = -1;
    
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp) {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }
    
    void renderChatArea() {
        ImGui::BeginChild("ChatMessages", ImVec2(0, -80), true);
        
        for (const auto& message : messages) {
            ImVec4 textColor;
            std::string prefix;
            
            switch (message.type) {
                case ChatMessage::USER:
                    textColor = ImVec4(0.8f, 0.9f, 1.0f, 1.0f);
                    prefix = "You";
                    break;
                case ChatMessage::ASSISTANT:
                    textColor = ImVec4(0.7f, 1.0f, 0.7f, 1.0f);
                    prefix = "MixMind AI";
                    break;
                case ChatMessage::SYSTEM:
                    textColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                    prefix = "System";
                    break;
            }
            
            // Timestamp and sender
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
                              "[%s] %s:", 
                              formatTimestamp(message.timestamp).c_str(),
                              prefix.c_str());
            
            // Message content
            ImGui::TextWrapped("%s", message.content.c_str());
            
            // Metadata (for structured responses)
            if (!message.metadata.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.7f, 1.0f), 
                                  "Metadata: %s", message.metadata.c_str());
            }
            
            ImGui::Separator();
        }
        
        // Processing indicator
        if (isProcessing) {
            ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "MixMind AI is thinking...");
            
            // Animated dots
            static float time = 0.0f;
            time += ImGui::GetIO().DeltaTime;
            int dots = static_cast<int>(time * 2) % 4;
            for (int i = 0; i < dots; ++i) {
                ImGui::SameLine();
                ImGui::Text(".");
            }
        }
        
        // Auto-scroll to bottom
        if (scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom = false;
        }
        
        ImGui::EndChild();
    }
    
    void renderInputArea() {
        // Input field
        ImGui::PushItemWidth(-80);
        bool enterPressed = ImGui::InputText("##ChatInput", inputBuffer, sizeof(inputBuffer), 
                                           ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        bool sendPressed = ImGui::Button("Send", ImVec2(70, 0));
        
        // Send message
        if ((enterPressed || sendPressed) && strlen(inputBuffer) > 0) {
            if (sendMessageCallback) {
                sendMessageCallback(std::string(inputBuffer));
                
                // Add user message to chat
                ChatMessage userMessage;
                userMessage.type = ChatMessage::USER;
                userMessage.content = inputBuffer;
                userMessage.timestamp = std::chrono::system_clock::now();
                messages.push_back(userMessage);
                
                scrollToBottom = true;
            }
            
            // Clear input
            inputBuffer[0] = '\0';
        }
        
        // Keep focus on input field
        if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive()) {
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    
    void renderCapabilitiesPanel() {
        if (ImGui::CollapsingHeader("AI Capabilities", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            for (size_t i = 0; i < capabilities.size(); ++i) {
                const auto& capability = capabilities[i];
                
                // Capability status indicator
                ImVec4 statusColor = capability.isAvailable ? 
                                   ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : 
                                   ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
                
                ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
                ImGui::Text("●");
                ImGui::PopStyleColor();
                
                ImGui::SameLine();
                ImGui::Text("%s", capability.name.c_str());
                
                if (capability.confidence > 0.0f) {
                    ImGui::SameLine();
                    ImGui::Text("(%.0f%% confidence)", capability.confidence * 100.0f);
                }
                
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s\nStatus: %s", 
                                     capability.description.c_str(),
                                     capability.isAvailable ? "Available" : "Unavailable");
                }
            }
        }
    }
    
    void renderQuickActions() {
        if (ImGui::CollapsingHeader("Quick Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            // Project Analysis
            if (ImGui::Button("Analyze Project", ImVec2(-1, 30))) {
                if (analyzeProjectCallback) {
                    analyzeProjectCallback();
                    
                    ChatMessage systemMessage;
                    systemMessage.type = ChatMessage::SYSTEM;
                    systemMessage.content = "Starting project analysis...";
                    systemMessage.timestamp = std::chrono::system_clock::now();
                    messages.push_back(systemMessage);
                    scrollToBottom = true;
                }
            }
            
            ImGui::Separator();
            
            // Plugin Suggestions
            ImGui::Text("Genre:");
            static char genreBuffer[64] = "Electronic";
            ImGui::InputText("##Genre", genreBuffer, sizeof(genreBuffer));
            
            ImGui::Text("Mood:");
            static char moodBuffer[64] = "Energetic";
            ImGui::InputText("##Mood", moodBuffer, sizeof(moodBuffer));
            
            if (ImGui::Button("Suggest Plugins", ImVec2(-1, 25))) {
                if (suggestPluginsCallback) {
                    suggestPluginsCallback(genreBuffer, moodBuffer);
                    
                    ChatMessage systemMessage;
                    systemMessage.type = ChatMessage::SYSTEM;
                    systemMessage.content = "Generating plugin suggestions for " + 
                                          std::string(genreBuffer) + " / " + std::string(moodBuffer);
                    systemMessage.timestamp = std::chrono::system_clock::now();
                    messages.push_back(systemMessage);
                    scrollToBottom = true;
                }
            }
            
            ImGui::Separator();
            
            // Melody Generation
            ImGui::Text("Style:");
            static char styleBuffer[64] = "Modern Pop";
            ImGui::InputText("##Style", styleBuffer, sizeof(styleBuffer));
            
            ImGui::Text("Key:");
            static int keyIndex = 0;
            const char* keys[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            ImGui::Combo("##Key", &keyIndex, keys, IM_ARRAYSIZE(keys));
            
            if (ImGui::Button("Generate Melody", ImVec2(-1, 25))) {
                if (generateMelodyCallback) {
                    generateMelodyCallback(styleBuffer, keys[keyIndex]);
                    
                    ChatMessage systemMessage;
                    systemMessage.type = ChatMessage::SYSTEM;
                    systemMessage.content = "Generating melody in " + std::string(keys[keyIndex]) + 
                                          " for " + std::string(styleBuffer);
                    systemMessage.timestamp = std::chrono::system_clock::now();
                    messages.push_back(systemMessage);
                    scrollToBottom = true;
                }
            }
            
            ImGui::Separator();
            
            // Mixing Assistance
            if (ImGui::Button("Mix Analysis", ImVec2(-1, 25))) {
                if (sendMessageCallback) {
                    sendMessageCallback("Please analyze my current mix and provide feedback");
                    
                    ChatMessage userMessage;
                    userMessage.type = ChatMessage::USER;
                    userMessage.content = "Please analyze my current mix and provide feedback";
                    userMessage.timestamp = std::chrono::system_clock::now();
                    messages.push_back(userMessage);
                    scrollToBottom = true;
                }
            }
            
            if (ImGui::Button("Mastering Tips", ImVec2(-1, 25))) {
                if (sendMessageCallback) {
                    sendMessageCallback("Give me mastering tips for my current project");
                    
                    ChatMessage userMessage;
                    userMessage.type = ChatMessage::USER;
                    userMessage.content = "Give me mastering tips for my current project";
                    userMessage.timestamp = std::chrono::system_clock::now();
                    messages.push_back(userMessage);
                    scrollToBottom = true;
                }
            }
            
            ImGui::Separator();
            
            // Clear Chat
            if (ImGui::Button("Clear Chat", ImVec2(-1, 25))) {
                messages.clear();
            }
        }
    }
};

AIAssistantPanel::AIAssistantPanel() : pImpl_(std::make_unique<Impl>()) {
    // Add welcome message
    ChatMessage welcomeMessage;
    welcomeMessage.type = ChatMessage::ASSISTANT;
    welcomeMessage.content = "Hello! I'm your MixMind AI assistant. I can help you with:\n\n"
                           "• Project analysis and feedback\n"
                           "• Plugin recommendations\n" 
                           "• Melody and harmony generation\n"
                           "• Mixing and mastering advice\n"
                           "• Music theory questions\n\n"
                           "How can I help you today?";
    welcomeMessage.timestamp = std::chrono::system_clock::now();
    pImpl_->messages.push_back(welcomeMessage);
    
    // Initialize capabilities
    pImpl_->capabilities = {
        {"Project Analysis", "Analyze your project structure and provide feedback", true, 0.95f},
        {"Plugin Suggestions", "Recommend plugins based on genre and style", true, 0.90f},
        {"Melody Generation", "Generate melodies and harmonies", true, 0.85f},
        {"Mix Analysis", "Analyze frequency balance and dynamics", true, 0.88f},
        {"Music Theory", "Answer music theory and composition questions", true, 0.98f},
        {"MIDI Generation", "Create MIDI patterns and sequences", true, 0.80f},
        {"Audio Processing", "Suggest audio processing chains", true, 0.92f}
    };
}

AIAssistantPanel::~AIAssistantPanel() = default;

void AIAssistantPanel::render() {
    ImGui::BeginChild("AIAssistantMain", ImVec2(0, 0), false);
    
    // Split into left panel (chat) and right panel (capabilities/actions)
    ImGui::Columns(2, "AIColumns", true);
    
    // Left column - Chat area
    ImGui::BeginChild("ChatArea", ImVec2(0, 0), true);
    
    ImGui::Text("AI Chat Assistant");
    ImGui::Separator();
    
    pImpl_->renderChatArea();
    pImpl_->renderInputArea();
    
    ImGui::EndChild();
    
    // Right column - Capabilities and Quick Actions
    ImGui::NextColumn();
    
    ImGui::BeginChild("ControlArea", ImVec2(0, 0), true);
    
    pImpl_->renderCapabilitiesPanel();
    ImGui::Separator();
    pImpl_->renderQuickActions();
    
    ImGui::EndChild();
    
    ImGui::Columns(1);
    ImGui::EndChild();
}

void AIAssistantPanel::addMessage(const ChatMessage& message) {
    pImpl_->messages.push_back(message);
    pImpl_->scrollToBottom = true;
}

void AIAssistantPanel::clearMessages() {
    pImpl_->messages.clear();
}

void AIAssistantPanel::setAvailableCapabilities(const std::vector<AICapability>& capabilities) {
    pImpl_->capabilities = capabilities;
}

void AIAssistantPanel::setProcessingState(bool isProcessing) {
    pImpl_->isProcessing = isProcessing;
}

void AIAssistantPanel::setSendMessageCallback(SendMessageCallback callback) {
    pImpl_->sendMessageCallback = callback;
}

void AIAssistantPanel::setAnalyzeProjectCallback(AnalyzeProjectCallback callback) {
    pImpl_->analyzeProjectCallback = callback;
}

void AIAssistantPanel::setSuggestPluginsCallback(SuggestPluginsCallback callback) {
    pImpl_->suggestPluginsCallback = callback;
}

void AIAssistantPanel::setGenerateMelodyCallback(GenerateMelodyCallback callback) {
    pImpl_->generateMelodyCallback = callback;
}

} // namespace mixmind::ui