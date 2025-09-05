#pragma once
#include "../ai/AIAssistant.h"
#include "../ai/ProactiveMonitor.h"
#include "../ai/MusicKnowledgeBase.h"
#include "../ai/IntelligentProcessor.h"
#include "../ai/StyleMatcher.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <chrono>

class AIChatWidget {
public:
    AIChatWidget(std::shared_ptr<ai::AIAssistant> assistant);
    
    // Proactive monitoring integration
    void setProactiveMonitor(std::shared_ptr<mixmind::ai::ProactiveAIMonitor> monitor);
    void updateProactiveSuggestions(const std::vector<mixmind::ai::ProactiveAIMonitor::ProactiveSuggestion>& suggestions);
    
    void render();  // Call in main render loop
    void showSuggestion(const std::string& suggestion, const ImVec4& color);
    void setVisible(bool visible) { is_visible_ = visible; }
    
private:
    struct ChatMessage {
        std::string text;
        bool is_user_message;
        std::chrono::steady_clock::time_point timestamp;
        ImVec4 color;
    };
    
    std::shared_ptr<ai::AIAssistant> ai_assistant_;
    std::vector<ChatMessage> chat_history_;
    char input_buffer_[512] = {};
    bool is_visible_ = true;
    bool auto_scroll_ = true;
    
    // Proactive suggestions support
    std::shared_ptr<mixmind::ai::ProactiveAIMonitor> proactive_monitor_;
    std::vector<mixmind::ai::ProactiveAIMonitor::ProactiveSuggestion> pending_suggestions_;
    
    // Music intelligence components
    std::shared_ptr<mixmind::ai::MusicKnowledgeBase> music_knowledge_;
    std::shared_ptr<mixmind::ai::IntelligentProcessor> intelligent_processor_;
    std::shared_ptr<mixmind::ai::StyleMatcher> style_matcher_;
    
    // DAW integration
    std::shared_ptr<core::ITrack> current_track_;
    bool daw_integration_enabled_ = false;
    
    void processUserInput();
    void addMessage(const std::string& text, bool is_user);
    void renderChatHistory();
    void renderInputField();
    void renderSuggestions();
    void renderProactiveSuggestions();
    void handleSuggestionAction(const std::string& suggestion_id, bool accept);
    void onProactiveSuggestions(const std::vector<mixmind::ai::ProactiveAIMonitor::ProactiveSuggestion>& suggestions);
    std::string processMusicIntelligenceRequest(const std::string& user_message);
    
    // DAW Integration methods
    void setCurrentTrack(std::shared_ptr<core::ITrack> track);
    void enableDAWIntegration(bool enabled);
    void applyProcessingToDAW(const std::string& processing_type, const std::map<std::string, float>& parameters);
    void showVisualFeedback(const std::string& action_description);
};