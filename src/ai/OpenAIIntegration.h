#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace mixmind::ai {

// ============================================================================
// AI Request/Response Types
// ============================================================================

struct ChatMessage {
    std::string role;     // "system", "user", "assistant"
    std::string content;  // Message content
};

struct ChatRequest {
    std::string model = "gpt-4";
    std::vector<ChatMessage> messages;
    float temperature = 0.7f;
    int maxTokens = 1000;
    float topP = 1.0f;
    int n = 1;
};

struct AIResponse {
    bool success = false;
    std::string content;
    std::string error;
    int promptTokens = 0;
    int completionTokens = 0;
    int totalTokens = 0;
};

struct AudioAnalysisContext {
    std::string audioFormat;    // "WAV", "MP3", etc.
    double duration = 0.0;      // seconds
    int sampleRate = 0;         // Hz
    int channels = 0;           // 1=mono, 2=stereo
    std::string genre;          // "Electronic", "Rock", etc.
    std::string key;            // "C major", "Am", etc.
    int tempo = 0;              // BPM
    double lufs = 0.0;          // Loudness
    std::string additionalInfo; // Free-form context
};

struct MusicGenerationRequest {
    std::string genre;           // Target genre
    std::string mood;            // "energetic", "chill", "dark"
    std::string key;             // "C major", "Dm"
    int tempo = 0;               // BPM
    std::vector<std::string> instruments; // List of desired instruments
    std::string structure;       // "verse-chorus-verse-chorus-bridge-chorus"
    std::string additionalPrompt; // Custom requirements
};

struct MixingRequest {
    std::string problemDescription; // What needs to be fixed/improved
    std::string trackType;          // "vocal", "drums", "bass", "full mix"
    std::string genre;              // Musical genre for context
    std::string currentIssues;      // Specific problems identified
    std::string desiredSound;       // Target sound description
    std::string referenceTrack;     // Reference song/artist
};

// ============================================================================
// OpenAI Client - Direct API Integration
// ============================================================================

class OpenAIClient {
public:
    OpenAIClient();
    ~OpenAIClient();
    
    // Non-copyable
    OpenAIClient(const OpenAIClient&) = delete;
    OpenAIClient& operator=(const OpenAIClient&) = delete;
    
    // Initialize with API key
    bool initialize(const std::string& apiKey);
    
    // Direct chat completion
    core::AsyncResult<AIResponse> sendChatRequest(const ChatRequest& request);
    
    // Specialized audio analysis
    core::AsyncResult<AIResponse> analyzeAudio(const std::string& description, const AudioAnalysisContext& context);
    
    // Music generation assistance
    core::AsyncResult<AIResponse> generateMusicIdeas(const MusicGenerationRequest& request);
    
    // Mixing and mastering advice
    core::AsyncResult<AIResponse> provideMixingAdvice(const MixingRequest& request);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Audio Intelligence Engine - High-Level AI Interface
// ============================================================================

class AudioIntelligenceEngine {
public:
    AudioIntelligenceEngine();
    ~AudioIntelligenceEngine();
    
    // Non-copyable
    AudioIntelligenceEngine(const AudioIntelligenceEngine&) = delete;
    AudioIntelligenceEngine& operator=(const AudioIntelligenceEngine&) = delete;
    
    // Initialize AI services
    bool initialize(const std::string& openaiApiKey);
    
    // Audio content analysis
    core::AsyncResult<AIResponse> analyzeAudioContent(const std::string& description, const AudioAnalysisContext& context = {});
    
    // Creative music generation
    core::AsyncResult<AIResponse> generateCreativeIdeas(const MusicGenerationRequest& request);
    
    // Professional mixing guidance
    core::AsyncResult<AIResponse> provideMixingGuidance(const MixingRequest& request);
    
    // Status queries
    bool isAnalyzing() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// AI-Powered Audio Processors
// ============================================================================

class IntelligentEQProcessor {
public:
    struct EQBand {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
        bool enabled = false;
        std::string aiReasoning; // Why AI suggested this setting
    };
    
    // AI-suggested EQ curve based on audio analysis
    std::vector<EQBand> generateAIEQCurve(const AudioAnalysisContext& context);
    
    // Real-time AI feedback on EQ changes
    std::string getAIFeedback(const std::vector<EQBand>& currentEQ);
};

class IntelligentCompressor {
public:
    struct CompressionSettings {
        float threshold = -6.0f;    // dB
        float ratio = 4.0f;         // 4:1
        float attack = 10.0f;       // ms
        float release = 100.0f;     // ms
        float knee = 2.0f;          // dB
        std::string aiReasoning;    // AI explanation
    };
    
    // AI-optimized compression settings
    CompressionSettings generateAISettings(const AudioAnalysisContext& context);
};

// ============================================================================
// Global AI Engine Access
// ============================================================================

// Get the global AI engine (singleton)
AudioIntelligenceEngine& getGlobalAIEngine();

// Shutdown AI engine (call at app exit)
void shutdownGlobalAIEngine();

// ============================================================================
// AI Utility Functions
// ============================================================================

namespace utils {
    // Load OpenAI API key from environment or config
    std::string loadOpenAIApiKey();
    
    // Validate AI response content
    bool isValidAIResponse(const AIResponse& response);
    
    // Extract musical information from AI response
    struct ExtractedMusicInfo {
        std::vector<std::string> chords;
        std::vector<std::string> suggestions;
        std::string key;
        int suggestedTempo = 0;
    };
    ExtractedMusicInfo extractMusicInfo(const AIResponse& response);
    
    // Convert audio context to descriptive string
    std::string contextToString(const AudioAnalysisContext& context);
}

} // namespace mixmind::ai