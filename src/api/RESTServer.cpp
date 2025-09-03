#include "RESTServer.h"
#include "../core/async.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <regex>

namespace mixmind::api {

// ============================================================================
// RESTServer Implementation
// ============================================================================

RESTServer::RESTServer(std::shared_ptr<ActionAPI> actionAPI)
    : actionAPI_(std::move(actionAPI))
    , server_(std::make_unique<httplib::Server>())
    , host_(DEFAULT_HOST)
    , port_(DEFAULT_PORT)
{
    if (!actionAPI_) {
        throw std::invalid_argument("ActionAPI cannot be null");
    }
    
    setupEndpoints();
    
    // Initialize statistics
    statistics_.serverStartTime = std::chrono::system_clock::now();
}

RESTServer::~RESTServer() {
    if (isRunning()) {
        stop().get();
    }
}

// ========================================================================
// Server Management
// ========================================================================

core::AsyncResult<core::VoidResult> RESTServer::start(const std::string& host, int port) {
    return executeAsync<core::VoidResult>([this, host, port]() -> core::VoidResult {
        try {
            if (isRunning()) {
                return core::VoidResult::failure("Server is already running");
            }
            
            host_ = host;
            port_ = port;
            
            // Configure server
            server_->set_read_timeout(30);  // 30 seconds
            server_->set_write_timeout(30); // 30 seconds
            server_->set_keep_alive_max_count(100);
            
            // Start server in separate thread
            serverThread_ = std::thread([this]() {
                if (server_->listen(host_.c_str(), port_)) {
                    isRunning_.store(true);
                } else {
                    if (errorCallback_) {
                        errorCallback_("server_start", "Failed to start server on " + host_ + ":" + std::to_string(port_));
                    }
                }
            });
            
            // Wait a bit and check if server started successfully
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (!server_->is_running()) {
                if (serverThread_.joinable()) {
                    serverThread_.join();
                }
                return core::VoidResult::failure("Failed to start server on " + host + ":" + std::to_string(port));
            }
            
            isRunning_.store(true);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Server start failed: " + std::string(e.what()));
        }
    });
}

core::AsyncResult<core::VoidResult> RESTServer::stop() {
    return executeAsync<core::VoidResult>([this]() -> core::VoidResult {
        try {
            if (!isRunning()) {
                return core::VoidResult::success();
            }
            
            server_->stop();
            
            if (serverThread_.joinable()) {
                serverThread_.join();
            }
            
            isRunning_.store(false);
            return core::VoidResult::success();
            
        } catch (const std::exception& e) {
            return core::VoidResult::failure("Server stop failed: " + std::string(e.what()));
        }
    });
}

bool RESTServer::isRunning() const {
    return isRunning_.load();
}

std::string RESTServer::getHost() const {
    return host_;
}

int RESTServer::getPort() const {
    return port_;
}

std::string RESTServer::getServerURL() const {
    return "http://" + host_ + ":" + std::to_string(port_);
}

// ========================================================================
// Configuration
// ========================================================================

void RESTServer::setCORSSettings(const CORSSettings& settings) {
    std::lock_guard<std::mutex> lock(configMutex_);
    corsSettings_ = settings;
}

RESTServer::CORSSettings RESTServer::getCORSSettings() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return corsSettings_;
}

void RESTServer::setRequestLoggingEnabled(bool enabled) {
    requestLoggingEnabled_.store(enabled);
}

bool RESTServer::isRequestLoggingEnabled() const {
    return requestLoggingEnabled_.load();
}

void RESTServer::setAuthToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(configMutex_);
    authToken_ = token;
}

void RESTServer::clearAuthToken() {
    std::lock_guard<std::mutex> lock(configMutex_);
    authToken_.clear();
}

bool RESTServer::isAuthEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return !authToken_.empty();
}

// ========================================================================
// Middleware and Hooks
// ========================================================================

void RESTServer::addRequestMiddleware(RequestMiddleware middleware) {
    std::lock_guard<std::mutex> lock(middlewareMutex_);
    requestMiddleware_.push_back(std::move(middleware));
}

