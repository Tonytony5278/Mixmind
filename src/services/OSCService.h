#pragma once

#include "IOSSService.h"
#include "../core/types.h"
#include "../core/result.h"
#include <lo/lo.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <chrono>

namespace mixmind::services {

// ============================================================================
// OSC Service - Open Sound Control networking using liblo
// ============================================================================

class OSCService : public INetworkService {
public:
    OSCService();
    ~OSCService() override;
    
    // Non-copyable, movable
    OSCService(const OSCService&) = delete;
    OSCService& operator=(const OSCService&) = delete;
    OSCService(OSCService&&) = default;
    OSCService& operator=(OSCService&&) = default;
    
    // ========================================================================
    // IOSSService Implementation
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> initialize() override;
    core::AsyncResult<core::VoidResult> shutdown() override;
    bool isInitialized() const override;
    std::string getServiceName() const override;
    std::string getServiceVersion() const override;
    ServiceInfo getServiceInfo() const override;
    core::VoidResult configure(const std::unordered_map<std::string, std::string>& config) override;
    std::optional<std::string> getConfigValue(const std::string& key) const override;
    core::VoidResult resetConfiguration() override;
    bool isHealthy() const override;
    std::string getLastError() const override;
    core::AsyncResult<core::VoidResult> runSelfTest() override;
    PerformanceMetrics getPerformanceMetrics() const override;
    void resetPerformanceMetrics() override;
    
    // ========================================================================
    // INetworkService Implementation
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> sendMessage(
        const std::string& address,
        const std::vector<uint8_t>& data
    ) override;
    
    core::AsyncResult<core::Result<std::vector<uint8_t>>> receiveMessage(
        int32_t timeoutMs = 1000
    ) override;
    
    core::AsyncResult<core::VoidResult> startListening(
        const std::string& address,
        int32_t port
    ) override;
    
    core::AsyncResult<core::VoidResult> stopListening() override;
    bool isListening() const override;
    void setMessageCallback(MessageCallback callback) override;
    
    // ========================================================================
    // OSC-Specific Types
    // ========================================================================
    
    /// OSC argument types
    enum class OSCType {
        Int32,      // 'i'
        Float32,    // 'f'
        String,     // 's'
        Blob,       // 'b'
        Int64,      // 'h'
        Double,     // 'd'
        Symbol,     // 'S'
        Char,       // 'c'
        RGBA,       // 'r'
        MIDI,       // 'm'
        True,       // 'T'
        False,      // 'F'
        Nil,        // 'N'
        Infinitum   // 'I'
    };
    
    /// OSC argument value
    struct OSCValue {
        OSCType type;
        union {
            int32_t i32;
            float f32;
            int64_t i64;
            double d64;
            char c;
            uint32_t rgba;
            struct {
                uint8_t port;
                uint8_t status;
                uint8_t data1;
                uint8_t data2;
            } midi;
        };
        std::string str; // For strings, symbols, and blobs
        
        OSCValue() : type(OSCType::Nil) {}
        OSCValue(int32_t value) : type(OSCType::Int32), i32(value) {}
        OSCValue(float value) : type(OSCType::Float32), f32(value) {}
        OSCValue(const std::string& value, OSCType t = OSCType::String) : type(t), str(value) {}
        OSCValue(double value) : type(OSCType::Double), d64(value) {}
        OSCValue(int64_t value) : type(OSCType::Int64), i64(value) {}
    };
    
    /// OSC message structure
    struct OSCMessage {
        std::string path;                    // OSC address pattern
        std::vector<OSCValue> arguments;     // Message arguments
        std::chrono::system_clock::time_point timestamp; // When message was created/received
        std::string sourceAddress;          // Sender IP:port
        
        OSCMessage() = default;
        OSCMessage(const std::string& p) : path(p), timestamp(std::chrono::system_clock::now()) {}
        
        // Convenience constructors
        template<typename... Args>
        OSCMessage(const std::string& p, Args... args) : path(p), timestamp(std::chrono::system_clock::now()) {
            (arguments.emplace_back(args), ...);
        }
    };
    
    /// OSC bundle structure
    struct OSCBundle {
        uint64_t timestamp = 0;              // OSC timetag (NTP format)
        std::vector<OSCMessage> messages;    // Messages in bundle
        std::vector<OSCBundle> bundles;      // Nested bundles
        std::string sourceAddress;          // Sender IP:port
    };
    
    // ========================================================================
    // OSC Message Operations
    // ========================================================================
    
    /// Send OSC message to specific target
    core::AsyncResult<core::VoidResult> sendOSCMessage(
        const std::string& targetHost,
        int32_t targetPort,
        const OSCMessage& message
    );
    
    /// Send OSC bundle to specific target
    core::AsyncResult<core::VoidResult> sendOSCBundle(
        const std::string& targetHost,
        int32_t targetPort,
        const OSCBundle& bundle
    );
    
