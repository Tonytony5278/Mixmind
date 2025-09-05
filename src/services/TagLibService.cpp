#include "TagLibService.h"
#include "../core/async.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/audioproperties.h>
#include <filesystem>
#include <algorithm>
#include <chrono>

using namespace TagLib;

namespace mixmind::services {

// ============================================================================
// TagLibService Implementation
// ============================================================================

TagLibService::TagLibService() = default;
TagLibService::~TagLibService() = default;

// ========================================================================
// IOSSService Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> TagLibService::initialize() {
    return core::executeAsyncGlobal<core::Result<void>>([this]() -> core::Result<void> {
        try {
            // Initialize TagLib (no explicit initialization needed)
            
            // Reset performance metrics
            {
                std::lock_guard<std::mutex> lock(metricsMutex_);
                performanceMetrics_ = PerformanceMetrics{};
                performanceMetrics_.initializationTime = std::chrono::system_clock::now();
            }
            
            isInitialized_.store(true);
            return core::core::Result<void>::success();
            
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Initialization failed: " + std::string(e.what());
            return core::core::Result<void>::failure(lastError_);
        }
    });
}

core::AsyncResult<core::VoidResult> TagLibService::shutdown() {
    return core::executeAsyncGlobal<core::Result<void>>([this]() -> core::Result<void> {
        isInitialized_.store(false);
        return core::core::Result<void>::success();
    });
}

bool TagLibService::isInitialized() const {
    return isInitialized_.load();
}

std::string TagLibService::getServiceName() const {
    return "TagLib Metadata Service";
}

std::string TagLibService::getServiceVersion() const {
    return "1.13"; // TagLib version
}

// ========================================================================
// IMetadataService Implementation  
// ========================================================================

core::AsyncResult<core::Result<IMetadataService::AudioMetadata>> TagLibService::readMetadata(const std::string& filePath) {
    return core::executeAsyncGlobal<core::Result<AudioMetadata>>([this, filePath]() -> core::Result<AudioMetadata> {
        try {
            if (!isInitialized()) {
                return core::Result<AudioMetadata>::failure("Service not initialized");
            }
            
            auto startTime = std::chrono::high_resolution_clock::now();
            
            TagLib::FileRef file(filePath.c_str());
            if (file.isNull()) {
                return core::Result<AudioMetadata>::failure("Could not open file: " + filePath);
            }
            
            AudioMetadata metadata;
            
            // Basic metadata
            if (auto* tag = file.tag()) {
                metadata.title = tag->title().to8Bit(true);
                metadata.artist = tag->artist().to8Bit(true);
                metadata.album = tag->album().to8Bit(true);
                metadata.genre = tag->genre().to8Bit(true);
                metadata.comment = tag->comment().to8Bit(true);
                metadata.year = tag->year();
                metadata.track = tag->track();
            }
            
            // Audio properties
            if (auto* audioProps = file.audioProperties()) {
                metadata.duration = audioProps->lengthInSeconds();
                metadata.bitrate = audioProps->bitrate();
                metadata.sampleRate = audioProps->sampleRate();
                metadata.channels = audioProps->channels();
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            
            updatePerformanceMetrics(duration, true);
            
            return core::Result<AudioMetadata>::success(std::move(metadata));
            
        } catch (const std::exception& e) {
            return core::Result<AudioMetadata>::failure("Read metadata failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> TagLibService::writeMetadata(const std::string& filePath, const AudioMetadata& metadata) {
    return core::executeAsyncGlobal<core::Result<void>>([this, filePath, metadata]() -> core::Result<void> {
        try {
            if (!isInitialized()) {
                return core::core::Result<void>::failure("Service not initialized");
            }
            
            TagLib::FileRef file(filePath.c_str());
            if (file.isNull()) {
                return core::core::Result<void>::failure("Could not open file for writing: " + filePath);
            }
            
            if (auto* tag = file.tag()) {
                tag->setTitle(metadata.title);
                tag->setArtist(metadata.artist);
                tag->setAlbum(metadata.album);
                tag->setGenre(metadata.genre);
                tag->setComment(metadata.comment);
                tag->setYear(metadata.year);
                tag->setTrack(metadata.track);
            }
            
            bool saved = file.save();
            if (!saved) {
                return core::core::Result<void>::failure("Failed to save metadata to file: " + filePath);
            }
            
            return core::core::Result<void>::success();
            
        } catch (const std::exception& e) {
            return core::core::Result<void>::failure("Write metadata failed: " + std::string(e.what()));
        }
    });
}

bool TagLibService::isFormatSupported(const std::string& fileExtension) const {
    static const std::vector<std::string> supportedFormats = {
        "mp3", "ogg", "flac", "wav", "aiff", "mp4", "m4a", "wma", "ape", "mpc"
    };
    
    std::string lowerExt = fileExtension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
    
    return std::find(supportedFormats.begin(), supportedFormats.end(), lowerExt) != supportedFormats.end();
}

std::vector<std::string> TagLibService::getSupportedFormats() const {
    return {"mp3", "ogg", "flac", "wav", "aiff", "mp4", "m4a", "wma", "ape", "mpc"};
}

// Simplified implementations for remaining methods...
core::AsyncResult<core::VoidResult> TagLibService::clearMetadata(const std::string& filePath) {
    AudioMetadata emptyMetadata;
    return writeMetadata(filePath, emptyMetadata);
}

void TagLibService::updatePerformanceMetrics(double processingTime, bool success) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    performanceMetrics_.totalOperations++;
    performanceMetrics_.totalProcessingTime += processingTime;
    
    if (success) {
        performanceMetrics_.successfulOperations++;
    } else {
        performanceMetrics_.failedOperations++;
    }
    
    performanceMetrics_.lastOperationTime = std::chrono::system_clock::now();
}

IOSSService::PerformanceMetrics TagLibService::getPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return performanceMetrics_;
}

void TagLibService::resetPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    performanceMetrics_ = PerformanceMetrics{};
}

// Additional method implementations would follow the same pattern...

} // namespace mixmind::services