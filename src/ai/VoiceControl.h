#pragma once

#include "../core/result.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>

namespace mixmind::ai {

// ============================================================================
// Voice Control Types and Enums
// ============================================================================

enum class VoiceControlMode {
    DISABLED,           // Voice control off
    PUSH_TO_TALK,      // Only listen when key/button held
    CONTINUOUS,        // Always listening
    KEYWORD_ACTIVATION // Listen for wake word, then continuous
};

enum class CommandType {
    UNKNOWN,
    
    // Transport controls
    TRANSPORT_PLAY,
    TRANSPORT_PAUSE,
    TRANSPORT_STOP,
    TRANSPORT_RECORD,
    TRANSPORT_REWIND,
    TRANSPORT_FAST_FORWARD,
    
    // Mixer controls
    MIXER_VOLUME,
    MIXER_MUTE,
    MIXER_UNMUTE,
    MIXER_SOLO,
    MIXER_PAN,
    MIXER_SELECT_TRACK,
    
    // Effects
    EFFECT_BYPASS,
    EFFECT_ENABLE,
    EFFECT_REVERB,
    EFFECT_DELAY,
    EFFECT_EQ,
    EFFECT_COMPRESSOR,
    EFFECT_DISTORTION,
    
    // Navigation
    NAV_ZOOM_IN,
    NAV_ZOOM_OUT,
    NAV_GO_TO_TIME,
    NAV_SELECT_REGION,
    
    // AI-powered commands
    AI_ANALYZE,
    AI_SUGGEST,
    AI_GENERATE,
    AI_MIX_ADVICE,
    AI_HELP,
    AI_NATURAL_LANGUAGE, // Complex AI interpretation needed
    
    // File operations
    FILE_SAVE,
    FILE_LOAD,
    FILE_EXPORT,
    
    // Session management
    SESSION_NEW,
    SESSION_UNDO,
    SESSION_REDO
};

struct VoiceCommand {
    std::string originalText;                           // Raw speech text
    CommandType type = CommandType::UNKNOWN;            // Parsed command type
    std::unordered_map<std::string, std::string> parameters; // Extracted parameters
    double confidence = 0.0;                           // Speech recognition confidence
    std::chrono::system_clock::time_point timestamp;   // When command was issued
    bool executed = false;                             // Whether command was executed
    std::string executionResult;                       // Result/feedback from execution
};

using VoiceCommandCallback = std::function<void(const VoiceCommand& command)>;

// ============================================================================
// Voice Controller - Natural Language DAW Control
// ============================================================================

class VoiceController {
public:
    VoiceController();
    ~VoiceController();
    
    // Non-copyable
    VoiceController(const VoiceController&) = delete;
    VoiceController& operator=(const VoiceController&) = delete;
    
    // Initialize voice control system
    bool initialize();
    
    // Voice control lifecycle
    bool startListening(VoiceControlMode mode = VoiceControlMode::CONTINUOUS);
    void stopListening();
    bool isListening() const;
    
    // Configuration
    VoiceControlMode getCurrentMode() const;
    void setConfidenceThreshold(double threshold);
    void setLanguage(const std::string& languageCode);
    
    // Command handling
    void setCommandCallback(VoiceCommandCallback callback);
    std::vector<VoiceCommand> getCommandHistory() const;
    void clearCommandHistory();
    
    // Manual command processing (for testing)
    void processTextCommand(const std::string& text);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Voice Command Examples and Templates
// ============================================================================

namespace examples {
    // Transport commands
    constexpr const char* TRANSPORT_EXAMPLES[] = {
        "play", "start playback", "begin",
        "pause", "stop", "halt",
        "record", "start recording",
        "rewind", "go to beginning",
        "fast forward", "skip ahead"
    };
    
    // Mixer commands
    constexpr const char* MIXER_EXAMPLES[] = {
        "set volume to 75", "increase volume by 10",
        "mute track 3", "unmute bass",
        "solo drums", "pan left 50",
        "select track 2", "switch to vocals"
    };
    
    // Effects commands
    constexpr const char* EFFECT_EXAMPLES[] = {
        "add reverb", "remove delay",
        "bypass compressor", "enable EQ",
        "boost 2kHz by 3dB", "cut low frequencies",
        "increase attack time", "reduce threshold"
    };
    
    // AI commands
    constexpr const char* AI_EXAMPLES[] = {
        "analyze this track", "suggest improvements",
        "how can I make this sound better?",
        "what's wrong with the mix?",
        "generate a chord progression in C major",
        "help me with the vocals",
        "make it sound more modern"
    };
    
    // Navigation commands
    constexpr const char* NAVIGATION_EXAMPLES[] = {
        "zoom in", "zoom out",
        "go to 2 minutes 30 seconds",
        "select from 1:15 to 2:45",
        "show full timeline"
    };
}

// ============================================================================
// Voice Command Utilities
// ============================================================================

namespace utils {
    // Convert command type to human readable string
    std::string commandTypeToString(CommandType type);
    
    // Parse time expressions ("2 minutes 30 seconds", "1:45", etc.)
    double parseTimeExpression(const std::string& timeStr);
    
    // Extract track references ("track 3", "bass track", "drums")
    std::string extractTrackReference(const std::string& text);
    
    // Convert natural language to parameter values
    float parseParameterValue(const std::string& text, const std::string& parameterType);
    
    // Validate voice command for safety
    bool isCommandSafe(const VoiceCommand& command);
    
    // Generate help text for available commands
    std::string generateHelpText(VoiceControlMode mode);
}

// ============================================================================
// Voice Control Configuration
// ============================================================================

struct VoiceControlConfig {
    VoiceControlMode mode = VoiceControlMode::CONTINUOUS;
    double confidenceThreshold = 0.7;
    std::string language = "en-US";
    bool enableAIProcessing = true;
    bool logCommands = true;
    int commandHistorySize = 100;
    
    // Wake word settings (for keyword activation mode)
    std::string wakeWord = "mixmind";
    double wakeWordThreshold = 0.8;
    
    // Safety settings
    bool requireConfirmationForDestructive = true;
    std::vector<std::string> blockedCommands;
};

// ============================================================================
// Global Voice Controller Access
// ============================================================================

// Get the global voice controller (singleton)
VoiceController& getGlobalVoiceController();

// Shutdown voice controller (call at app exit)
void shutdownGlobalVoiceController();

} // namespace mixmind::ai