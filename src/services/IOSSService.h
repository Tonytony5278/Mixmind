#pragma once

#include "../core/types.h"
#include "../core/result.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace mixmind::services {

// ============================================================================
// Base OSS Service Interface - Common functionality for all OSS services
// ============================================================================

class IOSSService {
public:
    virtual ~IOSSService() = default;
    
    // ========================================================================
    // Service Lifecycle
    // ========================================================================
    
    /// Initialize the service
    virtual core::AsyncResult<core::VoidResult> initialize() = 0;
    
    /// Shutdown the service
    virtual core::AsyncResult<core::VoidResult> shutdown() = 0;
    
    /// Check if service is initialized
    virtual bool isInitialized() const = 0;
    
    /// Get service name
    virtual std::string getServiceName() const = 0;
    
    /// Get service version
    virtual std::string getServiceVersion() const = 0;
    
    // ========================================================================
    // Service Information
    // ========================================================================
    
    struct ServiceInfo {
        std::string name;
        std::string version;
        std::string description;
        std::string libraryVersion;
        bool isInitialized = false;
        bool isThreadSafe = false;
        std::vector<std::string> supportedFormats;
        std::vector<std::string> capabilities;
    };
    
    /// Get comprehensive service information
    virtual ServiceInfo getServiceInfo() const = 0;
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /// Configure service with key-value pairs
    virtual core::VoidResult configure(const std::unordered_map<std::string, std::string>& config) = 0;
    
    /// Get configuration value
    virtual std::optional<std::string> getConfigValue(const std::string& key) const = 0;
    
    /// Reset configuration to defaults
    virtual core::VoidResult resetConfiguration() = 0;
    
    // ========================================================================
    // Health and Diagnostics
    // ========================================================================
    
    /// Check service health
    virtual bool isHealthy() const = 0;
    
    /// Get last error message
    virtual std::string getLastError() const = 0;
    
    /// Run service self-test
    virtual core::AsyncResult<core::VoidResult> runSelfTest() = 0;
    
    /// Get performance metrics
    struct PerformanceMetrics {
        double averageProcessingTime = 0.0;  // milliseconds
        double peakProcessingTime = 0.0;     // milliseconds
        int64_t totalOperations = 0;
        int64_t failedOperations = 0;
        size_t memoryUsage = 0;              // bytes
        double cpuUsage = 0.0;               // percentage
    };
    
    virtual PerformanceMetrics getPerformanceMetrics() const = 0;
    
    /// Reset performance metrics
    virtual void resetPerformanceMetrics() = 0;
};

// ============================================================================
// OSS Service Registry - Manages all OSS services
// ============================================================================

class IOSSServiceRegistry {
public:
    virtual ~IOSSServiceRegistry() = default;
    
    /// Register an OSS service
    virtual core::VoidResult registerService(const std::string& serviceName, std::shared_ptr<IOSSService> service) = 0;
    
    /// Unregister an OSS service
    virtual core::VoidResult unregisterService(const std::string& serviceName) = 0;
    
    /// Get service by name
    virtual std::shared_ptr<IOSSService> getService(const std::string& serviceName) const = 0;
    
    /// Get all registered services
    virtual std::vector<std::string> getAllServiceNames() const = 0;
    
    /// Initialize all services
    virtual core::AsyncResult<core::VoidResult> initializeAllServices() = 0;
    
    /// Shutdown all services
    virtual core::AsyncResult<core::VoidResult> shutdownAllServices() = 0;
    
    /// Check if all services are healthy
    virtual bool areAllServicesHealthy() const = 0;
    
    /// Get service registry statistics
    struct RegistryStats {
        int32_t totalServices = 0;
        int32_t initializedServices = 0;
        int32_t healthyServices = 0;
        int32_t failedServices = 0;
    };
    
    virtual RegistryStats getRegistryStats() const = 0;
};

// ============================================================================
// Audio Analysis Service Interface - Base for analysis services
// ============================================================================

class IAudioAnalysisService : public IOSSService {
public:
    ~IAudioAnalysisService() override = default;
    
    // ========================================================================
    // Audio Analysis Operations
    // ========================================================================
    
    /// Analyze audio buffer
    virtual core::AsyncResult<core::VoidResult> analyzeBuffer(
        const core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate,
        core::ProgressCallback progress = nullptr
    ) = 0;
    
