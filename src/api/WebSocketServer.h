#pragma once

#include "ActionAPI.h"
#include "../core/types.h"
#include "../core/result.h"
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <queue>

namespace mixmind::api {

using json = nlohmann::json;
using websocketpp::connection_hdl;

// ============================================================================
// WebSocket Server - Real-time communication for the Action API
// ============================================================================

class WebSocketServer {
public:
    using server = websocketpp::server<websocketpp::config::asio>;
    using message_ptr = server::message_ptr;
    using connection_ptr = server::connection_ptr;
    
    explicit WebSocketServer(std::shared_ptr<ActionAPI> actionAPI);
    ~WebSocketServer();
    
    // Non-copyable, non-movable
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;
    WebSocketServer(WebSocketServer&&) = delete;
    WebSocketServer& operator=(WebSocketServer&&) = delete;
    
    // ========================================================================
    // Server Management
    // ========================================================================
    
    /// Start the WebSocket server
    core::AsyncResult<core::VoidResult> start(
        const std::string& host = "localhost",
        int port = 8081
    );
    
    /// Stop the WebSocket server
    core::AsyncResult<core::VoidResult> stop();
    
    /// Check if server is running
    bool isRunning() const;
    
    /// Get server host
    std::string getHost() const;
    
    /// Get server port
    int getPort() const;
    
    /// Get server URL
    std::string getServerURL() const;
    
    // ========================================================================
    // Connection Management
    // ========================================================================
    
    /// Client connection information
    struct ClientInfo {
        std::string id;
        connection_hdl handle;
        std::string userAgent;
        std::string origin;
        std::chrono::system_clock::time_point connectedAt;
        std::atomic<bool> authenticated{false};
        std::string userId;
        json metadata = json::object();
        std::unordered_set<std::string> subscriptions;
    };
    
    /// Get connected clients count
    size_t getConnectedClientsCount() const;
    
    /// Get client information
    std::vector<ClientInfo> getConnectedClients() const;
    
    /// Get client by ID
    std::shared_ptr<ClientInfo> getClient(const std::string& clientId) const;
    
    /// Disconnect client
    core::VoidResult disconnectClient(const std::string& clientId);
    
    /// Disconnect all clients
    core::VoidResult disconnectAllClients();
    
    // ========================================================================
    // Message Types and Protocols
    // ========================================================================
    
    /// WebSocket message types
    enum class MessageType {
        // Action execution
        ExecuteAction,
        ActionResult,
        
        // Subscriptions
        Subscribe,
        Unsubscribe,
        
        // Real-time events
        StateUpdate,
        TransportUpdate,
        TrackUpdate,
        ClipUpdate,
        
        // Authentication
        Authenticate,
        AuthResult,
        
        // System
        Ping,
        Pong,
        Error,
        
        // Custom
        Custom
    };
    
    /// WebSocket message structure
    struct WSMessage {
        MessageType type;
        std::string messageId;
        json payload = json::object();
        std::chrono::system_clock::time_point timestamp;
        
        WSMessage() : timestamp(std::chrono::system_clock::now()) {}
        WSMessage(MessageType t, const json& p) : type(t), payload(p), timestamp(std::chrono::system_clock::now()) {}
        
        /// Convert to JSON
        json toJson() const {
            return json{
                {"type", messageTypeToString(type)},
                {"messageId", messageId},
                {"payload", payload},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count()}
            };
        }
        
        /// Create from JSON
        static WSMessage fromJson(const json& j);
        
        /// Convert message type to string
        static std::string messageTypeToString(MessageType type);
        
        /// Convert string to message type
        static MessageType stringToMessageType(const std::string& str);
    };
    
    // ========================================================================
    // Message Sending
    // ========================================================================
    
    /// Send message to specific client
    core::VoidResult sendMessage(const std::string& clientId, const WSMessage& message);
    
    /// Send message to all clients
    core::VoidResult broadcastMessage(const WSMessage& message);
    
    /// Send message to subscribed clients
    core::VoidResult sendToSubscribers(const std::string& topic, const WSMessage& message);
    
    /// Send action result
    core::VoidResult sendActionResult(const std::string& clientId, const std::string& messageId, const ActionResult& result);
    
    /// Send error message
    core::VoidResult sendError(const std::string& clientId, const std::string& messageId, const std::string& error);
    
    // ========================================================================
    // Real-time State Broadcasting
    // ========================================================================
    
    /// Enable/disable automatic state broadcasting
    void setStateBroadcastingEnabled(bool enabled);
    
    /// Check if state broadcasting is enabled
    bool isStateBroadcastingEnabled() const;
    
    /// Set broadcast interval in milliseconds
    void setBroadcastInterval(int intervalMs);
    
    /// Get broadcast interval
    int getBroadcastInterval() const;
    
    /// Broadcast current session state
    void broadcastSessionState();
    
    /// Broadcast transport state
    void broadcastTransportState();
    
    /// Broadcast track updates
    void broadcastTrackUpdates();
    
    /// Start state monitoring thread
    void startStateMonitoring();
    
    /// Stop state monitoring thread
    void stopStateMonitoring();
    
    // ========================================================================
    // Subscription Management
    // ========================================================================
    
    /// Available subscription topics
    static const std::vector<std::string> AVAILABLE_TOPICS;
    
    /// Subscribe client to topic
    core::VoidResult subscribeClient(const std::string& clientId, const std::string& topic);
    
    /// Unsubscribe client from topic
    core::VoidResult unsubscribeClient(const std::string& clientId, const std::string& topic);
    
    /// Get client subscriptions
    std::unordered_set<std::string> getClientSubscriptions(const std::string& clientId) const;
    
