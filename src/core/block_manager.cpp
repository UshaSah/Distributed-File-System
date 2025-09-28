#include "core/block_manager.h"
#include "utils/logger.h"
#include "utils/exceptions.h"
#include <algorithm>
#include <random>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dfs {
namespace core {

// BlockManager implementation
BlockManager::BlockManager(uint32_t total_blocks, uint32_t block_size)
    : total_blocks_(total_blocks), block_size_(block_size), next_free_block_(0) {
    
    LOG_INFO("Creating BlockManager with " + std::to_string(total_blocks) + 
             " blocks of size " + std::to_string(block_size));
    
    // Initialize block bitmap (all blocks start as free)
    block_bitmap_.resize(total_blocks, true);
    
    // Reserve block 0 for superblock
    if (total_blocks > 0) {
        block_bitmap_[0] = false;
        next_free_block_ = 1;
    }
    
    LOG_INFO("BlockManager created successfully");
}

uint32_t BlockManager::allocate_block() {
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    
    // Find next free block
    uint32_t block_id = find_next_free_block(next_free_block_);
    
    if (block_id == UINT32_MAX) {
        LOG_ERROR("No free blocks available");
        throw dfs::utils::InsufficientSpaceException(1, get_free_block_count());
    }
    
    // Mark block as used
    block_bitmap_[block_id] = false;
    next_free_block_ = (block_id + 1) % total_blocks_;
    
    LOG_DEBUG("Allocated block " + std::to_string(block_id));
    return block_id;
}

std::vector<uint32_t> BlockManager::allocate_blocks(uint32_t count) {
    if (count == 0) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    
    std::vector<uint32_t> allocated_blocks;
    allocated_blocks.reserve(count);
    
    uint32_t start_block = next_free_block_;
    uint32_t consecutive_count = 0;
    uint32_t current_block = start_block;
    
    // Try to find consecutive blocks first
    for (uint32_t i = 0; i < total_blocks_; ++i) {
        if (block_bitmap_[current_block]) {
            consecutive_count++;
            if (consecutive_count == count) {
                // Found enough consecutive blocks
                for (uint32_t j = 0; j < count; ++j) {
                    uint32_t block_id = (start_block + j) % total_blocks_;
                    block_bitmap_[block_id] = false;
                    allocated_blocks.push_back(block_id);
                }
                next_free_block_ = (start_block + count) % total_blocks_;
                
                LOG_DEBUG("Allocated " + std::to_string(count) + " consecutive blocks starting at " + 
                         std::to_string(start_block));
                return allocated_blocks;
            }
        } else {
            consecutive_count = 0;
            start_block = (current_block + 1) % total_blocks_;
        }
        current_block = (current_block + 1) % total_blocks_;
    }
    
    // If we can't find consecutive blocks, allocate individually
    LOG_WARN("Could not find " + std::to_string(count) + " consecutive blocks, allocating individually");
    
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t block_id = find_next_free_block(next_free_block_);
        if (block_id == UINT32_MAX) {
            // Deallocate already allocated blocks
            for (uint32_t allocated_block : allocated_blocks) {
                block_bitmap_[allocated_block] = true;
            }
            throw dfs::utils::InsufficientSpaceException(count, get_free_block_count());
        }
        
        block_bitmap_[block_id] = false;
        allocated_blocks.push_back(block_id);
        next_free_block_ = (block_id + 1) % total_blocks_;
    }
    
    LOG_DEBUG("Allocated " + std::to_string(count) + " individual blocks");
    return allocated_blocks;
}

void BlockManager::deallocate_block(uint32_t block_id) {
    if (block_id >= total_blocks_) {
        LOG_ERROR("Invalid block ID: " + std::to_string(block_id));
        throw dfs::utils::BlockNotFoundException(block_id);
    }
    
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    
    if (block_bitmap_[block_id]) {
        LOG_WARN("Attempting to deallocate already free block: " + std::to_string(block_id));
        return;
    }
    
    block_bitmap_[block_id] = true;
    
    LOG_DEBUG("Deallocated block " + std::to_string(block_id));
}

void BlockManager::deallocate_blocks(const std::vector<uint32_t>& block_ids) {
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    
    for (uint32_t block_id : block_ids) {
        if (block_id >= total_blocks_) {
            LOG_ERROR("Invalid block ID: " + std::to_string(block_id));
            continue;
        }
        
        if (!block_bitmap_[block_id]) {
            block_bitmap_[block_id] = true;
            LOG_DEBUG("Deallocated block " + std::to_string(block_id));
        } else {
            LOG_WARN("Attempting to deallocate already free block: " + std::to_string(block_id));
        }
    }
}

bool BlockManager::is_block_free(uint32_t block_id) const {
    if (block_id >= total_blocks_) {
        return false;
    }
    
    return block_bitmap_[block_id];
}

