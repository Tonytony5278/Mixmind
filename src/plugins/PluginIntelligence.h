#pragma once

#include "PluginHost.h"
#include "../ai/OpenAIIntegration.h"
#include "../core/result.h"
#include "../core/async.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace mixmind::plugins {

// ============================================================================
// Advanced Plugin Quality Analysis System
// ============================================================================

class PluginQualityAnalyzer {
public:
    // Comprehensive quality metrics
    struct QualityMetrics {
        float soundQuality = 0.0f;        // Audio fidelity and character (0.0-1.0)
        float cpuEfficiency = 0.0f;       // Performance optimization (0.0-1.0)
        float stability = 0.0f;           // Crash resistance and reliability (0.0-1.0)
        float userInterface = 0.0f;       // UI design and workflow (0.0-1.0)
        float documentation = 0.0f;       // Manual and help quality (0.0-1.0)
        float compatibility = 0.0f;       // Host compatibility (0.0-1.0)
        float updateFrequency = 0.0f;     // Developer support (0.0-1.0)
        float userSatisfaction = 0.0f;    // Community ratings (0.0-1.0)
        
        // Advanced metrics
        float latencyHandling = 0.0f;     // Low-latency performance
        float automationAccuracy = 0.0f; // Parameter automation precision
        float presetQuality = 0.0f;       // Factory preset usefulness
        float midiImplementation = 0.0f;  // MIDI feature completeness
        
        float overallScore = 0.0f;        // Weighted overall quality
    };
    
    // Analysis context for better accuracy
    struct AnalysisContext {
        std::string musicalGenre;         // "Rock", "Electronic", "Jazz", etc.
        std::string useCase;              // "Mixing", "Mastering", "Creative", etc.
        std::string userLevel;            // "Beginner", "Intermediate", "Professional"
        std::vector<std::string> requirements; // Specific needs
        float cpuBudget = 1.0f;           // Available CPU resources (0.0-1.0)
        bool needsLowLatency = false;     // Real-time performance critical
        bool needsAutomation = true;      // Parameter automation required
    };
    
    // AI-powered quality analysis
    core::AsyncResult<QualityMetrics> analyzePlugin(
        const PluginInfo& plugin,
        const AnalysisContext& context = {});
    
    // Comparative analysis
    struct ComparisonResult {
        std::string pluginA;
        std::string pluginB;
        QualityMetrics metricsA;
        QualityMetrics metricsB;
        std::vector<std::string> advantagesA;
        std::vector<std::string> advantagesB;
        std::string recommendation;
        float confidenceScore = 0.0f;
    };
    
    core::AsyncResult<ComparisonResult> comparePlugins(
        const PluginInfo& pluginA,
        const PluginInfo& pluginB,
        const AnalysisContext& context = {});
    
    // Real-time performance testing
    struct PerformanceTest {
        double averageLatency = 0.0;      // ms
        double cpuUsage = 0.0;            // percentage
        double memoryUsage = 0.0;         // MB
        int bufferUnderruns = 0;          // Audio dropouts
        bool passedStressTest = false;    // High-load stability
        std::vector<std::string> issues;  // Performance problems found
    };
    
    core::AsyncResult<PerformanceTest> performanceTest(
        std::shared_ptr<PluginInstance> plugin,
        int testDurationSeconds = 30);
    
    // Generate detailed analysis reports
    std::string generateQualityReport(const PluginInfo& plugin, const QualityMetrics& metrics);
    std::string generateComparisonReport(const ComparisonResult& comparison);
    std::string generatePerformanceReport(const PerformanceTest& test);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
public:
    PluginQualityAnalyzer();
    ~PluginQualityAnalyzer();
};

// ============================================================================
// Intelligent Plugin Recommendation System
// ============================================================================

class PluginRecommendationEngine {
public:
    // User profile for personalized recommendations
    struct UserProfile {
        std::string name;
        std::string skillLevel;               // "Beginner", "Intermediate", "Expert"
        std::vector<std::string> genres;      // Preferred musical genres
        std::vector<std::string> ownedPlugins; // Currently owned plugins
        std::unordered_map<std::string, float> pluginRatings; // User ratings (1-5)
        std::unordered_map<std::string, int> pluginUsage;     // Usage counts
        
