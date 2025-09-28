#include "core/inode.h"
#include "utils/logger.h"
#include "utils/exceptions.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace dfs {
namespace core {

Inode::Inode() {
    // Initialize with default values
    mode = 0;
    uid = 0;
    gid = 0;
    size = 0;
    blocks = 0;
    atime = 0;
    mtime = 0;
    ctime = 0;
    replication_count = 1;
    checksum = 0;
    link_count = 0;
    
    // Clear all block pointers
    std::memset(direct_blocks, 0, sizeof(direct_blocks));
    indirect_block = 0;
    double_indirect = 0;
    triple_indirect = 0;
    
    // Clear padding
    std::memset(padding, 0, sizeof(padding));
}

void Inode::initialize(uint16_t file_mode, uint16_t user_id, uint16_t group_id) {
    LOG_DEBUG("Initializing inode with mode " + std::to_string(file_mode));
    
    mode = file_mode;
    uid = user_id;
    gid = group_id;
    size = 0;
    blocks = 0;
    link_count = 1; // Start with 1 link (the inode itself)
    replication_count = 1;
    
    // Set timestamps
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t timestamp = static_cast<uint64_t>(time_t);
    
    atime = timestamp;
    mtime = timestamp;
    ctime = timestamp;
    
    // Clear all block pointers
    std::memset(direct_blocks, 0, sizeof(direct_blocks));
    indirect_block = 0;
    double_indirect = 0;
    triple_indirect = 0;
    
    // Clear padding
    std::memset(padding, 0, sizeof(padding));
    
    // Calculate and set checksum
    update_checksum();
    
    LOG_DEBUG("Inode initialized successfully");
}

bool Inode::is_directory() const {
    return (mode & S_IFMT) == S_IFDIR;
}

bool Inode::is_file() const {
    return (mode & S_IFMT) == S_IFREG;
}

bool Inode::is_symlink() const {
    return (mode & S_IFMT) == S_IFLNK;
}

std::string Inode::get_permissions_string() const {
    std::string perms(10, '-');
    
    // File type
    if (is_directory()) perms[0] = 'd';
    else if (is_symlink()) perms[0] = 'l';
    else if (is_file()) perms[0] = '-';
    else perms[0] = '?';
    
    // Owner permissions
    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
    
    // Group permissions
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
    
    // Other permissions
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    
    return perms;
}

void Inode::update_atime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    atime = static_cast<uint64_t>(time_t);
    update_checksum();
}

void Inode::update_mtime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    mtime = static_cast<uint64_t>(time_t);
    update_checksum();
}

void Inode::update_ctime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    ctime = static_cast<uint64_t>(time_t);
    update_checksum();
}

void Inode::update_checksum() {
    // Zero out the checksum field before calculating
    uint32_t old_checksum = checksum;
    checksum = 0;
    
    // Calculate new checksum
    checksum = calculate_checksum(this, sizeof(Inode));
    
    LOG_DEBUG("Updated inode checksum: " + std::to_string(checksum));
}

bool Inode::is_valid() const {
    // Check basic validity
    if (mode == 0) {
        LOG_ERROR("Invalid inode: mode is 0");
        return false;
    }
    
    if (size < 0) {
        LOG_ERROR("Invalid inode: negative size");
        return false;
    }
    
    if (blocks < 0) {
        LOG_ERROR("Invalid inode: negative blocks");
        return false;
    }
    
    if (link_count == 0) {
        LOG_ERROR("Invalid inode: link count is 0");
        return false;
    }
    
    // Check timestamp validity
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_time = static_cast<uint64_t>(time_t);
    
    if (atime > current_time || mtime > current_time || ctime > current_time) {
        LOG_ERROR("Invalid inode: future timestamps");
        return false;
    }
    
    // Verify checksum
    Inode temp = *this;
    temp.checksum = 0;
    uint32_t calculated_checksum = calculate_checksum(&temp, sizeof(Inode));
    
    if (checksum != calculated_checksum) {
        LOG_ERROR("Inode checksum mismatch: stored=" + std::to_string(checksum) + 
                 ", calculated=" + std::to_string(calculated_checksum));
        return false;
    }
    
    return true;
}

