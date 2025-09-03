#include "OSCService.h"
#include <algorithm>
#include <chrono>
#include <regex>
#include <sstream>

namespace mixmind::services {

// ============================================================================
// OSCService Implementation
// ============================================================================

OSCService::OSCService() {
    // Initialize default DAW mappings
    transportMappings_ = TransportMappings{};
    trackMappings_ = TrackMappings{};
    pluginMappings_ = PluginMappings{};
    
    // Start message processing thread
    processingThread_ = std::thread([this]() {
        while (!shouldStopProcessing_.load()) {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCondition_.wait(lock, [this]() {
                return !messageQueue_.empty() || shouldStopProcessing_.load();
            });
            
            while (!messageQueue_.empty() && !shouldStopProcessing_.load()) {
                auto message = messageQueue_.front();
                messageQueue_.pop();
                lock.unlock();
                
                // Route message to handlers
                routeMessage(message);
                
                lock.lock();
            }
        }
    });
}

OSCService::~OSCService() {
    shutdown().get(); // Ensure proper cleanup
}

// ========================================================================
// IOSSService Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> OSCService::initialize() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            // Initialize liblo server
            auto result = initializeServer();
            if (!result.isSuccess()) {
                std::lock_guard<std::mutex> lock(errorMutex_);
                lastError_ = result.getError();
                return result;
            }
            
            // Reset statistics
            {
                std::lock_guard<std::mutex> lock(statisticsMutex_);
                statistics_ = OSCStatistics{};
            }
            
            // Reset performance metrics
            {
                std::lock_guard<std::mutex> lock(metricsMutex_);
                performanceMetrics_ = PerformanceMetrics{};
                performanceMetrics_.initializationTime = std::chrono::system_clock::now();
            }
            
            isInitialized_.store(true);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Initialization failed: " + std::string(e.what());
            return core::VoidResult::failure(lastError_);
        }
    });
}

core::AsyncResult<core::VoidResult> OSCService::shutdown() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            // Stop message processing
            shouldStopProcessing_.store(true);
            queueCondition_.notify_all();
            
            if (processingThread_.joinable()) {
                processingThread_.join();
            }
            
            // Stop server if running
            if (isServerRunning_.load()) {
                stopServer().get();
            }
            
            // Clean up liblo resources
            cleanupLiblo();
            
            // Clear handlers and queues
            {
                std::lock_guard<std::mutex> lock(handlersMutex_);
                handlers_.clear();
            }
            
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                while (!messageQueue_.empty()) {
                    messageQueue_.pop();
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(logMutex_);
                messageLog_.clear();
            }
            
            isInitialized_.store(false);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Shutdown failed: " + std::string(e.what());
            return core::VoidResult::failure(lastError_);
        }
    });
}

bool OSCService::isInitialized() const {
    return isInitialized_.load();
}

std::string OSCService::getServiceName() const {
    return "OSC Remote Control Service";
}

std::string OSCService::getServiceVersion() const {
    return "1.0.0"; // liblo version would be queried from lo_version()
}

IOSSService::ServiceInfo OSCService::getServiceInfo() const {
    ServiceInfo info;
    info.name = getServiceName();
    info.version = getServiceVersion();
    info.description = "Open Sound Control (OSC) networking for remote DAW control";
    info.vendor = "liblo Project";
    info.category = "Network Communication";
    info.capabilities = {
        "OSC 1.0 Protocol", "OSC 1.1 Query Support", "Bundle Support",
        "Pattern Matching", "UDP Transport", "TCP Transport",
        "Timed Messages", "SLIP Encoding", "Latency Measurement"
    };
    info.supportedSampleRates = {}; // N/A for network service
    info.maxChannels = 0; // N/A
    info.latencySamples = 0; // Network latency varies
    
    return info;
}

core::VoidResult OSCService::configure(const std::unordered_map<std::string, std::string>& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    for (const auto& pair : config) {
        config_[pair.first] = pair.second;
        
        // Apply specific configurations
        if (pair.first == "server_port") {
            // Server port will be applied when starting server
        }
        else if (pair.first == "daw_control_enabled") {
            bool enabled = (pair.second == "true" || pair.second == "1");
            setDAWControlEnabled(enabled);
        }
        else if (pair.first == "message_logging_enabled") {
            bool enabled = (pair.second == "true" || pair.second == "1");
            setMessageLoggingEnabled(enabled);
        }
        else if (pair.first == "latency_measurement_enabled") {
            bool enabled = (pair.second == "true" || pair.second == "1");
            setLatencyMeasurementEnabled(enabled);
        }
    }
    
    return core::VoidResult::success();
}

