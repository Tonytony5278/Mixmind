#include "TERenderService.h"
#include "TEUtils.h"
#include <tracktion_engine/tracktion_engine.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <chrono>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TERenderService Implementation
// ============================================================================

TERenderService::TERenderService(te::Engine& engine)
    : TEAdapter(engine)
{
    // Initialize supported formats
    initializeSupportedFormats();
    
    // Start render queue processing
    startRenderQueueProcessing();
}

// Session Rendering

core::AsyncResult<core::VoidResult> TERenderService::renderSession(
    const std::string& outputPath,
    const RenderSettings& settings,
    core::ProgressCallback progress
) {
    return executeAsync<core::VoidResult>([this, outputPath, settings, progress]() -> core::VoidResult {
        try {
            te::Edit* edit = getCurrentEdit();
            if (!edit) {
                return core::VoidResult::failure("No active edit for rendering");
            }
            
            // Validate render settings
            auto validationResult = validateRenderSettings(settings);
            if (!validationResult.isSuccess()) {
                return core::VoidResult::failure("Invalid render settings: " + validationResult.getError());
            }
            
            // Convert settings to TE parameters
            auto teParams = convertRenderSettingsToTE(settings);
            
            // Create output file
            juce::File outputFile(outputPath);
            outputFile.getParentDirectory().createDirectory();
            
            // Setup audio format
            auto audioFormat = convertAudioFormatToTE(settings.format);
            if (!audioFormat) {
                return core::VoidResult::failure("Unsupported audio format");
            }
            
            // Create renderer
            te::Renderer renderer(*edit, teParams);
            
            // Setup progress tracking
            setupRenderProgressTracking(renderer, progress, renderProgressCallback_);
            
            // Set rendering state
            isRenderingActive_.store(true);
            
            auto startTime = std::chrono::steady_clock::now();
            
            // Perform render
            juce::FileOutputStream outputStream(outputFile);
            if (!outputStream.openedOk()) {
                isRenderingActive_.store(false);
                return core::VoidResult::failure("Failed to create output file: " + outputPath);
            }
            
            // Create audio format writer
            auto writer = audioFormat->createWriterFor(
                &outputStream,
                static_cast<double>(settings.sampleRate),
                static_cast<unsigned int>(settings.channels),
                settings.bitDepth,
                {},
                0
            );
            
            if (!writer) {
                isRenderingActive_.store(false);
                return core::VoidResult::failure("Failed to create audio writer");
            }
            
            // Render audio
            bool renderSuccess = renderer.renderToFile(outputFile, teParams);
            
            auto endTime = std::chrono::steady_clock::now();
            auto renderDuration = std::chrono::duration<double>(endTime - startTime).count();
            
            // Update statistics
            RenderJobInfo jobInfo;
            jobInfo.jobId = generateRenderJobID();
            jobInfo.outputPath = outputPath;
            jobInfo.settings = settings;
            jobInfo.status = renderSuccess ? RenderJobStatus::Completed : RenderJobStatus::Failed;
            
            updateRenderStatistics(jobInfo, renderSuccess, renderDuration);
            
            // Reset rendering state
            isRenderingActive_.store(false);
            
            // Emit completion event
            emitRenderCompleteEvent(jobInfo.jobId, renderSuccess, outputPath);
            
            return renderSuccess ? 
                core::VoidResult::success() : 
                core::VoidResult::failure("Render operation failed");
            
        } catch (const std::exception& e) {
            isRenderingActive_.store(false);
            return core::VoidResult::failure("Render failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<core::FloatAudioBuffer>> TERenderService::renderSessionToBuffer(
    const RenderSettings& settings,
    core::ProgressCallback progress
) {
    return executeAsync<core::Result<core::FloatAudioBuffer>>([this, settings, progress]() -> core::Result<core::FloatAudioBuffer> {
        try {
            te::Edit* edit = getCurrentEdit();
            if (!edit) {
                return core::Result<core::FloatAudioBuffer>::failure("No active edit for rendering");
            }
            
            // Validate render settings
            auto validationResult = validateRenderSettings(settings);
            if (!validationResult.isSuccess()) {
                return core::Result<core::FloatAudioBuffer>::failure("Invalid render settings: " + validationResult.getError());
            }
            
            // Convert settings to TE parameters
            auto teParams = convertRenderSettingsToTE(settings);
            
            // Create renderer
            te::Renderer renderer(*edit, teParams);
            
            // Setup progress tracking
            setupRenderProgressTracking(renderer, progress, renderProgressCallback_);
            
            // Set rendering state
            isRenderingActive_.store(true);
            
            // Calculate buffer size
            double duration = edit->getLength();
            int64_t numSamples = static_cast<int64_t>(duration * settings.sampleRate);
            
            // Create output buffer
            core::FloatAudioBuffer audioBuffer;
            audioBuffer.setSize(settings.channels, numSamples);
            audioBuffer.clear();
            
            // Render to buffer
            juce::AudioBuffer<float> juceBuffer(audioBuffer.getArrayOfWritePointers(), 
                                               settings.channels, 
                                               static_cast<int>(numSamples));
            
            bool renderSuccess = renderer.renderToBuffer(juceBuffer, teParams);
            
            // Reset rendering state
            isRenderingActive_.store(false);
            
            if (!renderSuccess) {
                return core::Result<core::FloatAudioBuffer>::failure("Render to buffer failed");
            }
            
            return core::Result<core::FloatAudioBuffer>::success(std::move(audioBuffer));
            
        } catch (const std::exception& e) {
            isRenderingActive_.store(false);
            return core::Result<core::FloatAudioBuffer>::failure("Render to buffer failed: " + std::string(e.what()));
        }
    });
}

// Track Rendering

core::AsyncResult<core::VoidResult> TERenderService::renderTrack(
    core::TrackID trackId,
    const std::string& outputPath,
    const RenderSettings& settings,
    core::ProgressCallback progress
) {
    return executeAsync<core::VoidResult>([this, trackId, outputPath, settings, progress]() -> core::VoidResult {
        try {
            te::Edit* edit = getCurrentEdit();
            if (!edit) {
                return core::VoidResult::failure("No active edit for rendering");
            }
            
            // Find the track
            te::Track* track = nullptr;
            for (auto* t : edit->getTrackList()) {
                if (t->getIndexInEditTrackList() == static_cast<int>(trackId.value())) {
                    track = t;
                    break;
                }
            }
            
            if (!track) {
                return core::VoidResult::failure("Track not found");
            }
            
            // Validate render settings
            auto validationResult = validateRenderSettings(settings);
            if (!validationResult.isSuccess()) {
                return core::VoidResult::failure("Invalid render settings: " + validationResult.getError());
            }
            
            // Create modified render parameters for single track
            auto teParams = convertRenderSettingsToTE(settings);
            
            // Solo the target track temporarily
            bool wasAlreadySoloed = track->isSolo(false);
            if (!wasAlreadySoloed) {
                track->setSolo(true);
            }
            
            // Create renderer with track isolation
            te::Renderer renderer(*edit, teParams);
            
            // Setup progress tracking
            setupRenderProgressTracking(renderer, progress, renderProgressCallback_);
            
            // Set rendering state
            isRenderingActive_.store(true);
            
            auto startTime = std::chrono::steady_clock::now();
            
            // Create output file
            juce::File outputFile(outputPath);
            outputFile.getParentDirectory().createDirectory();
            
            // Perform render
            bool renderSuccess = renderer.renderToFile(outputFile, teParams);
            
            auto endTime = std::chrono::steady_clock::now();
            auto renderDuration = std::chrono::duration<double>(endTime - startTime).count();
            
            // Restore track solo state
            if (!wasAlreadySoloed) {
                track->setSolo(false);
            }
            
            // Update statistics
            RenderJobInfo jobInfo;
            jobInfo.jobId = generateRenderJobID();
            jobInfo.outputPath = outputPath;
            jobInfo.settings = settings;
            jobInfo.status = renderSuccess ? RenderJobStatus::Completed : RenderJobStatus::Failed;
            
            updateRenderStatistics(jobInfo, renderSuccess, renderDuration);
            
            // Reset rendering state
            isRenderingActive_.store(false);
            
            // Emit completion event
            emitRenderCompleteEvent(jobInfo.jobId, renderSuccess, outputPath);
            
            return renderSuccess ? 
                core::VoidResult::success() : 
                core::VoidResult::failure("Track render operation failed");
            
        } catch (const std::exception& e) {
            isRenderingActive_.store(false);
            return core::VoidResult::failure("Track render failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<core::FloatAudioBuffer>> TERenderService::renderTrackToBuffer(
    core::TrackID trackId,
    const RenderSettings& settings,
    core::ProgressCallback progress
) {
    return executeAsync<core::Result<core::FloatAudioBuffer>>([this, trackId, settings, progress]() -> core::Result<core::FloatAudioBuffer> {
        try {
            te::Edit* edit = getCurrentEdit();
            if (!edit) {
                return core::Result<core::FloatAudioBuffer>::failure("No active edit for rendering");
            }
            
            // Find the track
            te::Track* track = nullptr;
            for (auto* t : edit->getTrackList()) {
                if (t->getIndexInEditTrackList() == static_cast<int>(trackId.value())) {
                    track = t;
                    break;
                }
            }
            
            if (!track) {
                return core::Result<core::FloatAudioBuffer>::failure("Track not found");
            }
            
            // Validate render settings
            auto validationResult = validateRenderSettings(settings);
            if (!validationResult.isSuccess()) {
                return core::Result<core::FloatAudioBuffer>::failure("Invalid render settings: " + validationResult.getError());
            }
            
            // Solo the target track temporarily
            bool wasAlreadySoloed = track->isSolo(false);
            if (!wasAlreadySoloed) {
                track->setSolo(true);
            }
            
            // Convert settings to TE parameters
            auto teParams = convertRenderSettingsToTE(settings);
            
            // Create renderer
            te::Renderer renderer(*edit, teParams);
            
            // Setup progress tracking
            setupRenderProgressTracking(renderer, progress, renderProgressCallback_);
            
            // Set rendering state
            isRenderingActive_.store(true);
            
            // Calculate buffer size
            double duration = edit->getLength();
            int64_t numSamples = static_cast<int64_t>(duration * settings.sampleRate);
            
            // Create output buffer
            core::FloatAudioBuffer audioBuffer;
            audioBuffer.setSize(settings.channels, numSamples);
            audioBuffer.clear();
            
            // Render to buffer
            juce::AudioBuffer<float> juceBuffer(audioBuffer.getArrayOfWritePointers(), 
                                               settings.channels, 
                                               static_cast<int>(numSamples));
            
            bool renderSuccess = renderer.renderToBuffer(juceBuffer, teParams);
            
            // Restore track solo state
            if (!wasAlreadySoloed) {
                track->setSolo(false);
            }
            
            // Reset rendering state
            isRenderingActive_.store(false);
            
            if (!renderSuccess) {
                return core::Result<core::FloatAudioBuffer>::failure("Track render to buffer failed");
            }
            
            return core::Result<core::FloatAudioBuffer>::success(std::move(audioBuffer));
            
        } catch (const std::exception& e) {
            isRenderingActive_.store(false);
            return core::Result<core::FloatAudioBuffer>::failure("Track render to buffer failed: " + std::string(e.what()));
        }
    });
}

// Time Range Rendering

core::AsyncResult<core::VoidResult> TERenderService::renderTimeRange(
    core::TimePosition startTime,
    core::TimePosition endTime,
    const std::string& outputPath,
    const RenderSettings& settings,
    core::ProgressCallback progress
) {
    return executeAsync<core::VoidResult>([this, startTime, endTime, outputPath, settings, progress]() -> core::VoidResult {
        try {
            te::Edit* edit = getCurrentEdit();
            if (!edit) {
                return core::VoidResult::failure("No active edit for rendering");
            }
            
            // Validate time range
            if (startTime >= endTime) {
                return core::VoidResult::failure("Invalid time range: start time must be before end time");
            }
            
            // Validate render settings
            auto validationResult = validateRenderSettings(settings);
            if (!validationResult.isSuccess()) {
                return core::VoidResult::failure("Invalid render settings: " + validationResult.getError());
            }
            
            // Convert time positions
            double teStartTime = TEUtils::convertTimePosition(startTime);
            double teEndTime = TEUtils::convertTimePosition(endTime);
            
            // Create modified render parameters for time range
            auto teParams = convertRenderSettingsToTE(settings);
            teParams.time = te::EditTimeRange(teStartTime, teEndTime);
            
            // Create renderer
            te::Renderer renderer(*edit, teParams);
            
            // Setup progress tracking
            setupRenderProgressTracking(renderer, progress, renderProgressCallback_);
            
            // Set rendering state
            isRenderingActive_.store(true);
            
            auto renderStartTime = std::chrono::steady_clock::now();
            
            // Create output file
            juce::File outputFile(outputPath);
            outputFile.getParentDirectory().createDirectory();
            
            // Perform render
            bool renderSuccess = renderer.renderToFile(outputFile, teParams);
            
            auto renderEndTime = std::chrono::steady_clock::now();
            auto renderDuration = std::chrono::duration<double>(renderEndTime - renderStartTime).count();
            
            // Update statistics
            RenderJobInfo jobInfo;
            jobInfo.jobId = generateRenderJobID();
            jobInfo.outputPath = outputPath;
            jobInfo.settings = settings;
            jobInfo.status = renderSuccess ? RenderJobStatus::Completed : RenderJobStatus::Failed;
            
            updateRenderStatistics(jobInfo, renderSuccess, renderDuration);
            
            // Reset rendering state
            isRenderingActive_.store(false);
            
            // Emit completion event
            emitRenderCompleteEvent(jobInfo.jobId, renderSuccess, outputPath);
            
            return renderSuccess ? 
                core::VoidResult::success() : 
                core::VoidResult::failure("Time range render operation failed");
            
        } catch (const std::exception& e) {
            isRenderingActive_.store(false);
            return core::VoidResult::failure("Time range render failed: " + std::string(e.what()));
        }
    });
}

// Real-time Rendering

core::AsyncResult<core::VoidResult> TERenderService::startRealtimeRender(
    const std::string& outputPath,
    const RenderSettings& settings
) {
    return executeAsync<core::VoidResult>([this, outputPath, settings]() -> core::VoidResult {
        try {
            if (isRealtimeRenderActive_.load()) {
                return core::VoidResult::failure("Real-time render already active");
            }
            
            te::Edit* edit = getCurrentEdit();
            if (!edit) {
                return core::VoidResult::failure("No active edit for real-time rendering");
            }
            
            // Validate render settings
            auto validationResult = validateRenderSettings(settings);
            if (!validationResult.isSuccess()) {
                return core::VoidResult::failure("Invalid render settings: " + validationResult.getError());
            }
            
            // Create output file
            juce::File outputFile(outputPath);
            outputFile.getParentDirectory().createDirectory();
            
            // Store realtime render state
            {
                std::lock_guard<std::mutex> lock(realtimeRenderMutex_);
                realtimeOutputPath_ = outputPath;
                realtimeSettings_ = settings;
                
                // Convert settings to TE parameters
                auto teParams = convertRenderSettingsToTE(settings);
                
                // Create realtime renderer
                realtimeRenderer_ = std::make_unique<te::Renderer>(*edit, teParams);
            }
            
            // Set active state
            isRealtimeRenderActive_.store(true);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to start real-time render: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> TERenderService::stopRealtimeRender() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (!isRealtimeRenderActive_.load()) {
                return core::VoidResult::failure("No real-time render active");
            }
            
            // Stop rendering
            {
                std::lock_guard<std::mutex> lock(realtimeRenderMutex_);
                if (realtimeRenderer_) {
                    // TE will handle stopping the render automatically when renderer is destroyed
                    realtimeRenderer_.reset();
                }
            }
            
            // Reset active state
            isRealtimeRenderActive_.store(false);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to stop real-time render: " + std::string(e.what()));
        }
    });
}

bool TERenderService::isRealtimeRenderActive() const {
    return isRealtimeRenderActive_.load();
}

// Render Queue Management

core::AsyncResult<core::Result<core::RenderJobID>> TERenderService::queueRenderJob(
    const RenderJob& job,
    int32_t priority
) {
    return executeAsync<core::Result<core::RenderJobID>>([this, job, priority]() -> core::Result<core::RenderJobID> {
        try {
            // Validate job
            auto validationResult = validateRenderSettings(job.settings);
            if (!validationResult.isSuccess()) {
                return core::Result<core::RenderJobID>::failure("Invalid job settings: " + validationResult.getError());
            }
            
            // Create job info
            RenderJobInfo jobInfo;
            jobInfo.jobId = generateRenderJobID();
            jobInfo.job = job;
            jobInfo.priority = priority;
            jobInfo.status = RenderJobStatus::Queued;
            jobInfo.progress = 0.0f;
            jobInfo.queueTime = std::chrono::system_clock::now();
            
            // Add to queue
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                renderQueue_.push_back(jobInfo);
                
                // Sort by priority (higher priority first)
                std::sort(renderQueue_.begin(), renderQueue_.end(), 
                    [](const RenderJobInfo& a, const RenderJobInfo& b) {
                        if (a.status != b.status) {
                            // Queued jobs first
                            return (a.status == RenderJobStatus::Queued) && (b.status != RenderJobStatus::Queued);
                        }
                        return a.priority > b.priority;
                    });
            }
            
            // Notify queue processor
            queueCondition_.notify_one();
            
            return core::Result<core::RenderJobID>::success(jobInfo.jobId);
            
        } catch (const std::exception& e) {
            return core::Result<core::RenderJobID>::failure("Failed to queue render job: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> TERenderService::cancelRenderJob(core::RenderJobID jobId) {
    return executeAsync<core::VoidResult>([this, jobId]() -> core::VoidResult {
        try {
            std::lock_guard<std::mutex> lock(queueMutex_);
            
            auto it = std::find_if(renderQueue_.begin(), renderQueue_.end(),
                [jobId](const RenderJobInfo& info) { return info.jobId == jobId; });
            
            if (it == renderQueue_.end()) {
                return core::VoidResult::failure("Render job not found");
            }
            
            if (it->status == RenderJobStatus::InProgress) {
                // Mark for cancellation
                it->status = RenderJobStatus::Cancelled;
                // TE renderer will need to be stopped externally
            } else {
                // Remove from queue
                renderQueue_.erase(it);
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to cancel render job: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<std::vector<TERenderService::RenderJobInfo>>> TERenderService::getRenderQueue() const {
    return executeAsync<core::Result<std::vector<RenderJobInfo>>>([this]() -> core::Result<std::vector<RenderJobInfo>> {
        try {
            std::lock_guard<std::mutex> lock(queueMutex_);
            return core::Result<std::vector<RenderJobInfo>>::success(renderQueue_);
            
        } catch (const std::exception& e) {
            return core::Result<std::vector<RenderJobInfo>>::failure("Failed to get render queue: " + std::string(e.what()));
        }
    });
}

// Render Presets and Templates

core::AsyncResult<core::VoidResult> TERenderService::saveRenderPreset(
    const std::string& presetName,
    const RenderSettings& settings
) {
    return executeAsync<core::VoidResult>([this, presetName, settings]() -> core::VoidResult {
        try {
            // Validate render settings
            auto validationResult = validateRenderSettings(settings);
            if (!validationResult.isSuccess()) {
                return core::VoidResult::failure("Invalid render settings: " + validationResult.getError());
            }
            
            // Save preset
            {
                std::unique_lock<std::shared_mutex> lock(renderPresetsMutex_);
                renderPresets_[presetName] = settings;
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Failed to save render preset: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::Result<TERenderService::RenderSettings>> TERenderService::loadRenderPreset(
    const std::string& presetName
) const {
    return executeAsync<core::Result<RenderSettings>>([this, presetName]() -> core::Result<RenderSettings> {
        try {
            std::shared_lock<std::shared_mutex> lock(renderPresetsMutex_);
            
            auto it = renderPresets_.find(presetName);
            if (it == renderPresets_.end()) {
                return core::Result<RenderSettings>::failure("Render preset not found: " + presetName);
            }
            
            return core::Result<RenderSettings>::success(it->second);
            
        } catch (const std::exception& e) {
            return core::Result<RenderSettings>::failure("Failed to load render preset: " + std::string(e.what()));
        }
    });
}

// Format Support and Validation

std::vector<TERenderService::AudioFormat> TERenderService::getSupportedFormats() const {
    std::lock_guard<std::mutex> lock(supportedFormatsMutex_);
    
    if (!supportedFormatsInitialized_) {
        // Initialize supported formats
        supportedFormats_.clear();
        
        // WAV format
        AudioFormat wavFormat;
        wavFormat.extension = "wav";
        wavFormat.description = "WAV Audio File";
        wavFormat.supportedSampleRates = {22050, 44100, 48000, 88200, 96000, 176400, 192000};
        wavFormat.supportedBitDepths = {16, 24, 32};
        wavFormat.maxChannels = 32;
        wavFormat.supportsMetadata = true;
        supportedFormats_.push_back(wavFormat);
        
        // AIFF format
        AudioFormat aiffFormat;
        aiffFormat.extension = "aiff";
        aiffFormat.description = "AIFF Audio File";
        aiffFormat.supportedSampleRates = {22050, 44100, 48000, 88200, 96000, 176400, 192000};
        aiffFormat.supportedBitDepths = {16, 24, 32};
        aiffFormat.maxChannels = 32;
        aiffFormat.supportsMetadata = true;
        supportedFormats_.push_back(aiffFormat);
        
        // FLAC format
        AudioFormat flacFormat;
        flacFormat.extension = "flac";
        flacFormat.description = "FLAC Audio File";
        flacFormat.supportedSampleRates = {22050, 44100, 48000, 88200, 96000, 176400, 192000};
        flacFormat.supportedBitDepths = {16, 24};
        flacFormat.maxChannels = 8;
        flacFormat.supportsMetadata = true;
        supportedFormats_.push_back(flacFormat);
        
        // OGG format
        AudioFormat oggFormat;
        oggFormat.extension = "ogg";
        oggFormat.description = "OGG Vorbis Audio File";
        oggFormat.supportedSampleRates = {22050, 44100, 48000};
        oggFormat.supportedBitDepths = {16}; // OGG uses quality settings, not bit depth
        oggFormat.maxChannels = 2;
        oggFormat.supportsMetadata = true;
        supportedFormats_.push_back(oggFormat);
        
        supportedFormatsInitialized_ = true;
    }
    
    return supportedFormats_;
}

bool TERenderService::isFormatSupported(const AudioFormat& format) const {
    auto supportedFormats = getSupportedFormats();
    
    for (const auto& supportedFormat : supportedFormats) {
        if (supportedFormat.extension == format.extension) {
            // Check sample rate
            bool sampleRateSupported = std::find(supportedFormat.supportedSampleRates.begin(),
                                                supportedFormat.supportedSampleRates.end(),
                                                format.sampleRate) != supportedFormat.supportedSampleRates.end();
            
            // Check bit depth
            bool bitDepthSupported = std::find(supportedFormat.supportedBitDepths.begin(),
                                              supportedFormat.supportedBitDepths.end(),
                                              format.bitDepth) != supportedFormat.supportedBitDepths.end();
            
            // Check channel count
            bool channelsSupported = format.channels <= supportedFormat.maxChannels;
            
            return sampleRateSupported && bitDepthSupported && channelsSupported;
        }
    }
    
    return false;
}

core::Result<core::VoidResult> TERenderService::validateRenderSettings(const RenderSettings& settings) const {
    // Validate format
    if (!isFormatSupported(settings.format)) {
        return core::Result<core::VoidResult>::failure("Unsupported audio format");
    }
    
    // Validate sample rate
    if (settings.sampleRate < 8000 || settings.sampleRate > 192000) {
        return core::Result<core::VoidResult>::failure("Invalid sample rate");
    }
    
    // Validate channels
    if (settings.channels < 1 || settings.channels > 32) {
        return core::Result<core::VoidResult>::failure("Invalid channel count");
    }
    
    // Validate bit depth
    if (settings.bitDepth != 16 && settings.bitDepth != 24 && settings.bitDepth != 32) {
        return core::Result<core::VoidResult>::failure("Invalid bit depth");
    }
    
    return core::Result<core::VoidResult>::success();
}

// Event Callbacks

void TERenderService::setRenderProgressCallback(RenderProgressCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    renderProgressCallback_ = std::move(callback);
}

void TERenderService::setRenderCompleteCallback(RenderCompleteCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    renderCompleteCallback_ = std::move(callback);
}

void TERenderService::clearRenderCallbacks() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    renderProgressCallback_ = nullptr;
    renderCompleteCallback_ = nullptr;
}

// Protected Implementation Methods

te::Edit* TERenderService::getCurrentEdit() const {
    std::lock_guard<std::mutex> lock(editMutex_);
    if (!currentEdit_) {
        currentEdit_ = engine_.getUIBehaviour().getCurrentlyFocusedEdit();
    }
    return currentEdit_;
}

te::Renderer::Parameters TERenderService::convertRenderSettingsToTE(const RenderSettings& settings) const {
    te::Renderer::Parameters params;
    
    // Basic settings
    params.sampleRate = static_cast<double>(settings.sampleRate);
    params.blockSize = 512; // Default block size
    params.bitDepth = settings.bitDepth;
    params.quality = te::Renderer::Parameters::Quality::intermediate;
    
    // Time range (full session by default)
    if (te::Edit* edit = getCurrentEdit()) {
        params.time = te::EditTimeRange(0.0, edit->getLength());
    }
    
    // Output channels
    params.destChannels = settings.channels;
    
    // Real-time settings
    params.realtime = false; // Offline rendering by default
    params.usePlugins = true;
    params.mustRenderInMono = (settings.channels == 1);
    
    return params;
}

std::unique_ptr<te::AudioFormat> TERenderService::convertAudioFormatToTE(const AudioFormat& format) const {
    // Create JUCE audio format
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    if (format.extension == "wav") {
        return std::make_unique<juce::WavAudioFormat>();
    } else if (format.extension == "aiff") {
        return std::make_unique<juce::AiffAudioFormat>();
    } else if (format.extension == "flac") {
        return std::make_unique<juce::FlacAudioFormat>();
    }
    // Add other formats as needed
    
    return nullptr;
}

void TERenderService::setupRenderProgressTracking(
    te::Renderer& renderer,
    core::ProgressCallback progress,
    RenderProgressCallback renderProgress
) {
    // TE doesn't expose direct progress callbacks, but we can track it through other means
    // This is a simplified implementation - in practice, you'd need to integrate with TE's progress system
    
    if (progress || renderProgress) {
        // Start a thread to monitor render progress
        std::thread progressThread([this, &renderer, progress, renderProgress]() {
            while (isRenderingActive_.load()) {
                // Estimate progress (simplified)
                float progressValue = 0.5f; // Would need actual TE progress tracking
                
                if (progress) {
                    progress(progressValue);
                }
                
                if (renderProgress) {
                    RenderProgress renderProg;
                    renderProg.progress = progressValue;
                    renderProg.currentTime = progressValue * 100.0; // Estimated
                    renderProg.totalTime = 100.0; // Estimated
                    renderProg.phase = RenderPhase::Rendering;
                    renderProgress(renderProg);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        
        progressThread.detach();
    }
}

void TERenderService::processRenderJob(const RenderJobInfo& jobInfo) {
    // Implementation would process individual render jobs from the queue
    // This is called by the queue processing thread
}

void TERenderService::startRenderQueueProcessing() {
    shouldStopQueueProcessing_.store(false);
    
    queueProcessingThread_ = std::thread([this]() {
        while (!shouldStopQueueProcessing_.load()) {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            // Wait for jobs or stop signal
            queueCondition_.wait(lock, [this]() {
                return !renderQueue_.empty() || shouldStopQueueProcessing_.load();
            });
            
            if (shouldStopQueueProcessing_.load()) {
                break;
            }
            
            // Process next job
            if (!renderQueue_.empty()) {
                auto jobInfo = renderQueue_.front();
                if (jobInfo.status == RenderJobStatus::Queued) {
                    jobInfo.status = RenderJobStatus::InProgress;
                    lock.unlock();
                    
                    // Process the job
                    processRenderJob(jobInfo);
                    
                    lock.lock();
                    // Remove completed job
                    renderQueue_.erase(renderQueue_.begin());
                }
            }
        }
    });
}

void TERenderService::stopRenderQueueProcessing() {
    shouldStopQueueProcessing_.store(true);
    queueCondition_.notify_all();
    
    if (queueProcessingThread_.joinable()) {
        queueProcessingThread_.join();
    }
}

core::RenderJobID TERenderService::generateRenderJobID() {
    return core::RenderJobID{nextRenderJobId_.fetch_add(1)};
}

void TERenderService::updateRenderStatistics(const RenderJobInfo& jobInfo, bool success, double renderTime) {
    RenderStatistics stats;
    stats.jobId = jobInfo.jobId;
    stats.success = success;
    stats.renderTime = renderTime;
    stats.outputPath = jobInfo.outputPath;
    stats.settings = jobInfo.settings;
    stats.timestamp = std::chrono::system_clock::now();
    
    std::unique_lock<std::shared_mutex> lock(renderHistoryMutex_);
    renderHistory_.push_back(stats);
    
    // Keep only recent history
    if (renderHistory_.size() > MAX_RENDER_HISTORY) {
        renderHistory_.erase(renderHistory_.begin());
    }
}

void TERenderService::emitRenderProgressEvent(const RenderProgress& progress) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (renderProgressCallback_) {
        renderProgressCallback_(progress);
    }
}

void TERenderService::emitRenderCompleteEvent(core::RenderJobID jobId, bool success, const std::string& outputPath) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (renderCompleteCallback_) {
        renderCompleteCallback_(jobId, success, outputPath);
    }
}

} // namespace mixmind::adapters::tracktion