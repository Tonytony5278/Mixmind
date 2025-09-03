#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include "../api/ActionAPI.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>

// Forward declarations for speech recognition libraries
struct WhisperContext;
struct WhisperState;

namespace mixmind::ai {

// ============================================================================
// Voice Controller - Advanced voice recognition and natural language processing
// ============================================================================

class VoiceController {
public:
    explicit VoiceController(std::shared_ptr<api::ActionAPI> actionAPI);
    ~VoiceController();
    
    // Non-copyable, movable
    VoiceController(const VoiceController&) = delete;
    VoiceController& operator=(const VoiceController&) = delete;
    VoiceController(VoiceController&&) = default;
    VoiceController& operator=(VoiceController&&) = default;
    
    // ========================================================================
    // Voice Recognition Configuration
    // ========================================================================
    
    /// Speech recognition engines
    enum class SpeechEngine {
        Whisper,        // OpenAI Whisper (offline)
        Azure,          // Azure Speech Services (online)
        Google,         // Google Speech-to-Text (online)
        WebSpeech       // Browser Web Speech API (online)
    };
    
    /// Voice recognition settings
    struct VoiceSettings {
        SpeechEngine engine = SpeechEngine::Whisper;
        std::string language = "en";
        std::string modelPath = "./models/whisper-base.bin";
        float confidenceThreshold = 0.7f;
        bool continuousListening = true;
        bool pushToTalk = false;
        std::string pushToTalkKey = "Space";
        float noiseGate = -40.0f;  // dB
        int32_t silenceTimeout = 2000;  // ms
        bool enableVoiceCommands = true;
        bool enableNaturalLanguage = true;
        std::vector<std::string> wakeWords = {"hey mixmind", "mixmind"};
    };
    
    /// Initialize voice controller
    core::AsyncResult<core::VoidResult> initialize(const VoiceSettings& settings);
    
    /// Shutdown voice controller
    core::AsyncResult<core::VoidResult> shutdown();
    
    /// Check if voice controller is active
    bool isActive() const;
    
    /// Update voice settings
    core::VoidResult updateSettings(const VoiceSettings& settings);
    
    /// Get current settings
    VoiceSettings getSettings() const;
    
    // ========================================================================
    // Voice Recognition Control
    // ========================================================================
    
    /// Start voice recognition
    core::AsyncResult<core::VoidResult> startListening();
    
    /// Stop voice recognition
    core::AsyncResult<core::VoidResult> stopListening();
    
    /// Toggle listening state
    core::AsyncResult<core::VoidResult> toggleListening();
    
    /// Check if currently listening
    bool isListening() const;
    
    /// Process audio buffer for speech recognition
    core::VoidResult processAudioBuffer(const core::FloatAudioBuffer& buffer, core::SampleRate sampleRate);
    
    /// Process text input (for testing or text-based input)
    core::AsyncResult<api::ActionResult> processTextCommand(const std::string& text);
    
    // ========================================================================
    // Natural Language Processing
    // ========================================================================
    
    /// Command interpretation results
    struct InterpretationResult {
        bool successful = false;
        std::string originalText;
        std::string interpretedAction;
        nlohmann::json actionParameters;
        float confidence = 0.0f;
        std::vector<std::string> alternativeInterpretations;
        std::string explanation;
    };
    
    /// Interpret natural language command
    core::AsyncResult<core::Result<InterpretationResult>> interpretCommand(const std::string& text);
    
    /// Add custom voice command pattern
    core::VoidResult addVoiceCommand(
        const std::string& pattern,
        const std::string& actionName,
        const std::function<nlohmann::json(const std::vector<std::string>&)>& parameterExtractor = nullptr
    );
    
    /// Remove voice command pattern
    core::VoidResult removeVoiceCommand(const std::string& pattern);
    
    /// Get registered voice commands
    std::vector<std::string> getRegisteredCommands() const;
    
    // ========================================================================
    // Context-Aware Processing
    // ========================================================================
    
    /// Audio context information
    struct AudioContext {
        bool isPlaying = false;
        bool isRecording = false;
        core::TimePosition currentPosition{0};
        double currentTempo = 120.0;
        int32_t selectedTrackCount = 0;
        int32_t totalTrackCount = 0;
        std::string currentSessionName;
        std::vector<std::string> recentActions;
    };
    
    /// Set current audio context for better command interpretation
    void setAudioContext(const AudioContext& context);
    
    /// Get current audio context
    AudioContext getAudioContext() const;
    
    /// Enable context-aware interpretation
    void setContextAwarenessEnabled(bool enabled);
    
    /// Check if context awareness is enabled
    bool isContextAwarenessEnabled() const;
    
    // ========================================================================
    // Voice Feedback and Responses
    // ========================================================================
    
    /// Text-to-speech settings
    struct TTSSettings {
        bool enabled = true;
        std::string voice = "default";
        float rate = 1.0f;
        float pitch = 1.0f;
        float volume = 0.8f;
        std::string language = "en-US";
    };
    
    /// Configure text-to-speech
    core::VoidResult configureTTS(const TTSSettings& settings);
    
    /// Speak response text
    core::AsyncResult<core::VoidResult> speakResponse(const std::string& text);
    
    /// Enable/disable voice feedback
    void setVoiceFeedbackEnabled(bool enabled);
    
    /// Check if voice feedback is enabled
    bool isVoiceFeedbackEnabled() const;
    
    // ========================================================================
    // Voice Training and Adaptation
    // ========================================================================
    
    /// User voice profile
    struct VoiceProfile {
        std::string userId;
        std::string name;
        std::vector<std::string> voiceSamples;
        std::unordered_map<std::string, std::string> commandAliases;
        float recognitionAccuracy = 0.0f;
        std::chrono::system_clock::time_point lastTrained;
    };
    
