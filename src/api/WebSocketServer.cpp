#include "WebSocketServer.h"
#include "../core/async.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace mixmind::api {

// ============================================================================
// WSMessage Implementation
// ============================================================================

WSMessage WSMessage::fromJson(const json& j) {
    WSMessage msg;
    
    if (j.contains("type") && j["type"].is_string()) {
        msg.type = stringToMessageType(j["type"].get<std::string>());
    }
    
    if (j.contains("messageId") && j["messageId"].is_string()) {
        msg.messageId = j["messageId"].get<std::string>();
    }
    
    if (j.contains("payload")) {
        msg.payload = j["payload"];
    }
    
    if (j.contains("timestamp") && j["timestamp"].is_number()) {
        auto timestamp = std::chrono::milliseconds(j["timestamp"].get<int64_t>());
        msg.timestamp = std::chrono::system_clock::time_point(timestamp);
    }
    
    return msg;
}

std::string WSMessage::messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::ExecuteAction: return "execute_action";
        case MessageType::ActionResult: return "action_result";
        case MessageType::Subscribe: return "subscribe";
        case MessageType::Unsubscribe: return "unsubscribe";
        case MessageType::StateUpdate: return "state_update";
        case MessageType::TransportUpdate: return "transport_update";
        case MessageType::TrackUpdate: return "track_update";
        case MessageType::ClipUpdate: return "clip_update";
        case MessageType::Authenticate: return "authenticate";
        case MessageType::AuthResult: return "auth_result";
        case MessageType::Ping: return "ping";
        case MessageType::Pong: return "pong";
        case MessageType::Error: return "error";
        case MessageType::Custom: return "custom";
        default: return "unknown";
    }
}

WSMessage::MessageType WSMessage::stringToMessageType(const std::string& str) {
    if (str == "execute_action") return MessageType::ExecuteAction;
    if (str == "action_result") return MessageType::ActionResult;
    if (str == "subscribe") return MessageType::Subscribe;
    if (str == "unsubscribe") return MessageType::Unsubscribe;
    if (str == "state_update") return MessageType::StateUpdate;
    if (str == "transport_update") return MessageType::TransportUpdate;
    if (str == "track_update") return MessageType::TrackUpdate;
    if (str == "clip_update") return MessageType::ClipUpdate;
    if (str == "authenticate") return MessageType::Authenticate;
    if (str == "auth_result") return MessageType::AuthResult;
    if (str == "ping") return MessageType::Ping;
    if (str == "pong") return MessageType::Pong;
    if (str == "error") return MessageType::Error;
    if (str == "custom") return MessageType::Custom;
    return MessageType::Custom; // Default to custom for unknown types
}

// ============================================================================
// WebSocketServer Implementation
// ============================================================================

WebSocketServer::WebSocketServer(std::shared_ptr<ActionAPI> actionAPI)
    : actionAPI_(std::move(actionAPI))
    , host_(DEFAULT_WS_HOST)
    , port_(DEFAULT_WS_PORT)
{
    if (!actionAPI_) {
        throw std::invalid_argument("ActionAPI cannot be null");
    }
    
    initializeServer();
    
    // Initialize statistics
    statistics_.serverStartTime = std::chrono::system_clock::now();
}

WebSocketServer::~WebSocketServer() {
    if (isRunning()) {
        stop().get();
    }
}

// ========================================================================
// Server Management
// ========================================================================

