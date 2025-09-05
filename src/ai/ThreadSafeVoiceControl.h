#pragma once

#include "VoiceControl.h"
#include "../core/result.h"
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <chrono>

namespace mixmind::ai {

// ============================================================================
// Thread-Safe Voice Controller - CRITICAL AUDIO SAFETY
// 
// This version NEVER blocks the audio thread:
// - Simple commands (play/pause) execute immediately
// - Complex AI commands are queued to background threads
// - Lock-free communication between audio and AI threads
// - Real-time performance monitoring
// ============================================================================

class ThreadSafeVoiceController {
public:
    ThreadSafeVoiceController();
    ~ThreadSafeVoiceController();
    
    // Non-copyable
    ThreadSafeVoiceController(const ThreadSafeVoiceController&) = delete;
    ThreadSafeVoiceController& operator=(const ThreadSafeVoiceController&) = delete;
    
    // Core functionality (same interface as regular VoiceController)
    bool initialize();
    bool startListening(VoiceControlMode mode = VoiceControlMode::CONTINUOUS);
    void stopListening();
    
    bool isListening() const;
    VoiceControlMode getCurrentMode() const;
    
    void setCommandCallback(VoiceCommandCallback callback);
    std::vector<VoiceCommand> getCommandHistory() const;
    
    // CRITICAL: Must be called from audio thread every buffer
    // This processes AI responses without blocking
    void processAudioThreadUpdates();
    
    // Health monitoring
    bool isHealthy() const;
    
    struct PerformanceStats {
        uint64_t commandsProcessed = 0;
        uint64_t aiRequestsQueued = 0;
        double avgProcessingLatencyMicros = 0.0;
        bool isHealthy = true;
        size_t pendingAIRequests = 0;
    };
    
    PerformanceStats getPerformanceStats() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// Global thread-safe voice controller
ThreadSafeVoiceController& getGlobalThreadSafeVoiceController();
void shutdownGlobalThreadSafeVoiceController();

} // namespace mixmind::ai