    /// Create new voice profile
    core::AsyncResult<core::VoidResult> createVoiceProfile(const std::string& userId, const std::string& name);
    
    /// Load voice profile
    core::AsyncResult<core::VoidResult> loadVoiceProfile(const std::string& userId);
    
    /// Save current voice profile
    core::AsyncResult<core::VoidResult> saveVoiceProfile();
    
    /// Add voice sample for training
    core::AsyncResult<core::VoidResult> addVoiceSample(
        const core::FloatAudioBuffer& sample,
        const std::string& transcript,
        core::SampleRate sampleRate
    );
    
    /// Train voice recognition model with user samples
    core::AsyncResult<core::VoidResult> trainUserModel(core::ProgressCallback progress = nullptr);
    
    /// Get available voice profiles
    std::vector<VoiceProfile> getAvailableProfiles() const;
    
    // ========================================================================
    // Voice Command Templates
    // ========================================================================
    
    /// Built-in command templates
    void registerBuiltInCommands();
    
    /// Transport commands
    void registerTransportCommands();
    
    /// Track commands
    void registerTrackCommands();
    
    /// Session commands
    void registerSessionCommands();
    
    /// Plugin commands
    void registerPluginCommands();
    
    /// Automation commands
    void registerAutomationCommands();
    
    // ========================================================================
    // Event Callbacks
    // ========================================================================
    
    /// Voice recognition events
    enum class VoiceEvent {
        ListeningStarted,
        ListeningStopped,
        SpeechDetected,
        SpeechRecognized,
        CommandExecuted,
        CommandFailed,
        WakeWordDetected,
        NoiseDetected
    };
    
    /// Voice event callback type
    using VoiceEventCallback = std::function<void(VoiceEvent event, const std::string& details)>;
    
    /// Set voice event callback
    void setVoiceEventCallback(VoiceEventCallback callback);
    
    /// Clear voice event callback
    void clearVoiceEventCallback();
    
    // ========================================================================
    // Statistics and Monitoring
    // ========================================================================
    
    struct VoiceStatistics {
        int64_t totalCommands = 0;
        int64_t successfulCommands = 0;
        int64_t failedCommands = 0;
        double averageConfidence = 0.0;
        double averageProcessingTime = 0.0;  // ms
        std::unordered_map<std::string, int64_t> commandCounts;
        std::unordered_map<std::string, int64_t> errorCounts;
        std::chrono::system_clock::time_point lastCommand;
    };
    
    /// Get voice recognition statistics
    VoiceStatistics getStatistics() const;
    
    /// Reset statistics
    void resetStatistics();

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize speech recognition engine
    core::VoidResult initializeSpeechEngine();
    
    /// Cleanup speech recognition resources
    void cleanupSpeechEngine();
    
    /// Process speech recognition results
    void processSpeechResult(const std::string& text, float confidence);
    
    /// Audio processing thread
    void audioProcessingLoop();
    
    /// Wake word detection
    bool detectWakeWord(const std::string& text) const;
    
    /// Extract parameters from recognized text
    nlohmann::json extractCommandParameters(
        const std::string& text,
        const std::string& pattern
    ) const;
    
    /// Match command pattern
    bool matchCommandPattern(const std::string& text, const std::string& pattern) const;
    
    /// Update voice statistics
    void updateStatistics(bool success, float confidence, double processingTime);
    
    /// Emit voice event
    void emitVoiceEvent(VoiceEvent event, const std::string& details);
    
    /// Load voice models
    core::VoidResult loadVoiceModels();
    
    /// Process natural language understanding
    InterpretationResult processNLU(const std::string& text);

private:
    // Action API reference
    std::shared_ptr<api::ActionAPI> actionAPI_;
    
    // Voice recognition engine
    std::unique_ptr<WhisperContext> whisperContext_;
    std::unique_ptr<WhisperState> whisperState_;
    
    // Settings and state
    VoiceSettings settings_;
    std::atomic<bool> isActive_{false};
    std::atomic<bool> isListening_{false};
    mutable std::mutex settingsMutex_;
    
    // Audio processing
    std::queue<core::FloatAudioBuffer> audioQueue_;
    std::mutex audioQueueMutex_;
    std::condition_variable audioQueueCondition_;
    std::thread audioProcessingThread_;
    std::atomic<bool> shouldStopProcessing_{false};
    
    // Command patterns
    struct CommandPattern {
        std::string pattern;
        std::string actionName;
        std::function<nlohmann::json(const std::vector<std::string>&)> parameterExtractor;
    };
    
    std::vector<CommandPattern> commandPatterns_;
    mutable std::shared_mutex commandPatternsMutex_;
    
    // Context awareness
    AudioContext audioContext_;
    std::atomic<bool> contextAwarenessEnabled_{true};
    mutable std::mutex contextMutex_;
    
    // Text-to-speech
    TTSSettings ttsSettings_;
    std::atomic<bool> voiceFeedbackEnabled_{true};
    mutable std::mutex ttsMutex_;
    
    // Voice profiles
    VoiceProfile currentProfile_;
    std::unordered_map<std::string, VoiceProfile> voiceProfiles_;
    mutable std::shared_mutex profilesMutex_;
    
    // Statistics
    mutable VoiceStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Event callback
    VoiceEventCallback voiceEventCallback_;
    std::mutex callbackMutex_;
    
    // Constants
    static constexpr int32_t AUDIO_BUFFER_SIZE = 1024;
    static constexpr int32_t MAX_AUDIO_QUEUE_SIZE = 100;
    static constexpr float DEFAULT_SAMPLE_RATE = 16000.0f; // Whisper prefers 16kHz
};

} // namespace mixmind::ai