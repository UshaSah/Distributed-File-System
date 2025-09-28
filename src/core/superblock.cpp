#include "core/superblock.h"
#include "utils/logger.h"
#include "utils/exceptions.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dfs {
namespace core {

SuperBlock::SuperBlock() {
    // Initialize with default values
    magic_number = 0;
    block_size = 0;
    total_blocks = 0;
    free_blocks = 0;
    inode_count = 0;
    free_inodes = 0;
    root_inode = 0;
    last_mount_time = 0;
    last_write_time = 0;
    version = 1;
    checksum = 0;
    
    // Clear padding
    std::memset(padding, 0, sizeof(padding));
}

void SuperBlock::initialize(uint32_t total_blocks, uint32_t block_size) {
    LOG_INFO("Initializing SuperBlock with " + std::to_string(total_blocks) + 
             " blocks of size " + std::to_string(block_size));
    
    // Set basic parameters
    this->magic_number = MAGIC_NUMBER;
    this->block_size = block_size;
    this->total_blocks = total_blocks;
    this->free_blocks = total_blocks - 1; // Reserve one block for superblock
    this->inode_count = total_blocks / 4; // 1 inode per 4 blocks (configurable)
    this->free_inodes = this->inode_count - 1; // Reserve one for root
    this->root_inode = 1; // Root inode is always 1
    this->version = 1;
    
    // Set timestamps
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    this->last_mount_time = static_cast<uint64_t>(time_t);
    this->last_write_time = this->last_mount_time;
    
    // Clear padding
    std::memset(padding, 0, sizeof(padding));
    
    // Calculate and set checksum
    update_checksum();
    
    LOG_INFO("SuperBlock initialized successfully");
}

bool SuperBlock::is_valid() const {
    // Check magic number
    if (magic_number != MAGIC_NUMBER) {
        LOG_ERROR("Invalid magic number: " + std::to_string(magic_number) + 
                 " (expected " + std::to_string(MAGIC_NUMBER) + ")");
        return false;
    }
    
    // Check block size (must be power of 2 and reasonable)
    if (block_size == 0 || (block_size & (block_size - 1)) != 0 || block_size > 65536) {
        LOG_ERROR("Invalid block size: " + std::to_string(block_size));
        return false;
    }
    
    // Check total blocks
    if (total_blocks == 0 || total_blocks < 10) {
        LOG_ERROR("Invalid total blocks: " + std::to_string(total_blocks));
        return false;
    }
    
    // Check inode count
    if (inode_count == 0 || inode_count > total_blocks) {
        LOG_ERROR("Invalid inode count: " + std::to_string(inode_count));
        return false;
    }
    
    // Check free blocks
    if (free_blocks > total_blocks) {
        LOG_ERROR("Invalid free blocks: " + std::to_string(free_blocks) + 
                 " (total: " + std::to_string(total_blocks) + ")");
        return false;
    }
    
    // Check free inodes
    if (free_inodes > inode_count) {
        LOG_ERROR("Invalid free inodes: " + std::to_string(free_inodes) + 
                 " (total: " + std::to_string(inode_count) + ")");
        return false;
    }
    
    // Check root inode
    if (root_inode == 0 || root_inode >= inode_count) {
        LOG_ERROR("Invalid root inode: " + std::to_string(root_inode));
        return false;
    }
    
    // Check version
    if (version == 0) {
        LOG_ERROR("Invalid version: " + std::to_string(version));
        return false;
    }
    
    // Verify checksum
    SuperBlock temp = *this;
    temp.checksum = 0;
    uint32_t calculated_checksum = calculate_checksum(&temp, sizeof(SuperBlock));
    
    if (checksum != calculated_checksum) {
        LOG_ERROR("Checksum mismatch: stored=" + std::to_string(checksum) + 
                 ", calculated=" + std::to_string(calculated_checksum));
        return false;
    }
    
    return true;
}

void SuperBlock::update_checksum() {
    // Zero out the checksum field before calculating
    uint32_t old_checksum = checksum;
    checksum = 0;
    
    // Calculate new checksum
    checksum = calculate_checksum(this, sizeof(SuperBlock));
    
    LOG_DEBUG("Updated SuperBlock checksum: " + std::to_string(checksum));
}

void SuperBlock::serialize(std::ofstream& file) const {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot serialize SuperBlock: file not open");
    }
    
    LOG_DEBUG("Serializing SuperBlock to file");
    
    // Write the entire SuperBlock structure
    file.write(reinterpret_cast<const char*>(this), sizeof(SuperBlock));
    
    if (file.fail()) {
        throw dfs::utils::FileSystemException("Failed to serialize SuperBlock");
    }
    
    // Update last write time
    const_cast<SuperBlock*>(this)->last_write_time = 
        static_cast<uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    
    LOG_DEBUG("SuperBlock serialized successfully");
}

