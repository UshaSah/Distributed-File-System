#pragma once

#include "httplib.h"
#include "../core/file_system.h"
#include "../utils/thread_pool.h"
#include "../utils/rate_limiter.h"
#include "../utils/retry_handler.h"
#include "../utils/logger.h"
#include "../utils/exceptions.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <chrono>

namespace dfs {
namespace api {

/**
 * REST API response structure
 */
struct APIResponse {
    std::string status;
    std::string message;
    std::string transaction_id;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> data;
    
    APIResponse(const std::string& status = "success", 
               const std::string& message = "",
               const std::string& tx_id = "");
    
    // Convert to JSON
    std::string to_json() const;
    
    // Create error response
    static APIResponse error(const std::string& message, 
                           const std::string& tx_id = "",
                           uint32_t error_code = 0);
    
    // Create success response
    static APIResponse success(const std::string& message = "",
                             const std::string& tx_id = "",
                             const std::unordered_map<std::string, std::string>& data = {});
};

/**
 * REST API request context
 */
struct RequestContext {
    std::string client_id;
    std::string transaction_id;
    std::chrono::steady_clock::time_point start_time;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    
    RequestContext(const std::string& client_id = "");
    
    // Get request duration
    std::chrono::milliseconds get_duration() const;
    
    // Get header value
    std::string get_header(const std::string& name) const;
    
    // Set header value
    void set_header(const std::string& name, const std::string& value);
};

/**
 * REST API server configuration
 */
struct ServerConfig {
    std::string host;
    uint16_t port;
    uint32_t max_connections;
    std::chrono::seconds request_timeout;
    std::string ssl_cert_path;
    std::string ssl_key_path;
    bool enable_ssl;
    bool enable_cors;
    std::string cors_origin;
    
    ServerConfig(const std::string& host = "0.0.0.0",
                uint16_t port = 8080,
                uint32_t max_connections = 1000,
                std::chrono::seconds timeout = std::chrono::seconds(30),
                bool ssl = false,
                bool cors = true);
};

/**
 * REST API server for distributed file system
 */
class RESTServer {
private:
    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<core::FileSystem> file_system_;
    std::unique_ptr<utils::ThreadPool> thread_pool_;
    std::unique_ptr<utils::RateLimiter> rate_limiter_;
    std::unique_ptr<utils::RetryHandler> retry_handler_;
    
    ServerConfig config_;
    std::atomic<bool> is_running_;
    std::mutex server_mutex_;
    
    // Request statistics
    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> successful_requests_;
    std::atomic<uint64_t> failed_requests_;
    std::chrono::steady_clock::time_point start_time_;
    
    // Helper methods
    RequestContext create_request_context(const httplib::Request& req);
    APIResponse handle_exception(const std::exception& e, const RequestContext& ctx);
    void log_request(const RequestContext& ctx, const APIResponse& response);
    std::string generate_transaction_id();
    bool validate_path(const std::string& path);
    std::string sanitize_path(const std::string& path);
    
    // File operation handlers
    APIResponse handle_create_file(const RequestContext& ctx, const std::string& path, 
                                 const std::string& content, uint16_t permissions);
    APIResponse handle_read_file(const RequestContext& ctx, const std::string& path);
    APIResponse handle_write_file(const RequestContext& ctx, const std::string& path, 
                                const std::string& content);
    APIResponse handle_delete_file(const RequestContext& ctx, const std::string& path);
    APIResponse handle_get_file_info(const RequestContext& ctx, const std::string& path);
    
    // Directory operation handlers
    APIResponse handle_create_directory(const RequestContext& ctx, const std::string& path, 
                                      uint16_t permissions);
    APIResponse handle_list_directory(const RequestContext& ctx, const std::string& path);
    APIResponse handle_delete_directory(const RequestContext& ctx, const std::string& path);
    APIResponse handle_rename(const RequestContext& ctx, const std::string& old_path, 
                            const std::string& new_path);
    
