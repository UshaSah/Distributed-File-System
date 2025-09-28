#include "core/transaction_manager.h"
#include "utils/logger.h"
#include "utils/exceptions.h"
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace dfs {
namespace core {

// LogEntry implementation
LogEntry::LogEntry()
    : transaction_id(0), operation_type(0), inode_number(0), block_number(0),
      timestamp(0), checksum(0) {}

LogEntry::LogEntry(uint64_t tx_id, uint32_t op_type, uint32_t inode, uint32_t block)
    : transaction_id(tx_id), operation_type(op_type), inode_number(inode), block_number(block),
      timestamp(0), checksum(0) {
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    timestamp = static_cast<uint64_t>(time_t);
}

void LogEntry::serialize(std::ofstream& file) const {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot serialize LogEntry: file not open");
    }
    
    // Write fixed-size fields
    file.write(reinterpret_cast<const char*>(&transaction_id), sizeof(transaction_id));
    file.write(reinterpret_cast<const char*>(&operation_type), sizeof(operation_type));
    file.write(reinterpret_cast<const char*>(&inode_number), sizeof(inode_number));
    file.write(reinterpret_cast<const char*>(&block_number), sizeof(block_number));
    file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
    file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    
    // Write variable-size data
    uint32_t old_data_size = static_cast<uint32_t>(old_data.size());
    uint32_t new_data_size = static_cast<uint32_t>(new_data.size());
    
    file.write(reinterpret_cast<const char*>(&old_data_size), sizeof(old_data_size));
    if (old_data_size > 0) {
        file.write(reinterpret_cast<const char*>(old_data.data()), old_data_size);
    }
    
    file.write(reinterpret_cast<const char*>(&new_data_size), sizeof(new_data_size));
    if (new_data_size > 0) {
        file.write(reinterpret_cast<const char*>(new_data.data()), new_data_size);
    }
    
    if (file.fail()) {
        throw dfs::utils::FileSystemException("Failed to serialize LogEntry");
    }
}

void LogEntry::deserialize(std::ifstream& file) {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot deserialize LogEntry: file not open");
    }
    
    // Read fixed-size fields
    file.read(reinterpret_cast<char*>(&transaction_id), sizeof(transaction_id));
    file.read(reinterpret_cast<char*>(&operation_type), sizeof(operation_type));
    file.read(reinterpret_cast<char*>(&inode_number), sizeof(inode_number));
    file.read(reinterpret_cast<char*>(&block_number), sizeof(block_number));
    file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
    file.read(reinterpret_cast<char*>(&checksum), sizeof(checksum));
    
    if (file.fail() || file.gcount() != sizeof(transaction_id) + sizeof(operation_type) + 
                      sizeof(inode_number) + sizeof(block_number) + 
                      sizeof(timestamp) + sizeof(checksum)) {
        throw dfs::utils::FileSystemException("Failed to deserialize LogEntry header");
    }
    
    // Read variable-size data
    uint32_t old_data_size, new_data_size;
    
    file.read(reinterpret_cast<char*>(&old_data_size), sizeof(old_data_size));
    if (file.fail() || file.gcount() != sizeof(old_data_size)) {
        throw dfs::utils::FileSystemException("Failed to deserialize LogEntry old_data_size");
    }
    
    if (old_data_size > 0) {
        old_data.resize(old_data_size);
        file.read(reinterpret_cast<char*>(old_data.data()), old_data_size);
        if (file.fail() || file.gcount() != old_data_size) {
            throw dfs::utils::FileSystemException("Failed to deserialize LogEntry old_data");
        }
    }
    
    file.read(reinterpret_cast<char*>(&new_data_size), sizeof(new_data_size));
    if (file.fail() || file.gcount() != sizeof(new_data_size)) {
        throw dfs::utils::FileSystemException("Failed to deserialize LogEntry new_data_size");
    }
    
    if (new_data_size > 0) {
        new_data.resize(new_data_size);
        file.read(reinterpret_cast<char*>(new_data.data()), new_data_size);
        if (file.fail() || file.gcount() != new_data_size) {
            throw dfs::utils::FileSystemException("Failed to deserialize LogEntry new_data");
        }
    }
}

