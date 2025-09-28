#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace dfs {
namespace core {

/**
 * SuperBlock - File system metadata and configuration
 * Contains essential information about the file system layout and state
 */
struct SuperBlock {
    // Magic number to identify the file system
    static constexpr uint32_t MAGIC_NUMBER = 0xDF5F0001;
    
    // File system signature
    uint32_t magic_number;
    
    // Block size in bytes (typically 4KB)
    uint32_t block_size;
    
    // Total number of blocks in the file system
    uint32_t total_blocks;
    
    // Number of free blocks available
    uint32_t free_blocks;
    
    // Total number of inodes
    uint32_t inode_count;
    
    // Number of free inodes
    uint32_t free_inodes;
    
    // Root directory inode number
    uint32_t root_inode;
    
    // Timestamp of last mount
    uint64_t last_mount_time;
    
    // Timestamp of last write
    uint64_t last_write_time;
    
    // File system version
    uint32_t version;
    
    // Checksum for integrity verification
    uint32_t checksum;
    
    // Padding to align to block boundary
    uint8_t padding[64];
    
    SuperBlock();
    
    // Initialize superblock with default values
    void initialize(uint32_t total_blocks, uint32_t block_size = 4096);
    
    // Validate superblock integrity
    bool is_valid() const;
    
    // Calculate and update checksum
    void update_checksum();
    
    // Serialize to binary format
    void serialize(std::ofstream& file) const;
    
    // Deserialize from binary format
    void deserialize(std::ifstream& file);
    
    // Get size of superblock structure
    static constexpr size_t size() { return sizeof(SuperBlock); }
    
    // Block and inode management
    bool allocate_block();
    bool deallocate_block();
    bool allocate_inode();
    bool deallocate_inode();
    
    // Utility methods
    void update_mount_time();
    bool is_space_available(uint32_t blocks_needed) const;
    bool are_inodes_available(uint32_t inodes_needed) const;
    uint32_t get_usage_percentage() const;
    uint32_t get_inode_usage_percentage() const;
    
    // Debug and information
    std::string to_string() const;
    
private:
    // Calculate checksum for integrity verification
    static uint32_t calculate_checksum(const void* data, size_t size);
};

} // namespace core
} // namespace dfs
