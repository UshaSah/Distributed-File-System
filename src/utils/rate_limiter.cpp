#include "utils/rate_limiter.h"
#include "utils/logger.h"
#include <algorithm>
#include <random>
#include <chrono>

namespace dfs {
namespace utils {

// RateLimitConfig implementation
RateLimiter::RateLimitConfig::RateLimitConfig(uint32_t rps, uint32_t burst, 
                                             std::chrono::seconds window, bool per_client)
    : max_requests_per_second(rps), burst_capacity(burst), window_size(window),
      enable_per_client_limits(per_client) {}

// TokenBucket implementation
RateLimiter::TokenBucket::TokenBucket(uint32_t capacity)
    : tokens(capacity), last_refill(std::chrono::steady_clock::now()) {}

bool RateLimiter::TokenBucket::try_consume(uint32_t tokens_needed, uint32_t refill_rate,
                                          std::chrono::seconds refill_interval) {
    std::lock_guard<std::mutex> lock(bucket_mutex);
    
    // Refill tokens based on time elapsed
    refill(refill_rate, refill_interval);
    
    // Check if we have enough tokens
    if (tokens.load() >= tokens_needed) {
        tokens -= tokens_needed;
        return true;
    }
    
    return false;
}

void RateLimiter::TokenBucket::refill(uint32_t refill_rate, std::chrono::seconds refill_interval) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_refill);
    
    if (elapsed >= refill_interval) {
        uint32_t tokens_to_add = static_cast<uint32_t>(elapsed.count()) * refill_rate;
        uint32_t current_tokens = tokens.load();
        uint32_t max_tokens = refill_rate * refill_interval.count();
        
        // Don't exceed the maximum capacity
        uint32_t new_tokens = std::min(current_tokens + tokens_to_add, max_tokens);
        tokens.store(new_tokens);
        
        last_refill = now;
    }
}

// ClientLimiter implementation
RateLimiter::ClientLimiter::ClientLimiter(uint32_t capacity)
    : bucket(std::make_unique<TokenBucket>(capacity)),
      request_count(0), window_start(std::chrono::steady_clock::now()) {}

bool RateLimiter::ClientLimiter::is_allowed(uint32_t tokens_needed, const RateLimitConfig& config) {
    // Check token bucket
    if (!bucket->try_consume(tokens_needed, config.max_requests_per_second, config.window_size)) {
        return false;
    }
    
    // Check sliding window
    auto now = std::chrono::steady_clock::now();
    auto window_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - window_start);
    
    if (window_elapsed >= config.window_size) {
        // Reset window
        request_count.store(0);
        window_start = now;
    }
    
    // Check if we're within the rate limit
    if (request_count.load() >= config.max_requests_per_second) {
        return false;
    }
    
    request_count++;
    return true;
}

void RateLimiter::ClientLimiter::reset_window() {
    request_count.store(0);
    window_start = std::chrono::steady_clock::now();
}

// RateLimiter implementation
RateLimiter::RateLimiter(const RateLimitConfig& config)
    : config_(config), start_time_(std::chrono::steady_clock::now()) {
    
    LOG_INFO("Creating RateLimiter with " + std::to_string(config.max_requests_per_second) + 
             " RPS, burst capacity " + std::to_string(config.burst_capacity));
    
    // Initialize statistics
    total_requests_ = 0;
    allowed_requests_ = 0;
    denied_requests_ = 0;
    
    // Create global bucket if per-client limits are disabled
    if (!config_.enable_per_client_limits) {
        global_bucket_ = std::make_unique<TokenBucket>(config_.burst_capacity);
    }
    
    LOG_INFO("RateLimiter created successfully");
}

RateLimiter::~RateLimiter() {
    // Cleanup is handled by smart pointers
}

bool RateLimiter::is_allowed(const std::string& client_id, uint32_t tokens_needed) {
    total_requests_++;
    
    if (!config_.enable_per_client_limits) {
        // Use global rate limiting
        return is_allowed(tokens_needed);
    }
    
    // Get or create client limiter
    ClientLimiter* limiter = get_client_limiter(client_id);
    if (!limiter) {
        denied_requests_++;
        return false;
    }
    
    bool allowed = limiter->is_allowed(tokens_needed, config_);
    
    if (allowed) {
        allowed_requests_++;
    } else {
        denied_requests_++;
    }
    
    return allowed;
}

bool RateLimiter::is_allowed(uint32_t tokens_needed) {
    if (config_.enable_per_client_limits) {
        LOG_ERROR("Global rate limiting called but per-client limits are enabled");
        return false;
    }
    
    if (!global_bucket_) {
        LOG_ERROR("Global bucket not initialized");
        return false;
    }
    
    total_requests_++;
    
    bool allowed = global_bucket_->try_consume(tokens_needed, 
                                             config_.max_requests_per_second, 
                                             config_.window_size);
    
    if (allowed) {
        allowed_requests_++;
    } else {
        denied_requests_++;
    }
    
    return allowed;
}

RateLimiter::ClientLimiter* RateLimiter::get_client_limiter(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    auto it = client_limiters_.find(client_id);
    if (it != client_limiters_.end()) {
        return it->second.get();
    }
    
    // Create new client limiter
    auto limiter = std::make_unique<ClientLimiter>(config_.burst_capacity);
    ClientLimiter* limiter_ptr = limiter.get();
    client_limiters_[client_id] = std::move(limiter);
    
    LOG_DEBUG("Created new client limiter for: " + client_id);
    return limiter_ptr;
}

