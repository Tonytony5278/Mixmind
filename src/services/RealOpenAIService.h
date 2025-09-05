#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>

namespace mixmind::services {

// ============================================================================
// Real OpenAI Service - Actual GPT-4 Integration
// ============================================================================

enum class AIModel {
    GPT_4_TURBO,
    GPT_4,
    GPT_3_5_TURBO,
    GPT_4_VISION // For audio waveform analysis
};

enum class AITaskType {
    PLUGIN_ANALYSIS,
    MUSIC_ANALYSIS,
    STYLE_MATCHING,
    CREATIVE_ASSISTANCE,
    MIXING_GUIDANCE,
    MASTERING_ADVICE,
    SOUND_DESIGN,
    COMPOSITION_HELP
};

struct AIRequest {
    AIModel model = AIModel::GPT_4_TURBO;
    AITaskType taskType = AITaskType::PLUGIN_ANALYSIS;
    std::string prompt;
    std::vector<std::string> contextData;
    std::unordered_map<std::string, std::string> metadata;
    int maxTokens = 1000;
    float temperature = 0.7f;
    float topP = 1.0f;
    std::string systemPrompt;
};

struct AIResponse {
    std::string content;
    std::string model;
    int tokensUsed = 0;
    double confidenceScore = 0.0;
    std::chrono::milliseconds responseTime{0};
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> structuredData;
    bool isSuccess = false;
    std::string errorMessage;
};

struct PluginAnalysisRequest {
    std::string pluginName;
    std::string manufacturer;
    std::string category;
    std::string version;
    std::vector<std::string> parameters;
    double cpuUsage = 0.0;
    int latencySamples = 0;
    bool isInstrument = false;
    std::string additionalContext;
};

struct PluginAnalysisResult {
    std::string analysis;
    std::string recommendations;
    std::vector<std::string> tags;
    float qualityScore = 0.0f;
    std::string workflow;
    std::vector<std::string> compatiblePlugins;
    std::string bestUseCase;
    std::string targetAudience;
};

struct MusicAnalysisRequest {
    std::string audioFilePath;
    std::string genre;
    std::string artistReference;
    std::string userGoal; // What they want to achieve
    std::vector<float> audioFeatures; // Spectral, rhythmic data
    double tempo = 0.0;
    std::string key;
    std::string mood;
};

struct MusicAnalysisResult {
    std::string analysis;
    std::string genreClassification;
    std::string moodAssessment;
    std::string energyLevel;
    std::vector<std::string> similarArtists;
    std::string mixingAdvice;
    std::string masteringAdvice;
    std::vector<std::string> recommendedPlugins;
    std::string creativeDirection;
};

struct StyleMatchingRequest {
    std::string targetArtist; // e.g., "Nirvana"
    std::string targetSong;   // e.g., "Smells Like Teen Spirit"
    std::string userAudioPath;
    std::string userGenre;
    std::vector<std::string> availablePlugins;
    std::string specificRequest; // e.g., "make guitar sound like Kurt Cobain"
};

struct StyleMatchingResult {
    std::string analysis;
    std::vector<std::string> pluginChain;
    std::unordered_map<std::string, float> pluginSettings;
    std::string processingSteps;
    std::string tonalCharacteristics;
    std::string recordingTechniques;
    std::string equipmentRecommendations;
    float matchConfidence = 0.0f;
};

class RealOpenAIService {
public:
    RealOpenAIService();
    ~RealOpenAIService();
    
    // Non-copyable
    RealOpenAIService(const RealOpenAIService&) = delete;
    RealOpenAIService& operator=(const RealOpenAIService&) = delete;
    
    // Configuration
    bool initialize(const std::string& apiKey, const std::string& organization = "");
    void setDefaultModel(AIModel model);
    void setMaxConcurrentRequests(int maxRequests);
    void setRequestTimeout(std::chrono::seconds timeout);
    
    // Generic AI requests
    mixmind::core::AsyncResult<AIResponse> sendRequest(const AIRequest& request);
    
    // Specialized AI services
    mixmind::core::AsyncResult<PluginAnalysisResult> analyzePlugin(const PluginAnalysisRequest& request);
    mixmind::core::AsyncResult<MusicAnalysisResult> analyzeMusic(const MusicAnalysisRequest& request);
    mixmind::core::AsyncResult<StyleMatchingResult> matchStyle(const StyleMatchingRequest& request);
    
    // Creative assistance
    mixmind::core::AsyncResult<std::string> generateMixingAdvice(
        const std::string& genre, 
        const std::string& challenges,
        const std::vector<std::string>& availablePlugins
    );
    
    mixmind::core::AsyncResult<std::string> generateMasteringAdvice(
        const std::string& musicStyle,
        const std::string& targetPlatform, // Spotify, Apple Music, CD, etc.
        const std::vector<std::string>& availableTools
    );
    
    mixmind::core::AsyncResult<std::vector<std::string>> suggestPluginChain(
        const std::string& instrumentType,
        const std::string& desiredSound,
        const std::vector<std::string>& availablePlugins
    );
    
    // Conversation and context
    mixmind::core::AsyncResult<std::string> chatWithAssistant(
        const std::string& message,
        const std::string& sessionId = ""
    );
    
    void addContextToSession(const std::string& sessionId, const std::string& context);
    void clearSession(const std::string& sessionId);
    
    // Status and statistics
    bool isInitialized() const;
    bool isOnline() const;
    int getQueuedRequestsCount() const;
    double getAverageResponseTime() const;
    int getTotalRequestsToday() const;
    int getRemainingQuota() const;
    