    /// Broadcast OSC message to all connected clients
    core::AsyncResult<core::VoidResult> broadcastOSCMessage(const OSCMessage& message);
    
    /// Send OSC message with timestamp (scheduled delivery)
    core::AsyncResult<core::VoidResult> sendOSCMessageTimed(
        const std::string& targetHost,
        int32_t targetPort,
        const OSCMessage& message,
        uint64_t timestamp
    );
    
    // ========================================================================
    // OSC Server/Client Management
    // ========================================================================
    
    /// Start OSC server on specified port
    core::AsyncResult<core::VoidResult> startServer(int32_t port);
    
    /// Stop OSC server
    core::AsyncResult<core::VoidResult> stopServer();
    
    /// Check if server is running
    bool isServerRunning() const;
    
    /// Get server port
    int32_t getServerPort() const;
    
    /// Connect to OSC client
    core::AsyncResult<core::VoidResult> connectToClient(
        const std::string& clientHost,
        int32_t clientPort
    );
    
    /// Disconnect from OSC client
    core::AsyncResult<core::VoidResult> disconnectFromClient(
        const std::string& clientHost,
        int32_t clientPort
    );
    
    /// Get connected clients
    std::vector<std::pair<std::string, int32_t>> getConnectedClients() const;
    
    // ========================================================================
    // OSC Address Pattern Matching and Routing
    // ========================================================================
    
    /// OSC message handler function type
    using OSCHandler = std::function<void(const OSCMessage& message)>;
    
    /// Register handler for OSC address pattern
    core::VoidResult registerHandler(const std::string& addressPattern, OSCHandler handler);
    
    /// Unregister handler for address pattern
    core::VoidResult unregisterHandler(const std::string& addressPattern);
    
    /// Get registered address patterns
    std::vector<std::string> getRegisteredPatterns() const;
    
    /// Check if address matches pattern
    static bool matchesPattern(const std::string& address, const std::string& pattern);
    
    /// Route incoming message to appropriate handlers
    void routeMessage(const OSCMessage& message);
    
    // ========================================================================
    // OSC Type Conversion Utilities
    // ========================================================================
    
    /// Convert OSC message to byte array
    std::vector<uint8_t> messageToBytes(const OSCMessage& message) const;
    
    /// Convert byte array to OSC message
    core::Result<OSCMessage> bytesToMessage(const std::vector<uint8_t>& data) const;
    
    /// Convert OSC bundle to byte array
    std::vector<uint8_t> bundleToBytes(const OSCBundle& bundle) const;
    
    /// Convert byte array to OSC bundle
    core::Result<OSCBundle> bytesToBundle(const std::vector<uint8_t>& data) const;
    
    /// Get OSC type tag string from message
    std::string getTypeTagString(const OSCMessage& message) const;
    
    /// Parse type tag string to types
    std::vector<OSCType> parseTypeTagString(const std::string& typeTags) const;
    
    // ========================================================================
    // DAW Integration - Predefined OSC Mappings
    // ========================================================================
    
    /// Transport control mappings
    struct TransportMappings {
        std::string playPath = "/daw/transport/play";
        std::string stopPath = "/daw/transport/stop";
        std::string recordPath = "/daw/transport/record";
        std::string pausePath = "/daw/transport/pause";
        std::string locatePath = "/daw/transport/locate";
        std::string tempoPath = "/daw/transport/tempo";
        std::string positionPath = "/daw/transport/position";
    };
    
    /// Track control mappings
    struct TrackMappings {
        std::string volumePath = "/daw/track/{}/volume";          // {} = track number
        std::string panPath = "/daw/track/{}/pan";
        std::string mutePath = "/daw/track/{}/mute";
        std::string soloPath = "/daw/track/{}/solo";
        std::string recordArmPath = "/daw/track/{}/record";
        std::string selectPath = "/daw/track/{}/select";
    };
    
    /// Plugin control mappings
    struct PluginMappings {
        std::string parameterPath = "/daw/track/{}/plugin/{}/param/{}"; // track/plugin/param
        std::string bypassPath = "/daw/track/{}/plugin/{}/bypass";
        std::string presetPath = "/daw/track/{}/plugin/{}/preset";
    };
    
    /// Setup DAW control mappings
    core::VoidResult setupDAWMappings(
        const TransportMappings& transport,
        const TrackMappings& track,
        const PluginMappings& plugin
    );
    
    /// Enable/disable DAW control
    core::VoidResult setDAWControlEnabled(bool enabled);
    
    /// Check if DAW control is enabled
    bool isDAWControlEnabled() const;
    
    // ========================================================================
    // Advanced OSC Features
    // ========================================================================
    
    /// OSC query support (OSC 1.1 feature)
    struct OSCQueryNode {
        std::string fullPath;
        std::string description;
        std::vector<OSCType> acceptedTypes;
        std::vector<OSCValue> currentValues;
        std::vector<OSCValue> minValues;
        std::vector<OSCValue> maxValues;
        std::string units;
        std::vector<std::shared_ptr<OSCQueryNode>> children;
    };
    
