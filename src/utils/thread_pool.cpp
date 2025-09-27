#include "utils/thread_pool.h"
#include "utils/logger.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace dfs {
namespace utils {

// Task implementation
ThreadPool::Task::Task(std::function<void()> func, Priority prio)
    : function(std::move(func)), priority(prio), 
      created_time(std::chrono::steady_clock::now()) {}

bool ThreadPool::Task::operator<(const Task& other) const {
    // Higher priority tasks come first
    if (priority != other.priority) {
        return static_cast<int>(priority) < static_cast<int>(other.priority);
    }
    
    // For same priority, older tasks come first (FIFO)
    return created_time > other.created_time;
}

// ThreadPool implementation
ThreadPool::ThreadPool(size_t min_threads, size_t max_threads)
    : stop_(false), max_threads_(max_threads), min_threads_(min_threads),
      thread_timeout_(std::chrono::seconds(300)), start_time_(std::chrono::steady_clock::now()) {
    
    LOG_INFO("Creating ThreadPool with " + std::to_string(min_threads) + 
             " min threads, " + std::to_string(max_threads) + " max threads");
    
    // Initialize statistics
    total_tasks_executed_ = 0;
    total_tasks_queued_ = 0;
    active_tasks_ = 0;
    
    // Create initial worker threads
    for (size_t i = 0; i < min_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_thread, this);
    }
    
    LOG_INFO("ThreadPool created with " + std::to_string(workers_.size()) + " worker threads");
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::enqueue(std::function<void()> task, Priority priority) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (stop_) {
            LOG_WARN("Cannot enqueue task: ThreadPool is stopped");
            throw std::runtime_error("ThreadPool is stopped");
        }
        
        tasks_.emplace(std::move(task), priority);
        total_tasks_queued_++;
    }
    
    condition_.notify_one();
    
    // Adjust thread count if needed
    adjust_thread_count();
}

size_t ThreadPool::get_queue_size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

size_t ThreadPool::get_active_thread_count() const {
    return active_tasks_.load();
}

size_t ThreadPool::get_thread_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return workers_.size();
}

void ThreadPool::wait_for_all_tasks() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    condition_.wait(lock, [this] { 
        return tasks_.empty() && active_tasks_.load() == 0; 
    });
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    
    // Join all worker threads
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers_.clear();
    
    LOG_INFO("ThreadPool shutdown completed");
}

bool ThreadPool::is_running() const {
    return !stop_.load();
}

ThreadPool::ThreadPoolStats ThreadPool::get_stats() const {
    ThreadPoolStats stats;
    
    stats.total_threads = workers_.size();
    stats.active_threads = active_tasks_.load();
    stats.queued_tasks = get_queue_size();
    stats.total_tasks_executed = total_tasks_executed_.load();
    stats.total_tasks_queued = total_tasks_queued_.load();
    stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
    
    // Calculate average task duration (simplified)
    if (stats.total_tasks_executed > 0) {
        stats.average_task_duration = std::chrono::milliseconds(
            stats.uptime.count() / stats.total_tasks_executed);
    } else {
        stats.average_task_duration = std::chrono::milliseconds(0);
    }
    
    return stats;
}

void ThreadPool::set_thread_timeout(std::chrono::seconds timeout) {
    thread_timeout_ = timeout;
    LOG_DEBUG("Thread timeout set to " + std::to_string(timeout.count()) + " seconds");
}

std::chrono::seconds ThreadPool::get_thread_timeout() const {
    return thread_timeout_;
}

void ThreadPool::worker_thread() {
    LOG_DEBUG("Worker thread started");
    
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // Wait for work or shutdown signal
            condition_.wait(lock, [this] { 
                return stop_ || !tasks_.empty(); 
            });
            
            if (stop_ && tasks_.empty()) {
                break;
            }
            
            if (!tasks_.empty()) {
                task = std::move(const_cast<Task&>(tasks_.top()).function);
                tasks_.pop();
                active_tasks_++;
            }
        }
        
        if (task) {
            try {
                auto start_time = std::chrono::steady_clock::now();
                
                // Execute the task
                task();
                
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);
                
                total_tasks_executed_++;
                
                LOG_DEBUG("Task executed in " + std::to_string(duration.count()) + "ms");
                
            } catch (const std::exception& e) {
                LOG_ERROR("Task execution failed: " + std::string(e.what()));
            } catch (...) {
                LOG_ERROR("Task execution failed with unknown exception");
            }
            
            active_tasks_--;
        }
        
        // Adjust thread count periodically
        adjust_thread_count();
    }
    
    LOG_DEBUG("Worker thread stopped");
}

void ThreadPool::adjust_thread_count() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    size_t current_threads = workers_.size();
    size_t queue_size = tasks_.size();
    size_t active_threads = active_tasks_.load();
    
    // Add threads if we have work and can add more
    if (queue_size > 0 && current_threads < max_threads_ && 
        active_threads >= current_threads * 0.8) {
        
        size_t threads_to_add = std::min(
            std::min(queue_size, max_threads_ - current_threads),
            static_cast<size_t>(2) // Add at most 2 threads at a time
        );
        
        for (size_t i = 0; i < threads_to_add; ++i) {
            workers_.emplace_back(&ThreadPool::worker_thread, this);
            LOG_DEBUG("Added worker thread, total: " + std::to_string(workers_.size()));
        }
    }
    // Remove threads if we have too many and little work
    else if (current_threads > min_threads_ && queue_size == 0 && 
             active_threads < current_threads * 0.2) {
        
        // Mark threads for removal (they will exit on next iteration)
        // This is a simplified approach - in practice, you might want
        // a more sophisticated thread management system
        LOG_DEBUG("Thread pool has excess capacity, but keeping minimum threads");
    }
}

} // namespace utils
} // namespace dfs