    // System operation handlers
    APIResponse handle_get_filesystem_info(const RequestContext& ctx);
    APIResponse handle_get_filesystem_stats(const RequestContext& ctx);
    APIResponse handle_health_check(const RequestContext& ctx);
    
    // HTTP endpoint handlers
    void setup_routes();
    void handle_file_operations(const httplib::Request& req, httplib::Response& res);
    void handle_directory_operations(const httplib::Request& req, httplib::Response& res);
    void handle_system_operations(const httplib::Request& req, httplib::Response& res);
    void handle_metadata_operations(const httplib::Request& req, httplib::Response& res);

public:
    RESTServer(const ServerConfig& config);
    ~RESTServer();
    
    // Server lifecycle
    bool start();
    void stop();
    bool is_running() const;
    
    // Configuration
    void update_config(const ServerConfig& new_config);
    const ServerConfig& get_config() const;
    
    // File system operations
    void set_file_system(std::unique_ptr<core::FileSystem> fs);
    core::FileSystem* get_file_system() const;
    
    // Utility services
    void set_thread_pool(std::unique_ptr<utils::ThreadPool> pool);
    void set_rate_limiter(std::unique_ptr<utils::RateLimiter> limiter);
    void set_retry_handler(std::unique_ptr<utils::RetryHandler> handler);
    
    // Statistics
    struct ServerStats {
        uint64_t total_requests;
        uint64_t successful_requests;
        uint64_t failed_requests;
        std::chrono::milliseconds uptime;
        double success_rate;
        uint32_t active_connections;
        uint32_t queued_requests;
    };
    ServerStats get_stats() const;
    
    // Health check
    bool is_healthy() const;
    
    // Graceful shutdown
    void graceful_shutdown(std::chrono::seconds timeout = std::chrono::seconds(30));
};

/**
 * REST API client for distributed file system
 */
class RESTClient {
private:
    std::string base_url_;
    std::unique_ptr<httplib::Client> client_;
    std::string api_key_;
    std::chrono::seconds timeout_;
    std::mutex client_mutex_;
    
    // Request statistics
    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> successful_requests_;
    std::atomic<uint64_t> failed_requests_;
    
    // Helper methods
    httplib::Result make_request(const std::string& method, const std::string& path, 
                               const std::string& body = "", 
                               const std::unordered_map<std::string, std::string>& headers = {});
    APIResponse parse_response(const httplib::Result& result);
    void log_request(const std::string& method, const std::string& path, 
                    const APIResponse& response);

public:
    RESTClient(const std::string& base_url, const std::string& api_key = "",
              std::chrono::seconds timeout = std::chrono::seconds(30));
    ~RESTClient();
    
    // File operations
    APIResponse create_file(const std::string& path, const std::string& content, 
                          uint16_t permissions = 0644);
    APIResponse read_file(const std::string& path);
    APIResponse write_file(const std::string& path, const std::string& content);
    APIResponse delete_file(const std::string& path);
    APIResponse get_file_info(const std::string& path);
    
    // Directory operations
    APIResponse create_directory(const std::string& path, uint16_t permissions = 0755);
    APIResponse list_directory(const std::string& path);
    APIResponse delete_directory(const std::string& path);
    APIResponse rename(const std::string& old_path, const std::string& new_path);
    
    // System operations
    APIResponse get_filesystem_info();
    APIResponse get_filesystem_stats();
    APIResponse health_check();
    
    // Configuration
    void set_api_key(const std::string& api_key);
    void set_timeout(std::chrono::seconds timeout);
    void set_base_url(const std::string& base_url);
    
    // Statistics
    struct ClientStats {
        uint64_t total_requests;
        uint64_t successful_requests;
        uint64_t failed_requests;
        double success_rate;
    };
    ClientStats get_stats() const;
    
    // Connection management
    bool is_connected() const;
    void reconnect();
};

} // namespace api
} // namespace dfs