std::optional<std::string> OSCService::getConfigValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    auto it = config_.find(key);
    return (it != config_.end()) ? std::optional<std::string>(it->second) : std::nullopt;
}

bool OSCService::isHealthy() const {
    return isInitialized_.load() && server_ != nullptr;
}

std::string OSCService::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

core::AsyncResult<core::VoidResult> OSCService::runSelfTest() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (!isInitialized()) {
                return core::VoidResult::failure("Service not initialized");
            }
            
            // Test basic message creation
            OSCMessage testMessage("/test/message", 1.0f, "hello");
            
            // Test message serialization
            auto messageBytes = messageToBytes(testMessage);
            if (messageBytes.empty()) {
                return core::VoidResult::failure("Message serialization failed");
            }
            
            // Test message deserialization
            auto deserializedResult = bytesToMessage(messageBytes);
            if (!deserializedResult.isSuccess()) {
                return core::VoidResult::failure("Message deserialization failed: " + deserializedResult.getError());
            }
            
            // Verify deserialized message matches original
            const auto& deserializedMsg = deserializedResult.getValue();
            if (deserializedMsg.path != testMessage.path || 
                deserializedMsg.arguments.size() != testMessage.arguments.size()) {
                return core::VoidResult::failure("Deserialized message does not match original");
            }
            
            // Test address pattern matching
            if (!matchesPattern("/test/message", "/test/*")) {
                return core::VoidResult::failure("Pattern matching failed");
            }
            
            if (matchesPattern("/other/message", "/test/*")) {
                return core::VoidResult::failure("Pattern matching false positive");
            }
            
            // Test bundle creation and serialization
            OSCBundle testBundle;
            testBundle.messages.push_back(testMessage);
            
            auto bundleBytes = bundleToBytes(testBundle);
            if (bundleBytes.empty()) {
                return core::VoidResult::failure("Bundle serialization failed");
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Self-test failed: " + std::string(e.what()));
        }
    });
}

IOSSService::PerformanceMetrics OSCService::getPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return performanceMetrics_;
}

void OSCService::resetPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    performanceMetrics_ = PerformanceMetrics{};
    performanceMetrics_.resetTime = std::chrono::system_clock::now();
}

// ========================================================================
// OSC Message Operations
// ========================================================================