void RESTServer::addResponseMiddleware(ResponseMiddleware middleware) {
    std::lock_guard<std::mutex> lock(middlewareMutex_);
    responseMiddleware_.push_back(std::move(middleware));
}

void RESTServer::clearMiddleware() {
    std::lock_guard<std::mutex> lock(middlewareMutex_);
    requestMiddleware_.clear();
    responseMiddleware_.clear();
}

// ========================================================================
// Statistics and Monitoring
// ========================================================================

RESTServer::ServerStatistics RESTServer::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

void RESTServer::resetStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_ = ServerStatistics{};
    statistics_.serverStartTime = std::chrono::system_clock::now();
}

void RESTServer::setRequestCallback(RequestCallback callback) {
    requestCallback_ = std::move(callback);
}

void RESTServer::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = std::move(callback);
}

void RESTServer::setDetailedErrorsEnabled(bool enabled) {
    detailedErrorsEnabled_.store(enabled);
}

bool RESTServer::areDetailedErrorsEnabled() const {
    return detailedErrorsEnabled_.load();
}

// ========================================================================
// Internal Implementation
// ========================================================================

void RESTServer::setupEndpoints() {
    setupActionEndpoints();
    setupInfoEndpoints();
    setupStateEndpoints();
    setupUtilityEndpoints();
    
    // Setup OPTIONS handler for CORS
    server_->Options(".*", [this](const httplib::Request& req, httplib::Response& res) {
        handleCORSPreflight(req, res);
    });
    
    // Setup global pre-handler for middleware and auth
    server_->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Apply CORS headers
        applyCORSHeaders(res);
        
        // Build request context
        auto context = buildRequestContext(req);
        
        // Apply request middleware
        {
            std::lock_guard<std::mutex> lock(middlewareMutex_);
            for (const auto& middleware : requestMiddleware_) {
                if (!middleware(context, res)) {
                    return httplib::Server::HandlerResponse::Done;
                }
            }
        }
        
        // Authenticate request
        if (!authenticateRequest(req, res)) {
            return httplib::Server::HandlerResponse::Done;
        }
        
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // Setup global post-handler for response processing
    server_->set_post_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        auto context = buildRequestContext(req);
        
        // Convert httplib::Response to our Response type
        Response response;
        response.status = res.status;
        
        try {
            if (!res.body.empty()) {
                response.body = json::parse(res.body);
            }
        } catch (const json::parse_error&) {
            // Body is not JSON, leave as empty object
        }
        
        for (const auto& [key, value] : res.headers) {
            response.headers[key] = value;
        }
        
        // Apply response middleware
        {
            std::lock_guard<std::mutex> lock(middlewareMutex_);
            for (const auto& middleware : responseMiddleware_) {
                middleware(context, response);
            }
        }
        
        // Update response
        res.status = response.status;
        if (!response.body.empty()) {
            res.body = response.body.dump();
            res.headers.emplace("Content-Type", "application/json");
        }
        
        for (const auto& [key, value] : response.headers) {
            res.headers[key] = value;
        }
        
        // Calculate response time and update statistics
        auto endTime = std::chrono::high_resolution_clock::now();
        auto responseTimeMs = std::chrono::duration<double, std::milli>(endTime - std::chrono::high_resolution_clock::now()).count();
        
        updateStatistics(context, response, responseTimeMs);
        
        // Log request
        if (isRequestLoggingEnabled()) {
            logRequest(context, response);
        }
        
        // Call request callback
        if (requestCallback_) {
            requestCallback_(context, response);
        }
    });
}