void BlockManager::mark_block_used(uint32_t block_id) {
    if (block_id >= total_blocks_) {
        LOG_ERROR("Invalid block ID: " + std::to_string(block_id));
        throw dfs::utils::BlockNotFoundException(block_id);
    }
    
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    block_bitmap_[block_id] = false;
    
    LOG_DEBUG("Marked block " + std::to_string(block_id) + " as used");
}

void BlockManager::mark_block_free(uint32_t block_id) {
    if (block_id >= total_blocks_) {
        LOG_ERROR("Invalid block ID: " + std::to_string(block_id));
        throw dfs::utils::BlockNotFoundException(block_id);
    }
    
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    block_bitmap_[block_id] = true;
    
    LOG_DEBUG("Marked block " + std::to_string(block_id) + " as free");
}

uint32_t BlockManager::get_free_block_count() const {
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    
    uint32_t count = 0;
    for (bool is_free : block_bitmap_) {
        if (is_free) {
            count++;
        }
    }
    
    return count;
}

uint32_t BlockManager::get_total_block_count() const {
    return total_blocks_;
}

uint32_t BlockManager::get_block_size() const {
    return block_size_;
}

BlockManager::BlockStats BlockManager::get_block_stats() const {
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    
    BlockStats stats;
    stats.total_blocks = total_blocks_;
    stats.free_blocks = 0;
    
    for (bool is_free : block_bitmap_) {
        if (is_free) {
            stats.free_blocks++;
        }
    }
    
    stats.used_blocks = stats.total_blocks - stats.free_blocks;
    stats.usage_percentage = (stats.total_blocks > 0) ? 
        (static_cast<double>(stats.used_blocks) / stats.total_blocks) * 100.0 : 0.0;
    
    return stats;
}

void BlockManager::serialize_bitmap(std::ofstream& file) const {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot serialize block bitmap: file not open");
    }
    
    LOG_DEBUG("Serializing block bitmap to file");
    
    // Write bitmap size
    uint32_t bitmap_size = static_cast<uint32_t>(block_bitmap_.size());
    file.write(reinterpret_cast<const char*>(&bitmap_size), sizeof(bitmap_size));
    
    // Write bitmap data
    for (bool is_free : block_bitmap_) {
        file.write(reinterpret_cast<const char*>(&is_free), sizeof(bool));
    }
    
    if (file.fail()) {
        throw dfs::utils::FileSystemException("Failed to serialize block bitmap");
    }
    
    LOG_DEBUG("Block bitmap serialized successfully");
}

void BlockManager::deserialize_bitmap(std::ifstream& file) {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot deserialize block bitmap: file not open");
    }
    
    LOG_DEBUG("Deserializing block bitmap from file");
    
    // Read bitmap size
    uint32_t bitmap_size;
    file.read(reinterpret_cast<char*>(&bitmap_size), sizeof(bitmap_size));
    
    if (file.fail() || file.gcount() != sizeof(bitmap_size)) {
        throw dfs::utils::FileSystemException("Failed to deserialize block bitmap size");
    }
    
    if (bitmap_size != total_blocks_) {
        throw dfs::utils::FileSystemException("Block bitmap size mismatch");
    }
    
    // Resize and read bitmap data
    block_bitmap_.resize(bitmap_size);
    for (auto& is_free : block_bitmap_) {
        file.read(reinterpret_cast<char*>(&is_free), sizeof(bool));
        if (file.fail() || file.gcount() != sizeof(bool)) {
            throw dfs::utils::FileSystemException("Failed to deserialize block bitmap data");
        }
    }
    
    LOG_DEBUG("Block bitmap deserialized successfully");
}

void BlockManager::defragment_blocks() {
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    
    LOG_INFO("Starting block defragmentation");
    
    // Simple defragmentation: move used blocks to the beginning
    std::vector<bool> new_bitmap(total_blocks_, true);
    uint32_t used_count = 0;
    
    // Count used blocks
    for (bool is_free : block_bitmap_) {
        if (!is_free) {
            used_count++;
        }
    }
    
    // Mark first 'used_count' blocks as used
    for (uint32_t i = 0; i < used_count; ++i) {
        new_bitmap[i] = false;
    }
    
    block_bitmap_ = std::move(new_bitmap);
    next_free_block_ = used_count;
    
    LOG_INFO("Block defragmentation completed, " + std::to_string(used_count) + " blocks moved");
}

bool BlockManager::is_valid() const {
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    
    // Check bitmap size
    if (block_bitmap_.size() != total_blocks_) {
        LOG_ERROR("Block bitmap size mismatch");
        return false;
    }
    
    // Check that block 0 is reserved (not free)
    if (total_blocks_ > 0 && block_bitmap_[0]) {
        LOG_ERROR("Block 0 should be reserved but is marked as free");
        return false;
    }
    
    return true;
}