    /// Build OSC query tree
    std::shared_ptr<OSCQueryNode> buildQueryTree() const;
    
    /// Handle OSC query request
    OSCMessage handleQueryRequest(const OSCMessage& queryMessage) const;
    
    /// Enable/disable OSC query support
    core::VoidResult setOSCQueryEnabled(bool enabled);
    
    /// SLIP (Serial Line Internet Protocol) encoding for reliable transport
    std::vector<uint8_t> encodeSLIP(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decodeSLIP(const std::vector<uint8_t>& data) const;
    
    // ========================================================================
    // OSC Statistics and Monitoring
    // ========================================================================
    
    struct OSCStatistics {
        int64_t messagesSent = 0;
        int64_t messagesReceived = 0;
        int64_t bundlesSent = 0;
        int64_t bundlesReceived = 0;
        int64_t bytesSent = 0;
        int64_t bytesReceived = 0;
        int64_t droppedMessages = 0;
        double averageLatency = 0.0;     // milliseconds
        double maxLatency = 0.0;         // milliseconds
        std::chrono::system_clock::time_point lastActivity;
    };
    
    /// Get OSC statistics
    OSCStatistics getOSCStatistics() const;
    
    /// Reset OSC statistics
    void resetOSCStatistics();
    
    /// Enable/disable latency measurement
    core::VoidResult setLatencyMeasurementEnabled(bool enabled);
    
    /// Send ping message for latency measurement
    core::AsyncResult<core::VoidResult> sendPing(const std::string& targetHost, int32_t targetPort);
    
    // ========================================================================
    // Error Handling and Debugging
    // ========================================================================
    
    /// OSC error codes
    enum class OSCError {
        None = 0,
        InvalidAddress,
        InvalidMessage,
        NetworkError,
        ServerError,
        ClientError,
        TimeoutError,
        MessageTooLarge
    };
    
    /// Error callback type
    using OSCErrorCallback = std::function<void(OSCError error, const std::string& message)>;
    
    /// Set error callback
    void setErrorCallback(OSCErrorCallback callback);
    
    /// Enable/disable message logging
    core::VoidResult setMessageLoggingEnabled(bool enabled);
    
    /// Get message log
    std::vector<OSCMessage> getMessageLog() const;
    
    /// Clear message log
    void clearMessageLog();
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize liblo server
    core::VoidResult initializeServer();
    
    /// Cleanup liblo resources
    void cleanupLiblo();
    
    /// Static callback for liblo message handling
    static int messageHandler(const char* path, const char* types, lo_arg** argv, int argc, lo_message msg, void* userData);
    
    /// Static callback for liblo error handling
    static void errorHandler(int num, const char* msg, const char* path);
    
    /// Process incoming OSC message
    void processIncomingMessage(const char* path, const char* types, lo_arg** argv, int argc, const char* source);
    
    /// Convert liblo arguments to our OSC values
    std::vector<OSCValue> convertLibloArgs(const char* types, lo_arg** argv, int argc) const;
    
    /// Convert our OSC values to liblo format
    void addArgsToLibloMessage(lo_message msg, const std::vector<OSCValue>& args) const;
    
    /// Update performance metrics
    void updatePerformanceMetrics(double processingTime, bool success);
    
    /// Validate OSC address pattern
    bool isValidOSCAddress(const std::string& address) const;
    
private:
    // liblo server and client
    lo_server_thread server_ = nullptr;
    std::vector<lo_address> clients_;
    std::mutex clientsMutex_;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> isServerRunning_{false};
    std::atomic<int32_t> serverPort_{0};
    
    // Configuration
    std::unordered_map<std::string, std::string> config_;
    std::mutex configMutex_;
    
    // Message routing
    std::unordered_map<std::string, OSCHandler> handlers_;
    std::mutex handlersMutex_;
    
    // Callbacks
    MessageCallback messageCallback_;
    OSCErrorCallback errorCallback_;
    
    // DAW integration
    std::atomic<bool> dawControlEnabled_{false};
    TransportMappings transportMappings_;
    TrackMappings trackMappings_;
    PluginMappings pluginMappings_;
    
    // Message queue for async processing
    std::queue<OSCMessage> messageQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::thread processingThread_;
    std::atomic<bool> shouldStopProcessing_{false};
    
    // Statistics and monitoring
    mutable OSCStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    std::atomic<bool> latencyMeasurementEnabled_{false};
    
    // Message logging
    std::atomic<bool> messageLoggingEnabled_{false};
    std::vector<OSCMessage> messageLog_;
    std::mutex logMutex_;
    static constexpr size_t MAX_LOG_SIZE = 1000;
    
    // Performance tracking
    mutable PerformanceMetrics performanceMetrics_;
    mutable std::mutex metricsMutex_;
    
    // Error tracking
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;
};

} // namespace mixmind::services