void RESTServer::setupActionEndpoints() {
    // Execute action
    server_->Post("/api/actions/execute", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto requestJson = json::parse(req.body);
            
            if (!requestJson.contains("actionType") || !requestJson["actionType"].is_string()) {
                auto response = Response::error(400, "Missing or invalid actionType");
                res.status = response.status;
                res.body = response.body.dump();
                return;
            }
            
            std::string actionType = requestJson["actionType"];
            json parameters = requestJson.value("parameters", json::object());
            
            auto context = extractActionContext(req);
            auto result = actionAPI_->executeAction(actionType, parameters, context);
            
            auto response = actionResultToResponse(result);
            res.status = response.status;
            res.body = response.body.dump();
            
        } catch (const json::parse_error& e) {
            auto response = Response::error(400, "Invalid JSON in request body");
            res.status = response.status;
            res.body = response.body.dump();
        } catch (const std::exception& e) {
            auto response = Response::error(500, "Internal server error");
            if (areDetailedErrorsEnabled()) {
                response.body["error"]["details"] = e.what();
            }
            res.status = response.status;
            res.body = response.body.dump();
        }
    });
    
    // Get available actions
    server_->Get("/api/actions", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto actions = actionAPI_->getAvailableActions();
            auto response = Response::success(json{{"actions", actions}});
            res.status = response.status;
            res.body = response.body.dump();
        } catch (const std::exception& e) {
            auto response = Response::error(500, "Failed to get available actions");
            if (areDetailedErrorsEnabled()) {
                response.body["error"]["details"] = e.what();
            }
            res.status = response.status;
            res.body = response.body.dump();
        }
    });
    
    // Get action schema
    server_->Get(R"(/api/actions/([^/]+)/schema)", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string actionType = req.matches[1];
            auto schema = actionAPI_->getActionSchema(actionType);
            
            if (schema.empty()) {
                auto response = Response::error(404, "Action type not found: " + actionType);
                res.status = response.status;
                res.body = response.body.dump();
                return;
            }
            
            auto response = Response::success(json{{"schema", schema}});
            res.status = response.status;
            res.body = response.body.dump();
        } catch (const std::exception& e) {
            auto response = Response::error(500, "Failed to get action schema");
            if (areDetailedErrorsEnabled()) {
                response.body["error"]["details"] = e.what();
            }
            res.status = response.status;
            res.body = response.body.dump();
        }
    });
}

void RESTServer::setupInfoEndpoints() {
    // Get server info
    server_->Get("/api/info", [this](const httplib::Request& req, httplib::Response& res) {
        json info = {
            {"name", "MixMind AI REST API"},
            {"version", "1.0.0"},
            {"host", getHost()},
            {"port", getPort()},
            {"url", getServerURL()},
            {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - statistics_.serverStartTime).count()}
        };
        
        auto response = Response::success(info);
        res.status = response.status;
        res.body = response.body.dump();
    });
    
    // Get API statistics
    server_->Get("/api/stats", [this](const httplib::Request& req, httplib::Response& res) {
        auto stats = getStatistics();
        json statsJson = {
            {"total_requests", stats.totalRequests},
            {"successful_requests", stats.successfulRequests},
            {"error_requests", stats.errorRequests},
            {"average_response_time_ms", stats.averageResponseTimeMs},
            {"max_response_time_ms", stats.maxResponseTimeMs},
            {"endpoint_counts", stats.endpointCounts},
            {"status_code_counts", stats.statusCodeCounts},
            {"server_start_time", std::chrono::duration_cast<std::chrono::seconds>(
                stats.serverStartTime.time_since_epoch()).count()}
        };
        
        auto response = Response::success(statsJson);
        res.status = response.status;
        res.body = response.body.dump();
    });
    
    // Health check
    server_->Get("/api/health", [this](const httplib::Request& req, httplib::Response& res) {
        json health = {
            {"status", "healthy"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        auto response = Response::success(health);
        res.status = response.status;
        res.body = response.body.dump();
    });
}

void RESTServer::setupStateEndpoints() {
    // Get session state
    server_->Get("/api/session/state", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto context = extractActionContext(req);
            auto state = actionAPI_->getSessionState(context);
            auto response = Response::success(json{{"state", state}});
            res.status = response.status;
            res.body = response.body.dump();
        } catch (const std::exception& e) {
            auto response = Response::error(500, "Failed to get session state");
            if (areDetailedErrorsEnabled()) {
                response.body["error"]["details"] = e.what();
            }
            res.status = response.status;
            res.body = response.body.dump();
        }
    });
    
    // Update session state
    server_->Put("/api/session/state", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto requestJson = json::parse(req.body);
            auto context = extractActionContext(req);
            auto result = actionAPI_->updateSessionState(requestJson, context);
            
            if (result.success) {
                auto response = Response::success();
                res.status = response.status;
                res.body = response.body.dump();
            } else {
                auto response = Response::error(400, result.error);
                res.status = response.status;
                res.body = response.body.dump();
            }
        } catch (const json::parse_error& e) {
            auto response = Response::error(400, "Invalid JSON in request body");
            res.status = response.status;
            res.body = response.body.dump();
        } catch (const std::exception& e) {
            auto response = Response::error(500, "Failed to update session state");
            if (areDetailedErrorsEnabled()) {
                response.body["error"]["details"] = e.what();
            }
            res.status = response.status;
            res.body = response.body.dump();
        }
    });
}