core::AsyncResult<core::VoidResult> WebSocketServer::start(const std::string& host, int port) {
    return executeAsync<core::VoidResult>([this, host, port]() -> core::VoidResult {
        try {
            if (isRunning()) {
                return core::VoidResult::failure("WebSocket server is already running");
            }
            
            host_ = host;
            port_ = port;
            
            // Configure server
            wsServer_.set_reuse_addr(true);
            wsServer_.listen(host_, port_);
            wsServer_.start_accept();
            
            // Start server thread
            serverThread_ = std::thread([this]() {
                try {
                    wsServer_.run();
                } catch (const std::exception& e) {
                    if (onError_) {
                        onError_("server", "Server thread error: " + std::string(e.what()));
                    }
                }
            });
            
            // Start message processing thread
            shouldStopProcessing_.store(false);
            messageProcessingThread_ = std::thread([this]() {
                while (!shouldStopProcessing_.load()) {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    queueCondition_.wait(lock, [this]() {
                        return !messageQueue_.empty() || shouldStopProcessing_.load();
                    });
                    
                    while (!messageQueue_.empty()) {
                        auto [clientId, message] = messageQueue_.front();
                        messageQueue_.pop();
                        lock.unlock();
                        
                        processMessage(clientId, message);
                        
                        lock.lock();
                    }
                }
            });
            
            isRunning_.store(true);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("WebSocket server start failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> WebSocketServer::stop() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (!isRunning()) {
                return core::VoidResult::success();
            }
            
            // Stop state monitoring
            stopStateMonitoring();
            
            // Stop message processing
            shouldStopProcessing_.store(true);
            queueCondition_.notify_all();
            
            if (messageProcessingThread_.joinable()) {
                messageProcessingThread_.join();
            }
            
            // Disconnect all clients
            disconnectAllClients();
            
            // Stop server
            wsServer_.stop();
            
            if (serverThread_.joinable()) {
                serverThread_.join();
            }
            
            isRunning_.store(false);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("WebSocket server stop failed: " + std::string(e.what()));
        }
    });
}

bool WebSocketServer::isRunning() const {
    return isRunning_.load();
}

std::string WebSocketServer::getHost() const {
    return host_;
}

int WebSocketServer::getPort() const {
    return port_;
}

std::string WebSocketServer::getServerURL() const {
    return "ws://" + host_ + ":" + std::to_string(port_);
}

// ========================================================================
// Connection Management
// ========================================================================

size_t WebSocketServer::getConnectedClientsCount() const {
    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
    return clients_.size();
}

std::vector<WebSocketServer::ClientInfo> WebSocketServer::getConnectedClients() const {
    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
    std::vector<ClientInfo> result;
    result.reserve(clients_.size());
    
    for (const auto& [id, client] : clients_) {
        result.push_back(*client);
    }
    
    return result;
}

std::shared_ptr<WebSocketServer::ClientInfo> WebSocketServer::getClient(const std::string& clientId) const {
    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
    auto it = clients_.find(clientId);
    return it != clients_.end() ? it->second : nullptr;
}

