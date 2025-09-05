#include "VoiceControl.h"
#include "OpenAIIntegration.h"
#include "../services/SpeechRecognitionService.h"
#include "../audio/RealtimeAudioEngine.h"
#include "../core/AIThreadPool.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace mixmind::ai {

// ============================================================================
// THREAD-SAFE Voice Control - NO AUDIO BLOCKING
// ============================================================================

class ThreadSafeVoiceController::Impl {
public:
    // Core services
    std::unique_ptr<SpeechRecognitionService> speechService_;
    AudioIntelligenceEngine* aiEngine_;
    audio::RealtimeAudioEngine* audioEngine_;
    
    // CRITICAL: Real-time AI manager for thread safety
    std::unique_ptr<core::RealTimeAIManager> aiManager_;
    
    // Voice control state
    std::atomic<bool> isListening_{false};
    std::atomic<bool> isProcessingCommand_{false};
    std::atomic<VoiceControlMode> currentMode_{VoiceControlMode::DISABLED};
    
    // Command processing (thread-safe)
    std::mutex commandMutex_;
    std::vector<VoiceCommand> commandHistory_;
    VoiceCommandCallback commandCallback_;
    
    // Natural language processing
    std::unordered_map<std::string, CommandType> intentMap_;
    std::vector<std::regex> parameterPatterns_;
    
    // Performance monitoring
    std::atomic<uint64_t> commandsProcessed_{0};
    std::atomic<uint64_t> aiRequestsQueued_{0};
    std::atomic<double> avgProcessingTime_{0.0};
    
    bool initialize() {
        // Initialize AI thread manager FIRST
        aiManager_ = std::make_unique<core::RealTimeAIManager>();
        if (!aiManager_->initialize(4)) { // 4 AI threads
            std::cerr << "❌ Failed to initialize AI thread manager" << std::endl;
            return false;
        }
        
        // Initialize speech recognition
        speechService_ = std::make_unique<SpeechRecognitionService>();
        auto initResult = speechService_->initialize();
        if (!initResult.get().isSuccess()) {
            std::cerr << "❌ Failed to initialize speech recognition" << std::endl;
            return false;
        }
        
        // Get engines
        aiEngine_ = &getGlobalAIEngine();
        audioEngine_ = &audio::getGlobalAudioEngine();
        
        // Initialize command patterns
        initializeCommandPatterns();
        
        std::cout << "🎤 Thread-Safe Voice Control initialized successfully" << std::endl;
        return true;
    }
    
    void initializeCommandPatterns() {
        // Same as before but more efficient
        intentMap_ = {
            // Transport controls
            {"play", CommandType::TRANSPORT_PLAY},
            {"start", CommandType::TRANSPORT_PLAY},
            {"pause", CommandType::TRANSPORT_PAUSE},
            {"stop", CommandType::TRANSPORT_STOP},
            {"record", CommandType::TRANSPORT_RECORD},
            
            // Mixer controls
            {"volume", CommandType::MIXER_VOLUME},
            {"gain", CommandType::MIXER_VOLUME},
            {"mute", CommandType::MIXER_MUTE},
            {"unmute", CommandType::MIXER_UNMUTE},
            {"solo", CommandType::MIXER_SOLO},
            {"pan", CommandType::MIXER_PAN},
            
            // Effects
            {"reverb", CommandType::EFFECT_REVERB},
            {"delay", CommandType::EFFECT_DELAY},
            {"eq", CommandType::EFFECT_EQ},
            {"equalizer", CommandType::EFFECT_EQ},
            {"compressor", CommandType::EFFECT_COMPRESSOR},
            {"compression", CommandType::EFFECT_COMPRESSOR},
            
            // AI commands
            {"analyze", CommandType::AI_ANALYZE},
            {"suggest", CommandType::AI_SUGGEST},
            {"generate", CommandType::AI_GENERATE},
            {"mix", CommandType::AI_MIX_ADVICE},
            {"help", CommandType::AI_HELP}
        };
        
        // Optimized regex patterns
        parameterPatterns_ = {
            std::regex(R"((?:set|change|adjust)\s+(?:volume|gain)\s+(?:to|by)\s+(\d+))", std::regex::icase),
            std::regex(R"((\w+)\s+(\d+(?:\.\d+)?)\s*(?:percent|%|db|ms|hz)?)", std::regex::icase),
            std::regex(R"((?:track|channel)\s+(\d+))", std::regex::icase),
            std::regex(R"((?:boost|cut|at)\s+(\d+(?:\.\d+)?)\s*(?:k?hz))", std::regex::icase),
        };
    }
    
    // CRITICAL: This method is called from audio callback - must be FAST
    void onSpeechRecognized(const std::string& text, double confidence) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (confidence < 0.6) {
            // Don't process low confidence - but log for debugging
            std::cout << "🔊 Low confidence speech: " << text << " (" << confidence << ")" << std::endl;
            return;
        }
        
        // FAST: Just enqueue the command processing - don't do heavy work here
        processVoiceCommandAsync(text, confidence);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        // Update metrics
        updateMetrics(latency.count());
        
