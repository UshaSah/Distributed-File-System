#pragma once

#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <exception>
#include <unordered_map>
#include <string>
#include <thread>
#include <stdexcept>

namespace dfs {
namespace utils {

/**
 * RetryHandler - Implements retry logic with exponential backoff and circuit breaker
 * Provides robust error handling with configurable retry strategies
 */
class RetryHandler {
public:
    // Retry configuration
    struct RetryConfig {
        uint32_t max_retries;
        std::chrono::milliseconds initial_delay;
        std::chrono::milliseconds max_delay;
        double backoff_multiplier;
        bool enable_jitter;
        std::chrono::seconds circuit_breaker_timeout;
        uint32_t circuit_breaker_failure_threshold;
        
        RetryConfig(uint32_t max_retries = 3,
                   std::chrono::milliseconds initial_delay = std::chrono::milliseconds(100),
                   std::chrono::milliseconds max_delay = std::chrono::milliseconds(5000),
                   double backoff_multiplier = 2.0,
                   bool enable_jitter = true,
                   std::chrono::seconds circuit_breaker_timeout = std::chrono::seconds(60),
                   uint32_t circuit_breaker_failure_threshold = 5);
    };
    
    // Error classification
    enum class ErrorType {
        TRANSIENT,      // Retry possible
        PERMANENT,      // No retry
        CORRUPTION,     // Data integrity issue
        CONCURRENCY,    // Lock contention
        TIMEOUT,        // Operation timeout
        NETWORK,        // Network-related error
        UNKNOWN         // Unknown error type
    };
    
    // Circuit breaker states
    enum class CircuitState {
        CLOSED,         // Normal operation
        OPEN,           // Circuit is open, no requests allowed
        HALF_OPEN       // Testing if service is back
    };

private:
    RetryConfig config_;
    std::atomic<CircuitState> circuit_state_;
    std::atomic<uint32_t> consecutive_failures_;
    std::chrono::steady_clock::time_point last_failure_time_;
    std::mutex circuit_mutex_;
    
    // Statistics
    std::atomic<uint64_t> total_attempts_;
    std::atomic<uint64_t> successful_attempts_;
    std::atomic<uint64_t> failed_attempts_;
    std::atomic<uint64_t> circuit_breaker_trips_;

public:
    RetryHandler(const RetryConfig& config);
    
    // Execute function with retry logic
    template<typename Func, typename... Args>
    auto execute_with_retry(Func&& func, Args&&... args) 
        -> decltype(func(args...));
    
    // Execute function with retry logic and error classification
    template<typename Func, typename... Args>
    auto execute_with_retry_and_classification(Func&& func, Args&&... args)
        -> decltype(func(args...));
    
    // Check if error should be retried
    bool should_retry(const std::exception& e, uint32_t attempt_count) const;
    
    // Classify error type
    ErrorType classify_error(const std::exception& e) const;
    
    // Calculate backoff delay
    std::chrono::milliseconds calculate_backoff_delay(uint32_t attempt_count) const;
    
    // Check circuit breaker state
    bool is_circuit_open() const;
    
    // Reset circuit breaker
    void reset_circuit_breaker();
    
    // Get current circuit breaker state
    CircuitState get_circuit_state() const;
    
    // Update retry configuration
    void update_config(const RetryConfig& new_config);
    
    // Get current configuration
    const RetryConfig& get_config() const;
    
    // Get retry handler statistics
    struct RetryStats {
        uint64_t total_attempts;
        uint64_t successful_attempts;
        uint64_t failed_attempts;
        uint64_t circuit_breaker_trips;
        CircuitState current_circuit_state;
        uint32_t consecutive_failures;
        double success_rate;
    };
    RetryStats get_stats() const;

private:
    // Add jitter to delay to prevent thundering herd
    std::chrono::milliseconds add_jitter(std::chrono::milliseconds delay) const;
    
    // Update circuit breaker state
    void update_circuit_breaker(bool success);
    
    // Check if circuit breaker should be reset
    bool should_reset_circuit_breaker() const;
};

// Template implementation
template<typename Func, typename... Args>
auto RetryHandler::execute_with_retry(Func&& func, Args&&... args) 
    -> decltype(func(args...)) {
    
    using return_type = decltype(func(args...));
    
    for (uint32_t attempt = 0; attempt <= config_.max_retries; ++attempt) {
        total_attempts_++;
        
        // Check circuit breaker
        if (is_circuit_open()) {
            throw std::runtime_error("Circuit breaker is open");
        }
        
        try {
            auto result = func(args...);
            successful_attempts_++;
            update_circuit_breaker(true);
            return result;
        } catch (const std::exception& e) {
            failed_attempts_++;
            update_circuit_breaker(false);
            
            if (attempt == config_.max_retries || !should_retry(e, attempt)) {
                throw;
            }
            
            // Calculate delay and wait
            auto delay = calculate_backoff_delay(attempt);
            std::this_thread::sleep_for(delay);
        }
    }
    
    throw std::runtime_error("Max retries exceeded");
}

template<typename Func, typename... Args>
auto RetryHandler::execute_with_retry_and_classification(Func&& func, Args&&... args)
    -> decltype(func(args...)) {
    
    using return_type = decltype(func(args...));
    
    for (uint32_t attempt = 0; attempt <= config_.max_retries; ++attempt) {
        total_attempts_++;
        
        // Check circuit breaker
        if (is_circuit_open()) {
            throw std::runtime_error("Circuit breaker is open");
        }
        
        try {
            auto result = func(args...);
            successful_attempts_++;
            update_circuit_breaker(true);
            return result;
        } catch (const std::exception& e) {
            failed_attempts_++;
            update_circuit_breaker(false);
            
            ErrorType error_type = classify_error(e);
            
            if (attempt == config_.max_retries || error_type == ErrorType::PERMANENT) {
                throw;
            }
            
            // Calculate delay and wait
            auto delay = calculate_backoff_delay(attempt);
            std::this_thread::sleep_for(delay);
        }
    }
    
    throw std::runtime_error("Max retries exceeded");
}

/**
 * RetryManager - Manages multiple retry handlers for different operations
 */
class RetryManager {
private:
    std::unordered_map<std::string, std::unique_ptr<RetryHandler>> handlers_;
    mutable std::mutex handlers_mutex_;
    RetryConfig default_config_;

public:
    RetryManager(const RetryConfig& default_config);
    
    // Get or create retry handler for operation
    RetryHandler* get_handler(const std::string& operation_name);
    
    // Create custom retry handler
    void create_handler(const std::string& operation_name, const RetryConfig& config);
    
    // Remove retry handler
    void remove_handler(const std::string& operation_name);
    
    // Execute operation with retry
    template<typename Func, typename... Args>
    auto execute_with_retry(const std::string& operation_name, Func&& func, Args&&... args)
        -> decltype(func(args...));
    
    // Get all handler statistics
    std::unordered_map<std::string, RetryHandler::RetryStats> get_all_stats() const;
    
    // Reset all handlers
    void reset_all_handlers();
};

// Template implementation
template<typename Func, typename... Args>
auto RetryManager::execute_with_retry(const std::string& operation_name, Func&& func, Args&&... args)
    -> decltype(func(args...)) {
    
    auto handler = get_handler(operation_name);
    return handler->execute_with_retry(std::forward<Func>(func), std::forward<Args>(args)...);
}

} // namespace utils
} // namespace dfs