void RESTServer::setupUtilityEndpoints() {
    // Validate action
    server_->Post("/api/validate", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto requestJson = json::parse(req.body);
            
            if (!requestJson.contains("actionType") || !requestJson["actionType"].is_string()) {
                auto response = Response::error(400, "Missing or invalid actionType");
                res.status = response.status;
                res.body = response.body.dump();
                return;
            }
            
            std::string actionType = requestJson["actionType"];
            json parameters = requestJson.value("parameters", json::object());
            
            auto validation = actionAPI_->validateAction(actionType, parameters);
            auto response = Response::success(json{{"validation", validation}});
            res.status = response.status;
            res.body = response.body.dump();
            
        } catch (const json::parse_error& e) {
            auto response = Response::error(400, "Invalid JSON in request body");
            res.status = response.status;
            res.body = response.body.dump();
        } catch (const std::exception& e) {
            auto response = Response::error(500, "Validation failed");
            if (areDetailedErrorsEnabled()) {
                response.body["error"]["details"] = e.what();
            }
            res.status = response.status;
            res.body = response.body.dump();
        }
    });
    
    // Reset statistics
    server_->Post("/api/stats/reset", [this](const httplib::Request& req, httplib::Response& res) {
        resetStatistics();
        auto response = Response::success();
        res.status = response.status;
        res.body = response.body.dump();
    });
}

void RESTServer::handleCORSPreflight(const httplib::Request& req, httplib::Response& res) {
    applyCORSHeaders(res);
    res.status = 200;
}

void RESTServer::applyCORSHeaders(httplib::Response& res) {
    auto settings = getCORSSettings();
    if (!settings.enabled) {
        return;
    }
    
    res.headers.emplace("Access-Control-Allow-Origin", settings.allowOrigin);
    res.headers.emplace("Access-Control-Allow-Methods", settings.allowMethods);
    res.headers.emplace("Access-Control-Allow-Headers", settings.allowHeaders);
    res.headers.emplace("Access-Control-Max-Age", std::to_string(settings.maxAge));
}

bool RESTServer::authenticateRequest(const httplib::Request& req, httplib::Response& res) {
    if (!isAuthEnabled()) {
        return true; // No auth required
    }
    
    std::string authToken;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        authToken = authToken_;
    }
    
    auto authHeader = req.get_header_value("Authorization");
    if (authHeader.empty()) {
        auto response = Response::error(401, "Authorization header required");
        res.status = response.status;
        res.body = response.body.dump();
        return false;
    }
    
    std::string expectedAuth = "Bearer " + authToken;
    if (authHeader != expectedAuth) {
        auto response = Response::error(401, "Invalid authorization token");
        res.status = response.status;
        res.body = response.body.dump();
        return false;
    }
    
    return true;
}

