#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <atomic>
#include <queue>
#include <thread>
#include <condition_variable>
#include <cstdint>
#include <cstdio>

namespace dfs {
namespace utils {

/**
 * Logger - Thread-safe logging system with multiple output destinations
 * Supports different log levels, file rotation, and async logging
 */
class Logger {
public:
    // Log levels
    enum class Level {
        LOG_DEBUG = 0,
        LOG_INFO = 1,
        LOG_WARN = 2,
        LOG_ERROR = 3,
        LOG_CRITICAL = 4
    };
    
    // Log entry structure
    struct LogEntry {
        Level level;
        std::string message;
        std::string source_file;
        uint32_t line_number;
        std::string function_name;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id thread_id;
        
        LogEntry(Level lvl, const std::string& msg, const std::string& file = "",
                uint32_t line = 0, const std::string& func = "");
    };
    
    // Logger configuration
    struct LoggerConfig {
        Level min_level;
        std::string log_file_path;
        bool enable_console_output;
        bool enable_file_output;
        bool enable_async_logging;
        size_t max_log_file_size;
        uint32_t max_log_files;
        std::chrono::seconds log_rotation_interval;
        
        LoggerConfig(Level min_lvl = Level::LOG_INFO,
                    const std::string& file_path = "dfs.log",
                    bool console = true,
                    bool file = true,
                    bool async = true,
                    size_t max_size = 10 * 1024 * 1024, // 10MB
                    uint32_t max_files = 5,
                    std::chrono::seconds rotation = std::chrono::hours(24));
    };

private:
    LoggerConfig config_;
    std::atomic<Level> current_level_;
    std::ofstream log_file_;
    std::mutex file_mutex_;
    
    // Async logging
    std::queue<LogEntry> log_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::thread worker_thread_;
    std::atomic<bool> stop_worker_;
    
    // Statistics
    std::atomic<uint64_t> total_logs_;
    std::atomic<uint64_t> logs_by_level_[5];
    std::chrono::steady_clock::time_point start_time_;
    
    // Helper methods
    std::string level_to_string(Level level) const;
    std::string format_log_entry(const LogEntry& entry) const;
    void write_to_file(const LogEntry& entry);
    void write_to_console(const LogEntry& entry);
    void rotate_log_file();
    void worker_thread_function();

public:
    Logger(const LoggerConfig& config);
    ~Logger();
    
    // Log methods
    void log(Level level, const std::string& message, const std::string& file = "",
            uint32_t line = 0, const std::string& function = "");
    
    // Convenience methods
    void debug(const std::string& message, const std::string& file = "",
              uint32_t line = 0, const std::string& function = "");
    void info(const std::string& message, const std::string& file = "",
             uint32_t line = 0, const std::string& function = "");
    void warn(const std::string& message, const std::string& file = "",
             uint32_t line = 0, const std::string& function = "");
    void error(const std::string& message, const std::string& file = "",
              uint32_t line = 0, const std::string& function = "");
    void critical(const std::string& message, const std::string& file = "",
                 uint32_t line = 0, const std::string& function = "");
    
    // Specialized logging methods
    void log_transaction(uint64_t tx_id, const std::string& operation, 
                        const std::string& details = "");
    void log_performance(const std::string& operation, 
                        std::chrono::milliseconds duration);
    void log_error(const std::exception& e, const std::string& context = "");
    void log_system_event(const std::string& event, const std::string& details = "");
    
    // Configuration methods
    void set_level(Level level);
    Level get_level() const;
    void update_config(const LoggerConfig& new_config);
    const LoggerConfig& get_config() const;
    
    // File management
    void flush();
    void rotate_logs();
    void close();
    
    // Statistics
    struct LoggerStats {
        uint64_t total_logs;
        uint64_t debug_logs;
        uint64_t info_logs;
        uint64_t warn_logs;
        uint64_t error_logs;
        uint64_t critical_logs;
        std::chrono::milliseconds uptime;
        size_t queue_size;
        bool async_enabled;
    };
    LoggerStats get_stats() const;
    
    // Static methods for global logger
    static Logger* get_instance();
    static void set_instance(std::unique_ptr<Logger> logger);
    static void shutdown();
    
private:
    static std::unique_ptr<Logger> instance_;
    static std::mutex instance_mutex_;
};

// Macro definitions for convenient logging
#define LOG_DEBUG(msg) Logger::get_instance()->debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO(msg) Logger::get_instance()->info(msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARN(msg) Logger::get_instance()->warn(msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(msg) Logger::get_instance()->error(msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_CRITICAL(msg) Logger::get_instance()->critical(msg, __FILE__, __LINE__, __FUNCTION__)

#define LOG_TRANSACTION(tx_id, op, details) Logger::get_instance()->log_transaction(tx_id, op, details)
#define LOG_PERFORMANCE(op, duration) Logger::get_instance()->log_performance(op, duration)
#define LOG_ERROR_EXCEPTION(e, context) Logger::get_instance()->log_error(e, context)
#define LOG_SYSTEM_EVENT(event, details) Logger::get_instance()->log_system_event(event, details)

} // namespace utils
} // namespace dfs