void SuperBlock::deserialize(std::ifstream& file) {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot deserialize SuperBlock: file not open");
    }
    
    LOG_DEBUG("Deserializing SuperBlock from file");
    
    // Read the entire SuperBlock structure
    file.read(reinterpret_cast<char*>(this), sizeof(SuperBlock));
    
    if (file.fail() || file.gcount() != sizeof(SuperBlock)) {
        throw dfs::utils::FileSystemException("Failed to deserialize SuperBlock");
    }
    
    // Validate the deserialized SuperBlock
    if (!is_valid()) {
        throw dfs::utils::FileSystemCorruptedException("Deserialized SuperBlock is invalid");
    }
    
    LOG_DEBUG("SuperBlock deserialized successfully");
}

uint32_t SuperBlock::calculate_checksum(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t checksum = 0;
    
    // Simple checksum algorithm (CRC32-like)
    for (size_t i = 0; i < size; ++i) {
        checksum = (checksum << 1) ^ bytes[i];
        if (checksum & 0x80000000) {
            checksum ^= 0x04C11DB7; // CRC32 polynomial
        }
    }
    
    return checksum;
}

std::string SuperBlock::to_string() const {
    std::ostringstream oss;
    
    oss << "SuperBlock Information:\n";
    oss << "  Magic Number: 0x" << std::hex << std::setw(8) << std::setfill('0') << magic_number << std::dec << "\n";
    oss << "  Block Size: " << block_size << " bytes\n";
    oss << "  Total Blocks: " << total_blocks << "\n";
    oss << "  Free Blocks: " << free_blocks << "\n";
    oss << "  Total Inodes: " << inode_count << "\n";
    oss << "  Free Inodes: " << free_inodes << "\n";
    oss << "  Root Inode: " << root_inode << "\n";
    oss << "  Version: " << version << "\n";
    oss << "  Last Mount Time: " << last_mount_time << "\n";
    oss << "  Last Write Time: " << last_write_time << "\n";
    oss << "  Checksum: 0x" << std::hex << std::setw(8) << std::setfill('0') << checksum << std::dec << "\n";
    
    // Calculate usage statistics
    double block_usage = ((double)(total_blocks - free_blocks) / total_blocks) * 100.0;
    double inode_usage = ((double)(inode_count - free_inodes) / inode_count) * 100.0;
    
    oss << "  Block Usage: " << std::fixed << std::setprecision(2) << block_usage << "%\n";
    oss << "  Inode Usage: " << std::fixed << std::setprecision(2) << inode_usage << "%\n";
    
    return oss.str();
}

bool SuperBlock::allocate_block() {
    if (free_blocks == 0) {
        LOG_WARN("No free blocks available for allocation");
        return false;
    }
    
    free_blocks--;
    last_write_time = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    update_checksum();
    
    LOG_DEBUG("Allocated block, " + std::to_string(free_blocks) + " blocks remaining");
    return true;
}

bool SuperBlock::deallocate_block() {
    if (free_blocks >= total_blocks) {
        LOG_WARN("Cannot deallocate block: already at maximum free blocks");
        return false;
    }
    
    free_blocks++;
    last_write_time = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    update_checksum();
    
    LOG_DEBUG("Deallocated block, " + std::to_string(free_blocks) + " blocks available");
    return true;
}

bool SuperBlock::allocate_inode() {
    if (free_inodes == 0) {
        LOG_WARN("No free inodes available for allocation");
        return false;
    }
    
    free_inodes--;
    last_write_time = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    update_checksum();
    
    LOG_DEBUG("Allocated inode, " + std::to_string(free_inodes) + " inodes remaining");
    return true;
}

bool SuperBlock::deallocate_inode() {
    if (free_inodes >= inode_count) {
        LOG_WARN("Cannot deallocate inode: already at maximum free inodes");
        return false;
    }
    
    free_inodes++;
    last_write_time = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    update_checksum();
    
    LOG_DEBUG("Deallocated inode, " + std::to_string(free_inodes) + " inodes available");
    return true;
}

void SuperBlock::update_mount_time() {
    last_mount_time = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    update_checksum();
    
    LOG_DEBUG("Updated mount time");
}

bool SuperBlock::is_space_available(uint32_t blocks_needed) const {
    return free_blocks >= blocks_needed;
}

bool SuperBlock::are_inodes_available(uint32_t inodes_needed) const {
    return free_inodes >= inodes_needed;
}

uint32_t SuperBlock::get_usage_percentage() const {
    if (total_blocks == 0) return 0;
    return ((total_blocks - free_blocks) * 100) / total_blocks;
}

uint32_t SuperBlock::get_inode_usage_percentage() const {
    if (inode_count == 0) return 0;
    return ((inode_count - free_inodes) * 100) / inode_count;
}

} // namespace core
} // namespace dfs
