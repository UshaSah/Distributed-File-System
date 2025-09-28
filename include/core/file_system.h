#pragma once

#include "superblock.h"
#include "inode.h"
#include "block_manager.h"
#include "transaction_manager.h"
#include <string>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace dfs {
namespace core {

/**
 * FileSystem - Main file system implementation
 * Coordinates all file system components and provides high-level operations
 */
class FileSystem {
private:
    std::unique_ptr<SuperBlock> superblock_;
    std::unique_ptr<InodeTable> inode_table_;
    std::unique_ptr<BlockManager> block_manager_;
    std::unique_ptr<TransactionManager> transaction_manager_;
    
    // File system state
    std::string mount_point_;
    bool is_mounted_;
    mutable std::shared_mutex fs_mutex_;
    
    // Inode locking for concurrent access
    mutable std::unordered_map<uint32_t, std::shared_mutex> inode_mutexes_;
    mutable std::mutex inode_mutex_map_mutex_;
    
    // Get or create mutex for specific inode
    std::shared_mutex& get_inode_mutex(uint32_t inode_num) const;
    
    // Path resolution
    uint32_t resolve_path(const std::string& path) const;
    std::string get_parent_directory(const std::string& path) const;
    std::string get_filename(const std::string& path) const;
    
    // Directory operations
    bool add_directory_entry(uint32_t dir_inode, const std::string& name, uint32_t inode_num);
    bool remove_directory_entry(uint32_t dir_inode, const std::string& name);
    std::vector<std::string> list_directory(uint32_t dir_inode) const;
    
    // Block operations
    std::vector<uint32_t> get_file_blocks(uint32_t inode_num) const;
    bool write_file_blocks(uint32_t inode_num, const std::vector<uint8_t>& data);
    std::vector<uint8_t> read_file_blocks(uint32_t inode_num) const;
    
public:
    FileSystem();
    ~FileSystem();
    
    // File system lifecycle
    bool format(const std::string& device_path, uint32_t total_blocks, uint32_t block_size = 4096);
    bool mount(const std::string& device_path);
    bool unmount();
    bool is_mounted() const;
    
    // File operations
    bool create_file(const std::string& path, uint16_t permissions = 0644);
    bool create_directory(const std::string& path, uint16_t permissions = 0755);
    bool delete_file(const std::string& path);
    bool delete_directory(const std::string& path);
    bool file_exists(const std::string& path) const;
    bool directory_exists(const std::string& path) const;
    
    // File I/O operations
    std::vector<uint8_t> read_file(const std::string& path) const;
    bool write_file(const std::string& path, const std::vector<uint8_t>& data);
    bool append_file(const std::string& path, const std::vector<uint8_t>& data);
    uint64_t get_file_size(const std::string& path) const;
    
    // Directory operations
    std::vector<std::string> list_directory(const std::string& path) const;
    bool rename(const std::string& old_path, const std::string& new_path);
    bool move(const std::string& old_path, const std::string& new_path);
    
    // Metadata operations
    Inode* get_inode(const std::string& path) const;
    bool set_permissions(const std::string& path, uint16_t permissions);
    bool set_ownership(const std::string& path, uint16_t uid, uint16_t gid);
    
    // Transaction operations
    uint64_t begin_transaction();
    bool commit_transaction(uint64_t tx_id);
    bool rollback_transaction(uint64_t tx_id);
    
    // File system information
    struct FileSystemInfo {
        uint32_t total_blocks;
        uint32_t free_blocks;
        uint32_t total_inodes;
        uint32_t free_inodes;
        uint32_t block_size;
        double usage_percentage;
    };
    FileSystemInfo get_filesystem_info() const;
    
    // Maintenance operations
    bool check_filesystem() const;
    bool repair_filesystem();
    void defragment();
    
    // Statistics
    struct FileSystemStats {
        uint32_t total_files;
        uint32_t total_directories;
        uint64_t total_data_size;
        uint32_t active_transactions;
    };
    FileSystemStats get_filesystem_stats() const;
};

} // namespace core
} // namespace dfs
