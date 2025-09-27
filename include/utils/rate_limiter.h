#pragma once

#include <chrono>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

namespace dfs {
namespace utils {

/**
 * RateLimiter - Implements token bucket algorithm for request rate limiting
 * Provides per-client rate limiting with configurable thresholds
 */
class RateLimiter {
public:
    // Rate limiting configuration
    struct RateLimitConfig {
        uint32_t max_requests_per_second;
        uint32_t burst_capacity;
        std::chrono::seconds window_size;
        bool enable_per_client_limits;
        
        RateLimitConfig(uint32_t rps = 100, uint32_t burst = 200, 
                       std::chrono::seconds window = std::chrono::seconds(1),
                       bool per_client = true);
    };

private:
    // Token bucket for a single client
    struct TokenBucket {
        std::atomic<uint32_t> tokens;
        std::chrono::steady_clock::time_point last_refill;
        std::mutex bucket_mutex;
        
        TokenBucket(uint32_t capacity);
        
        // Try to consume tokens
        bool try_consume(uint32_t tokens_needed, uint32_t refill_rate, 
                        std::chrono::seconds refill_interval);
        
        // Refill tokens based on time elapsed
        void refill(uint32_t refill_rate, std::chrono::seconds refill_interval);
    };
    
    // Client-specific rate limiter
    struct ClientLimiter {
        std::unique_ptr<TokenBucket> bucket;
        std::atomic<uint32_t> request_count;
        std::chrono::steady_clock::time_point window_start;
        std::mutex window_mutex;
        
        ClientLimiter(uint32_t capacity);
        
        // Check if request is allowed
        bool is_allowed(uint32_t tokens_needed, const RateLimitConfig& config);
        
        // Reset window for sliding window rate limiting
        void reset_window();
    };
    
    RateLimitConfig config_;
    std::unordered_map<std::string, std::unique_ptr<ClientLimiter>> client_limiters_;
    mutable std::mutex client_mutex_;
    
    // Global rate limiter (when per-client limits are disabled)
    std::unique_ptr<TokenBucket> global_bucket_;
    
    // Statistics
    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> allowed_requests_;
    std::atomic<uint64_t> denied_requests_;
    std::chrono::steady_clock::time_point start_time_;

public:
    RateLimiter(const RateLimitConfig& config);
    ~RateLimiter();
    
    // Check if request is allowed for a specific client
    bool is_allowed(const std::string& client_id, uint32_t tokens_needed = 1);
    
    // Check if request is allowed (global rate limiting)
    bool is_allowed(uint32_t tokens_needed = 1);
    
    // Get client-specific rate limiter
    ClientLimiter* get_client_limiter(const std::string& client_id);
    
    // Remove client limiter (cleanup)
    void remove_client(const std::string& client_id);
    
    // Update rate limit configuration
    void update_config(const RateLimitConfig& new_config);
    
    // Get current configuration
    const RateLimitConfig& get_config() const;
    
    // Get rate limiter statistics
    struct RateLimiterStats {
        uint64_t total_requests;
        uint64_t allowed_requests;
        uint64_t denied_requests;
        uint32_t active_clients;
        double allow_rate;
        std::chrono::milliseconds uptime;
    };
    RateLimiterStats get_stats() const;
    
    // Get client-specific statistics
    struct ClientStats {
        uint32_t request_count;
        uint32_t available_tokens;
        std::chrono::milliseconds window_remaining;
    };
    ClientStats get_client_stats(const std::string& client_id) const;
    
    // Reset all client limiters
    void reset_all_clients();
    
    // Clean up expired client limiters
    void cleanup_expired_clients(std::chrono::seconds max_idle_time);
};

/**
 * SlidingWindowRateLimiter - Alternative rate limiter using sliding window
 */
class SlidingWindowRateLimiter {
private:
    struct RequestWindow {
        std::vector<std::chrono::steady_clock::time_point> requests;
        mutable std::mutex window_mutex;
        
        void add_request();
        uint32_t get_request_count(std::chrono::seconds window_size) const;
        void cleanup_old_requests(std::chrono::seconds window_size);
    };
    
    std::unordered_map<std::string, std::unique_ptr<RequestWindow>> client_windows_;
    mutable std::mutex client_mutex_;
    uint32_t max_requests_per_window_;
    std::chrono::seconds window_size_;

public:
    SlidingWindowRateLimiter(uint32_t max_requests, std::chrono::seconds window);
    
    // Check if request is allowed
    bool is_allowed(const std::string& client_id);
    
    // Get request count for client in current window
    uint32_t get_request_count(const std::string& client_id) const;
    
    // Clean up old requests
    void cleanup_old_requests();
};

} // namespace utils
} // namespace dfs