ActionContext RESTServer::extractActionContext(const httplib::Request& req) {
    ActionContext context;
    context.requestId = req.get_header_value("X-Request-ID");
    if (context.requestId.empty()) {
        context.requestId = "req_" + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    }
    
    context.sessionId = req.get_header_value("X-Session-ID");
    context.clientInfo.userAgent = req.get_header_value("User-Agent");
    context.clientInfo.ipAddress = req.get_header_value("X-Forwarded-For");
    if (context.clientInfo.ipAddress.empty()) {
        context.clientInfo.ipAddress = "127.0.0.1"; // Default for local requests
    }
    
    return context;
}

RESTServer::Response RESTServer::actionResultToResponse(const ActionResult& result) {
    if (result.success) {
        json responseBody = {
            {"success", true},
            {"result", result.data}
        };
        
        if (!result.metadata.empty()) {
            responseBody["metadata"] = result.metadata;
        }
        
        return Response::success(responseBody);
    } else {
        return Response::error(400, result.error, result.errorCode);
    }
}

void RESTServer::logRequest(const RequestContext& context, const Response& response) {
    std::ostringstream log;
    log << "[" << std::put_time(std::localtime(&std::chrono::system_clock::to_time_t(context.timestamp)), "%Y-%m-%d %H:%M:%S") << "] ";
    log << context.method << " " << context.path;
    log << " - " << response.status;
    log << " - " << context.clientIP;
    log << " - " << context.userAgent;
    
    std::cout << log.str() << std::endl;
}

void RESTServer::updateStatistics(const RequestContext& context, const Response& response, double responseTimeMs) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    statistics_.totalRequests++;
    statistics_.lastRequest = context.timestamp;
    
    if (response.status >= 200 && response.status < 400) {
        statistics_.successfulRequests++;
    } else {
        statistics_.errorRequests++;
    }
    
    // Update response time statistics
    if (statistics_.totalRequests == 1) {
        statistics_.averageResponseTimeMs = responseTimeMs;
        statistics_.maxResponseTimeMs = responseTimeMs;
    } else {
        double totalTime = statistics_.averageResponseTimeMs * (statistics_.totalRequests - 1) + responseTimeMs;
        statistics_.averageResponseTimeMs = totalTime / statistics_.totalRequests;
        statistics_.maxResponseTimeMs = std::max(statistics_.maxResponseTimeMs, responseTimeMs);
    }
    
    // Update endpoint counts
    statistics_.endpointCounts[context.path]++;
    
    // Update status code counts
    statistics_.statusCodeCounts[response.status]++;
}

RESTServer::RequestContext RESTServer::buildRequestContext(const httplib::Request& req) {
    RequestContext context;
    context.method = req.method;
    context.path = req.path;
    context.userAgent = req.get_header_value("User-Agent");
    context.clientIP = req.get_header_value("X-Forwarded-For");
    if (context.clientIP.empty()) {
        context.clientIP = "127.0.0.1";
    }
    
    for (const auto& [key, value] : req.headers) {
        context.headers[key] = value;
    }
    
    for (const auto& [key, value] : req.params) {
        context.params[key] = value;
    }
    
    context.timestamp = std::chrono::system_clock::now();
    
    return context;
}

// ============================================================================
// APIDocGenerator Implementation
// ============================================================================

APIDocGenerator::APIDocGenerator(std::shared_ptr<ActionAPI> actionAPI)
    : actionAPI_(std::move(actionAPI))
{
    if (!actionAPI_) {
        throw std::invalid_argument("ActionAPI cannot be null");
    }
}

