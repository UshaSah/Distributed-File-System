#include "utils/retry_handler.h"
#include "utils/exceptions.h"
#include <random>
#include <thread>
#include <chrono>
#include <iostream>

namespace dfs {
namespace utils {

// RetryConfig implementation
RetryHandler::RetryConfig::RetryConfig(uint32_t max_retries,
                                     std::chrono::milliseconds initial_delay,
                                     std::chrono::milliseconds max_delay,
                                     double backoff_multiplier,
                                     bool enable_jitter,
                                     std::chrono::seconds circuit_breaker_timeout,
                                     uint32_t circuit_breaker_failure_threshold)
    : max_retries(max_retries), initial_delay(initial_delay), max_delay(max_delay),
      backoff_multiplier(backoff_multiplier), enable_jitter(enable_jitter),
      circuit_breaker_timeout(circuit_breaker_timeout),
      circuit_breaker_failure_threshold(circuit_breaker_failure_threshold) {}

// RetryHandler implementation
RetryHandler::RetryHandler(const RetryConfig& config)
    : config_(config), circuit_state_(CircuitState::CLOSED),
      consecutive_failures_(0), total_attempts_(0), successful_attempts_(0),
      failed_attempts_(0), circuit_breaker_trips_(0) {
    
    std::cout << "Creating RetryHandler with " << config.max_retries << 
                 " max retries, circuit breaker threshold " << 
                 config.circuit_breaker_failure_threshold << std::endl;
}

bool RetryHandler::should_retry(const std::exception& e, uint32_t attempt_count) const {
    if (attempt_count >= config_.max_retries) {
        return false;
    }
    
    ErrorType error_type = classify_error(e);
    
    switch (error_type) {
        case ErrorType::TRANSIENT:
        case ErrorType::CONCURRENCY:
        case ErrorType::TIMEOUT:
        case ErrorType::NETWORK:
            return true;
            
        case ErrorType::PERMANENT:
        case ErrorType::CORRUPTION:
            return false;
            
        case ErrorType::UNKNOWN:
            // Conservative approach for unknown errors
            return attempt_count < 1;
            
        default:
            return false;
    }
}

RetryHandler::ErrorType RetryHandler::classify_error(const std::exception& e) const {
    const std::string error_msg = e.what();
    
    // Check for specific exception types
    if (dynamic_cast<const dfs::utils::InodeNotFoundException*>(&e) ||
        dynamic_cast<const dfs::utils::BlockNotFoundException*>(&e) ||
        dynamic_cast<const dfs::utils::FileNotFoundException*>(&e)) {
        return ErrorType::PERMANENT;
    }
    
    if (dynamic_cast<const dfs::utils::ConcurrentAccessException*>(&e)) {
        return ErrorType::CONCURRENCY;
    }
    
    if (dynamic_cast<const dfs::utils::InodeCorruptedException*>(&e) ||
        dynamic_cast<const dfs::utils::BlockCorruptedException*>(&e) ||
        dynamic_cast<const dfs::utils::FileSystemCorruptedException*>(&e)) {
        return ErrorType::CORRUPTION;
    }
    
    if (dynamic_cast<const dfs::utils::NetworkException*>(&e) ||
        dynamic_cast<const dfs::utils::RateLimitExceededException*>(&e)) {
        return ErrorType::NETWORK;
    }
    
    if (dynamic_cast<const dfs::utils::TransactionTimeoutException*>(&e)) {
        return ErrorType::TIMEOUT;
    }
    
    // Check error message content for transient errors
    if (error_msg.find("timeout") != std::string::npos ||
        error_msg.find("temporary") != std::string::npos ||
        error_msg.find("retry") != std::string::npos ||
        error_msg.find("busy") != std::string::npos) {
        return ErrorType::TRANSIENT;
    }
    
    return ErrorType::UNKNOWN;
}

std::chrono::milliseconds RetryHandler::calculate_backoff_delay(uint32_t attempt_count) const {
    if (attempt_count == 0) {
        return config_.initial_delay;
    }
    
    // Calculate exponential backoff
    double delay_ms = config_.initial_delay.count() * 
                     std::pow(config_.backoff_multiplier, attempt_count);
    
    // Clamp to maximum delay
    delay_ms = std::min(delay_ms, static_cast<double>(config_.max_delay.count()));
    
    std::chrono::milliseconds delay(static_cast<uint64_t>(delay_ms));
    
    // Add jitter if enabled
    if (config_.enable_jitter) {
        delay = add_jitter(delay);
    }
    
    return delay;
}

bool RetryHandler::is_circuit_open() const {
    return circuit_state_.load() == CircuitState::OPEN;
}

void RetryHandler::reset_circuit_breaker() {
    circuit_state_ = CircuitState::CLOSED;
    consecutive_failures_ = 0;
    last_failure_time_ = std::chrono::steady_clock::time_point();
    
    std::cout << "Circuit breaker reset" << std::endl;
}

RetryHandler::CircuitState RetryHandler::get_circuit_state() const {
    return circuit_state_.load();
}

void RetryHandler::update_config(const RetryConfig& new_config) {
    config_ = new_config;
    std::cout << "RetryHandler configuration updated" << std::endl;
}

const RetryHandler::RetryConfig& RetryHandler::get_config() const {
    return config_;
}

RetryHandler::RetryStats RetryHandler::get_stats() const {
    RetryStats stats;
    
    stats.total_attempts = total_attempts_.load();
    stats.successful_attempts = successful_attempts_.load();
    stats.failed_attempts = failed_attempts_.load();
    stats.circuit_breaker_trips = circuit_breaker_trips_.load();
    stats.current_circuit_state = circuit_state_.load();
    stats.consecutive_failures = consecutive_failures_.load();
    
    if (stats.total_attempts > 0) {
        stats.success_rate = static_cast<double>(stats.successful_attempts) / stats.total_attempts;
    } else {
        stats.success_rate = 0.0;
    }
    
    return stats;
}

std::chrono::milliseconds RetryHandler::add_jitter(std::chrono::milliseconds delay) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // Add ±25% jitter
    double jitter_factor = 0.25;
    double min_factor = 1.0 - jitter_factor;
    double max_factor = 1.0 + jitter_factor;
    