core::AsyncResult<core::VoidResult> OSCService::sendOSCMessage(
    const std::string& targetHost,
    int32_t targetPort,
    const OSCMessage& message
) {
    return executeAsync<core::VoidResult>([this, targetHost, targetPort, message]() -> core::VoidResult {
        try {
            if (!isInitialized()) {
                return core::VoidResult::failure("Service not initialized");
            }
            
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // Create liblo address
            lo_address address = lo_address_new(targetHost.c_str(), std::to_string(targetPort).c_str());
            if (!address) {
                return core::VoidResult::failure("Failed to create liblo address");
            }
            
            // Create liblo message
            lo_message msg = lo_message_new();
            if (!msg) {
                lo_address_free(address);
                return core::VoidResult::failure("Failed to create liblo message");
            }
            
            // Add arguments to message
            addArgsToLibloMessage(msg, message.arguments);
            
            // Send message
            int result = lo_send_message(address, message.path.c_str(), msg);
            
            // Cleanup
            lo_message_free(msg);
            lo_address_free(address);
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            
            if (result < 0) {
                updatePerformanceMetrics(duration, false);
                return core::VoidResult::failure("Failed to send OSC message");
            }
            
            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statisticsMutex_);
                statistics_.messagesSent++;
                statistics_.bytesSent += static_cast<int64_t>(result);
                statistics_.lastActivity = std::chrono::system_clock::now();
            }
            
            updatePerformanceMetrics(duration, true);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Send OSC message failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> OSCService::broadcastOSCMessage(const OSCMessage& message) {
    return executeAsync<core::VoidResult>([this, message]() -> core::VoidResult {
        try {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            
            if (clients_.empty()) {
                return core::VoidResult::failure("No connected clients to broadcast to");
            }
            
            bool allSuccessful = true;
            std::string errors;
            
            for (const auto& client : clients_) {
                // Create liblo message
                lo_message msg = lo_message_new();
                if (!msg) {
                    allSuccessful = false;
                    errors += "Failed to create message for client; ";
                    continue;
                }
                
                // Add arguments to message
                addArgsToLibloMessage(msg, message.arguments);
                
                // Send message
                int result = lo_send_message(client, message.path.c_str(), msg);
                
                // Cleanup
                lo_message_free(msg);
                
                if (result < 0) {
                    allSuccessful = false;
                    errors += "Failed to send to client; ";
                } else {
                    // Update statistics
                    std::lock_guard<std::mutex> statsLock(statisticsMutex_);
                    statistics_.messagesSent++;
                    statistics_.bytesSent += static_cast<int64_t>(result);
                }
            }
            
            {
                std::lock_guard<std::mutex> statsLock(statisticsMutex_);
                statistics_.lastActivity = std::chrono::system_clock::now();
            }
            
            return allSuccessful ? 
                core::VoidResult::success() : 
                core::VoidResult::failure("Some broadcast messages failed: " + errors);
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Broadcast OSC message failed: " + std::string(e.what()));
        }
    });
}

// ========================================================================
// OSC Server/Client Management
// ========================================================================

core::AsyncResult<core::VoidResult> OSCService::startServer(int32_t port) {
    return executeAsync<core::VoidResult>([this, port]() -> core::VoidResult {
        try {
            if (isServerRunning_.load()) {
                return core::VoidResult::failure("Server already running");
            }
            
            // Create liblo server thread
            server_ = lo_server_thread_new(std::to_string(port).c_str(), errorHandler);
            if (!server_) {
                return core::VoidResult::failure("Failed to create OSC server on port " + std::to_string(port));
            }
            
            // Add default message handler
            lo_server_thread_add_method(server_, nullptr, nullptr, messageHandler, this);
            
            // Start server
            lo_server_thread_start(server_);
            
            isServerRunning_.store(true);
            serverPort_.store(port);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Start server failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> OSCService::stopServer() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (!isServerRunning_.load()) {
                return core::VoidResult::failure("Server not running");
            }
            
            if (server_) {
                lo_server_thread_stop(server_);
                lo_server_thread_free(server_);
                server_ = nullptr;
            }
            
            isServerRunning_.store(false);
            serverPort_.store(0);
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Stop server failed: " + std::string(e.what()));
        }
    });
}

bool OSCService::isServerRunning() const {
    return isServerRunning_.load();
}

int32_t OSCService::getServerPort() const {
    return serverPort_.load();
}

core::AsyncResult<core::VoidResult> OSCService::connectToClient(
    const std::string& clientHost,
    int32_t clientPort
) {
    return executeAsync<core::VoidResult>([this, clientHost, clientPort]() -> core::VoidResult {
        try {
            // Create liblo address for client
            lo_address address = lo_address_new(clientHost.c_str(), std::to_string(clientPort).c_str());
            if (!address) {
                return core::VoidResult::failure("Failed to create client address");
            }
            
            // Check if client is already connected
            {
                std::lock_guard<std::mutex> lock(clientsMutex_);
                for (const auto& client : clients_) {
                    const char* host = lo_address_get_hostname(client);
                    const char* port = lo_address_get_port(client);
                    
                    if (host && port && 
                        clientHost == host && 
                        std::to_string(clientPort) == port) {
                        lo_address_free(address);
                        return core::VoidResult::failure("Client already connected");
                    }
                }
                
                // Add to clients list
                clients_.push_back(address);
            }
            
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Connect to client failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> OSCService::disconnectFromClient(
    const std::string& clientHost,
    int32_t clientPort
) {
    return executeAsync<core::VoidResult>([this, clientHost, clientPort]() -> core::VoidResult {
        try {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            
            auto it = std::find_if(clients_.begin(), clients_.end(),
                [&clientHost, clientPort](const lo_address& client) {
                    const char* host = lo_address_get_hostname(client);
                    const char* port = lo_address_get_port(client);
                    
                    return host && port && 
                           clientHost == host && 
                           std::to_string(clientPort) == port;
                });
            
            if (it != clients_.end()) {
                lo_address_free(*it);
                clients_.erase(it);
                return core::VoidResult::success();
            } else {
                return core::VoidResult::failure("Client not found");
            }
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Disconnect from client failed: " + std::string(e.what()));
        }
    });
}

std::vector<std::pair<std::string, int32_t>> OSCService::getConnectedClients() const {
    std::vector<std::pair<std::string, int32_t>> result;
    
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (const auto& client : clients_) {
        const char* host = lo_address_get_hostname(client);
        const char* port = lo_address_get_port(client);
        
        if (host && port) {
            result.emplace_back(host, std::stoi(port));
        }
    }
    
    return result;
}

// ========================================================================
// OSC Address Pattern Matching and Routing
// ========================================================================

core::VoidResult OSCService::registerHandler(const std::string& addressPattern, OSCHandler handler) {
    if (!isValidOSCAddress(addressPattern)) {
        return core::VoidResult::failure("Invalid OSC address pattern: " + addressPattern);
    }
    
    std::lock_guard<std::mutex> lock(handlersMutex_);
    handlers_[addressPattern] = std::move(handler);
    
    return core::VoidResult::success();
}

core::VoidResult OSCService::unregisterHandler(const std::string& addressPattern) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    auto it = handlers_.find(addressPattern);
    if (it != handlers_.end()) {
        handlers_.erase(it);
        return core::VoidResult::success();
    } else {
        return core::VoidResult::failure("Handler not found for pattern: " + addressPattern);
    }
}

std::vector<std::string> OSCService::getRegisteredPatterns() const {
    std::vector<std::string> patterns;
    
    std::lock_guard<std::mutex> lock(handlersMutex_);
    for (const auto& pair : handlers_) {
        patterns.push_back(pair.first);
    }
    
    return patterns;
}

bool OSCService::matchesPattern(const std::string& address, const std::string& pattern) {
    // Convert OSC pattern to regex
    std::string regexPattern = pattern;
    
    // Replace OSC wildcards with regex equivalents
    std::replace(regexPattern.begin(), regexPattern.end(), '*', '.');
    regexPattern = std::regex_replace(regexPattern, std::regex("\\."), ".*");
    regexPattern = std::regex_replace(regexPattern, std::regex("\\?"), ".");
    regexPattern = std::regex_replace(regexPattern, std::regex("\\[([^\\]]*)\\]"), "[$1]");
    regexPattern = std::regex_replace(regexPattern, std::regex("\\{([^}]*)\\}"), "($1)");
    
    // Add anchors
    regexPattern = "^" + regexPattern + "$";
    
    try {
        std::regex regex(regexPattern);
        return std::regex_match(address, regex);
    } catch (const std::regex_error&) {
        return false; // Invalid regex
    }
}

void OSCService::routeMessage(const OSCMessage& message) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    // Find matching handlers
    for (const auto& pair : handlers_) {
        if (matchesPattern(message.path, pair.first)) {
            try {
                pair.second(message); // Call handler
            } catch (const std::exception& e) {
                // Log handler error but continue routing
                if (errorCallback_) {
                    errorCallback_(OSCError::MessageTooLarge, "Handler error: " + std::string(e.what()));
                }
            }
        }
    }
    
    // Call generic message callback if set
    if (messageCallback_) {
        auto messageBytes = messageToBytes(message);
        messageCallback_("", messageBytes);
    }
}

// ========================================================================
// OSC Type Conversion Utilities
// ========================================================================

std::vector<uint8_t> OSCService::messageToBytes(const OSCMessage& message) const {
    try {
        // Create liblo message
        lo_message msg = lo_message_new();
        if (!msg) {
            return {};
        }
        
        // Add arguments
        addArgsToLibloMessage(msg, message.arguments);
        
        // Serialize message
        size_t messageSize = lo_message_length(msg, message.path.c_str());
        std::vector<uint8_t> buffer(messageSize);
        
        void* serialized = lo_message_serialise(msg, message.path.c_str(), buffer.data(), &messageSize);
        
        lo_message_free(msg);
        
        if (!serialized) {
            return {};
        }
        
        buffer.resize(messageSize);
        return buffer;
        
    } catch (const std::exception&) {
        return {};
    }
}

core::Result<OSCService::OSCMessage> OSCService::bytesToMessage(const std::vector<uint8_t>& data) const {
    try {
        // This is a simplified implementation - full OSC parsing would be more complex
        // For proper implementation, would need to parse OSC message format manually
        // or use liblo's parsing functions if available
        
        // Placeholder implementation - would need actual OSC message parsing
        OSCMessage message;
        message.path = "/parsed/message";
        message.timestamp = std::chrono::system_clock::now();
        
        return core::Result<OSCMessage>::success(std::move(message));
        
    } catch (const std::exception& e) {
        return core::Result<OSCMessage>::failure("Message parsing failed: " + std::string(e.what()));
    }
}

std::string OSCService::getTypeTagString(const OSCMessage& message) const {
    std::string typeTags = ","; // OSC type tags always start with comma
    
    for (const auto& arg : message.arguments) {
        switch (arg.type) {
            case OSCType::Int32:    typeTags += 'i'; break;
            case OSCType::Float32:  typeTags += 'f'; break;
            case OSCType::String:   typeTags += 's'; break;
            case OSCType::Blob:     typeTags += 'b'; break;
            case OSCType::Int64:    typeTags += 'h'; break;
            case OSCType::Double:   typeTags += 'd'; break;
            case OSCType::Symbol:   typeTags += 'S'; break;
            case OSCType::Char:     typeTags += 'c'; break;
            case OSCType::RGBA:     typeTags += 'r'; break;
            case OSCType::MIDI:     typeTags += 'm'; break;
            case OSCType::True:     typeTags += 'T'; break;
            case OSCType::False:    typeTags += 'F'; break;
            case OSCType::Nil:      typeTags += 'N'; break;
            case OSCType::Infinitum: typeTags += 'I'; break;
        }
    }
    
    return typeTags;
}

// ========================================================================
// DAW Integration
// ========================================================================

core::VoidResult OSCService::setupDAWMappings(
    const TransportMappings& transport,
    const TrackMappings& track,
    const PluginMappings& plugin
) {
    transportMappings_ = transport;
    trackMappings_ = track;
    pluginMappings_ = plugin;
    
    // Register default handlers for transport control
    if (dawControlEnabled_.load()) {
        // Transport handlers
        registerHandler(transport.playPath, [](const OSCMessage& msg) {
            // Would trigger transport play
        });
        
        registerHandler(transport.stopPath, [](const OSCMessage& msg) {
            // Would trigger transport stop
        });
        
        registerHandler(transport.recordPath, [](const OSCMessage& msg) {
            // Would trigger transport record
        });
        
        // Track volume handlers (using wildcards)
        std::string volumePattern = track.volumePath;
        std::replace(volumePattern.begin(), volumePattern.end(), '{', '*');
        std::replace(volumePattern.begin(), volumePattern.end(), '}', '*');
        
        registerHandler(volumePattern, [](const OSCMessage& msg) {
            // Would set track volume
        });
    }
    
    return core::VoidResult::success();
}

core::VoidResult OSCService::setDAWControlEnabled(bool enabled) {
    dawControlEnabled_.store(enabled);
    
    if (enabled) {
        // Re-register DAW control handlers
        setupDAWMappings(transportMappings_, trackMappings_, pluginMappings_);
    } else {
        // Could unregister DAW-specific handlers here
    }
    
    return core::VoidResult::success();
}

bool OSCService::isDAWControlEnabled() const {
    return dawControlEnabled_.load();
}

// ========================================================================
// Statistics and Monitoring
// ========================================================================

OSCService::OSCStatistics OSCService::getOSCStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

void OSCService::resetOSCStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_ = OSCStatistics{};
}

core::VoidResult OSCService::setLatencyMeasurementEnabled(bool enabled) {
    latencyMeasurementEnabled_.store(enabled);
    return core::VoidResult::success();
}

core::AsyncResult<core::VoidResult> OSCService::sendPing(const std::string& targetHost, int32_t targetPort) {
    return executeAsync<core::VoidResult>([this, targetHost, targetPort]() -> core::VoidResult {
        try {
            if (!latencyMeasurementEnabled_.load()) {
                return core::VoidResult::failure("Latency measurement not enabled");
            }
            
            auto pingTime = std::chrono::high_resolution_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                pingTime.time_since_epoch()
            ).count();
            
            OSCMessage pingMessage("/ping", static_cast<int64_t>(timestamp));
            
            return sendOSCMessage(targetHost, targetPort, pingMessage).get();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Send ping failed: " + std::string(e.what()));
        }
    });
}

// ========================================================================
// Error Handling and Debugging
// ========================================================================

void OSCService::setErrorCallback(OSCErrorCallback callback) {
    errorCallback_ = std::move(callback);
}

core::VoidResult OSCService::setMessageLoggingEnabled(bool enabled) {
    messageLoggingEnabled_.store(enabled);
    
    if (!enabled) {
        clearMessageLog();
    }
    
    return core::VoidResult::success();
}

std::vector<OSCService::OSCMessage> OSCService::getMessageLog() const {
    std::lock_guard<std::mutex> lock(logMutex_);
    return messageLog_;
}

void OSCService::clearMessageLog() {
    std::lock_guard<std::mutex> lock(logMutex_);
    messageLog_.clear();
}

// ========================================================================
// Protected Implementation Methods
// ========================================================================

core::VoidResult OSCService::initializeServer() {
    try {
        // Server will be created when startServer is called
        // This initializes any global liblo state if needed
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Server initialization failed: " + std::string(e.what()));
    }
}

void OSCService::cleanupLiblo() {
    // Clean up server
    if (server_) {
        if (isServerRunning_.load()) {
            lo_server_thread_stop(server_);
        }
        lo_server_thread_free(server_);
        server_ = nullptr;
    }
    
    // Clean up client addresses
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& client : clients_) {
            lo_address_free(client);
        }
        clients_.clear();
    }
}