        // Preferences
        float cpuEfficiencyWeight = 0.7f;     // How much CPU efficiency matters
        float soundQualityWeight = 0.9f;      // How much sound quality matters
        float priceWeight = 0.5f;             // How much price matters
        bool preferFreePlugins = false;      // Prefer free alternatives
        bool preferVintageSound = false;     // Prefer vintage character
        bool preferModernSound = true;       // Prefer clean, modern sound
    };
    
    // Recommendation request
    struct RecommendationRequest {
        UserProfile userProfile;
        PluginCategory category;
        std::string specificNeed;             // "Vocal compression", "Guitar amp", etc.
        float budgetLimit = 1000.0f;          // Maximum price
        int maxRecommendations = 5;
        bool includeAlternatives = true;      // Include similar/alternative plugins
        std::string musicalContext;           // Current project context
    };
    
    // Recommendation result
    struct Recommendation {
        PluginInfo plugin;
        float relevanceScore = 0.0f;          // How well it fits the request (0.0-1.0)
        float qualityScore = 0.0f;            // Quality assessment (0.0-1.0)
        float valueScore = 0.0f;              // Value for money (0.0-1.0)
        std::vector<std::string> reasons;     // Why recommended
        std::vector<std::string> warnings;    // Potential concerns
        std::string usageAdvice;              // How to best use this plugin
        
        // Compatibility with user's setup
        float compatibilityScore = 0.0f;
        std::vector<std::string> compatibilityNotes;
    };
    
    // Generate personalized recommendations
    core::AsyncResult<std::vector<Recommendation>> getRecommendations(
        const RecommendationRequest& request);
    
    // Style-specific recommendations
    core::AsyncResult<std::vector<Recommendation>> getStyleRecommendations(
        const std::string& musicalStyle,
        const UserProfile& profile);
    
    // Workflow-based recommendations
    struct WorkflowRecommendation {
        std::string workflowName;             // "Vocal Chain", "Master Bus", etc.
        std::vector<Recommendation> chain;     // Ordered plugin chain
        std::string description;
        std::vector<std::string> usageTips;
        float workflowScore = 0.0f;           // Overall workflow quality
    };
    
    core::AsyncResult<std::vector<WorkflowRecommendation>> getWorkflowRecommendations(
        const std::string& workflowType,
        const UserProfile& profile);
    
    // Learning system
    void recordPluginUsage(const std::string& pluginUid, int sessionDurationMinutes);
    void recordPluginRating(const std::string& pluginUid, float rating);
    void recordPluginPurchase(const std::string& pluginUid, float price);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
public:
    PluginRecommendationEngine();
    ~PluginRecommendationEngine();
};

// ============================================================================
// Real-time Tone Modification and Parameter Automation
// ============================================================================

class ToneModificationEngine {
public:
    // Tone characteristics that can be modified
    struct ToneProfile {
        float warmth = 0.5f;          // Cold to warm character (0.0-1.0)
        float brightness = 0.5f;      // Dark to bright frequency balance
        float punch = 0.5f;           // Soft to punchy transients
        float width = 0.5f;           // Narrow to wide stereo image
        float depth = 0.5f;           // Flat to deep spatial dimension
        float saturation = 0.0f;      // Clean to saturated harmonic content
        float compression = 0.0f;     // Dynamic range control
        float character = 0.5f;       // Neutral to colored sound signature
        
        std::string description;      // Human-readable description
        std::vector<std::string> tags; // Descriptive tags
    };
    
    // Target tone specification
    struct ToneTarget {
        std::string styleName;        // "Nirvana Guitar", "Smooth Jazz Vocal", etc.
        ToneProfile profile;
        std::vector<std::string> referenceAudio; // Reference track paths
        std::string instructions;     // Natural language instructions
        float intensity = 1.0f;       // How strongly to apply the transformation
    };
    
    // Tone transformation result
    struct ToneTransformation {
        ToneProfile sourceTone;
        ToneProfile targetTone;
        std::vector<PluginSlot> suggestedChain;    // Recommended plugin chain
        std::unordered_map<std::string, float> parameterMap; // Plugin parameter values
        std::string analysis;         // AI analysis of the transformation
        float confidenceScore = 0.0f; // Confidence in achieving target
    };
    
