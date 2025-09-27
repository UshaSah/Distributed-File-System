#include "utils/exceptions.h"
#include <sstream>
#include <iomanip>

namespace dfs {
namespace utils {

// Base FileSystemException implementation
FileSystemException::FileSystemException(const std::string& message, uint32_t error_code, 
                                       const std::string& context)
    : message_(message), error_code_(error_code), context_(context) {}

const char* FileSystemException::what() const noexcept {
    return message_.c_str();
}

uint32_t FileSystemException::get_error_code() const {
    return error_code_;
}

const std::string& FileSystemException::get_context() const {
    return context_;
}

std::string FileSystemException::get_error_type() const {
    return "FileSystemException";
}

// Inode-related exceptions
InodeNotFoundException::InodeNotFoundException(uint32_t inode_num, const std::string& context)
    : FileSystemException("Inode not found: " + std::to_string(inode_num), 1001, context) {}

std::string InodeNotFoundException::get_error_type() const {
    return "InodeNotFoundException";
}

InodeAlreadyExistsException::InodeAlreadyExistsException(uint32_t inode_num, const std::string& context)
    : FileSystemException("Inode already exists: " + std::to_string(inode_num), 1002, context) {}

std::string InodeAlreadyExistsException::get_error_type() const {
    return "InodeAlreadyExistsException";
}

InodeCorruptedException::InodeCorruptedException(uint32_t inode_num, const std::string& context)
    : FileSystemException("Inode corrupted: " + std::to_string(inode_num), 1003, context) {}

std::string InodeCorruptedException::get_error_type() const {
    return "InodeCorruptedException";
}

// Block-related exceptions
BlockNotFoundException::BlockNotFoundException(uint32_t block_id, const std::string& context)
    : FileSystemException("Block not found: " + std::to_string(block_id), 2001, context) {}

std::string BlockNotFoundException::get_error_type() const {
    return "BlockNotFoundException";
}

InsufficientSpaceException::InsufficientSpaceException(uint64_t required, uint64_t available, 
                                                     const std::string& context)
    : FileSystemException("Insufficient space: required " + std::to_string(required) + 
                         ", available " + std::to_string(available), 2002, context) {}

std::string InsufficientSpaceException::get_error_type() const {
    return "InsufficientSpaceException";
}

BlockCorruptedException::BlockCorruptedException(uint32_t block_id, const std::string& context)
    : FileSystemException("Block corrupted: " + std::to_string(block_id), 2003, context) {}

std::string BlockCorruptedException::get_error_type() const {
    return "BlockCorruptedException";
}

// File operation exceptions
FileNotFoundException::FileNotFoundException(const std::string& file_path, const std::string& context)
    : FileSystemException("File not found: " + file_path, 3001, context) {}

std::string FileNotFoundException::get_error_type() const {
    return "FileNotFoundException";
}

FileAlreadyExistsException::FileAlreadyExistsException(const std::string& file_path, const std::string& context)
    : FileSystemException("File already exists: " + file_path, 3002, context) {}

std::string FileAlreadyExistsException::get_error_type() const {
    return "FileAlreadyExistsException";
}

DirectoryNotFoundException::DirectoryNotFoundException(const std::string& dir_path, const std::string& context)
    : FileSystemException("Directory not found: " + dir_path, 3003, context) {}

std::string DirectoryNotFoundException::get_error_type() const {
    return "DirectoryNotFoundException";
}

DirectoryNotEmptyException::DirectoryNotEmptyException(const std::string& dir_path, const std::string& context)
    : FileSystemException("Directory not empty: " + dir_path, 3004, context) {}

std::string DirectoryNotEmptyException::get_error_type() const {
    return "DirectoryNotEmptyException";
}

PermissionDeniedException::PermissionDeniedException(const std::string& path, const std::string& operation,
                                                   const std::string& context)
    : FileSystemException("Permission denied: " + operation + " on " + path, 3005, context) {}

std::string PermissionDeniedException::get_error_type() const {
    return "PermissionDeniedException";
}

// Transaction-related exceptions
TransactionNotFoundException::TransactionNotFoundException(uint64_t tx_id, const std::string& context)
    : FileSystemException("Transaction not found: " + std::to_string(tx_id), 4001, context) {}

std::string TransactionNotFoundException::get_error_type() const {
    return "TransactionNotFoundException";
}

TransactionAbortedException::TransactionAbortedException(uint64_t tx_id, const std::string& reason,
                                                       const std::string& context)
    : FileSystemException("Transaction aborted: " + std::to_string(tx_id) + " - " + reason, 4002, context) {}

std::string TransactionAbortedException::get_error_type() const {
    return "TransactionAbortedException";
}

TransactionTimeoutException::TransactionTimeoutException(uint64_t tx_id, std::chrono::seconds timeout,
                                                       const std::string& context)
    : FileSystemException("Transaction timeout: " + std::to_string(tx_id) + 
                         " after " + std::to_string(timeout.count()) + " seconds", 4003, context) {}

std::string TransactionTimeoutException::get_error_type() const {
    return "TransactionTimeoutException";
}

ConcurrentAccessException::ConcurrentAccessException(uint32_t inode_num, const std::string& operation,
                                                   const std::string& context)
    : FileSystemException("Concurrent access: " + operation + " on inode " + std::to_string(inode_num), 4004, context) {}

std::string ConcurrentAccessException::get_error_type() const {
    return "ConcurrentAccessException";
}

// System-level exceptions
FileSystemNotMountedException::FileSystemNotMountedException(const std::string& context)
    : FileSystemException("File system not mounted", 5001, context) {}

std::string FileSystemNotMountedException::get_error_type() const {
    return "FileSystemNotMountedException";
}

FileSystemCorruptedException::FileSystemCorruptedException(const std::string& reason, const std::string& context)
    : FileSystemException("File system corrupted: " + reason, 5002, context) {}

std::string FileSystemCorruptedException::get_error_type() const {
    return "FileSystemCorruptedException";
}

ConfigurationException::ConfigurationException(const std::string& parameter, const std::string& value,
                                             const std::string& context)
    : FileSystemException("Configuration error: " + parameter + " = " + value, 5003, context) {}

std::string ConfigurationException::get_error_type() const {
    return "ConfigurationException";
}

// Network and API exceptions
NetworkException::NetworkException(const std::string& endpoint, const std::string& reason,
                                 const std::string& context)
    : FileSystemException("Network error: " + endpoint + " - " + reason, 6001, context) {}

std::string NetworkException::get_error_type() const {
    return "NetworkException";
}

APIException::APIException(const std::string& endpoint, uint32_t http_status,
                         const std::string& response, const std::string& context)
    : FileSystemException("API error: " + endpoint + " returned " + std::to_string(http_status) + 
                         " - " + response, 6002, context) {}

std::string APIException::get_error_type() const {
    return "APIException";
}

RateLimitExceededException::RateLimitExceededException(const std::string& client_id, uint32_t limit,
                                                     const std::string& context)
    : FileSystemException("Rate limit exceeded: " + client_id + " limit " + std::to_string(limit), 6003, context) {}

std::string RateLimitExceededException::get_error_type() const {
    return "RateLimitExceededException";
}

// ExceptionHandler utility functions
ExceptionHandler::ExceptionType ExceptionHandler::classify_exception(const std::exception& e) {
    const std::string error_msg = e.what();
    
    // Check for specific exception types
    if (dynamic_cast<const InodeNotFoundException*>(&e) ||
        dynamic_cast<const BlockNotFoundException*>(&e) ||
        dynamic_cast<const FileNotFoundException*>(&e)) {
        return ExceptionType::PERMANENT;
    }
    
    if (dynamic_cast<const ConcurrentAccessException*>(&e) ||
        dynamic_cast<const TransactionTimeoutException*>(&e)) {
        return ExceptionType::CONCURRENCY;
    }
    
    if (dynamic_cast<const InodeCorruptedException*>(&e) ||
        dynamic_cast<const BlockCorruptedException*>(&e) ||
        dynamic_cast<const FileSystemCorruptedException*>(&e)) {
        return ExceptionType::CORRUPTION;
    }
    
    if (dynamic_cast<const NetworkException*>(&e) ||
        dynamic_cast<const RateLimitExceededException*>(&e)) {
        return ExceptionType::NETWORK;
    }
    
    if (dynamic_cast<const TransactionTimeoutException*>(&e)) {
        return ExceptionType::TIMEOUT;
    }
    
    // Check error message content for transient errors
    if (error_msg.find("timeout") != std::string::npos ||
        error_msg.find("temporary") != std::string::npos ||
        error_msg.find("retry") != std::string::npos) {
        return ExceptionType::TRANSIENT;
    }
    
    return ExceptionType::UNKNOWN;
}

bool ExceptionHandler::should_retry(const std::exception& e, uint32_t attempt_count) {
    ExceptionType type = classify_exception(e);
    
    switch (type) {
        case ExceptionType::TRANSIENT:
        case ExceptionType::CONCURRENCY:
        case ExceptionType::TIMEOUT:
        case ExceptionType::NETWORK:
            return attempt_count < 3; // Max 3 retries
            
        case ExceptionType::PERMANENT:
        case ExceptionType::CORRUPTION:
            return false;
            
        case ExceptionType::UNKNOWN:
            return attempt_count < 1; // Conservative retry for unknown errors
            
        default:
            return false;
    }
}

std::string ExceptionHandler::get_user_message(const std::exception& e) {
    const std::string error_msg = e.what();
    
    // Convert technical error messages to user-friendly ones
    if (error_msg.find("not found") != std::string::npos) {
        return "The requested resource was not found.";
    }
    
    if (error_msg.find("permission denied") != std::string::npos) {
        return "You don't have permission to perform this operation.";
    }
    
    if (error_msg.find("insufficient space") != std::string::npos) {
        return "There is not enough space to complete this operation.";
    }
    
    if (error_msg.find("timeout") != std::string::npos) {
        return "The operation timed out. Please try again.";
    }
    
    if (error_msg.find("network") != std::string::npos) {
        return "A network error occurred. Please check your connection.";
    }
    
    return "An unexpected error occurred. Please try again.";
}

void ExceptionHandler::log_exception(const std::exception& e, const std::string& context) {
    // This would integrate with the logger system
    // For now, we'll use a simple approach
    std::cerr << "[" << context << "] Exception: " << e.what() << std::endl;
}

std::string ExceptionHandler::to_json(const std::exception& e) {
    std::ostringstream json;
    json << "{";
    json << "\"error_type\":\"" << typeid(e).name() << "\",";
    json << "\"message\":\"" << e.what() << "\",";
    json << "\"timestamp\":\"" << std::time(nullptr) << "\"";
    json << "}";
    return json.str();
}

} // namespace utils
} // namespace dfs