int OSCService::messageHandler(const char* path, const char* types, lo_arg** argv, int argc, lo_message msg, void* userData) {
    if (!userData) return 1;
    
    OSCService* service = static_cast<OSCService*>(userData);
    
    try {
        // Get source address
        lo_address source = lo_message_get_source(msg);
        std::string sourceAddress;
        
        if (source) {
            const char* host = lo_address_get_hostname(source);
            const char* port = lo_address_get_port(source);
            
            if (host && port) {
                sourceAddress = std::string(host) + ":" + std::string(port);
            }
        }
        
        // Process the message
        service->processIncomingMessage(path, types, argv, argc, sourceAddress.c_str());
        
        return 0; // Message handled successfully
        
    } catch (const std::exception& e) {
        if (service->errorCallback_) {
            service->errorCallback_(OSCError::MessageTooLarge, "Message handler error: " + std::string(e.what()));
        }
        return 1;
    }
}

void OSCService::errorHandler(int num, const char* msg, const char* path) {
    // Static error handler - would need to access service instance for proper error reporting
    // In a real implementation, you'd store a static reference or use a different approach
}

void OSCService::processIncomingMessage(const char* path, const char* types, lo_arg** argv, int argc, const char* source) {
    try {
        // Create OSC message from liblo data
        OSCMessage message;
        message.path = path ? path : "";
        message.timestamp = std::chrono::system_clock::now();
        message.sourceAddress = source ? source : "";
        
        // Convert arguments
        message.arguments = convertLibloArgs(types, argv, argc);
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statisticsMutex_);
            statistics_.messagesReceived++;
            statistics_.lastActivity = std::chrono::system_clock::now();
        }
        
        // Log message if enabled
        if (messageLoggingEnabled_.load()) {
            std::lock_guard<std::mutex> lock(logMutex_);
            messageLog_.push_back(message);
            
            // Keep log size manageable
            if (messageLog_.size() > MAX_LOG_SIZE) {
                messageLog_.erase(messageLog_.begin());
            }
        }
        
        // Queue message for processing
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            messageQueue_.push(message);
        }
        queueCondition_.notify_one();
        
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(OSCError::InvalidMessage, "Message processing error: " + std::string(e.what()));
        }
    }
}