    /// Analyze audio file
    virtual core::AsyncResult<core::VoidResult> analyzeFile(
        const std::string& filePath,
        core::ProgressCallback progress = nullptr
    ) = 0;
    
    /// Get analysis results
    virtual std::unordered_map<std::string, double> getAnalysisResults() const = 0;
    
    /// Clear analysis results
    virtual void clearResults() = 0;
    
    /// Check if analysis is in progress
    virtual bool isAnalyzing() const = 0;
    
    /// Cancel ongoing analysis
    virtual core::VoidResult cancelAnalysis() = 0;
};

// ============================================================================
// Audio Processing Service Interface - Base for processing services  
// ============================================================================

class IAudioProcessingService : public IOSSService {
public:
    ~IAudioProcessingService() override = default;
    
    // ========================================================================
    // Audio Processing Operations
    // ========================================================================
    
    /// Process audio buffer in place
    virtual core::VoidResult processBuffer(
        core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate
    ) = 0;
    
    /// Process audio buffer with separate input/output
    virtual core::VoidResult processBuffer(
        const core::FloatAudioBuffer& inputBuffer,
        core::FloatAudioBuffer& outputBuffer,
        core::SampleRate sampleRate
    ) = 0;
    
    /// Set processing parameters
    virtual core::VoidResult setParameters(const std::unordered_map<std::string, double>& parameters) = 0;
    
    /// Get processing parameters
    virtual std::unordered_map<std::string, double> getParameters() const = 0;
    
    /// Reset internal state
    virtual core::VoidResult resetState() = 0;
    
    /// Get processing latency in samples
    virtual int32_t getLatencySamples() const = 0;
};

// ============================================================================
// Metadata Service Interface - Base for metadata services
// ============================================================================

class IMetadataService : public IOSSService {
public:
    ~IMetadataService() override = default;
    
    // ========================================================================
    // Metadata Operations
    // ========================================================================
    
    struct AudioMetadata {
        std::string title;
        std::string artist;
        std::string album;
        std::string genre;
        std::string comment;
        int32_t year = 0;
        int32_t trackNumber = 0;
        int32_t discNumber = 0;
        double duration = 0.0;
        core::SampleRate sampleRate = 0;
        int32_t bitRate = 0;
        int32_t channels = 0;
        std::string format;
        
        // Additional metadata
        std::unordered_map<std::string, std::string> customTags;
    };
    
    /// Read metadata from file
    virtual core::AsyncResult<core::Result<AudioMetadata>> readMetadata(const std::string& filePath) = 0;
    
    /// Write metadata to file
    virtual core::AsyncResult<core::VoidResult> writeMetadata(
        const std::string& filePath,
        const AudioMetadata& metadata
    ) = 0;
    
    /// Check if file format is supported
    virtual bool isFormatSupported(const std::string& fileExtension) const = 0;
    
    /// Get supported file formats
    virtual std::vector<std::string> getSupportedFormats() const = 0;
    
    /// Remove all metadata from file
    virtual core::AsyncResult<core::VoidResult> clearMetadata(const std::string& filePath) = 0;
};

// ============================================================================
// Network Service Interface - Base for network services
// ============================================================================

class INetworkService : public IOSSService {
public:
    ~INetworkService() override = default;
    
    // ========================================================================
    // Network Operations
    // ========================================================================
    
    /// Send message/data
    virtual core::AsyncResult<core::VoidResult> sendMessage(
        const std::string& address,
        const std::vector<uint8_t>& data
    ) = 0;
    
    /// Receive message/data
    virtual core::AsyncResult<core::Result<std::vector<uint8_t>>> receiveMessage(
        int32_t timeoutMs = 1000
    ) = 0;
    
    /// Start listening on address/port
    virtual core::AsyncResult<core::VoidResult> startListening(
        const std::string& address,
        int32_t port
    ) = 0;
    
    /// Stop listening
    virtual core::AsyncResult<core::VoidResult> stopListening() = 0;
    
    /// Check if currently listening
    virtual bool isListening() const = 0;
    
    /// Set message callback
    using MessageCallback = std::function<void(const std::string& sender, const std::vector<uint8_t>& data)>;
    virtual void setMessageCallback(MessageCallback callback) = 0;
};

} // namespace mixmind::services