core::VoidResult WebSocketServer::disconnectClient(const std::string& clientId) {
    try {
        std::unique_lock<std::shared_mutex> lock(clientsMutex_);
        auto it = clients_.find(clientId);
        if (it == clients_.end()) {
            return core::VoidResult::failure("Client not found: " + clientId);
        }
        
        auto client = it->second;
        lock.unlock();
        
        // Close connection
        std::error_code ec;
        wsServer_.close(client->handle, websocketpp::close::status::going_away, "Server disconnect", ec);
        
        if (ec) {
            return core::VoidResult::failure("Failed to disconnect client: " + ec.message());
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Disconnect client failed: " + std::string(e.what()));
    }
}

core::VoidResult WebSocketServer::disconnectAllClients() {
    try {
        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        std::vector<std::string> clientIds;
        clientIds.reserve(clients_.size());
        
        for (const auto& [id, client] : clients_) {
            clientIds.push_back(id);
        }
        lock.unlock();
        
        for (const auto& clientId : clientIds) {
            disconnectClient(clientId);
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Disconnect all clients failed: " + std::string(e.what()));
    }
}

// ========================================================================
// Message Sending
// ========================================================================

core::VoidResult WebSocketServer::sendMessage(const std::string& clientId, const WSMessage& message) {
    try {
        auto client = getClient(clientId);
        if (!client) {
            return core::VoidResult::failure("Client not found: " + clientId);
        }
        
        std::error_code ec;
        wsServer_.send(client->handle, message.toJson().dump(), websocketpp::frame::opcode::text, ec);
        
        if (ec) {
            return core::VoidResult::failure("Send message failed: " + ec.message());
        }
        
        updateStatistics("message_sent", &message);
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Send message failed: " + std::string(e.what()));
    }
}

core::VoidResult WebSocketServer::broadcastMessage(const WSMessage& message) {
    try {
        auto clients = getConnectedClients();
        std::string messageStr = message.toJson().dump();
        
        for (const auto& client : clients) {
            std::error_code ec;
            wsServer_.send(client.handle, messageStr, websocketpp::frame::opcode::text, ec);
            
            if (ec && onError_) {
                onError_(client.id, "Broadcast failed: " + ec.message());
            }
        }
        
        updateStatistics("broadcast_sent", &message);
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Broadcast message failed: " + std::string(e.what()));
    }
}

core::VoidResult WebSocketServer::sendToSubscribers(const std::string& topic, const WSMessage& message) {
    try {
        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        std::string messageStr = message.toJson().dump();
        
        for (const auto& [id, client] : clients_) {
            if (client->subscriptions.count(topic)) {
                std::error_code ec;
                wsServer_.send(client->handle, messageStr, websocketpp::frame::opcode::text, ec);
                
                if (ec && onError_) {
                    onError_(id, "Send to subscriber failed: " + ec.message());
                }
            }
        }
        
        updateStatistics("topic_sent", &message);
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Send to subscribers failed: " + std::string(e.what()));
    }
}

core::VoidResult WebSocketServer::sendActionResult(const std::string& clientId, const std::string& messageId, const ActionResult& result) {
    WSMessage message(MessageType::ActionResult, json{
        {"messageId", messageId},
        {"success", result.success},
        {"result", result.data},
        {"error", result.error},
        {"errorCode", result.errorCode},
        {"metadata", result.metadata}
    });
    message.messageId = generateMessageId();
    
    return sendMessage(clientId, message);
}

core::VoidResult WebSocketServer::sendError(const std::string& clientId, const std::string& messageId, const std::string& error) {
    WSMessage message(MessageType::Error, json{
        {"messageId", messageId},
        {"error", error}
    });
    message.messageId = generateMessageId();
    
    return sendMessage(clientId, message);
}

// ========================================================================
// Real-time State Broadcasting
// ========================================================================

void WebSocketServer::setStateBroadcastingEnabled(bool enabled) {
    bool wasEnabled = stateBroadcastingEnabled_.exchange(enabled);
    
    if (enabled && !wasEnabled) {
        startStateMonitoring();
    } else if (!enabled && wasEnabled) {
        stopStateMonitoring();
    }
}

bool WebSocketServer::isStateBroadcastingEnabled() const {
    return stateBroadcastingEnabled_.load();
}

void WebSocketServer::setBroadcastInterval(int intervalMs) {
    broadcastIntervalMs_ = intervalMs;
}

int WebSocketServer::getBroadcastInterval() const {
    return broadcastIntervalMs_;
}

void WebSocketServer::broadcastSessionState() {
    try {
        ActionContext context;
        context.requestId = generateMessageId();
        auto sessionState = actionAPI_->getSessionState(context);
        
        WSMessage message(MessageType::StateUpdate, json{
            {"type", "session"},
            {"state", sessionState}
        });
        message.messageId = generateMessageId();
        
        sendToSubscribers("session.state", message);
        
    } catch (const std::exception& e) {
        if (onError_) {
            onError_("system", "Broadcast session state failed: " + std::string(e.what()));
        }
    }
}

void WebSocketServer::broadcastTransportState() {
    try {
        // This would integrate with the actual transport system
        WSMessage message(MessageType::TransportUpdate, json{
            {"isPlaying", false},
            {"isRecording", false},
            {"position", 0.0},
            {"tempo", 120.0}
        });
        message.messageId = generateMessageId();
        
        sendToSubscribers("transport.state", message);
        
    } catch (const std::exception& e) {
        if (onError_) {
            onError_("system", "Broadcast transport state failed: " + std::string(e.what()));
        }
    }
}

void WebSocketServer::broadcastTrackUpdates() {
    try {
        // This would integrate with the actual track system
        WSMessage message(MessageType::TrackUpdate, json{
            {"tracks", json::array()}
        });
        message.messageId = generateMessageId();
        
        sendToSubscribers("tracks.updates", message);
        
    } catch (const std::exception& e) {
        if (onError_) {
            onError_("system", "Broadcast track updates failed: " + std::string(e.what()));
        }
    }
}

void WebSocketServer::startStateMonitoring() {
    shouldStopMonitoring_.store(false);
    stateMonitoringThread_ = std::thread([this]() {
        stateMonitoringLoop();
    });
}

void WebSocketServer::stopStateMonitoring() {
    shouldStopMonitoring_.store(true);
    if (stateMonitoringThread_.joinable()) {
        stateMonitoringThread_.join();
    }
}

// ========================================================================
// Subscription Management
// ========================================================================

core::VoidResult WebSocketServer::subscribeClient(const std::string& clientId, const std::string& topic) {
    try {
        // Validate topic
        if (std::find(AVAILABLE_TOPICS.begin(), AVAILABLE_TOPICS.end(), topic) == AVAILABLE_TOPICS.end()) {
            return core::VoidResult::failure("Invalid topic: " + topic);
        }
        
        auto client = getClient(clientId);
        if (!client) {
            return core::VoidResult::failure("Client not found: " + clientId);
        }
        
        client->subscriptions.insert(topic);
        
        // Update statistics
        updateStatistics("subscription_added");
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Subscribe client failed: " + std::string(e.what()));
    }
}

core::VoidResult WebSocketServer::unsubscribeClient(const std::string& clientId, const std::string& topic) {
    try {
        auto client = getClient(clientId);
        if (!client) {
            return core::VoidResult::failure("Client not found: " + clientId);
        }
        
        client->subscriptions.erase(topic);
        
        // Update statistics
        updateStatistics("subscription_removed");
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Unsubscribe client failed: " + std::string(e.what()));
    }
}

std::unordered_set<std::string> WebSocketServer::getClientSubscriptions(const std::string& clientId) const {
    auto client = getClient(clientId);
    return client ? client->subscriptions : std::unordered_set<std::string>{};
}

std::vector<std::string> WebSocketServer::getTopicSubscribers(const std::string& topic) const {
    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
    std::vector<std::string> subscribers;
    
    for (const auto& [id, client] : clients_) {
        if (client->subscriptions.count(topic)) {
            subscribers.push_back(id);
        }
    }
    
    return subscribers;
}

// ========================================================================
// Authentication and Authorization
// ========================================================================

void WebSocketServer::setAuthToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(configMutex_);
    authToken_ = token;
}

void WebSocketServer::clearAuthToken() {
    std::lock_guard<std::mutex> lock(configMutex_);
    authToken_.clear();
}

bool WebSocketServer::isAuthEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return !authToken_.empty();
}