uint32_t Inode::calculate_checksum(const void* data, size_t size) {
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

std::string Inode::to_string() const {
    std::ostringstream oss;
    
    oss << "Inode Information:\n";
    oss << "  Mode: " << get_permissions_string() << " (0" << std::oct << mode << std::dec << ")\n";
    oss << "  UID: " << uid << ", GID: " << gid << "\n";
    oss << "  Size: " << size << " bytes\n";
    oss << "  Blocks: " << blocks << "\n";
    oss << "  Link Count: " << link_count << "\n";
    oss << "  Replication Count: " << replication_count << "\n";
    oss << "  Access Time: " << atime << "\n";
    oss << "  Modify Time: " << mtime << "\n";
    oss << "  Change Time: " << ctime << "\n";
    oss << "  Checksum: 0x" << std::hex << std::setw(8) << std::setfill('0') << checksum << std::dec << "\n";
    
    // Show block pointers
    oss << "  Direct Blocks: ";
    for (int i = 0; i < 12; ++i) {
        if (direct_blocks[i] != 0) {
            oss << direct_blocks[i] << " ";
        }
    }
    oss << "\n";
    
    if (indirect_block != 0) {
        oss << "  Indirect Block: " << indirect_block << "\n";
    }
    if (double_indirect != 0) {
        oss << "  Double Indirect Block: " << double_indirect << "\n";
    }
    if (triple_indirect != 0) {
        oss << "  Triple Indirect Block: " << triple_indirect << "\n";
    }
    
    return oss.str();
}

// InodeTable implementation
InodeTable::InodeTable(uint32_t max_inodes) 
    : next_free_inode_(1) {
    
    LOG_INFO("Creating InodeTable with " + std::to_string(max_inodes) + " inodes");
    
    // Initialize inode array
    inodes_.resize(max_inodes);
    
    // Initialize free inode bitmap
    free_inodes_.resize(max_inodes, true);
    
    // Reserve inode 0 (invalid) and inode 1 (root)
    if (max_inodes > 0) {
        free_inodes_[0] = false; // Inode 0 is invalid
    }
    if (max_inodes > 1) {
        free_inodes_[1] = false; // Inode 1 is reserved for root
    }
    
    LOG_INFO("InodeTable created successfully");
}

uint32_t InodeTable::allocate_inode() {
    std::lock_guard<std::mutex> lock(table_mutex_);
    
    // Find next free inode
    for (uint32_t i = next_free_inode_; i < free_inodes_.size(); ++i) {
        if (free_inodes_[i]) {
            free_inodes_[i] = false;
            next_free_inode_ = (i + 1) % free_inodes_.size();
            
            LOG_DEBUG("Allocated inode " + std::to_string(i));
            return i;
        }
    }
    
    // Wrap around and search from beginning
    for (uint32_t i = 1; i < next_free_inode_; ++i) {
        if (free_inodes_[i]) {
            free_inodes_[i] = false;
            next_free_inode_ = (i + 1) % free_inodes_.size();
            
            LOG_DEBUG("Allocated inode " + std::to_string(i));
            return i;
        }
    }
    
    LOG_ERROR("No free inodes available");
    throw dfs::utils::InsufficientSpaceException(1, get_free_inode_count());
}

void InodeTable::deallocate_inode(uint32_t inode_num) {
    if (inode_num >= free_inodes_.size()) {
        LOG_ERROR("Invalid inode number: " + std::to_string(inode_num));
        throw dfs::utils::InodeNotFoundException(inode_num);
    }
    
    std::lock_guard<std::mutex> lock(table_mutex_);
    
    if (free_inodes_[inode_num]) {
        LOG_WARN("Attempting to deallocate already free inode: " + std::to_string(inode_num));
        return;
    }
    
    free_inodes_[inode_num] = true;
    
    // Clear the inode data
    inodes_[inode_num] = Inode();
    
    LOG_DEBUG("Deallocated inode " + std::to_string(inode_num));
}

Inode* InodeTable::get_inode(uint32_t inode_num) {
    if (inode_num >= inodes_.size()) {
        LOG_ERROR("Invalid inode number: " + std::to_string(inode_num));
        throw dfs::utils::InodeNotFoundException(inode_num);
    }
    
    if (free_inodes_[inode_num]) {
        LOG_ERROR("Accessing free inode: " + std::to_string(inode_num));
        throw dfs::utils::InodeNotFoundException(inode_num);
    }
    
    return &inodes_[inode_num];
}

bool InodeTable::is_inode_free(uint32_t inode_num) const {
    if (inode_num >= free_inodes_.size()) {
        return false;
    }
    
    return free_inodes_[inode_num];
}

uint32_t InodeTable::get_free_inode_count() const {
    std::lock_guard<std::mutex> lock(table_mutex_);
    
    uint32_t count = 0;
    for (bool is_free : free_inodes_) {
        if (is_free) {
            count++;
        }
    }
    
    return count;
}

uint32_t InodeTable::get_total_inode_count() const {
    return static_cast<uint32_t>(inodes_.size());
}

void InodeTable::serialize(std::ofstream& file) const {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot serialize InodeTable: file not open");
    }
    
    LOG_DEBUG("Serializing InodeTable to file");
    
    // Write inode count
    uint32_t inode_count = static_cast<uint32_t>(inodes_.size());
    file.write(reinterpret_cast<const char*>(&inode_count), sizeof(inode_count));
    
    // Write all inodes
    for (const auto& inode : inodes_) {
        file.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));
    }
    
    // Write free inode bitmap
    for (bool is_free : free_inodes_) {
        file.write(reinterpret_cast<const char*>(&is_free), sizeof(bool));
    }
    
    if (file.fail()) {
        throw dfs::utils::FileSystemException("Failed to serialize InodeTable");
    }
    
    LOG_DEBUG("InodeTable serialized successfully");
}

void InodeTable::deserialize(std::ifstream& file) {
    if (!file.is_open()) {
        throw dfs::utils::FileSystemException("Cannot deserialize InodeTable: file not open");
    }
    
    LOG_DEBUG("Deserializing InodeTable from file");
    
    // Read inode count
    uint32_t inode_count;
    file.read(reinterpret_cast<char*>(&inode_count), sizeof(inode_count));
    
    if (file.fail() || file.gcount() != sizeof(inode_count)) {
        throw dfs::utils::FileSystemException("Failed to deserialize InodeTable inode count");
    }
    
    // Resize arrays
    inodes_.resize(inode_count);
    free_inodes_.resize(inode_count);
    
    // Read all inodes
    for (auto& inode : inodes_) {
        file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
        if (file.fail() || file.gcount() != sizeof(Inode)) {
            throw dfs::utils::FileSystemException("Failed to deserialize InodeTable inode");
        }
    }
    
    // Read free inode bitmap
    for (auto& is_free : free_inodes_) {
        file.read(reinterpret_cast<char*>(&is_free), sizeof(bool));
        if (file.fail() || file.gcount() != sizeof(bool)) {
            throw dfs::utils::FileSystemException("Failed to deserialize InodeTable bitmap");
        }
    }
    
    LOG_DEBUG("InodeTable deserialized successfully");
}

} // namespace core
} // namespace dfs
