#pragma once

#include "../core/file_system.h"
#include "../utils/thread_pool.h"
#include "../utils/retry_handler.h"
#include "../utils/logger.h"
#include "../utils/exceptions.h"
#include <string>
#include <memory>
#include <vector>
#include <future>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <chrono>

namespace dfs {
namespace api {

/**
 * Client configuration
 */
struct ClientConfig {
    std::string server_host;
    uint16_t server_port;
    std::string api_key;
    std::chrono::seconds connection_timeout;
    std::chrono::seconds request_timeout;
    uint32_t max_connections;
    bool enable_compression;
    bool enable_encryption;
    std::string ssl_cert_path;
    
    ClientConfig(const std::string& host = "localhost",
                uint16_t port = 8080,
                const std::string& key = "",
                std::chrono::seconds conn_timeout = std::chrono::seconds(10),
                std::chrono::seconds req_timeout = std::chrono::seconds(30),
                uint32_t max_conn = 10);
};

/**
 * File handle for client operations
 */
class FileHandle {
private:
    std::string path_;
    std::string mode_;
    uint64_t position_;
    uint64_t size_;
    std::mutex handle_mutex_;
    std::atomic<bool> is_open_;
    
public:
    FileHandle(const std::string& path, const std::string& mode);
    ~FileHandle();
    
    // File operations
    bool is_open() const;
    const std::string& get_path() const;
    const std::string& get_mode() const;
    uint64_t get_position() const;
    uint64_t get_size() const;
    
    // Position management
    void seek(uint64_t position);
    void seek_end();
    uint64_t tell() const;
    
    // Close file handle
    void close();
};

/**
 * Directory iterator for client operations
 */
class DirectoryIterator {
private:
    std::string path_;
    std::vector<std::string> entries_;
    size_t current_index_;
    std::mutex iterator_mutex_;
    
public:
    DirectoryIterator(const std::string& path);
    
    // Iterator operations
    bool has_next() const;
    std::string next();
    void reset();
    size_t size() const;
    
    // Get all entries
    std::vector<std::string> get_all_entries() const;
};

/**
 * Client library for distributed file system
 */
class ClientLibrary {
private:
    ClientConfig config_;
    std::unique_ptr<utils::ThreadPool> thread_pool_;
    std::unique_ptr<utils::RetryHandler> retry_handler_;
    std::unique_ptr<utils::Logger> logger_;
    
    // Connection management
    std::atomic<bool> is_connected_;
    std::mutex connection_mutex_;
    std::chrono::steady_clock::time_point last_connection_time_;
    
    // Request statistics
    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> successful_requests_;
    std::atomic<uint64_t> failed_requests_;
    std::chrono::steady_clock::time_point start_time_;
    
    // Helper methods
    bool establish_connection();
    void close_connection();
    bool is_connection_valid() const;
    void handle_connection_error();
    
    // Request execution
    template<typename Func>
    auto execute_request(Func&& func) -> decltype(func());
    
    // Path validation
    bool validate_path(const std::string& path);
    std::string normalize_path(const std::string& path);
    std::string get_parent_directory(const std::string& path);
    std::string get_filename(const std::string& path);

public:
    ClientLibrary(const ClientConfig& config);
    ~ClientLibrary();
    
    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;
    void reconnect();
    
    // File operations
    bool create_file(const std::string& path, const std::vector<uint8_t>& data, 
                    uint16_t permissions = 0644);
    bool create_file(const std::string& path, const std::string& content, 
                    uint16_t permissions = 0644);
    
    std::vector<uint8_t> read_file(const std::string& path);
    std::string read_file_as_string(const std::string& path);
    
    bool write_file(const std::string& path, const std::vector<uint8_t>& data);
    bool write_file(const std::string& path, const std::string& content);
    
    bool append_file(const std::string& path, const std::vector<uint8_t>& data);
    bool append_file(const std::string& path, const std::string& content);
    
    bool delete_file(const std::string& path);
    bool file_exists(const std::string& path);
    uint64_t get_file_size(const std::string& path);
    