void LogEntry::update_checksum() {
    checksum = 0;
    
    // Calculate checksum over all fields except checksum itself
    uint32_t temp_checksum = 0;
    
    // Add transaction_id
    temp_checksum ^= static_cast<uint32_t>(transaction_id);
    temp_checksum ^= static_cast<uint32_t>(transaction_id >> 32);
    
    // Add other fields
    temp_checksum ^= operation_type;
    temp_checksum ^= inode_number;
    temp_checksum ^= block_number;
    temp_checksum ^= static_cast<uint32_t>(timestamp);
    temp_checksum ^= static_cast<uint32_t>(timestamp >> 32);
    
    // Add data
    for (uint8_t byte : old_data) {
        temp_checksum = (temp_checksum << 1) ^ byte;
        if (temp_checksum & 0x80000000) {
            temp_checksum ^= 0x04C11DB7; // CRC32 polynomial
        }
    }
    
    for (uint8_t byte : new_data) {
        temp_checksum = (temp_checksum << 1) ^ byte;
        if (temp_checksum & 0x80000000) {
            temp_checksum ^= 0x04C11DB7; // CRC32 polynomial
        }
    }
    
    checksum = temp_checksum;
}

bool LogEntry::is_valid() const {
    // Calculate expected checksum
    LogEntry temp = *this;
    temp.checksum = 0;
    temp.update_checksum();
    
    return checksum == temp.checksum;
}

// Transaction implementation
Transaction::Transaction(uint64_t tx_id)
    : transaction_id(tx_id), start_time(std::chrono::steady_clock::now()),
      is_active(true), is_committed(false), is_aborted(false) {}

void Transaction::add_log_entry(const LogEntry& entry) {
    log_entries.push_back(entry);
}

std::chrono::milliseconds Transaction::get_duration() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
}

bool Transaction::is_expired(std::chrono::seconds timeout) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    return elapsed > timeout;
}

// TransactionManager implementation
TransactionManager::TransactionManager(const std::string& log_file_path)
    : log_file_path_(log_file_path), next_transaction_id_(1),
      transaction_timeout_(std::chrono::seconds(30)) {
    
    LOG_INFO("Creating TransactionManager with log file: " + log_file_path);
    
    // Open log file for appending
    log_file_.open(log_file_path, std::ios::app | std::ios::binary);
    if (!log_file_.is_open()) {
        LOG_ERROR("Failed to open transaction log file: " + log_file_path);
        throw dfs::utils::FileSystemException("Failed to open transaction log file");
    }
    
    LOG_INFO("TransactionManager created successfully");
}

TransactionManager::~TransactionManager() {
    // Close log file
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    // Clean up any remaining transactions
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    active_transactions_.clear();
}

uint64_t TransactionManager::begin_transaction() {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    uint64_t tx_id = generate_transaction_id();
    auto transaction = std::make_unique<Transaction>(tx_id);
    active_transactions_[tx_id] = std::move(transaction);
    
    LOG_DEBUG("Started transaction " + std::to_string(tx_id));
    return tx_id;
}

