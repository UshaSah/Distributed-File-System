#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <chrono>
#include <atomic>

namespace dfs {
namespace core {

/**
 * LogEntry - Represents a single operation in the transaction log
 */
struct LogEntry {
    uint64_t transaction_id;
    uint32_t operation_type;     // CREATE, WRITE, DELETE, etc.
    uint32_t inode_number;
    uint32_t block_number;
    std::vector<uint8_t> old_data;  // For rollback
    std::vector<uint8_t> new_data;  // New data
    uint64_t timestamp;
    uint32_t checksum;
    
    LogEntry();
    LogEntry(uint64_t tx_id, uint32_t op_type, uint32_t inode, uint32_t block);
    
    // Serialize log entry to binary format
    void serialize(std::ofstream& file) const;
    
    // Deserialize log entry from binary format
    void deserialize(std::ifstream& file);
    
    // Calculate and update checksum
    void update_checksum();
    
    // Validate log entry integrity
    bool is_valid() const;
};

/**
 * Transaction - Represents an active transaction
 */
struct Transaction {
    uint64_t transaction_id;
    std::vector<LogEntry> log_entries;
    std::chrono::steady_clock::time_point start_time;
    std::atomic<bool> is_active;
    std::atomic<bool> is_committed;
    std::atomic<bool> is_aborted;
    
    Transaction(uint64_t tx_id);
    
    // Add log entry to transaction
    void add_log_entry(const LogEntry& entry);
    
    // Get transaction duration
    std::chrono::milliseconds get_duration() const;
    
    // Check if transaction is expired
    bool is_expired(std::chrono::seconds timeout) const;
};

/**
 * TransactionManager - Manages ACID transactions with write-ahead logging
 */
class TransactionManager {
private:
    std::unordered_map<uint64_t, std::unique_ptr<Transaction>> active_transactions_;
    std::mutex transaction_mutex_;
    std::atomic<uint64_t> next_transaction_id_;
    std::string log_file_path_;
    std::ofstream log_file_;
    std::mutex log_mutex_;
    
    // Transaction timeout (default 30 seconds)
    std::chrono::seconds transaction_timeout_;
    
    // Generate unique transaction ID
    uint64_t generate_transaction_id();
    
    // Write log entry to disk
    void write_log_entry(const LogEntry& entry);
    
    // Replay log entries for recovery
    void replay_log_entries();
    
public:
    TransactionManager(const std::string& log_file_path);
    ~TransactionManager();
    
    // Begin a new transaction
    uint64_t begin_transaction();
    
    // Commit a transaction
    bool commit_transaction(uint64_t tx_id);
    
    // Abort/rollback a transaction
    bool rollback_transaction(uint64_t tx_id);
    
    // Check if transaction is active
    bool is_transaction_active(uint64_t tx_id) const;
    
    // Add log entry to transaction
    void add_log_entry(uint64_t tx_id, const LogEntry& entry);
    
    // Get transaction by ID
    Transaction* get_transaction(uint64_t tx_id);
    
    // Clean up expired transactions
    void cleanup_expired_transactions();
    
    // Get active transaction count
    uint32_t get_active_transaction_count() const;
    
    // Set transaction timeout
    void set_transaction_timeout(std::chrono::seconds timeout);
    
    // Get transaction timeout
    std::chrono::seconds get_transaction_timeout() const;
    
    // Force checkpoint (flush all committed transactions)
    void checkpoint();
    
    // Recover from log file
    void recover();
    
    // Get transaction statistics
    struct TransactionStats {
        uint32_t active_transactions;
        uint32_t total_transactions;
        uint64_t total_log_entries;
        std::chrono::milliseconds average_duration;
    };
    TransactionStats get_transaction_stats() const;
};

/**
 * TransactionGuard - RAII wrapper for automatic transaction management
 */
class TransactionGuard {
private:
    uint64_t transaction_id_;
    TransactionManager* manager_;
    bool committed_;
    
public:
    TransactionGuard(TransactionManager* manager);
    ~TransactionGuard();
    
    // Get transaction ID
    uint64_t get_transaction_id() const;
    
    // Commit transaction
    void commit();
    
    // Abort transaction
    void abort();
    
    // Disable copy constructor and assignment
    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;
};

} // namespace core
} // namespace dfs