core::VoidResult WebSocketServer::authenticateClient(const std::string& clientId, const std::string& token) {
    try {
        std::string authToken;
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            authToken = authToken_;
        }
        
        if (authToken.empty()) {
            return core::VoidResult::failure("Authentication not enabled");
        }
        
        auto client = getClient(clientId);
        if (!client) {
            return core::VoidResult::failure("Client not found: " + clientId);
        }
        
        if (token != authToken) {
            return core::VoidResult::failure("Invalid token");
        }
        
        client->authenticated.store(true);
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::failure("Authentication failed: " + std::string(e.what()));
    }
}

bool WebSocketServer::isClientAuthenticated(const std::string& clientId) const {
    auto client = getClient(clientId);
    return client ? client->authenticated.load() : false;
}

// ========================================================================
// Event Callbacks
// ========================================================================

void WebSocketServer::setOnClientConnected(ConnectionCallback callback) {
    onClientConnected_ = std::move(callback);
}

void WebSocketServer::setOnClientDisconnected(ConnectionCallback callback) {
    onClientDisconnected_ = std::move(callback);
}

void WebSocketServer::setOnMessageReceived(MessageCallback callback) {
    onMessageReceived_ = std::move(callback);
}

void WebSocketServer::setOnError(ErrorCallback callback) {
    onError_ = std::move(callback);
}