bool TransactionManager::commit_transaction(uint64_t tx_id) {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    auto it = active_transactions_.find(tx_id);
    if (it == active_transactions_.end()) {
        LOG_ERROR("Transaction not found: " + std::to_string(tx_id));
        throw dfs::utils::TransactionNotFoundException(tx_id);
    }
    
    Transaction* transaction = it->second.get();
    
    if (transaction->is_aborted.load()) {
        LOG_ERROR("Cannot commit aborted transaction: " + std::to_string(tx_id));
        return false;
    }
    
    if (transaction->is_committed.load()) {
        LOG_WARN("Transaction already committed: " + std::to_string(tx_id));
        return true;
    }
    
    try {
        // Write all log entries to disk
        for (const LogEntry& entry : transaction->log_entries) {
            write_log_entry(entry);
        }
        
        // Mark transaction as committed
        transaction->is_committed.store(true);
        transaction->is_active.store(false);
        
        // Remove from active transactions
        active_transactions_.erase(it);
        
        LOG_DEBUG("Committed transaction " + std::to_string(tx_id) + 
                 " with " + std::to_string(transaction->log_entries.size()) + " log entries");
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to commit transaction " + std::to_string(tx_id) + ": " + e.what());
        transaction->is_aborted.store(true);
        transaction->is_active.store(false);
        return false;
    }
}

bool TransactionManager::rollback_transaction(uint64_t tx_id) {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    auto it = active_transactions_.find(tx_id);
    if (it == active_transactions_.end()) {
        LOG_ERROR("Transaction not found: " + std::to_string(tx_id));
        throw dfs::utils::TransactionNotFoundException(tx_id);
    }
    
    Transaction* transaction = it->second.get();
    
    if (transaction->is_committed.load()) {
        LOG_ERROR("Cannot rollback committed transaction: " + std::to_string(tx_id));
        return false;
    }
    
    if (transaction->is_aborted.load()) {
        LOG_WARN("Transaction already aborted: " + std::to_string(tx_id));
        return true;
    }
    
    // Mark transaction as aborted
    transaction->is_aborted.store(true);
    transaction->is_active.store(false);
    
    // Remove from active transactions
    active_transactions_.erase(it);
    
    LOG_DEBUG("Rolled back transaction " + std::to_string(tx_id) + 
             " with " + std::to_string(transaction->log_entries.size()) + " log entries");
    
    return true;
}

bool TransactionManager::is_transaction_active(uint64_t tx_id) const {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    auto it = active_transactions_.find(tx_id);
    if (it == active_transactions_.end()) {
        return false;
    }
    
    return it->second->is_active.load();
}

void TransactionManager::add_log_entry(uint64_t tx_id, const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    auto it = active_transactions_.find(tx_id);
    if (it == active_transactions_.end()) {
        LOG_ERROR("Transaction not found: " + std::to_string(tx_id));
        throw dfs::utils::TransactionNotFoundException(tx_id);
    }
    
    Transaction* transaction = it->second.get();
    
    if (!transaction->is_active.load()) {
        LOG_ERROR("Cannot add log entry to inactive transaction: " + std::to_string(tx_id));
        throw dfs::utils::TransactionAbortedException(tx_id, "Transaction is not active");
    }
    
    // Update checksum before adding
    LogEntry entry_copy = entry;
    entry_copy.update_checksum();
    
    transaction->add_log_entry(entry_copy);
    
    LOG_DEBUG("Added log entry to transaction " + std::to_string(tx_id));
}