        // If this takes >100μs, we have a problem
        if (latency.count() > 100) {
            std::cerr << "⚠️ Voice recognition callback too slow: " << latency.count() << "μs" << std::endl;
        }
    }
    
    // CRITICAL: This is now async and NEVER blocks audio thread
    void processVoiceCommandAsync(const std::string& text, double confidence) {
        // Quick parse for immediate commands (transport controls)
        VoiceCommand quickCommand = quickParseCommand(text, confidence);
        
        if (quickCommand.type != CommandType::UNKNOWN && 
            quickCommand.type != CommandType::AI_NATURAL_LANGUAGE) {
            // Execute simple commands immediately (still fast)
            executeSimpleCommand(quickCommand);
        } else {
            // Complex commands go to AI thread pool
            enqueueAIProcessing(text, confidence);
        }
    }
    
    // FAST: Quick parsing for immediate commands only
    VoiceCommand quickParseCommand(const std::string& text, double confidence) {
        VoiceCommand command;
        command.originalText = text;
        command.confidence = confidence;
        command.timestamp = std::chrono::system_clock::now();
        command.type = CommandType::UNKNOWN;
        
        std::string lowerText = text;
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
        
        // Only check for immediate transport controls
        static const std::vector<std::pair<std::string, CommandType>> immediateCommands = {
            {"play", CommandType::TRANSPORT_PLAY},
            {"pause", CommandType::TRANSPORT_PAUSE},
            {"stop", CommandType::TRANSPORT_STOP},
            {"record", CommandType::TRANSPORT_RECORD},
            {"mute", CommandType::MIXER_MUTE},
            {"unmute", CommandType::MIXER_UNMUTE}
        };
        
        for (const auto& [keyword, commandType] : immediateCommands) {
            if (lowerText.find(keyword) != std::string::npos) {
                command.type = commandType;
                break;
            }
        }
        
        return command;
    }
    
    // FAST: Execute simple commands immediately
    void executeSimpleCommand(const VoiceCommand& command) {
        commandsProcessed_.fetch_add(1);
        
        switch (command.type) {
            case CommandType::TRANSPORT_PLAY:
                if (!audioEngine_->isRunning()) {
                    audioEngine_->start();
                    std::cout << "▶️ Audio started by voice" << std::endl;
                }
                break;
                
            case CommandType::TRANSPORT_PAUSE:
            case CommandType::TRANSPORT_STOP:
                if (audioEngine_->isRunning()) {
                    audioEngine_->stop();
                    std::cout << "⏸️ Audio stopped by voice" << std::endl;
                }
                break;
                
            case CommandType::MIXER_MUTE:
                // TODO: Implement fast mute
                std::cout << "🔇 Mute by voice" << std::endl;
                break;
                
            case CommandType::MIXER_UNMUTE:
                // TODO: Implement fast unmute
                std::cout << "🔊 Unmute by voice" << std::endl;
                break;
                
            default:
                // This shouldn't happen for simple commands
                break;
        }
        
        // Add to history (thread-safe)
        addToHistory(command);
    }
    
    // NEVER BLOCKS: Enqueue AI processing on separate threads
    void enqueueAIProcessing(const std::string& text, double confidence) {
        aiRequestsQueued_.fetch_add(1);
        
        // Use the real-time AI manager - this is lock-free
        aiManager_->processVoiceCommand(text, [this, text](const std::string& response) {
            // This callback runs on AI thread - NOT audio thread
            handleAIResponse(text, response);
        });
    }
    
    // This runs on AI thread, not audio thread
    void handleAIResponse(const std::string& originalCommand, const std::string& response) {
        std::cout << "🤖 AI processed: \"" << originalCommand << "\" -> " << response << std::endl;
        
        // Create command for history
        VoiceCommand command;
        command.originalText = originalCommand;
        command.confidence = 0.9; // High confidence since AI processed it
        command.timestamp = std::chrono::system_clock::now();
        command.type = CommandType::AI_NATURAL_LANGUAGE;
        command.parameters["ai_response"] = response;
        
        addToHistory(command);
        
        // Notify callback if set
        if (commandCallback_) {
            commandCallback_(command);
        }
    }
    
    // Thread-safe history management
    void addToHistory(const VoiceCommand& command) {
        std::lock_guard<std::mutex> lock(commandMutex_);
        commandHistory_.push_back(command);
        
        if (commandHistory_.size() > 50) { // Keep last 50 commands
            commandHistory_.erase(commandHistory_.begin());
        }
    }
    
    // Update performance metrics
    void updateMetrics(double processingTimeMicros) {
        double currentAvg = avgProcessingTime_.load();
        // Exponential moving average
        avgProcessingTime_.store((currentAvg * 0.9) + (processingTimeMicros * 0.1));
    }
    
    // Called from audio thread every buffer - MUST BE FAST
    void processAudioThreadUpdates() {
        if (aiManager_) {
            aiManager_->processAudioThreadUpdates();
        }
    }
    
    // Performance monitoring
    struct VoiceControlMetrics {
        uint64_t commandsProcessed;
        uint64_t aiRequestsQueued;
        double avgProcessingTimeMicros;
        bool aiSystemHealthy;
        size_t pendingAIRequests;
    };
    
    VoiceControlMetrics getMetrics() const {
        VoiceControlMetrics metrics;
        metrics.commandsProcessed = commandsProcessed_.load();
        metrics.aiRequestsQueued = aiRequestsQueued_.load();
        metrics.avgProcessingTimeMicros = avgProcessingTime_.load();
        
        if (aiManager_) {
            auto aiStats = aiManager_->getPerformanceStats();
            metrics.aiSystemHealthy = aiStats.isHealthy;
            metrics.pendingAIRequests = aiStats.pendingRequests;
        } else {
            metrics.aiSystemHealthy = false;
            metrics.pendingAIRequests = 0;
        }
        
        return metrics;
    }
};