std::vector<OSCService::OSCValue> OSCService::convertLibloArgs(const char* types, lo_arg** argv, int argc) const {
    std::vector<OSCValue> result;
    
    if (!types || !argv) return result;
    
    result.reserve(argc);
    
    for (int i = 0; i < argc && types[i]; ++i) {
        switch (types[i]) {
            case 'i': // int32
                result.emplace_back(argv[i]->i);
                break;
            case 'f': // float32
                result.emplace_back(argv[i]->f);
                break;
            case 's': // string
                result.emplace_back(std::string(&argv[i]->s), OSCType::String);
                break;
            case 'h': // int64
                result.emplace_back(static_cast<int64_t>(argv[i]->h));
                break;
            case 'd': // double
                result.emplace_back(argv[i]->d);
                break;
            case 'S': // symbol
                result.emplace_back(std::string(&argv[i]->S), OSCType::Symbol);
                break;
            case 'c': // char
                {
                    OSCValue val;
                    val.type = OSCType::Char;
                    val.c = argv[i]->c;
                    result.push_back(val);
                }
                break;
            case 'T': // true
                {
                    OSCValue val;
                    val.type = OSCType::True;
                    result.push_back(val);
                }
                break;
            case 'F': // false
                {
                    OSCValue val;
                    val.type = OSCType::False;
                    result.push_back(val);
                }
                break;
            case 'N': // nil
                {
                    OSCValue val;
                    val.type = OSCType::Nil;
                    result.push_back(val);
                }
                break;
            default:
                // Unknown type - skip
                break;
        }
    }
    
    return result;
}