Transaction* TransactionManager::get_transaction(uint64_t tx_id) {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    auto it = active_transactions_.find(tx_id);
    if (it == active_transactions_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

void TransactionManager::cleanup_expired_transactions() {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    auto it = active_transactions_.begin();
    while (it != active_transactions_.end()) {
        Transaction* transaction = it->second.get();
        
        if (transaction->is_expired(transaction_timeout_)) {
            LOG_WARN("Transaction " + std::to_string(it->first) + " expired, rolling back");
            
            transaction->is_aborted.store(true);
            transaction->is_active.store(false);
            
            it = active_transactions_.erase(it);
        } else {
            ++it;
        }
    }
}

uint32_t TransactionManager::get_active_transaction_count() const {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    return static_cast<uint32_t>(active_transactions_.size());
}

void TransactionManager::set_transaction_timeout(std::chrono::seconds timeout) {
    transaction_timeout_ = timeout;
    LOG_DEBUG("Transaction timeout set to " + std::to_string(timeout.count()) + " seconds");
}

std::chrono::seconds TransactionManager::get_transaction_timeout() const {
    return transaction_timeout_;
}

void TransactionManager::checkpoint() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (log_file_.is_open()) {
        log_file_.flush();
        LOG_DEBUG("Transaction log checkpoint completed");
    }
}

void TransactionManager::recover() {
    LOG_INFO("Starting transaction recovery from log file");
    
    try {
        replay_log_entries();
        LOG_INFO("Transaction recovery completed successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("Transaction recovery failed: " + std::string(e.what()));
        throw;
    }
}

TransactionManager::TransactionStats TransactionManager::get_transaction_stats() const {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    TransactionStats stats;
    stats.active_transactions = static_cast<uint32_t>(active_transactions_.size());
    stats.total_transactions = static_cast<uint32_t>(next_transaction_id_.load() - 1);
    stats.total_log_entries = 0;
    stats.average_duration = std::chrono::milliseconds(0);
    
    // Calculate total log entries and average duration
    uint64_t total_duration = 0;
    uint32_t completed_transactions = 0;
    
    for (const auto& pair : active_transactions_) {
        const Transaction* transaction = pair.second.get();
        stats.total_log_entries += static_cast<uint32_t>(transaction->log_entries.size());
        
        if (!transaction->is_active.load()) {
            total_duration += transaction->get_duration().count();
            completed_transactions++;
        }
    }
    
    if (completed_transactions > 0) {
        stats.average_duration = std::chrono::milliseconds(total_duration / completed_transactions);
    }
    
    return stats;
}

uint64_t TransactionManager::generate_transaction_id() {
    return next_transaction_id_.fetch_add(1);
}

void TransactionManager::write_log_entry(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (!log_file_.is_open()) {
        throw dfs::utils::FileSystemException("Transaction log file is not open");
    }
    
    entry.serialize(log_file_);
    log_file_.flush();
}

void TransactionManager::replay_log_entries() {
    std::ifstream log_file(log_file_path_, std::ios::binary);
    if (!log_file.is_open()) {
        LOG_WARN("Cannot open log file for recovery: " + log_file_path_);
        return;
    }
    
    uint32_t replayed_entries = 0;
    
    try {
        while (log_file.good() && !log_file.eof()) {
            LogEntry entry;
            entry.deserialize(log_file);
            
            if (log_file.good()) {
                if (entry.is_valid()) {
                    // Process the log entry for recovery
                    // This would typically involve applying the changes to the file system
                    LOG_DEBUG("Replayed log entry for transaction " + std::to_string(entry.transaction_id));
                    replayed_entries++;
                } else {
                    LOG_WARN("Invalid log entry found during recovery");
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error during log replay: " + std::string(e.what()));
    }
    
    LOG_INFO("Replayed " + std::to_string(replayed_entries) + " log entries during recovery");
}

// TransactionGuard implementation
TransactionGuard::TransactionGuard(TransactionManager* manager)
    : manager_(manager), committed_(false) {
    
    if (manager_) {
        transaction_id_ = manager_->begin_transaction();
    }
}

TransactionGuard::~TransactionGuard() {
    if (manager_ && !committed_) {
        try {
            manager_->rollback_transaction(transaction_id_);
        } catch (const std::exception& e) {
            // Log error but don't throw from destructor
            LOG_ERROR("Failed to rollback transaction in destructor: " + std::string(e.what()));
        }
    }
}

uint64_t TransactionGuard::get_transaction_id() const {
    return transaction_id_;
}

void TransactionGuard::commit() {
    if (manager_ && !committed_) {
        manager_->commit_transaction(transaction_id_);
        committed_ = true;
    }
}

void TransactionGuard::abort() {
    if (manager_ && !committed_) {
        manager_->rollback_transaction(transaction_id_);
        committed_ = true;
    }
}

} // namespace core
} // namespace dfs
