#pragma once

#include "ActionAPI.h"
#include "../core/types.h"
#include "../core/result.h"
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

namespace mixmind::api {

using json = nlohmann::json;

// ============================================================================
// REST Server - HTTP interface for the Action API
// ============================================================================

class RESTServer {
public:
    explicit RESTServer(std::shared_ptr<ActionAPI> actionAPI);
    ~RESTServer();
    
    // Non-copyable, non-movable
    RESTServer(const RESTServer&) = delete;
    RESTServer& operator=(const RESTServer&) = delete;
    RESTServer(RESTServer&&) = delete;
    RESTServer& operator=(RESTServer&&) = delete;
    
    // ========================================================================
    // Server Management
    // ========================================================================
    
    /// Start the REST server
    core::AsyncResult<core::VoidResult> start(
        const std::string& host = "localhost",
        int port = 8080
    );
    
    /// Stop the REST server
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
    // Configuration
    // ========================================================================
    
    /// CORS settings
    struct CORSSettings {
        bool enabled = true;
        std::string allowOrigin = "*";
        std::string allowMethods = "GET, POST, PUT, DELETE, OPTIONS";
        std::string allowHeaders = "Content-Type, Authorization";
        int maxAge = 86400; // 24 hours
    };
    
    /// Set CORS configuration
    void setCORSSettings(const CORSSettings& settings);
    
    /// Get CORS configuration
    CORSSettings getCORSSettings() const;
    
    /// Enable/disable request logging
    void setRequestLoggingEnabled(bool enabled);
    
    /// Check if request logging is enabled
    bool isRequestLoggingEnabled() const;
    
    /// Set authentication token (simple bearer token auth)
    void setAuthToken(const std::string& token);
    
    /// Clear authentication token
    void clearAuthToken();
    
    /// Check if authentication is enabled
    bool isAuthEnabled() const;
    
    // ========================================================================
    // Request/Response Types
    // ========================================================================
    
    /// HTTP request context
    struct RequestContext {
        std::string method;
        std::string path;
        std::string userAgent;
        std::string clientIP;
        std::unordered_map<std::string, std::string> headers;
        std::unordered_map<std::string, std::string> params;
        std::chrono::system_clock::time_point timestamp;
        
        RequestContext() : timestamp(std::chrono::system_clock::now()) {}
    };
    
    /// HTTP response
    struct Response {
        int status = 200;
        json body = json::object();
        std::unordered_map<std::string, std::string> headers;
        
        static Response success(const json& data = json::object()) {
            return Response{200, data, {}};
        }
        
        static Response error(int status, const std::string& message, const std::string& code = "") {
            json errorBody = json{
                {"success", false},
                {"error", {
                    {"message", message},
                    {"code", code}
                }}
            };
            return Response{status, errorBody, {}};
        }
    };
    
    // ========================================================================
    // Middleware and Hooks
    // ========================================================================
    
    /// Request middleware function type
    using RequestMiddleware = std::function<bool(const RequestContext& context, httplib::Response& response)>;
    
    /// Response middleware function type
    using ResponseMiddleware = std::function<void(const RequestContext& context, Response& response)>;
    
    /// Add request middleware (returns false to abort request)
    void addRequestMiddleware(RequestMiddleware middleware);
    
    /// Add response middleware
    void addResponseMiddleware(ResponseMiddleware middleware);
    
    /// Clear all middleware
    void clearMiddleware();
    
    // ========================================================================
    // Statistics and Monitoring
    // ========================================================================
    
    struct ServerStatistics {
        int64_t totalRequests = 0;
        int64_t successfulRequests = 0;
        int64_t errorRequests = 0;
        double averageResponseTimeMs = 0.0;
        double maxResponseTimeMs = 0.0;
        std::unordered_map<std::string, int64_t> endpointCounts;
        std::unordered_map<int, int64_t> statusCodeCounts;
        std::chrono::system_clock::time_point lastRequest;
        std::chrono::system_clock::time_point serverStartTime;
    };
    
    /// Get server statistics
    ServerStatistics getStatistics() const;
    
    /// Reset statistics
    void resetStatistics();
    
    /// Set request callback for monitoring
    using RequestCallback = std::function<void(const RequestContext& context, const Response& response)>;
    void setRequestCallback(RequestCallback callback);
    
    // ========================================================================
    // Error Handling
    // ========================================================================
    
    /// Error callback type
    using ErrorCallback = std::function<void(const std::string& endpoint, const std::string& error)>;
    
    /// Set error callback
    void setErrorCallback(ErrorCallback callback);
    
    /// Enable/disable detailed error responses
    void setDetailedErrorsEnabled(bool enabled);
    
    /// Check if detailed errors are enabled
    bool areDetailedErrorsEnabled() const;

protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Setup all REST endpoints
    void setupEndpoints();
    
    /// Setup action execution endpoints
    void setupActionEndpoints();
    
    /// Setup API information endpoints
    void setupInfoEndpoints();
    
    /// Setup session state endpoints
    void setupStateEndpoints();
    
    /// Setup utility endpoints
    void setupUtilityEndpoints();
    
    /// Handle CORS preflight requests
    void handleCORSPreflight(const httplib::Request& req, httplib::Response& res);
    
    /// Apply CORS headers
    void applyCORSHeaders(httplib::Response& res);
    
    /// Authenticate request
    bool authenticateRequest(const httplib::Request& req, httplib::Response& res);
    
    /// Extract action context from request
    ActionContext extractActionContext(const httplib::Request& req);
    
    /// Convert ActionResult to HTTP response
    Response actionResultToResponse(const ActionResult& result);
    
    /// Log request
    void logRequest(const RequestContext& context, const Response& response);
    
    /// Update statistics
    void updateStatistics(const RequestContext& context, const Response& response, double responseTimeMs);
    
    /// Build request context
    RequestContext buildRequestContext(const httplib::Request& req);

private:
    // Action API reference
    std::shared_ptr<ActionAPI> actionAPI_;
    
    // HTTP server
    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;
    
    // Server state
    std::atomic<bool> isRunning_{false};
    std::string host_;
    int port_ = 0;
    
    // Configuration
    CORSSettings corsSettings_;
    std::atomic<bool> requestLoggingEnabled_{true};
    std::atomic<bool> detailedErrorsEnabled_{true};
    std::string authToken_;
    mutable std::mutex configMutex_;
    
    // Middleware
    std::vector<RequestMiddleware> requestMiddleware_;
    std::vector<ResponseMiddleware> responseMiddleware_;
    mutable std::mutex middlewareMutex_;
    
    // Statistics
    mutable ServerStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Callbacks
    RequestCallback requestCallback_;
    ErrorCallback errorCallback_;
    
    // Constants
    static constexpr int DEFAULT_PORT = 8080;
    static constexpr const char* DEFAULT_HOST = "localhost";
};

// ============================================================================
// REST API Documentation Generator
// ============================================================================

class APIDocGenerator {
public:
    explicit APIDocGenerator(std::shared_ptr<ActionAPI> actionAPI);
    
    /// Generate OpenAPI 3.0 specification
    json generateOpenAPISpec() const;
    
    /// Generate HTML documentation
    std::string generateHTMLDocs() const;
    
    /// Generate Markdown documentation
    std::string generateMarkdownDocs() const;
    
    /// Generate Postman collection
    json generatePostmanCollection() const;
    
private:
    std::shared_ptr<ActionAPI> actionAPI_;
};

} // namespace mixmind::api