// ========================================================================
// Statistics and Monitoring
// ========================================================================

WebSocketServer::WSStatistics WebSocketServer::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    auto stats = statistics_;
    stats.currentConnections = getConnectedClientsCount();
    return stats;
}

void WebSocketServer::resetStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_ = WSStatistics{};
    statistics_.serverStartTime = std::chrono::system_clock::now();
    statistics_.currentConnections = getConnectedClientsCount();
}

// ========================================================================
// Configuration
// ========================================================================

void WebSocketServer::setServerConfig(const ServerConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

WebSocketServer::ServerConfig WebSocketServer::getServerConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

// ========================================================================
// Internal Implementation
// ========================================================================

void WebSocketServer::initializeServer() {
    setupHandlers();
    
    // Set logging to be less verbose
    wsServer_.clear_access_channels(websocketpp::log::alevel::all);
    wsServer_.set_access_channels(websocketpp::log::alevel::connect | websocketpp::log::alevel::disconnect);
    
    // Initialize Asio
    wsServer_.init_asio();
}

void WebSocketServer::setupHandlers() {
    wsServer_.set_validate_handler([this](connection_hdl hdl) {
        return onValidate(hdl);
    });
    
    wsServer_.set_open_handler([this](connection_hdl hdl) {
        onOpen(hdl);
    });
    
    wsServer_.set_close_handler([this](connection_hdl hdl) {
        onClose(hdl);
    });
    
    wsServer_.set_message_handler([this](connection_hdl hdl, message_ptr msg) {
        onMessage(hdl, msg);
    });
}

void WebSocketServer::onOpen(connection_hdl hdl) {
    try {
        auto conn = wsServer_.get_con_from_hdl(hdl);
        std::string clientId = generateClientId();
        
        auto client = std::make_shared<ClientInfo>();
        client->id = clientId;
        client->handle = hdl;
        client->userAgent = conn->get_request_header("User-Agent");
        client->origin = conn->get_origin();
        client->connectedAt = std::chrono::system_clock::now();
        
        {
            std::unique_lock<std::shared_mutex> lock(clientsMutex_);
            clients_[clientId] = client;
            connectionToClient_[hdl] = clientId;
        }
        
        updateStatistics("connection_opened");
        
        if (onClientConnected_) {
            onClientConnected_(clientId);
        }
        
    } catch (const std::exception& e) {
        if (onError_) {
            onError_("server", "Connection open failed: " + std::string(e.what()));
        }
    }
}

void WebSocketServer::onClose(connection_hdl hdl) {
    try {
        std::string clientId;
        {
            std::shared_lock<std::shared_mutex> lock(clientsMutex_);
            auto it = connectionToClient_.find(hdl);
            if (it != connectionToClient_.end()) {
                clientId = it->second;
            }
        }
        
        if (!clientId.empty()) {
            {
                std::unique_lock<std::shared_mutex> lock(clientsMutex_);
                clients_.erase(clientId);
                connectionToClient_.erase(hdl);
            }
            
            updateStatistics("connection_closed");
            
            if (onClientDisconnected_) {
                onClientDisconnected_(clientId);
            }
        }
        
    } catch (const std::exception& e) {
        if (onError_) {
            onError_("server", "Connection close failed: " + std::string(e.what()));
        }
    }
}

void WebSocketServer::onMessage(connection_hdl hdl, message_ptr msg) {
    try {
        std::string clientId;
        {
            std::shared_lock<std::shared_mutex> lock(clientsMutex_);
            auto it = connectionToClient_.find(hdl);
            if (it != connectionToClient_.end()) {
                clientId = it->second;
            }
        }
        
        if (clientId.empty()) {
            return;
        }
        
        // Parse message
        auto messageJson = json::parse(msg->get_payload());
        auto wsMessage = WSMessage::fromJson(messageJson);
        
        // Add to processing queue
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            messageQueue_.emplace(clientId, wsMessage);
        }
        queueCondition_.notify_one();
        
        updateStatistics("message_received", &wsMessage);
        
        if (onMessageReceived_) {
            onMessageReceived_(clientId, wsMessage);
        }
        
    } catch (const json::parse_error& e) {
        if (onError_) {
            onError_("unknown", "JSON parse error: " + std::string(e.what()));
        }
    } catch (const std::exception& e) {
        if (onError_) {
            onError_("unknown", "Message processing error: " + std::string(e.what()));
        }
    }
}

