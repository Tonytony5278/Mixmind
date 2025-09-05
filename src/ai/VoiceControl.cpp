#include "VoiceControl.h"
#include "OpenAIIntegration.h"
#include "../services/SpeechRecognitionService.h"
#include "../audio/RealtimeAudioEngine.h"
#include "../core/async.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace mixmind::ai {

// ============================================================================
// Voice Command Parsing and Execution
// ============================================================================

class VoiceController::Impl {
public:
    // Core services
    std::unique_ptr<SpeechRecognitionService> speechService_;
    AudioIntelligenceEngine* aiEngine_;
    audio::RealtimeAudioEngine* audioEngine_;
    
    // Voice control state
    std::atomic<bool> isListening_{false};
    std::atomic<bool> isProcessingCommand_{false};
    std::atomic<VoiceControlMode> currentMode_{VoiceControlMode::DISABLED};
    
    // Command processing
    std::mutex commandMutex_;
    std::vector<VoiceCommand> commandHistory_;
    VoiceCommandCallback commandCallback_;
    
    // Natural language processing
    std::unordered_map<std::string, CommandType> intentMap_;
    std::vector<std::regex> parameterPatterns_;
    
    bool initialize() {
        // Initialize speech recognition
        speechService_ = std::make_unique<SpeechRecognitionService>();
        auto initResult = speechService_->initialize();
        if (!initResult.get().isSuccess()) {
            std::cerr << "❌ Failed to initialize speech recognition" << std::endl;
            return false;
        }
        
        // Get AI and audio engines
        aiEngine_ = &getGlobalAIEngine();
        audioEngine_ = &audio::getGlobalAudioEngine();
        
        // Initialize command patterns
        initializeCommandPatterns();
        
        std::cout << "🎤 Voice Control initialized successfully" << std::endl;
        return true;
    }
    
    void initializeCommandPatterns() {
        // Map natural language intents to command types
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
        
        // Regular expressions for parameter extraction
        parameterPatterns_ = {
            // Volume/gain: "set volume to 75", "increase volume by 10"
            std::regex(R"((?:set|change|adjust)\s+(?:volume|gain)\s+(?:to|by)\s+(\d+))", std::regex::icase),
            
            // Numeric parameters: "reverb 30", "delay 250ms"
            std::regex(R"((\w+)\s+(\d+(?:\.\d+)?)\s*(?:percent|%|db|ms|hz)?)", std::regex::icase),
            
            // Track selection: "track 3", "channel 2"
            std::regex(R"((?:track|channel)\s+(\d+))", std::regex::icase),
            
            // Frequency: "boost 2khz", "cut 500hz"
            std::regex(R"((?:boost|cut|at)\s+(\d+(?:\.\d+)?)\s*(?:k?hz))", std::regex::icase),
        };
    }
    
    void onSpeechRecognized(const std::string& text, double confidence) {
        if (confidence < 0.6) {
            std::cout << "🔊 Low confidence speech: " << text << " (" << confidence << ")" << std::endl;
            return;
        }
        
        std::cout << "🎤 Voice command: \"" << text << "\" (confidence: " << confidence << ")" << std::endl;
        
        // Process the voice command
        processVoiceCommand(text, confidence);
    }
    
    void processVoiceCommand(const std::string& text, double confidence) {
        if (isProcessingCommand_.load()) {
            std::cout << "⏳ Previous command still processing, ignoring: " << text << std::endl;
            return;
        }
        
        isProcessingCommand_.store(true);
        
        // Parse the voice command
        core::executeAsyncGlobal<VoiceCommand>([this, text, confidence]() -> core::Result<VoiceCommand> {
            VoiceCommand command = parseVoiceCommand(text, confidence);
            
            // Execute the command
            executeVoiceCommand(command);
            
            // Add to history
            {
                std::lock_guard<std::mutex> lock(commandMutex_);
                commandHistory_.push_back(command);
                if (commandHistory_.size() > 50) { // Keep last 50 commands
                    commandHistory_.erase(commandHistory_.begin());
                }
            }
            
            // Notify callback
            if (commandCallback_) {
                commandCallback_(command);
            }
            
            isProcessingCommand_.store(false);
            return core::Result<VoiceCommand>::success(command);
        });
    }
    
