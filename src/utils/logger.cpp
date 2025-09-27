#include "utils/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace dfs {
namespace utils {

// Static instance management
std::unique_ptr<Logger> Logger::instance_ = nullptr;
std::mutex Logger::instance_mutex_;

// LogEntry implementation
Logger::LogEntry::LogEntry(Level lvl, const std::string& msg, const std::string& file,
                          uint32_t line, const std::string& func)
    : level(lvl), message(msg), source_file(file), line_number(line), 
      function_name(func), timestamp(std::chrono::system_clock::now()),
      thread_id(std::this_thread::get_id()) {}

// Logger implementation
Logger::Logger(const LoggerConfig& config)
    : config_(config), current_level_(config.min_level), start_time_(std::chrono::steady_clock::now()) {
    
    // Initialize statistics
    total_logs_ = 0;
    for (int i = 0; i < 5; ++i) {
        logs_by_level_[i] = 0;
    }
    
    // Open log file if file output is enabled
    if (config_.enable_file_output && !config_.log_file_path.empty()) {
        // Create directory if it doesn't exist
        std::filesystem::path log_path(config_.log_file_path);
        std::filesystem::create_directories(log_path.parent_path());
        
        log_file_.open(config_.log_file_path, std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "Failed to open log file: " << config_.log_file_path << std::endl;
        }
    }
    
    // Start worker thread if async logging is enabled
    if (config_.enable_async_logging) {
        stop_worker_ = false;
        worker_thread_ = std::thread(&Logger::worker_thread_function, this);
    }
}

Logger::~Logger() {
    // Stop worker thread
    if (config_.enable_async_logging) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stop_worker_ = true;
        }
        queue_condition_.notify_all();
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    // Close log file
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::log(Level level, const std::string& message, const std::string& file,
                uint32_t line, const std::string& function) {
    
    // Check if we should log this level
    if (level < current_level_.load()) {
        return;
    }
    
    LogEntry entry(level, message, file, line, function);
    
    // Update statistics
    total_logs_++;
    logs_by_level_[static_cast<int>(level)]++;
    
    if (config_.enable_async_logging) {
        // Add to queue for async processing
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            log_queue_.push(entry);
        }
        queue_condition_.notify_one();
    } else {
        // Process synchronously
        write_log_entry(entry);
    }
}

void Logger::debug(const std::string& message, const std::string& file,
                  uint32_t line, const std::string& function) {
    log(Level::DEBUG, message, file, line, function);
}

void Logger::info(const std::string& message, const std::string& file,
                 uint32_t line, const std::string& function) {
    log(Level::INFO, message, file, line, function);
}

void Logger::warn(const std::string& message, const std::string& file,
                 uint32_t line, const std::string& function) {
    log(Level::WARN, message, file, line, function);
}

void Logger::error(const std::string& message, const std::string& file,
                  uint32_t line, const std::string& function) {
    log(Level::ERROR, message, file, line, function);
}

void Logger::critical(const std::string& message, const std::string& file,
                     uint32_t line, const std::string& function) {
    log(Level::CRITICAL, message, file, line, function);
}

void Logger::log_transaction(uint64_t tx_id, const std::string& operation, const std::string& details) {
    std::ostringstream msg;
    msg << "Transaction " << tx_id << ": " << operation;
    if (!details.empty()) {
        msg << " - " << details;
    }
    info(msg.str());
}

void Logger::log_performance(const std::string& operation, std::chrono::milliseconds duration) {
    std::ostringstream msg;
    msg << "Performance: " << operation << " took " << duration.count() << "ms";
    info(msg.str());
}

void Logger::log_error(const std::exception& e, const std::string& context) {
    std::ostringstream msg;
    msg << "Exception in " << context << ": " << e.what();
    error(msg.str());
}

void Logger::log_system_event(const std::string& event, const std::string& details) {
    std::ostringstream msg;
    msg << "System Event: " << event;
    if (!details.empty()) {
        msg << " - " << details;
    }
    info(msg.str());
}

void Logger::set_level(Level level) {
    current_level_ = level;
}

Logger::Level Logger::get_level() const {
    return current_level_.load();
}

void Logger::update_config(const LoggerConfig& new_config) {
    config_ = new_config;
    current_level_ = new_config.min_level;
}

const Logger::LoggerConfig& Logger::get_config() const {
    return config_;
}

void Logger::flush() {
    if (log_file_.is_open()) {
        log_file_.flush();
    }
}

