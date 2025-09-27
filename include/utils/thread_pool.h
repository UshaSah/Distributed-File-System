#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>
#include <chrono>
#include <algorithm>

namespace dfs {
namespace utils {

/**
 * ThreadPool - Manages a pool of worker threads for concurrent task execution
 * Provides thread-safe task queuing and execution with priority support
 */
class ThreadPool {
public:
    // Task priority levels
    enum class Priority {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        CRITICAL = 3
    };
    
    // Task wrapper with priority
    struct Task {
        std::function<void()> function;
        Priority priority;
        std::chrono::steady_clock::time_point created_time;
        
        Task(std::function<void()> func, Priority prio = Priority::NORMAL);
        
        // Comparison operator for priority queue
        bool operator<(const Task& other) const;
    };

private:
    std::vector<std::thread> workers_;
    std::priority_queue<Task> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    std::atomic<size_t> active_tasks_;
    
    // Thread pool configuration
    size_t max_threads_;
    size_t min_threads_;
    std::chrono::seconds thread_timeout_;
    
    // Statistics
    std::atomic<uint64_t> total_tasks_executed_;
    std::atomic<uint64_t> total_tasks_queued_;
    std::chrono::steady_clock::time_point start_time_;

public:
    ThreadPool(size_t min_threads = 2, size_t max_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Enqueue task with priority
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args, Priority priority = Priority::NORMAL)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // Enqueue task without return value
    void enqueue(std::function<void()> task, Priority priority = Priority::NORMAL);
    
    // Get current queue size
    size_t get_queue_size() const;
    
    // Get number of active threads
    size_t get_active_thread_count() const;
    
    // Get total number of threads
    size_t get_thread_count() const;
    
    // Wait for all tasks to complete
    void wait_for_all_tasks();
    
    // Shutdown thread pool
    void shutdown();
    
    // Check if thread pool is running
    bool is_running() const;
    
    // Get thread pool statistics
    struct ThreadPoolStats {
        size_t total_threads;
        size_t active_threads;
        size_t queued_tasks;
        uint64_t total_tasks_executed;
        uint64_t total_tasks_queued;
        std::chrono::milliseconds uptime;
        double average_task_duration;
    };
    ThreadPoolStats get_stats() const;
    
    // Set thread timeout
    void set_thread_timeout(std::chrono::seconds timeout);
    
    // Get thread timeout
    std::chrono::seconds get_thread_timeout() const;

private:
    void worker_thread();
    void adjust_thread_count();
};

// Template implementation
template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args, Priority priority)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        
        tasks_.emplace([task]() { (*task)(); }, priority);
        total_tasks_queued_++;
    }
    
    condition_.notify_one();
    return result;
}

} // namespace utils
} // namespace dfs