json APIDocGenerator::generateOpenAPISpec() const {
    json spec = {
        {"openapi", "3.0.0"},
        {"info", {
            {"title", "MixMind AI REST API"},
            {"version", "1.0.0"},
            {"description", "REST API for MixMind AI DAW automation system"},
            {"contact", {
                {"name", "MixMind AI Support"}
            }}
        }},
        {"servers", json::array({
            {{"url", "http://localhost:8080"}, {"description", "Development server"}}
        })},
        {"paths", json::object()},
        {"components", {
            {"schemas", json::object()},
            {"securitySchemes", {
                {"bearerAuth", {
                    {"type", "http"},
                    {"scheme", "bearer"},
                    {"bearerFormat", "JWT"}
                }}
            }}
        }}
    };
    
    // Add action endpoints
    auto actions = actionAPI_->getAvailableActions();
    for (const auto& action : actions) {
        // Add schema for each action
        auto schema = actionAPI_->getActionSchema(action);
        if (!schema.empty()) {
            spec["components"]["schemas"][action + "Parameters"] = schema;
        }
    }
    
    // Define standard paths
    spec["paths"]["/api/actions/execute"] = {
        {"post", {
            {"summary", "Execute an action"},
            {"requestBody", {
                {"required", true},
                {"content", {
                    {"application/json", {
                        {"schema", {
                            {"type", "object"},
                            {"properties", {
                                {"actionType", {"type", "string"}},
                                {"parameters", {"type", "object"}}
                            }},
                            {"required", json::array({"actionType"})}
                        }}
                    }}
                }}
            }},
            {"responses", {
                {"200", {
                    {"description", "Action executed successfully"},
                    {"content", {
                        {"application/json", {
                            {"schema", {
                                {"type", "object"},
                                {"properties", {
                                    {"success", {"type", "boolean"}},
                                    {"result", {"type", "object"}},
                                    {"metadata", {"type", "object"}}
                                }}
                            }}
                        }}
                    }}
                }},
                {"400", {"description", "Bad request"}},
                {"500", {"description", "Internal server error"}}
            }}
        }}
    };
    
    spec["paths"]["/api/actions"] = {
        {"get", {
            {"summary", "Get available actions"},
            {"responses", {
                {"200", {
                    {"description", "List of available actions"},
                    {"content", {
                        {"application/json", {
                            {"schema", {
                                {"type", "object"},
                                {"properties", {
                                    {"actions", {
                                        {"type", "array"},
                                        {"items", {"type", "string"}}
                                    }}
                                }}
                            }}
                        }}
                    }}
                }}
            }}
        }}
    };
    
    return spec;
}

