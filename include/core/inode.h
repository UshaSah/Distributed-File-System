#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <iomanip>
#include <sstream>

namespace dfs {
namespace core {

/**
 * Inode - File system metadata for files and directories
 * Contains file information, permissions, and block pointers
 */
struct Inode {
    // File type and permissions
    uint16_t mode;
    
    // User and group ownership
    uint16_t uid, gid;
    
    // File size in bytes
    uint64_t size;
    
    // Number of blocks used by this file
    uint64_t blocks;
    
    // Access, modification, and change times
    uint64_t atime, mtime, ctime;
    
    // Direct block pointers (12 blocks = 48KB with 4KB blocks)
    uint32_t direct_blocks[12];
    
    // Single indirect block pointer
    uint32_t indirect_block;
    
    // Double indirect block pointer
    uint32_t double_indirect;
    
    // Triple indirect block pointer
    uint32_t triple_indirect;
    
    // Replication count for distributed storage
    uint32_t replication_count;
    
    // Data integrity checksum
    uint32_t checksum;
    
    // Reference count for hard links
    uint32_t link_count;
    
    // Padding for alignment
    uint8_t padding[32];
    
    Inode();
    
    // Initialize inode with default values
    void initialize(uint16_t file_mode, uint16_t user_id, uint16_t group_id);
    
    // Check if inode represents a directory
    bool is_directory() const;
    
    // Check if inode represents a regular file
    bool is_file() const;
    
    // Check if inode represents a symbolic link
    bool is_symlink() const;
    
    // Get file permissions as string (e.g., "rw-r--r--")
    std::string get_permissions_string() const;
    
    // Update access time
    void update_atime();
    
    // Update modification time
    void update_mtime();
    
    // Update change time
    void update_ctime();
    
    // Calculate and update checksum
    void update_checksum();
    
    // Validate inode integrity
    bool is_valid() const;
    
    // Get size of inode structure
    static constexpr size_t size() { return sizeof(Inode); }
    
    // Debug and information
    std::string to_string() const;
    
private:
    // Calculate checksum for integrity verification
    static uint32_t calculate_checksum(const void* data, size_t size);
};

/**
 * InodeTable - Manages inode allocation and storage
 */
class InodeTable {
private:
    std::vector<Inode> inodes_;
    std::vector<bool> free_inodes_;
    std::mutex table_mutex_;
    uint32_t next_free_inode_;
    
public:
    InodeTable(uint32_t max_inodes);
    
    // Allocate a new inode
    uint32_t allocate_inode();
    
    // Deallocate an inode
    void deallocate_inode(uint32_t inode_num);
    
    // Get inode by number
    Inode* get_inode(uint32_t inode_num);
    
    // Check if inode is free
    bool is_inode_free(uint32_t inode_num) const;
    
    // Get number of free inodes
    uint32_t get_free_inode_count() const;
    
    // Get total number of inodes
    uint32_t get_total_inode_count() const;
    
    // Serialize inode table to file
    void serialize(std::ofstream& file) const;
    
    // Deserialize inode table from file
    void deserialize(std::ifstream& file);
};

} // namespace core
} // namespace dfs
