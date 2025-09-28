#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <fstream>
#include <memory>

namespace dfs {
namespace core {

/**
 * BlockManager - Manages data block allocation and deallocation
 * Provides thread-safe block management with bitmap tracking
 */
class BlockManager {
private:
    std::vector<bool> block_bitmap_;
    std::mutex bitmap_mutex_;
    uint32_t total_blocks_;
    uint32_t block_size_;
    uint32_t next_free_block_;
    
    // Find next free block starting from given index
    uint32_t find_next_free_block(uint32_t start_index = 0) const;
    
public:
    BlockManager(uint32_t total_blocks, uint32_t block_size);
    
    // Allocate a single block
    uint32_t allocate_block();
    
    // Allocate multiple contiguous blocks
    std::vector<uint32_t> allocate_blocks(uint32_t count);
    
    // Deallocate a single block
    void deallocate_block(uint32_t block_id);
    
    // Deallocate multiple blocks
    void deallocate_blocks(const std::vector<uint32_t>& block_ids);
    
    // Check if block is free
    bool is_block_free(uint32_t block_id) const;
    
    // Mark block as used
    void mark_block_used(uint32_t block_id);
    
    // Mark block as free
    void mark_block_free(uint32_t block_id);
    
    // Get number of free blocks
    uint32_t get_free_block_count() const;
    
    // Get total number of blocks
    uint32_t get_total_block_count() const;
    
    // Get block size
    uint32_t get_block_size() const;
    
    // Get block usage statistics
    struct BlockStats {
        uint32_t total_blocks;
        uint32_t free_blocks;
        uint32_t used_blocks;
        double usage_percentage;
    };
    BlockStats get_block_stats() const;
    
    // Serialize block bitmap to file
    void serialize_bitmap(std::ofstream& file) const;
    
    // Deserialize block bitmap from file
    void deserialize_bitmap(std::ifstream& file);
    
    // Defragment blocks (optional optimization)
    void defragment_blocks();
    
    // Validate block manager integrity
    bool is_valid() const;
};

/**
 * DataBlock - Represents a single data block
 */
class DataBlock {
private:
    std::vector<uint8_t> data_;
    uint32_t block_id_;
    uint32_t block_size_;
    mutable std::mutex data_mutex_;
    
public:
    DataBlock(uint32_t block_id, uint32_t block_size);
    
    // Read data from block
    std::vector<uint8_t> read_data(uint32_t offset = 0, uint32_t size = 0) const;
    
    // Write data to block
    bool write_data(const std::vector<uint8_t>& data, uint32_t offset = 0);
    
    // Clear block data
    void clear();
    
    // Get block ID
    uint32_t get_block_id() const;
    
    // Get block size
    uint32_t get_block_size() const;
    
    // Get actual data size
    uint32_t get_data_size() const;
    
    // Check if block is empty
    bool is_empty() const;
    
    // Serialize block to file
    void serialize(std::ofstream& file) const;
    
    // Deserialize block from file
    void deserialize(std::ifstream& file);
};

} // namespace core
} // namespace dfs