    std::uniform_real_distribution<> dis(min_factor, max_factor);
    double jittered_delay = delay.count() * dis(gen);
    
    return std::chrono::milliseconds(static_cast<uint64_t>(jittered_delay));
}

void RetryHandler::update_circuit_breaker(bool success) {
    if (success) {
        // Reset on success
        consecutive_failures_ = 0;
        if (circuit_state_.load() == CircuitState::HALF_OPEN) {
            circuit_state_ = CircuitState::CLOSED;
            std::cout << "Circuit breaker closed after successful operation" << std::endl;
        }
    } else {
        // Increment failure count
        uint32_t failures = consecutive_failures_.fetch_add(1) + 1;
        last_failure_time_ = std::chrono::steady_clock::now();
        
        // Check if we should open the circuit
        if (failures >= config_.circuit_breaker_failure_threshold) {
            if (circuit_state_.load() != CircuitState::OPEN) {
                circuit_state_ = CircuitState::OPEN;
                circuit_breaker_trips_++;
                std::cout << "Circuit breaker opened after " << failures << " consecutive failures" << std::endl;
            }
        }
    }
}

bool RetryHandler::should_reset_circuit_breaker() const {
    if (circuit_state_.load() != CircuitState::OPEN) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_failure_time_);
    
    return elapsed >= config_.circuit_breaker_timeout;
}

// RetryManager implementation
RetryManager::RetryManager(const RetryConfig& default_config)
    : default_config_(default_config) {
    
    std::cout << "Creating RetryManager with default configuration" << std::endl;
}

RetryHandler* RetryManager::get_handler(const std::string& operation_name) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    auto it = handlers_.find(operation_name);
    if (it != handlers_.end()) {
        return it->second.get();
    }
    
    // Create new handler with default config
    auto handler = std::make_unique<RetryHandler>(default_config_);
    RetryHandler* handler_ptr = handler.get();
    handlers_[operation_name] = std::move(handler);
    
    std::cout << "Created new retry handler for operation: " << operation_name << std::endl;
    return handler_ptr;
}

void RetryManager::create_handler(const std::string& operation_name, const RetryConfig& config) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    auto handler = std::make_unique<RetryHandler>(config);
    handlers_[operation_name] = std::move(handler);
    
    std::cout << "Created custom retry handler for operation: " << operation_name << std::endl;
}

void RetryManager::remove_handler(const std::string& operation_name) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    auto it = handlers_.find(operation_name);
    if (it != handlers_.end()) {
        handlers_.erase(it);
        std::cout << "Removed retry handler for operation: " << operation_name << std::endl;
    }
}

std::unordered_map<std::string, RetryHandler::RetryStats> RetryManager::get_all_stats() const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    std::unordered_map<std::string, RetryHandler::RetryStats> stats;
    
    for (const auto& pair : handlers_) {
        stats[pair.first] = pair.second->get_stats();
    }
    
    return stats;
}

void RetryManager::reset_all_handlers() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    for (auto& pair : handlers_) {
        pair.second->reset_circuit_breaker();
    }
    
    std::cout << "Reset all retry handlers" << std::endl;
}

} // namespace utils
} // namespace dfs