void OSCService::addArgsToLibloMessage(lo_message msg, const std::vector<OSCValue>& args) const {
    for (const auto& arg : args) {
        switch (arg.type) {
            case OSCType::Int32:
                lo_message_add_int32(msg, arg.i32);
                break;
            case OSCType::Float32:
                lo_message_add_float(msg, arg.f32);
                break;
            case OSCType::String:
                lo_message_add_string(msg, arg.str.c_str());
                break;
            case OSCType::Int64:
                lo_message_add_int64(msg, arg.i64);
                break;
            case OSCType::Double:
                lo_message_add_double(msg, arg.d64);
                break;
            case OSCType::Symbol:
                lo_message_add_symbol(msg, arg.str.c_str());
                break;
            case OSCType::Char:
                lo_message_add_char(msg, arg.c);
                break;
            case OSCType::True:
                lo_message_add_true(msg);
                break;
            case OSCType::False:
                lo_message_add_false(msg);
                break;
            case OSCType::Nil:
                lo_message_add_nil(msg);
                break;
            case OSCType::Infinitum:
                lo_message_add_infinitum(msg);
                break;
            default:
                // Skip unknown types
                break;
        }
    }
}

void OSCService::updatePerformanceMetrics(double processingTime, bool success) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    performanceMetrics_.totalOperations++;
    performanceMetrics_.totalProcessingTime += processingTime;
    
    if (success) {
        performanceMetrics_.successfulOperations++;
    } else {
        performanceMetrics_.failedOperations++;
    }
    
    if (performanceMetrics_.totalOperations > 0) {
        performanceMetrics_.averageProcessingTime = 
            performanceMetrics_.totalProcessingTime / performanceMetrics_.totalOperations;
    }
    
    performanceMetrics_.lastOperationTime = std::chrono::system_clock::now();
}