    VoiceCommand parseVoiceCommand(const std::string& text, double confidence) {
        VoiceCommand command;
        command.originalText = text;
        command.confidence = confidence;
        command.timestamp = std::chrono::system_clock::now();
        command.type = CommandType::UNKNOWN;
        
        std::string lowerText = text;
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
        
        // Check for AI-assisted commands that need natural language processing
        if (containsAITriggers(lowerText)) {
            command.type = CommandType::AI_NATURAL_LANGUAGE;
            command.parameters["query"] = text;
            return command;
        }
        
        // Parse structured commands
        for (const auto& [keyword, commandType] : intentMap_) {
            if (lowerText.find(keyword) != std::string::npos) {
                command.type = commandType;
                break;
            }
        }
        
        // Extract parameters using regex patterns
        extractParameters(lowerText, command);
        
        return command;
    }
    
    bool containsAITriggers(const std::string& text) {
        std::vector<std::string> aiTriggers = {
            "how do i", "what should", "can you", "help me",
            "suggest", "analyze this", "make it sound",
            "improve", "fix the", "better"
        };
        
        for (const auto& trigger : aiTriggers) {
            if (text.find(trigger) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    void extractParameters(const std::string& text, VoiceCommand& command) {
        std::smatch match;
        
        for (const auto& pattern : parameterPatterns_) {
            if (std::regex_search(text, match, pattern)) {
                if (match.size() >= 2) {
                    // Extract numeric values
                    try {
                        float value = std::stof(match[1].str());
                        command.parameters["value"] = std::to_string(value);
                    } catch (...) {
                        command.parameters["text"] = match[1].str();
                    }
                }
                break;
            }
        }
        
        // Extract track/channel numbers
        std::regex trackPattern(R"((?:track|channel)\s+(\d+))", std::regex::icase);
        if (std::regex_search(text, match, trackPattern)) {
            command.parameters["track"] = match[1].str();
        }
    }
    
    void executeVoiceCommand(const VoiceCommand& command) {
        std::cout << "⚡ Executing command type: " << static_cast<int>(command.type) << std::endl;
        
        switch (command.type) {
            case CommandType::TRANSPORT_PLAY:
                executeTransportCommand("play", command);
                break;
            case CommandType::TRANSPORT_PAUSE:
                executeTransportCommand("pause", command);
                break;
            case CommandType::TRANSPORT_STOP:
                executeTransportCommand("stop", command);
                break;
            case CommandType::TRANSPORT_RECORD:
                executeTransportCommand("record", command);
                break;
                
            case CommandType::MIXER_VOLUME:
                executeMixerCommand("volume", command);
                break;
            case CommandType::MIXER_MUTE:
                executeMixerCommand("mute", command);
                break;
            case CommandType::MIXER_UNMUTE:
                executeMixerCommand("unmute", command);
                break;
                
            case CommandType::AI_ANALYZE:
                executeAICommand("analyze", command);
                break;
            case CommandType::AI_SUGGEST:
                executeAICommand("suggest", command);
                break;
            case CommandType::AI_NATURAL_LANGUAGE:
                executeAINaturalLanguageCommand(command);
                break;
                
            default:
                std::cout << "❓ Unknown command: " << command.originalText << std::endl;
                break;
        }
    }
    
    void executeTransportCommand(const std::string& action, const VoiceCommand& command) {
        std::cout << "🎵 Transport: " << action << std::endl;
        
        // In a full implementation, this would control the DAW transport
        // For now, we'll just log and potentially control audio engine state
        
        if (action == "play" || action == "start") {
            if (!audioEngine_->isRunning()) {
                audioEngine_->start();
                std::cout << "▶️ Audio engine started" << std::endl;
            }
        } else if (action == "pause" || action == "stop") {
            if (audioEngine_->isRunning()) {
                audioEngine_->stop();
                std::cout << "⏸️ Audio engine stopped" << std::endl;
            }
        }
    }
    
    void executeMixerCommand(const std::string& action, const VoiceCommand& command) {
        std::cout << "🎛️ Mixer: " << action;
        
        auto trackIt = command.parameters.find("track");
        if (trackIt != command.parameters.end()) {
            std::cout << " on track " << trackIt->second;
        }
        
        auto valueIt = command.parameters.find("value");
        if (valueIt != command.parameters.end()) {
            std::cout << " value: " << valueIt->second;
            
            // Example: Set gain processor parameter
            try {
                float value = std::stof(valueIt->second);
                if (action == "volume") {
                    // Convert percentage to linear gain
                    float gain = value / 100.0f;
                    audioEngine_->setParameter(0, 0, gain); // Processor 0, param 0 (gain)
                }
            } catch (...) {
                std::cout << " (invalid value)";
            }
        }
        
        std::cout << std::endl;
    }
    
    void executeAICommand(const std::string& action, const VoiceCommand& command) {
        std::cout << "🤖 AI: " << action << std::endl;
        
        if (action == "analyze") {
            // Trigger AI analysis of current audio
            AudioAnalysisContext context;
            context.genre = "Unknown";
            context.duration = 180.0; // Example duration
            
            auto analysisResult = aiEngine_->analyzeAudioContent("Current audio project", context);
            
            // Handle result asynchronously
            core::executeAsyncGlobal<core::Result<void>>([analysisResult]() -> core::Result<void> {
                auto result = analysisResult.get();
                if (result.isSuccess()) {
                    std::cout << "🎯 AI Analysis: " << result.getValue().content << std::endl;
                } else {
                    std::cout << "❌ AI Analysis failed: " << result.getError() << std::endl;
                }
                return core::Result<void>::success();
            });
        }
    }
    
    void executeAINaturalLanguageCommand(const VoiceCommand& command) {
        std::cout << "🧠 Processing natural language: " << command.originalText << std::endl;
        
        // Send to AI for interpretation and action
        ChatRequest chatRequest;
        chatRequest.model = "gpt-4";
        chatRequest.temperature = 0.3;
        
        ChatMessage systemMsg;
        systemMsg.role = "system";
        systemMsg.content = R"(
You are MixMind AI voice assistant. Users speak natural language commands for music production tasks.
Interpret the command and respond with specific, actionable instructions.
Keep responses concise and practical for audio engineers.
)";
        
        ChatMessage userMsg;
        userMsg.role = "user";
        userMsg.content = "Voice command: \"" + command.originalText + "\"";
        
        chatRequest.messages = {systemMsg, userMsg};
        
        auto aiResponse = aiEngine_->getGlobalAIEngine().sendChatRequest(chatRequest);
        
        // Handle AI response asynchronously
        core::executeAsyncGlobal<core::Result<void>>([aiResponse]() -> core::Result<void> {
            auto result = aiResponse.get();
            if (result.isSuccess()) {
                std::cout << "💬 AI Response: " << result.getValue().content << std::endl;
            }
            return core::Result<void>::success();
        });
    }
};

// ============================================================================
// VoiceController Public Interface
// ============================================================================

VoiceController::VoiceController() : pImpl_(std::make_unique<Impl>()) {}
VoiceController::~VoiceController() = default;

bool VoiceController::initialize() {
    return pImpl_->initialize();
}

bool VoiceController::startListening(VoiceControlMode mode) {
    if (pImpl_->isListening_.load()) {
        return true; // Already listening
    }
    
    pImpl_->currentMode_.store(mode);
    
    auto callback = [this](const std::string& text, double confidence) {
        pImpl_->onSpeechRecognized(text, confidence);
    };
    
    auto result = pImpl_->speechService_->startListening(callback);
    if (result.isSuccess()) {
        pImpl_->isListening_.store(true);
        std::cout << "🎤 Voice control started (mode: " << static_cast<int>(mode) << ")" << std::endl;
        return true;
    }
    
    std::cout << "❌ Failed to start voice control: " << result.getError() << std::endl;
    return false;
}

void VoiceController::stopListening() {
    if (!pImpl_->isListening_.load()) {
        return;
    }
    
    pImpl_->speechService_->stopListening();
    pImpl_->isListening_.store(false);
    pImpl_->currentMode_.store(VoiceControlMode::DISABLED);
    
    std::cout << "🔇 Voice control stopped" << std::endl;
}

bool VoiceController::isListening() const {
    return pImpl_->isListening_.load();
}

VoiceControlMode VoiceController::getCurrentMode() const {
    return pImpl_->currentMode_.load();
}

void VoiceController::setCommandCallback(VoiceCommandCallback callback) {
    pImpl_->commandCallback_ = std::move(callback);
}

std::vector<VoiceCommand> VoiceController::getCommandHistory() const {
    std::lock_guard<std::mutex> lock(pImpl_->commandMutex_);
    return pImpl_->commandHistory_;
}

// ============================================================================
// Global Voice Controller
// ============================================================================

static std::unique_ptr<VoiceController> g_voiceController;
static std::mutex g_voiceControllerMutex;

VoiceController& getGlobalVoiceController() {
    std::lock_guard<std::mutex> lock(g_voiceControllerMutex);
    if (!g_voiceController) {
        g_voiceController = std::make_unique<VoiceController>();
    }
    return *g_voiceController;
}

void shutdownGlobalVoiceController() {
    std::lock_guard<std::mutex> lock(g_voiceControllerMutex);
    if (g_voiceController) {
        g_voiceController->stopListening();
        g_voiceController.reset();
    }
}

} // namespace mixmind::ai