uint32_t BlockManager::find_next_free_block(uint32_t start_index) const {
    // Search from start_index to end
    for (uint32_t i = start_index; i < total_blocks_; ++i) {
        if (block_bitmap_[i]) {
            return i;
        }
    }
    
    // Wrap around and search from beginning to start_index
    for (uint32_t i = 0; i < start_index; ++i) {
        if (block_bitmap_[i]) {
            return i;
        }
    }
    
    return UINT32_MAX; // No free blocks found
}

// DataBlock implementation
DataBlock::DataBlock(uint32_t block_id, uint32_t block_size)
    : block_id_(block_id), block_size_(block_size) {
    
    data_.resize(block_size, 0);
    
    LOG_DEBUG("Created DataBlock " + std::to_string(block_id) + " with size " + std::to_string(block_size));
}

std::vector<uint8_t> DataBlock::read_data(uint32_t offset, uint32_t size) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (offset >= block_size_) {
        LOG_ERROR("Read offset " + std::to_string(offset) + " exceeds block size " + std::to_string(block_size_));
        return {};
    }
    
    // If size is 0, read from offset to end of block
    if (size == 0) {
        size = block_size_ - offset;
    }
    
    // Clamp size to available data
    size = std::min(size, block_size_ - offset);
    
    std::vector<uint8_t> result(data_.begin() + offset, data_.begin() + offset + size);
    
    LOG_DEBUG("Read " + std::to_string(size) + " bytes from block " + std::to_string(block_id_) + 
             " at offset " + std::to_string(offset));
    
    return result;
}

bool DataBlock::write_data(const std::vector<uint8_t>& data, uint32_t offset) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (offset >= block_size_) {
        LOG_ERROR("Write offset " + std::to_string(offset) + " exceeds block size " + std::to_string(block_size_));
        return false;
    }
    
    if (offset + data.size() > block_size_) {
        LOG_ERROR("Write would exceed block size");
        return false;
    }
    
    // Copy data to block
    std::copy(data.begin(), data.end(), data_.begin() + offset);
    
    LOG_DEBUG("Wrote " + std::to_string(data.size()) + " bytes to block " + std::to_string(block_id_) + 
             " at offset " + std::to_string(offset));
    
    return true;
}

void DataBlock::clear() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::fill(data_.begin(), data_.end(), 0);
    
    LOG_DEBUG("Cleared block " + std::to_string(block_id_));
}

uint32_t DataBlock::get_block_id() const {
    return block_id_;
}

uint32_t DataBlock::get_block_size() const {
    return block_size_;
}

uint32_t DataBlock::get_data_size() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Find the last non-zero byte
    for (int i = data_.size() - 1; i >= 0; --i) {
        if (data_[i] != 0) {
            return i + 1;
        }
    }
    
    return 0;
}

bool DataBlock::is_empty() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    return std::all_of(data_.begin(), data_.end(), [](uint8_t byte) { return byte == 0; });
}

void DataBlock::serialize(std::ofstream& file) const {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot serialize DataBlock: file not open");
    }
    
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    LOG_DEBUG("Serializing DataBlock " + std::to_string(block_id_) + " to file");
    
    // Write block metadata
    file.write(reinterpret_cast<const char*>(&block_id_), sizeof(block_id_));
    file.write(reinterpret_cast<const char*>(&block_size_), sizeof(block_size_));
    
    // Write block data
    file.write(reinterpret_cast<const char*>(data_.data()), block_size_);
    
    if (file.fail()) {
        throw dfs::utils::FileSystemException("Failed to serialize DataBlock");
    }
    
    LOG_DEBUG("DataBlock " + std::to_string(block_id_) + " serialized successfully");
}

void DataBlock::deserialize(std::ifstream& file) {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot deserialize DataBlock: file not open");
    }
    
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    LOG_DEBUG("Deserializing DataBlock from file");
    
    // Read block metadata
    uint32_t file_block_id, file_block_size;
    file.read(reinterpret_cast<char*>(&file_block_id), sizeof(file_block_id));
    file.read(reinterpret_cast<char*>(&file_block_size), sizeof(file_block_size));
    
    if (file.fail() || file.gcount() != sizeof(file_block_id) + sizeof(file_block_size)) {
        throw dfs::utils::FileSystemException("Failed to deserialize DataBlock metadata");
    }
    
    if (file_block_size != block_size_) {
        throw dfs::utils::FileSystemException("DataBlock size mismatch");
    }
    
    // Read block data
    data_.resize(file_block_size);
    file.read(reinterpret_cast<char*>(data_.data()), file_block_size);
    
    if (file.fail() || file.gcount() != file_block_size) {
        throw dfs::utils::FileSystemException("Failed to deserialize DataBlock data");
    }
    
    LOG_DEBUG("DataBlock deserialized successfully");
}

} // namespace core
} // namespace dfs