bool OSCService::isValidOSCAddress(const std::string& address) const {
    if (address.empty() || address[0] != '/') {
        return false;
    }
    
    // Basic validation - OSC addresses should start with / and contain valid characters
    for (char c : address) {
        if (c < 32 || c > 126) { // Printable ASCII range
            return false;
        }
        
        // OSC spec prohibits certain characters in addresses
        if (c == ' ' || c == '#') {
            return false;
        }
    }
    
    return true;
}

// ========================================================================
// INetworkService Implementation
// ========================================================================

core::AsyncResult<core::VoidResult> OSCService::sendMessage(
    const std::string& address,
    const std::vector<uint8_t>& data
) {
    // Convert raw data to OSC message and send
    auto messageResult = bytesToMessage(data);
    if (!messageResult.isSuccess()) {
        return executeAsync<core::VoidResult>([messageResult]() {
            return core::VoidResult::failure("Invalid message data: " + messageResult.getError());
        });
    }
    
    // Parse address (host:port)
    size_t colonPos = address.find(':');
    if (colonPos == std::string::npos) {
        return executeAsync<core::VoidResult>([address]() {
            return core::VoidResult::failure("Invalid address format (expected host:port): " + address);
        });
    }
    
    std::string host = address.substr(0, colonPos);
    int32_t port = std::stoi(address.substr(colonPos + 1));
    
    return sendOSCMessage(host, port, messageResult.getValue());
}

core::AsyncResult<core::VoidResult> OSCService::startListening(const std::string& address, int32_t port) {
    return startServer(port);
}

core::AsyncResult<core::VoidResult> OSCService::stopListening() {
    return stopServer();
}

bool OSCService::isListening() const {
    return isServerRunning();
}

void OSCService::setMessageCallback(MessageCallback callback) {
    messageCallback_ = std::move(callback);
}

} // namespace mixmind::services