bool WebSocketServer::onValidate(connection_hdl hdl) {
    try {
        auto conn = wsServer_.get_con_from_hdl(hdl);
        
        // Check max connections
        if (getConnectedClientsCount() >= static_cast<size_t>(config_.maxConnections)) {
            return false;
        }
        
        // Check origin if configured
        std::string origin = conn->get_origin();
        if (config_.corsOrigin != "*" && origin != config_.corsOrigin) {
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (onError_) {
            onError_("server", "Validation failed: " + std::string(e.what()));
        }
        return false;
    }
}

void WebSocketServer::processMessage(const std::string& clientId, const WSMessage& message) {
    try {
        switch (message.type) {
            case MessageType::ExecuteAction:
                handleExecuteAction(clientId, message);
                break;
                
            case MessageType::Subscribe:
                handleSubscribe(clientId, message);
                break;
                
            case MessageType::Unsubscribe:
                handleUnsubscribe(clientId, message);
                break;
                
            case MessageType::Authenticate:
                handleAuthenticate(clientId, message);
                break;
                
            case MessageType::Ping:
                handlePing(clientId, message);
                break;
                
            default:
                if (onError_) {
                    onError_(clientId, "Unhandled message type: " + WSMessage::messageTypeToString(message.type));
                }
                break;
        }
        
    } catch (const std::exception& e) {
        sendError(clientId, message.messageId, "Message processing failed: " + std::string(e.what()));
    }
}

void WebSocketServer::handleExecuteAction(const std::string& clientId, const WSMessage& message) {
    try {
        // Check authentication if enabled
        if (isAuthEnabled() && !isClientAuthenticated(clientId)) {
            sendError(clientId, message.messageId, "Authentication required");
            return;
        }
        
        // Extract action parameters
        if (!message.payload.contains("actionType") || !message.payload["actionType"].is_string()) {
            sendError(clientId, message.messageId, "Missing or invalid actionType");
            return;
        }
        
        std::string actionType = message.payload["actionType"];
        json parameters = message.payload.value("parameters", json::object());
        
        // Build action context
        ActionContext context;
        context.requestId = message.messageId;
        context.sessionId = "ws_" + clientId;
        
        // Execute action
        auto result = actionAPI_->executeAction(actionType, parameters, context);
        
        // Send result
        sendActionResult(clientId, message.messageId, result);
        
    } catch (const std::exception& e) {
        sendError(clientId, message.messageId, "Action execution failed: " + std::string(e.what()));
    }
}

void WebSocketServer::handleSubscribe(const std::string& clientId, const WSMessage& message) {
    try {
        if (!message.payload.contains("topic") || !message.payload["topic"].is_string()) {
            sendError(clientId, message.messageId, "Missing or invalid topic");
            return;
        }
        
        std::string topic = message.payload["topic"];
        auto result = subscribeClient(clientId, topic);
        
        if (result.success) {
            WSMessage response(MessageType::ActionResult, json{
                {"messageId", message.messageId},
                {"success", true},
                {"result", {{"subscribed", topic}}}
            });
            response.messageId = generateMessageId();
            sendMessage(clientId, response);
        } else {
            sendError(clientId, message.messageId, result.error);
        }
        
    } catch (const std::exception& e) {
        sendError(clientId, message.messageId, "Subscribe failed: " + std::string(e.what()));
    }
}

void WebSocketServer::handleUnsubscribe(const std::string& clientId, const WSMessage& message) {
    try {
        if (!message.payload.contains("topic") || !message.payload["topic"].is_string()) {
            sendError(clientId, message.messageId, "Missing or invalid topic");
            return;
        }
        
        std::string topic = message.payload["topic"];
        auto result = unsubscribeClient(clientId, topic);
        
        if (result.success) {
            WSMessage response(MessageType::ActionResult, json{
                {"messageId", message.messageId},
                {"success", true},
                {"result", {{"unsubscribed", topic}}}
            });
            response.messageId = generateMessageId();
            sendMessage(clientId, response);
        } else {
            sendError(clientId, message.messageId, result.error);
        }
        
    } catch (const std::exception& e) {
        sendError(clientId, message.messageId, "Unsubscribe failed: " + std::string(e.what()));
    }
}

void WebSocketServer::handleAuthenticate(const std::string& clientId, const WSMessage& message) {
    try {
        if (!message.payload.contains("token") || !message.payload["token"].is_string()) {
            sendError(clientId, message.messageId, "Missing or invalid token");
            return;
        }
        
        std::string token = message.payload["token"];
        auto result = authenticateClient(clientId, token);
        
        WSMessage response(MessageType::AuthResult, json{
            {"messageId", message.messageId},
            {"success", result.success},
            {"authenticated", result.success}
        });
        
        if (!result.success) {
            response.payload["error"] = result.error;
        }
        
        response.messageId = generateMessageId();
        sendMessage(clientId, response);
        
    } catch (const std::exception& e) {
        sendError(clientId, message.messageId, "Authentication failed: " + std::string(e.what()));
    }
}

void WebSocketServer::handlePing(const std::string& clientId, const WSMessage& message) {
    WSMessage pong(MessageType::Pong, json{
        {"messageId", message.messageId},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    });
    pong.messageId = generateMessageId();
    
    sendMessage(clientId, pong);
}

std::string WebSocketServer::generateClientId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "client_";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

std::string WebSocketServer::generateMessageId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "msg_";
    for (int i = 0; i < 12; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

void WebSocketServer::updateStatistics(const std::string& event, const WSMessage* message) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    statistics_.lastActivity = std::chrono::system_clock::now();
    
    if (event == "connection_opened") {
        statistics_.totalConnections++;
    } else if (event == "message_sent") {
        statistics_.messagesSent++;
        if (message) {
            statistics_.messageTypeCounts[WSMessage::messageTypeToString(message->type)]++;
        }
    } else if (event == "message_received") {
        statistics_.messagesReceived++;
        if (message) {
            statistics_.messageTypeCounts[WSMessage::messageTypeToString(message->type)]++;
        }
    } else if (event == "broadcast_sent") {
        statistics_.broadcastsSent++;
    } else if (event == "topic_sent") {
        if (message) {
            statistics_.topicCounts["topic_message"]++;
        }
    }
}

void WebSocketServer::stateMonitoringLoop() {
    while (!shouldStopMonitoring_.load()) {
        try {
            if (isRunning()) {
                broadcastSessionState();
                broadcastTransportState();
                broadcastTrackUpdates();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(broadcastIntervalMs_));
            
        } catch (const std::exception& e) {
            if (onError_) {
                onError_("monitoring", "State monitoring error: " + std::string(e.what()));
            }
            
            // Wait a bit before retrying
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

} // namespace mixmind::api