    // Main tone modification interface
    core::AsyncResult<ToneTransformation> createToneTransformation(
        const std::string& sourceDescription,
        const ToneTarget& target,
        const std::vector<PluginInfo>& availablePlugins);
    
    // Real-time tone morphing
    class ToneMorpher {
    public:
        void setSourceTone(const ToneProfile& source);
        void setTargetTone(const ToneProfile& target);
        void setMorphProgress(float progress); // 0.0 = source, 1.0 = target
        
        // Apply morphing to plugin chain
        void morphPluginChain(PluginChain& chain, float progress);
        
        // Smooth automation curves
        void enableSmoothing(bool enable, float smoothingTime = 0.1f);
        
    private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    public:
        ToneMorpher();
        ~ToneMorpher();
    };
    
    // Analyze existing audio to extract tone profile
    core::AsyncResult<ToneProfile> analyzeTone(const std::string& audioDescription);
    
    // Generate tone profile from natural language description
    core::AsyncResult<ToneProfile> generateToneFromDescription(const std::string& description);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
public:
    ToneModificationEngine();
    ~ToneModificationEngine();
};

// ============================================================================
// Intelligent Parameter Automation System
// ============================================================================

class SmartAutomationEngine {
public:
    // Automation curve types
    enum class CurveType {
        LINEAR,
        EXPONENTIAL,
        LOGARITHMIC,
        S_CURVE,
        BOUNCE,
        ELASTIC,
        CUSTOM
    };
    
    // Automation point with timing and value
    struct AutomationPoint {
        double timestamp = 0.0;       // Time in seconds
        float value = 0.0f;           // Parameter value (0.0-1.0)
        CurveType curveToNext = CurveType::LINEAR;
        float tension = 0.5f;         // Curve tension/sharpness
    };
    
    // Parameter automation track
    struct AutomationTrack {
        std::string pluginUid;
        std::string parameterId;
        std::string parameterName;
        std::vector<AutomationPoint> points;
        bool isEnabled = true;
        bool isRecording = false;
        float recordingThreshold = 0.01f; // Minimum change to record
        
        // AI enhancements
        std::string aiSuggestions;    // AI automation suggestions
        bool aiOptimizationEnabled = true;
    };
    
    // Musical automation patterns
    struct MusicalPattern {
        std::string name;             // "Build Up", "Drop", "Verse Dynamics", etc.
        std::string description;
        PluginCategory applicableCategory;
        std::vector<AutomationPoint> template;
        std::function<void(AutomationTrack&, double, double)> generator;
    };
    
    // Create automation from musical patterns
    AutomationTrack createMusicalAutomation(
        const std::string& pluginUid,
        const std::string& parameterId,
        const MusicalPattern& pattern,
        double startTime,
        double duration);
    
    // AI-generated automation
    core::AsyncResult<AutomationTrack> generateSmartAutomation(
        const std::string& pluginUid,
        const std::string& parameterId,
        const std::string& musicalContext,  // "build tension", "create movement", etc.
        double startTime,
        double duration);
    
    // Learn from user automation
    void recordUserAutomation(const AutomationTrack& track);
    std::vector<MusicalPattern> learnPatternsFromUser();
    
    // Automation optimization
    struct OptimizationResult {
        AutomationTrack optimizedTrack;
        int pointsReduced = 0;        // How many points were optimized away
        float curveAccuracy = 0.0f;   // How close to original curve
        std::string optimizationNotes;
    };
    
    OptimizationResult optimizeAutomationTrack(const AutomationTrack& track);
    
    // Real-time automation playback
    class AutomationPlayer {
    public:
        void loadTrack(const AutomationTrack& track);
        float getValueAtTime(double timestamp);
        void setPlaybackPosition(double timestamp);
        bool isActive() const;
        
    private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    public:
        AutomationPlayer();
        ~AutomationPlayer();
    };
    
private:
    std::vector<MusicalPattern> builtinPatterns_;
    std::vector<MusicalPattern> learnedPatterns_;
    