void RateLimiter::remove_client(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    auto it = client_limiters_.find(client_id);
    if (it != client_limiters_.end()) {
        client_limiters_.erase(it);
        LOG_DEBUG("Removed client limiter for: " + client_id);
    }
}

void RateLimiter::update_config(const RateLimitConfig& new_config) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    config_ = new_config;
    
    // Update global bucket if it exists
    if (global_bucket_) {
        global_bucket_ = std::make_unique<TokenBucket>(new_config.burst_capacity);
    }
    
    // Update all client limiters
    for (auto& pair : client_limiters_) {
        pair.second = std::make_unique<ClientLimiter>(new_config.burst_capacity);
    }
    
    LOG_INFO("RateLimiter configuration updated");
}

const RateLimiter::RateLimitConfig& RateLimiter::get_config() const {
    return config_;
}

RateLimiter::RateLimiterStats RateLimiter::get_stats() const {
    RateLimiterStats stats;
    
    stats.total_requests = total_requests_.load();
    stats.allowed_requests = allowed_requests_.load();
    stats.denied_requests = denied_requests_.load();
    stats.active_clients = client_limiters_.size();
    stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
    
    if (stats.total_requests > 0) {
        stats.allow_rate = static_cast<double>(stats.allowed_requests) / stats.total_requests;
    } else {
        stats.allow_rate = 0.0;
    }
    
    return stats;
}

RateLimiter::ClientStats RateLimiter::get_client_stats(const std::string& client_id) const {
    ClientStats stats;
    
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    auto it = client_limiters_.find(client_id);
    if (it != client_limiters_.end()) {
        ClientLimiter* limiter = it->second.get();
        stats.request_count = limiter->request_count.load();
        stats.available_tokens = limiter->bucket->tokens.load();
        
        auto now = std::chrono::steady_clock::now();
        auto window_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - limiter->window_start);
        stats.window_remaining = std::chrono::milliseconds(
            config_.window_size.count() * 1000) - window_elapsed;
        
        if (stats.window_remaining.count() < 0) {
            stats.window_remaining = std::chrono::milliseconds(0);
        }
    } else {
        stats.request_count = 0;
        stats.available_tokens = 0;
        stats.window_remaining = std::chrono::milliseconds(0);
    }
    
    return stats;
}

void RateLimiter::reset_all_clients() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    for (auto& pair : client_limiters_) {
        pair.second->reset_window();
    }
    
    LOG_INFO("Reset all client limiters");
}

void RateLimiter::cleanup_expired_clients(std::chrono::seconds max_idle_time) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto it = client_limiters_.begin();
    
    while (it != client_limiters_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second->window_start);
        
        if (elapsed > max_idle_time && it->second->request_count.load() == 0) {
            LOG_DEBUG("Removing expired client: " + it->first);
            it = client_limiters_.erase(it);
        } else {
            ++it;
        }
    }
}

// SlidingWindowRateLimiter implementation
SlidingWindowRateLimiter::SlidingWindowRateLimiter(uint32_t max_requests, std::chrono::seconds window)
    : max_requests_per_window_(max_requests), window_size_(window) {
    
    LOG_INFO("Creating SlidingWindowRateLimiter with " + std::to_string(max_requests) + 
             " requests per " + std::to_string(window.count()) + " second window");
}

bool SlidingWindowRateLimiter::is_allowed(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    // Get or create request window for client
    auto it = client_windows_.find(client_id);
    if (it == client_windows_.end()) {
        client_windows_[client_id] = std::make_unique<RequestWindow>();
        it = client_windows_.find(client_id);
    }
    
    RequestWindow* window = it->second.get();
    
    // Clean up old requests
    window->cleanup_old_requests(window_size_);
    
    // Check if we're within the limit
    uint32_t current_requests = window->get_request_count(window_size_);
    if (current_requests >= max_requests_per_window_) {
        return false;
    }
    
    // Add the new request
    window->add_request();
    return true;
}

uint32_t SlidingWindowRateLimiter::get_request_count(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    auto it = client_windows_.find(client_id);
    if (it != client_windows_.end()) {
        return it->second->get_request_count(window_size_);
    }
    
    return 0;
}

void SlidingWindowRateLimiter::cleanup_old_requests() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    for (auto& pair : client_windows_) {
        pair.second->cleanup_old_requests(window_size_);
    }
}

// RequestWindow implementation
void SlidingWindowRateLimiter::RequestWindow::add_request() {
    std::lock_guard<std::mutex> lock(window_mutex);
    requests.push_back(std::chrono::steady_clock::now());
}

uint32_t SlidingWindowRateLimiter::RequestWindow::get_request_count(std::chrono::seconds window_size) const {
    std::lock_guard<std::mutex> lock(window_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - window_size;
    
    uint32_t count = 0;
    for (const auto& request_time : requests) {
        if (request_time > cutoff) {
            count++;
        }
    }
    
    return count;
}

void SlidingWindowRateLimiter::RequestWindow::cleanup_old_requests(std::chrono::seconds window_size) {
    std::lock_guard<std::mutex> lock(window_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - window_size;
    
    // Remove old requests
    requests.erase(
        std::remove_if(requests.begin(), requests.end(),
                      [cutoff](const auto& request_time) {
                          return request_time <= cutoff;
                      }),
        requests.end()
    );
}

} // namespace utils
} // namespace dfs
