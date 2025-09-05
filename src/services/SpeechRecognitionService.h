#pragma once
#include "../core/result.h"
#include "../core/async.h"
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

class SpeechRecognitionService {
public:
    using TranscriptionCallback = std::function<void(const std::string& text, double confidence)>;
    
    mixmind::core::AsyncResult<mixmind::core::Result<void>> initialize();
    mixmind::core::Result<void> startListening(TranscriptionCallback callback);
    mixmind::core::Result<void> stopListening();
    
    // Settings
    void setLanguage(const std::string& language_code);
    void setConfidenceThreshold(double threshold);
    
    bool isListening() const { return is_listening_.load(); }
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> is_listening_{false};
    TranscriptionCallback callback_;
    std::thread audio_thread_;
    
    void audioProcessingLoop();
};