    class Impl;
    std::unique_ptr<Impl> pImpl_;
public:
    SmartAutomationEngine();
    ~SmartAutomationEngine();
};

// ============================================================================
// Plugin Chain Optimization and Intelligent Routing
// ============================================================================

class PluginChainOptimizer {
public:
    // Chain optimization goals
    enum class OptimizationGoal {
        MINIMIZE_CPU,              // Reduce CPU usage
        MINIMIZE_LATENCY,          // Reduce processing latency
        MAXIMIZE_QUALITY,          // Best possible sound quality
        MAXIMIZE_CREATIVITY,       // Most creative possibilities
        BALANCE_ALL               // Balanced optimization
    };
    
    // Chain analysis results
    struct ChainAnalysis {
        float totalCpuUsage = 0.0f;
        int totalLatencySamples = 0;
        int redundantPlugins = 0;
        std::vector<std::string> bottlenecks;
        std::vector<std::string> optimizationOpportunities;
        float overallEfficiency = 0.0f;
        
        // Quality metrics
        float signalToNoiseRatio = 0.0f;
        float dynamicRange = 0.0f;
        float frequencyResponse = 0.0f;
        
        std::string aiAssessment;
    };
    
    // Analyze existing plugin chain
    core::AsyncResult<ChainAnalysis> analyzeChain(const PluginChain& chain);
    
    // Optimization suggestions
    struct OptimizationSuggestion {
        std::string description;
        std::string category;          // "Performance", "Quality", "Workflow"
        float expectedImprovement = 0.0f; // Expected benefit (0.0-1.0)
        float implementationDifficulty = 0.0f; // How hard to implement
        std::function<void(PluginChain&)> applyOptimization;
    };
    
    std::vector<OptimizationSuggestion> generateOptimizations(
        const PluginChain& chain,
        OptimizationGoal goal);
    
    // Automatic chain optimization
    PluginChain optimizeChain(
        const PluginChain& originalChain,
        OptimizationGoal goal,
        float aggressiveness = 0.5f);
    
    // Intelligent plugin routing
    struct RoutingConfig {
        bool enableParallelProcessing = false;
        bool enableSidechainRouting = false;
        bool enableSendReturns = false;
        int maxParallelChains = 4;
        float cpuLoadBalancing = 0.7f;
    };
    
    PluginChain createOptimalRouting(
        const std::vector<PluginInfo>& desiredPlugins,
        const RoutingConfig& config);
    
    // A/B testing for optimization validation
    struct ABTest {
        std::string testName;
        PluginChain chainA;
        PluginChain chainB;
        std::vector<std::string> metrics; // What to compare
        std::string winner;            // "A", "B", or "Draw"
        float confidenceLevel = 0.0f;
        std::string analysis;
    };
    
    core::AsyncResult<ABTest> performABTest(
        const PluginChain& chainA,
        const PluginChain& chainB,
        const std::vector<std::string>& testMetrics);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
public:
    PluginChainOptimizer();
    ~PluginChainOptimizer();
};

// ============================================================================
// Global Plugin Intelligence System
// ============================================================================

class PluginIntelligenceSystem {
public:
    static PluginIntelligenceSystem& getInstance();
    
    // Initialize all subsystems
    bool initialize();
    void shutdown();
    
    // Access to subsystems
    PluginQualityAnalyzer& getQualityAnalyzer();
    PluginRecommendationEngine& getRecommendationEngine();
    ToneModificationEngine& getToneEngine();
    SmartAutomationEngine& getAutomationEngine();
    PluginChainOptimizer& getChainOptimizer();
    
    // Integrated workflows
    struct IntelligentWorkflow {
        std::string name;
        std::string description;
        std::function<core::AsyncResult<PluginChain>(const std::string&)> execute;
    };
    
    // Register custom workflows
    void registerWorkflow(const IntelligentWorkflow& workflow);
    std::vector<std::string> getAvailableWorkflows() const;
    
    // Execute intelligent workflow
    core::AsyncResult<PluginChain> executeWorkflow(
        const std::string& workflowName,
        const std::string& parameters);
    
private:
    PluginIntelligenceSystem() = default;
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace mixmind::plugins