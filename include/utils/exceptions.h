#pragma once

#include <exception>
#include <string>
#include <cstdint>

namespace dfs {
namespace utils {

/**
 * Base exception class for all file system exceptions
 */
class FileSystemException : public std::exception {
protected:
    std::string message_;
    uint32_t error_code_;
    std::string context_;
    
public:
    FileSystemException(const std::string& message, uint32_t error_code = 0, 
                       const std::string& context = "");
    
    const char* what() const noexcept override;
    uint32_t get_error_code() const;
    const std::string& get_context() const;
    virtual std::string get_error_type() const;
};

/**
 * Inode-related exceptions
 */
class InodeNotFoundException : public FileSystemException {
public:
    InodeNotFoundException(uint32_t inode_num, const std::string& context = "");
    std::string get_error_type() const override;
};

class InodeAlreadyExistsException : public FileSystemException {
public:
    InodeAlreadyExistsException(uint32_t inode_num, const std::string& context = "");
    std::string get_error_type() const override;
};

class InodeCorruptedException : public FileSystemException {
public:
    InodeCorruptedException(uint32_t inode_num, const std::string& context = "");
    std::string get_error_type() const override;
};

/**
 * Block-related exceptions
 */
class BlockNotFoundException : public FileSystemException {
public:
    BlockNotFoundException(uint32_t block_id, const std::string& context = "");
    std::string get_error_type() const override;
};

class InsufficientSpaceException : public FileSystemException {
public:
    InsufficientSpaceException(uint64_t required, uint64_t available, 
                              const std::string& context = "");
    std::string get_error_type() const override;
};

class BlockCorruptedException : public FileSystemException {
public:
    BlockCorruptedException(uint32_t block_id, const std::string& context = "");
    std::string get_error_type() const override;
};

/**
 * File operation exceptions
 */
class FileNotFoundException : public FileSystemException {
public:
    FileNotFoundException(const std::string& file_path, const std::string& context = "");
    std::string get_error_type() const override;
};

class FileAlreadyExistsException : public FileSystemException {
public:
    FileAlreadyExistsException(const std::string& file_path, const std::string& context = "");
    std::string get_error_type() const override;
};

class DirectoryNotFoundException : public FileSystemException {
public:
    DirectoryNotFoundException(const std::string& dir_path, const std::string& context = "");
    std::string get_error_type() const override;
};

class DirectoryNotEmptyException : public FileSystemException {
public:
    DirectoryNotEmptyException(const std::string& dir_path, const std::string& context = "");
    std::string get_error_type() const override;
};

class PermissionDeniedException : public FileSystemException {
public:
    PermissionDeniedException(const std::string& path, const std::string& operation,
                             const std::string& context = "");
    std::string get_error_type() const override;
};

/**
 * Transaction-related exceptions
 */
class TransactionNotFoundException : public FileSystemException {
public:
    TransactionNotFoundException(uint64_t tx_id, const std::string& context = "");
    std::string get_error_type() const override;
};

class TransactionAbortedException : public FileSystemException {
public:
    TransactionAbortedException(uint64_t tx_id, const std::string& reason,
                               const std::string& context = "");
    std::string get_error_type() const override;
};

class TransactionTimeoutException : public FileSystemException {
public:
    TransactionTimeoutException(uint64_t tx_id, std::chrono::seconds timeout,
                               const std::string& context = "");
    std::string get_error_type() const override;
};

class ConcurrentAccessException : public FileSystemException {
public:
    ConcurrentAccessException(uint32_t inode_num, const std::string& operation,
                             const std::string& context = "");
    std::string get_error_type() const override;
};

/**
 * System-level exceptions
 */
class FileSystemNotMountedException : public FileSystemException {
public:
    FileSystemNotMountedException(const std::string& context = "");
    std::string get_error_type() const override;
};

class FileSystemCorruptedException : public FileSystemException {
public:
    FileSystemCorruptedException(const std::string& reason, const std::string& context = "");
    std::string get_error_type() const override;
};

class ConfigurationException : public FileSystemException {
public:
    ConfigurationException(const std::string& parameter, const std::string& value,
                          const std::string& context = "");
    std::string get_error_type() const override;
};

/**
 * Network and API exceptions
 */
class NetworkException : public FileSystemException {
public:
    NetworkException(const std::string& endpoint, const std::string& reason,
                    const std::string& context = "");
    std::string get_error_type() const override;
};

class APIException : public FileSystemException {
public:
    APIException(const std::string& endpoint, uint32_t http_status,
                const std::string& response, const std::string& context = "");
    std::string get_error_type() const override;
};

class RateLimitExceededException : public FileSystemException {
public:
    RateLimitExceededException(const std::string& client_id, uint32_t limit,
                              const std::string& context = "");
    std::string get_error_type() const override;
};

/**
 * Utility functions for exception handling
 */
class ExceptionHandler {
public:
    // Classify exception type for retry logic
    enum class ExceptionType {
        TRANSIENT,      // Retry possible
        PERMANENT,      // No retry
        CORRUPTION,     // Data integrity issue
        CONCURRENCY,    // Lock contention
        TIMEOUT,        // Operation timeout
        NETWORK,        // Network-related error
        UNKNOWN         // Unknown error type
    };
    
    // Classify exception for retry decision
    static ExceptionType classify_exception(const std::exception& e);
    
    // Check if exception should be retried
    static bool should_retry(const std::exception& e, uint32_t attempt_count);
    
    // Get user-friendly error message
    static std::string get_user_message(const std::exception& e);
    
    // Log exception with context
    static void log_exception(const std::exception& e, const std::string& context);
    
    // Convert exception to JSON for API responses
    static std::string to_json(const std::exception& e);
};

} // namespace utils
} // namespace dfs