    // Rate limiting and costs
    bool canMakeRequest() const;
    double estimateTokenCost(const AIRequest& request) const;
    double getTotalCostToday() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Specialized AI Services for Music Production
// ============================================================================

class MusicProductionAI {
public:
    explicit MusicProductionAI(RealOpenAIService& openAIService);
    ~MusicProductionAI();
    
    // Advanced music analysis
    struct AdvancedMusicAnalysis {
        std::string harmonicAnalysis;
        std::string rhythmicAnalysis;
        std::string timbralAnalysis;
        std::string structuralAnalysis;
        std::string emotionalProfile;
        std::vector<std::string> influences;
        std::string productionStyle;
        std::string mixingCharacteristics;
        float commercialViability = 0.0f;
        std::vector<std::string> improvementSuggestions;
    };
    
    mixmind::core::AsyncResult<AdvancedMusicAnalysis> performAdvancedAnalysis(
        const std::string& audioPath,
        const std::vector<float>& audioFeatures
    );
    
    // Genre-specific processing chains
    struct GenreProcessingChain {
        std::string genre;
        std::vector<std::string> instrumentChains;
        std::vector<std::string> mixBusChains;
        std::vector<std::string> masterChain;
        std::unordered_map<std::string, std::unordered_map<std::string, float>> settings;
        std::string reasoning;
    };
    
    mixmind::core::AsyncResult<GenreProcessingChain> createGenreChain(
        const std::string& genre,
        const std::vector<std::string>& availablePlugins
    );
    
    // Artist style emulation
    struct ArtistStyleGuide {
        std::string artist;
        std::string era; // "early", "peak", "late"
        std::string signature_sounds;
        std::string recording_techniques;
        std::string mixing_approach;
        std::string mastering_approach;
        std::vector<std::string> key_plugins;
        std::vector<std::string> alternative_plugins;
        std::string step_by_step_guide;
        std::vector<std::string> reference_tracks;
    };
    
    mixmind::core::AsyncResult<ArtistStyleGuide> createArtistStyleGuide(
        const std::string& artist,
        const std::string& era = "",
        const std::vector<std::string>& availablePlugins = {}
    );
    
    // Real-time creative assistance
    mixmind::core::AsyncResult<std::string> getCreativeSuggestion(
        const std::string& currentProjectDescription,
        const std::string& stuckPoint, // Where user is stuck
        const std::string& desiredOutcome
    );
    
    // Arrangement and composition help
    struct ArrangementSuggestions {
        std::string currentStructure;
        std::string suggestedStructure;
        std::vector<std::string> sectionDevelopment;
        std::string transitionIdeas;
        std::string instrumentationSuggestions;
        std::string dynamicMapping;
        std::string tensionAndRelease;
    };
    
    mixmind::core::AsyncResult<ArrangementSuggestions> suggestArrangement(
        const std::string& genre,
        const std::string& currentArrangement,
        const std::string& vibe
    );

private:
    RealOpenAIService& openAI_;
    std::unordered_map<std::string, std::string> genreSystemPrompts_;
    std::unordered_map<std::string, std::string> artistKnowledgeBase_;
    
    void initializePrompts();
    std::string buildContextualPrompt(const std::string& basePrompt, 
                                    const std::unordered_map<std::string, std::string>& context);
};

// ============================================================================
// Plugin Intelligence with Real AI
// ============================================================================

class PluginIntelligenceAI {
public:
    explicit PluginIntelligenceAI(RealOpenAIService& openAIService);
    ~PluginIntelligenceAI();
    
    // Enhanced plugin analysis
    struct DeepPluginAnalysis {
        std::string technicalAnalysis;
        std::string musicalAnalysis;
        std::string workflowIntegration;
        std::string competitorComparison;
        std::string userExperienceAssessment;
        float overallScore = 0.0f;
        std::vector<std::string> pros;
        std::vector<std::string> cons;
        std::string targetUser;
        std::string recommendation;
    };
    
    mixmind::core::AsyncResult<DeepPluginAnalysis> performDeepAnalysis(
        const std::string& pluginName,
        const std::string& manufacturer,
        const std::vector<std::string>& parameters,
        const std::string& category
    );
    
    // Plugin chain optimization
    struct OptimizedPluginChain {
        std::vector<std::string> pluginOrder;
        std::unordered_map<std::string, float> settings;
        std::string reasoning;
        std::vector<std::string> alternatives;
        std::string expectedOutcome;
        float confidenceScore = 0.0f;
    };
    
    mixmind::core::AsyncResult<OptimizedPluginChain> optimizePluginChain(
        const std::vector<std::string>& currentChain,
        const std::string& sourceAudio,
        const std::string& targetSound,
        const std::vector<std::string>& availablePlugins
    );
    
    // Parameter automation suggestions
    struct AutomationSuggestions {
        std::unordered_map<std::string, std::vector<std::pair<float, float>>> automationCurves;
        std::string musicalReasoning;
        std::string technicalReasoning;
        std::vector<std::string> keyMoments; // Where automation is most important
        std::string styleReference;
    };
    
    mixmind::core::AsyncResult<AutomationSuggestions> suggestAutomation(
        const std::string& pluginName,
        const std::string& musicalContext,
        const std::string& genre,
        double songDurationSeconds
    );

private:
    RealOpenAIService& openAI_;
    std::unordered_map<std::string, std::string> pluginKnowledgeBase_;
    
    void initializePluginKnowledge();
    std::string formatPluginContext(const std::string& pluginName, 
                                  const std::string& manufacturer,
                                  const std::vector<std::string>& parameters);
};

} // namespace mixmind::services