    // Directory operations
    bool create_directory(const std::string& path, uint16_t permissions = 0755);
    bool delete_directory(const std::string& path);
    bool directory_exists(const std::string& path);
    
    std::vector<std::string> list_directory(const std::string& path);
    DirectoryIterator get_directory_iterator(const std::string& path);
    
    bool rename(const std::string& old_path, const std::string& new_path);
    bool move(const std::string& old_path, const std::string& new_path);
    
    // File handle operations
    std::unique_ptr<FileHandle> open_file(const std::string& path, const std::string& mode);
    std::vector<uint8_t> read_file_handle(FileHandle& handle, size_t size);
    bool write_file_handle(FileHandle& handle, const std::vector<uint8_t>& data);
    
    // Metadata operations
    struct FileInfo {
        std::string path;
        uint64_t size;
        uint16_t permissions;
        uint16_t uid, gid;
        std::chrono::system_clock::time_point created_time;
        std::chrono::system_clock::time_point modified_time;
        std::chrono::system_clock::time_point accessed_time;
        bool is_directory;
        bool is_file;
        bool is_symlink;
    };
    
    FileInfo get_file_info(const std::string& path);
    bool set_permissions(const std::string& path, uint16_t permissions);
    bool set_ownership(const std::string& path, uint16_t uid, uint16_t gid);
    
    // Async operations
    std::future<bool> create_file_async(const std::string& path, 
                                       const std::vector<uint8_t>& data, 
                                       uint16_t permissions = 0644);
    std::future<std::vector<uint8_t>> read_file_async(const std::string& path);
    std::future<bool> write_file_async(const std::string& path, 
                                      const std::vector<uint8_t>& data);
    std::future<bool> delete_file_async(const std::string& path);
    
    // Batch operations
    struct BatchOperation {
        enum Type { CREATE_FILE, WRITE_FILE, DELETE_FILE, CREATE_DIRECTORY, DELETE_DIRECTORY };
        Type type;
        std::string path;
        std::vector<uint8_t> data;
        uint16_t permissions;
    };
    
    std::vector<bool> execute_batch(const std::vector<BatchOperation>& operations);
    std::future<std::vector<bool>> execute_batch_async(const std::vector<BatchOperation>& operations);
    
    // System operations
    struct SystemInfo {
        uint64_t total_space;
        uint64_t free_space;
        uint64_t used_space;
        uint32_t total_files;
        uint32_t total_directories;
        std::string version;
        std::chrono::system_clock::time_point uptime;
    };
    
    SystemInfo get_system_info();
    bool is_system_healthy();
    
    // Configuration
    void update_config(const ClientConfig& new_config);
    const ClientConfig& get_config() const;
    
    // Statistics
    struct ClientStats {
        uint64_t total_requests;
        uint64_t successful_requests;
        uint64_t failed_requests;
        std::chrono::milliseconds uptime;
        double success_rate;
        bool is_connected;
        std::chrono::milliseconds last_connection_time;
    };
    ClientStats get_stats() const;
    
    // Error handling
    void set_error_handler(std::function<void(const std::exception&)> handler);
    void clear_error_handler();
    
    // Cleanup
    void cleanup();
    void shutdown();
};

/**
 * Client factory for creating client instances
 */
class ClientFactory {
public:
    // Create client with default configuration
    static std::unique_ptr<ClientLibrary> create_client();
    
    // Create client with custom configuration
    static std::unique_ptr<ClientLibrary> create_client(const ClientConfig& config);
    
    // Create client with connection string
    static std::unique_ptr<ClientLibrary> create_client(const std::string& connection_string);
    
    // Create client pool for high-performance scenarios
    static std::vector<std::unique_ptr<ClientLibrary>> create_client_pool(
        size_t pool_size, const ClientConfig& config);
    
    // Parse connection string
    static ClientConfig parse_connection_string(const std::string& connection_string);
    
    // Validate configuration
    static bool validate_config(const ClientConfig& config);
};

} // namespace api
} // namespace dfs