void Logger::rotate_logs() {
    if (!config_.enable_file_output || config_.log_file_path.empty()) {
        return;
    }
    
    // Close current log file
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    // Rotate existing log files
    std::filesystem::path log_path(config_.log_file_path);
    std::filesystem::path log_dir = log_path.parent_path();
    std::string log_name = log_path.stem().string();
    std::string log_ext = log_path.extension().string();
    
    // Move existing files
    for (int i = config_.max_log_files - 1; i > 0; --i) {
        std::filesystem::path old_file = log_dir / (log_name + "." + std::to_string(i) + log_ext);
        std::filesystem::path new_file = log_dir / (log_name + "." + std::to_string(i + 1) + log_ext);
        
        if (std::filesystem::exists(old_file)) {
            if (i == config_.max_log_files - 1) {
                std::filesystem::remove(old_file);
            } else {
                std::filesystem::rename(old_file, new_file);
            }
        }
    }
    
    // Move current log to .1
    if (std::filesystem::exists(log_path)) {
        std::filesystem::path rotated_file = log_dir / (log_name + ".1" + log_ext);
        std::filesystem::rename(log_path, rotated_file);
    }
    
    // Open new log file
    log_file_.open(config_.log_file_path, std::ios::app);
}

void Logger::close() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

Logger::LoggerStats Logger::get_stats() const {
    LoggerStats stats;
    stats.total_logs = total_logs_.load();
    stats.debug_logs = logs_by_level_[0];
    stats.info_logs = logs_by_level_[1];
    stats.warn_logs = logs_by_level_[2];
    stats.error_logs = logs_by_level_[3];
    stats.critical_logs = logs_by_level_[4];
    stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stats.queue_size = log_queue_.size();
    }
    
    stats.async_enabled = config_.enable_async_logging;
    return stats;
}

Logger* Logger::get_instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        // Create default instance
        LoggerConfig default_config;
        instance_ = std::make_unique<Logger>(default_config);
    }
    return instance_.get();
}

void Logger::set_instance(std::unique_ptr<Logger> logger) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    instance_ = std::move(logger);
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    instance_.reset();
}

std::string Logger::level_to_string(Level level) const {
    switch (level) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO: return "INFO";
        case Level::WARN: return "WARN";
        case Level::ERROR: return "ERROR";
        case Level::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::format_log_entry(const LogEntry& entry) const {
    std::ostringstream formatted;
    
    // Get timestamp
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch()) % 1000;
    
    formatted << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    formatted << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    // Add level
    formatted << " [" << level_to_string(entry.level) << "]";
    
    // Add thread ID
    formatted << " [T" << entry.thread_id << "]";
    
    // Add source location if available
    if (!entry.source_file.empty()) {
        std::filesystem::path file_path(entry.source_file);
        formatted << " [" << file_path.filename().string();
        if (entry.line_number > 0) {
            formatted << ":" << entry.line_number;
        }
        if (!entry.function_name.empty()) {
            formatted << ":" << entry.function_name;
        }
        formatted << "]";
    }
    
    // Add message
    formatted << " " << entry.message;
    
    return formatted.str();
}

void Logger::write_to_file(const LogEntry& entry) {
    if (!config_.enable_file_output || !log_file_.is_open()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(file_mutex_);
    
    // Check if we need to rotate logs
    if (log_file_.tellp() > static_cast<std::streampos>(config_.max_log_file_size)) {
        rotate_logs();
    }
    
    log_file_ << format_log_entry(entry) << std::endl;
    log_file_.flush();
}

void Logger::write_to_console(const LogEntry& entry) {
    if (!config_.enable_console_output) {
        return;
    }
    
    std::string formatted = format_log_entry(entry);
    
    // Color coding for different levels
    switch (entry.level) {
        case Level::DEBUG:
            std::cout << "\033[36m" << formatted << "\033[0m" << std::endl;
            break;
        case Level::INFO:
            std::cout << "\033[32m" << formatted << "\033[0m" << std::endl;
            break;
        case Level::WARN:
            std::cout << "\033[33m" << formatted << "\033[0m" << std::endl;
            break;
        case Level::ERROR:
            std::cerr << "\033[31m" << formatted << "\033[0m" << std::endl;
            break;
        case Level::CRITICAL:
            std::cerr << "\033[35m" << formatted << "\033[0m" << std::endl;
            break;
        default:
            std::cout << formatted << std::endl;
            break;
    }
}

void Logger::worker_thread_function() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // Wait for work or shutdown signal
        queue_condition_.wait(lock, [this] { 
            return !log_queue_.empty() || stop_worker_; 
        });
        
        if (stop_worker_ && log_queue_.empty()) {
            break;
        }
        
        // Process all available log entries
        while (!log_queue_.empty()) {
            LogEntry entry = log_queue_.front();
            log_queue_.pop();
            lock.unlock();
            
            write_log_entry(entry);
            
            lock.lock();
        }
    }
}

void Logger::write_log_entry(const LogEntry& entry) {
    write_to_console(entry);
    write_to_file(entry);
}

} // namespace utils
} // namespace dfs