    /// Get subscribers to topic
    std::vector<std::string> getTopicSubscribers(const std::string& topic) const;
    
    // ========================================================================
    // Authentication and Authorization
    // ========================================================================
    
    /// Set authentication token
    void setAuthToken(const std::string& token);
    
    /// Clear authentication token
    void clearAuthToken();
    
    /// Check if authentication is enabled
    bool isAuthEnabled() const;
    
    /// Authenticate client
    core::VoidResult authenticateClient(const std::string& clientId, const std::string& token);
    
    /// Check if client is authenticated
    bool isClientAuthenticated(const std::string& clientId) const;
    
    // ========================================================================
    // Event Callbacks
    // ========================================================================
    
    /// Connection event callback types
    using ConnectionCallback = std::function<void(const std::string& clientId)>;
    using MessageCallback = std::function<void(const std::string& clientId, const WSMessage& message)>;
    using ErrorCallback = std::function<void(const std::string& clientId, const std::string& error)>;
    
    /// Set connection callbacks
    void setOnClientConnected(ConnectionCallback callback);
    void setOnClientDisconnected(ConnectionCallback callback);
    void setOnMessageReceived(MessageCallback callback);
    void setOnError(ErrorCallback callback);
    
    // ========================================================================
    // Statistics and Monitoring
    // ========================================================================
    
    struct WSStatistics {
        int64_t totalConnections = 0;
        int64_t currentConnections = 0;
        int64_t messagesSent = 0;
        int64_t messagesReceived = 0;
        int64_t broadcastsSent = 0;
        double averageResponseTimeMs = 0.0;
        std::unordered_map<std::string, int64_t> messageTypeCounts;
        std::unordered_map<std::string, int64_t> topicCounts;
        std::chrono::system_clock::time_point lastActivity;
        std::chrono::system_clock::time_point serverStartTime;
    };
    
    /// Get WebSocket statistics
    WSStatistics getStatistics() const;
    
    /// Reset statistics
    void resetStatistics();
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /// Server configuration
    struct ServerConfig {
        int maxConnections = 1000;
        int maxMessageSize = 1024 * 1024; // 1MB
        int pingInterval = 30000; // 30 seconds
        int pongTimeout = 10000; // 10 seconds
        bool logMessages = true;
        std::string corsOrigin = "*";
    };
    
    /// Set server configuration
    void setServerConfig(const ServerConfig& config);
    
    /// Get server configuration
    ServerConfig getServerConfig() const;

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize WebSocket server
    void initializeServer();
    
    /// Setup WebSocket handlers
    void setupHandlers();
    
    /// Handle new connection
    void onOpen(connection_hdl hdl);
    
    /// Handle connection close
    void onClose(connection_hdl hdl);
    
    /// Handle incoming message
    void onMessage(connection_hdl hdl, message_ptr msg);
    
    /// Handle validation
    bool onValidate(connection_hdl hdl);
    
    /// Process incoming message
    void processMessage(const std::string& clientId, const WSMessage& message);
    
    /// Handle action execution request
    void handleExecuteAction(const std::string& clientId, const WSMessage& message);
    
    /// Handle subscription request
    void handleSubscribe(const std::string& clientId, const WSMessage& message);
    
    /// Handle unsubscribe request
    void handleUnsubscribe(const std::string& clientId, const WSMessage& message);
    
    /// Handle authentication request
    void handleAuthenticate(const std::string& clientId, const WSMessage& message);
    
    /// Handle ping request
    void handlePing(const std::string& clientId, const WSMessage& message);
    
    /// Generate unique client ID
    std::string generateClientId() const;
    
    /// Generate unique message ID
    std::string generateMessageId() const;
    
    /// Update statistics
    void updateStatistics(const std::string& event, const WSMessage* message = nullptr);
    
    /// State monitoring loop
    void stateMonitoringLoop();

private:
    // Action API reference
    std::shared_ptr<ActionAPI> actionAPI_;
    
    // WebSocket server
    server wsServer_;
    std::thread serverThread_;
    
    // Server state
    std::atomic<bool> isRunning_{false};
    std::string host_;
    int port_ = 0;
    
    // Client management
    std::unordered_map<std::string, std::shared_ptr<ClientInfo>> clients_;
    std::unordered_map<connection_hdl, std::string, std::owner_less<connection_hdl>> connectionToClient_;
    mutable std::shared_mutex clientsMutex_;
    
    // Configuration
    ServerConfig config_;
    std::string authToken_;
    mutable std::mutex configMutex_;
    
    // State broadcasting
    std::atomic<bool> stateBroadcastingEnabled_{false};
    int broadcastIntervalMs_ = 1000; // 1 second
    std::thread stateMonitoringThread_;
    std::atomic<bool> shouldStopMonitoring_{false};
    
    // Statistics
    mutable WSStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Callbacks
    ConnectionCallback onClientConnected_;
    ConnectionCallback onClientDisconnected_;
    MessageCallback onMessageReceived_;
    ErrorCallback onError_;
    
    // Message queue for async processing
    std::queue<std::pair<std::string, WSMessage>> messageQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::thread messageProcessingThread_;
    std::atomic<bool> shouldStopProcessing_{false};
    
    // Constants
    static constexpr int DEFAULT_WS_PORT = 8081;
    static constexpr const char* DEFAULT_WS_HOST = "localhost";
};

// Available subscription topics
const std::vector<std::string> WebSocketServer::AVAILABLE_TOPICS = {
    "session.state",
    "transport.state", 
    "tracks.updates",
    "clips.updates",
    "plugins.updates",
    "automation.updates",
    "media.updates",
    "analysis.results"
};

} // namespace mixmind::api