std::string APIDocGenerator::generateHTMLDocs() const {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MixMind AI REST API Documentation</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; line-height: 1.6; }
        .endpoint { margin: 20px 0; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }
        .method { font-weight: bold; padding: 5px 10px; border-radius: 3px; color: white; }
        .get { background-color: #61affe; }
        .post { background-color: #49cc90; }
        .put { background-color: #fca130; }
        .delete { background-color: #f93e3e; }
        code { background-color: #f4f4f4; padding: 2px 5px; border-radius: 3px; }
        pre { background-color: #f4f4f4; padding: 15px; border-radius: 5px; overflow-x: auto; }
    </style>
</head>
<body>
    <h1>MixMind AI REST API Documentation</h1>
    <p>This API provides HTTP endpoints for controlling the MixMind AI DAW automation system.</p>
    
    <h2>Authentication</h2>
    <p>If authentication is enabled, include the Authorization header with Bearer token:</p>
    <pre>Authorization: Bearer your-token-here</pre>
    
    <h2>Endpoints</h2>
)";
    
    // Add action endpoints documentation
    html << R"(
    <div class="endpoint">
        <h3><span class="method post">POST</span> /api/actions/execute</h3>
        <p>Execute an action in the DAW.</p>
        <h4>Request Body:</h4>
        <pre>{
  "actionType": "string",
  "parameters": {}
}</pre>
        <h4>Response:</h4>
        <pre>{
  "success": true,
  "result": {},
  "metadata": {}
}</pre>
    </div>
    
    <div class="endpoint">
        <h3><span class="method get">GET</span> /api/actions</h3>
        <p>Get list of available actions.</p>
        <h4>Response:</h4>
        <pre>{
  "actions": ["action1", "action2", ...]
}</pre>
    </div>
    
    <div class="endpoint">
        <h3><span class="method get">GET</span> /api/info</h3>
        <p>Get server information.</p>
        <h4>Response:</h4>
        <pre>{
  "name": "MixMind AI REST API",
  "version": "1.0.0",
  "host": "localhost",
  "port": 8080,
  "url": "http://localhost:8080",
  "uptime_seconds": 3600
}</pre>
    </div>
    
    <div class="endpoint">
        <h3><span class="method get">GET</span> /api/health</h3>
        <p>Health check endpoint.</p>
        <h4>Response:</h4>
        <pre>{
  "status": "healthy",
  "timestamp": 1640995200
}</pre>
    </div>
)";
    
    html << R"(
</body>
</html>)";
    
    return html.str();
}

std::string APIDocGenerator::generateMarkdownDocs() const {
    std::ostringstream md;
    md << "# MixMind AI REST API Documentation\n\n";
    md << "This API provides HTTP endpoints for controlling the MixMind AI DAW automation system.\n\n";
    
    md << "## Authentication\n\n";
    md << "If authentication is enabled, include the Authorization header with Bearer token:\n\n";
    md << "```\nAuthorization: Bearer your-token-here\n```\n\n";
    
    md << "## Endpoints\n\n";
    
    md << "### POST /api/actions/execute\n\n";
    md << "Execute an action in the DAW.\n\n";
    md << "**Request Body:**\n";
    md << "```json\n{\n  \"actionType\": \"string\",\n  \"parameters\": {}\n}\n```\n\n";
    md << "**Response:**\n";
    md << "```json\n{\n  \"success\": true,\n  \"result\": {},\n  \"metadata\": {}\n}\n```\n\n";
    
    md << "### GET /api/actions\n\n";
    md << "Get list of available actions.\n\n";
    md << "**Response:**\n";
    md << "```json\n{\n  \"actions\": [\"action1\", \"action2\", ...]\n}\n```\n\n";
    
    md << "### GET /api/info\n\n";
    md << "Get server information.\n\n";
    md << "**Response:**\n";
    md << "```json\n{\n  \"name\": \"MixMind AI REST API\",\n  \"version\": \"1.0.0\",\n  \"host\": \"localhost\",\n  \"port\": 8080,\n  \"url\": \"http://localhost:8080\",\n  \"uptime_seconds\": 3600\n}\n```\n\n";
    
    md << "### GET /api/health\n\n";
    md << "Health check endpoint.\n\n";
    md << "**Response:**\n";
    md << "```json\n{\n  \"status\": \"healthy\",\n  \"timestamp\": 1640995200\n}\n```\n\n";
    
    return md.str();
}

json APIDocGenerator::generatePostmanCollection() const {
    json collection = {
        {"info", {
            {"name", "MixMind AI REST API"},
            {"description", "Postman collection for MixMind AI DAW automation API"},
            {"version", "1.0.0"}
        }},
        {"item", json::array()}
    };
    
    // Add execute action request
    collection["item"].push_back({
        {"name", "Execute Action"},
        {"request", {
            {"method", "POST"},
            {"header", json::array({
                {{"key", "Content-Type"}, {"value", "application/json"}}
            })},
            {"url", {
                {"raw", "{{baseUrl}}/api/actions/execute"},
                {"host", json::array({"{{baseUrl}}"})},
                {"path", json::array({"api", "actions", "execute"})}
            }},
            {"body", {
                {"mode", "raw"},
                {"raw", "{\n  \"actionType\": \"transport.play\",\n  \"parameters\": {}\n}"}
            }}
        }}
    });
    
    // Add get actions request
    collection["item"].push_back({
        {"name", "Get Available Actions"},
        {"request", {
            {"method", "GET"},
            {"url", {
                {"raw", "{{baseUrl}}/api/actions"},
                {"host", json::array({"{{baseUrl}}"})},
                {"path", json::array({"api", "actions"})}
            }}
        }}
    });
    
    // Add variable for base URL
    collection["variable"] = json::array({
        {{"key", "baseUrl"}, {"value", "http://localhost:8080"}}
    });
    
    return collection;
}

} // namespace mixmind::api