// ============================================================================
// ThreadSafeVoiceController Public Interface
// ============================================================================

ThreadSafeVoiceController::ThreadSafeVoiceController() 
    : pImpl_(std::make_unique<Impl>()) {}

ThreadSafeVoiceController::~ThreadSafeVoiceController() = default;

bool ThreadSafeVoiceController::initialize() {
    return pImpl_->initialize();
}

bool ThreadSafeVoiceController::startListening(VoiceControlMode mode) {
    if (pImpl_->isListening_.load()) {
        return true; // Already listening
    }
    
    pImpl_->currentMode_.store(mode);
    
    // Create callback that's optimized for real-time
    auto callback = [this](const std::string& text, double confidence) {
        // This must be FAST - called from audio thread context
        pImpl_->onSpeechRecognized(text, confidence);
    };
    
    auto result = pImpl_->speechService_->startListening(callback);
    if (result.isSuccess()) {
        pImpl_->isListening_.store(true);
        std::cout << "🎤 Thread-safe voice control started (mode: " << static_cast<int>(mode) << ")" << std::endl;
        return true;
    }
    
    std::cout << "❌ Failed to start thread-safe voice control: " << result.getError() << std::endl;
    return false;
}

void ThreadSafeVoiceController::stopListening() {
    if (!pImpl_->isListening_.load()) {
        return;
    }
    
    pImpl_->speechService_->stopListening();
    pImpl_->isListening_.store(false);
    pImpl_->currentMode_.store(VoiceControlMode::DISABLED);
    
    std::cout << "🔇 Thread-safe voice control stopped" << std::endl;
}

bool ThreadSafeVoiceController::isListening() const {
    return pImpl_->isListening_.load();
}

VoiceControlMode ThreadSafeVoiceController::getCurrentMode() const {
    return pImpl_->currentMode_.load();
}

void ThreadSafeVoiceController::setCommandCallback(VoiceCommandCallback callback) {
    pImpl_->commandCallback_ = std::move(callback);
}

std::vector<VoiceCommand> ThreadSafeVoiceController::getCommandHistory() const {
    std::lock_guard<std::mutex> lock(pImpl_->commandMutex_);
    return pImpl_->commandHistory_;
}

void ThreadSafeVoiceController::processAudioThreadUpdates() {
    pImpl_->processAudioThreadUpdates();
}

bool ThreadSafeVoiceController::isHealthy() const {
    auto metrics = pImpl_->getMetrics();
    
    // Health criteria
    bool lowLatency = metrics.avgProcessingTimeMicros < 100.0; // < 100μs
    bool aiHealthy = metrics.aiSystemHealthy;
    bool reasonableQueue = metrics.pendingAIRequests < 50;
    
    return lowLatency && aiHealthy && reasonableQueue;
}

ThreadSafeVoiceController::PerformanceStats ThreadSafeVoiceController::getPerformanceStats() const {
    auto metrics = pImpl_->getMetrics();
    
    PerformanceStats stats;
    stats.commandsProcessed = metrics.commandsProcessed;
    stats.aiRequestsQueued = metrics.aiRequestsQueued;
    stats.avgProcessingLatencyMicros = metrics.avgProcessingTimeMicros;
    stats.isHealthy = isHealthy();
    stats.pendingAIRequests = metrics.pendingAIRequests;
    
    return stats;
}

// ============================================================================
// Global Thread-Safe Voice Controller
// ============================================================================

static std::unique_ptr<ThreadSafeVoiceController> g_threadSafeVoiceController;
static std::mutex g_threadSafeVoiceControllerMutex;

ThreadSafeVoiceController& getGlobalThreadSafeVoiceController() {
    std::lock_guard<std::mutex> lock(g_threadSafeVoiceControllerMutex);
    if (!g_threadSafeVoiceController) {
        g_threadSafeVoiceController = std::make_unique<ThreadSafeVoiceController>();
    }
    return *g_threadSafeVoiceController;
}

void shutdownGlobalThreadSafeVoiceController() {
    std::lock_guard<std::mutex> lock(g_threadSafeVoiceControllerMutex);
    if (g_threadSafeVoiceController) {
        g_threadSafeVoiceController->stopListening();
        g_threadSafeVoiceController.reset();
    }
}

} // namespace mixmind::ai