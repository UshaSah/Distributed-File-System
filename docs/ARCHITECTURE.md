# Architecture Documentation

## Overview

The Distributed File System is designed as a high-performance, concurrent file system with a distributed architecture. It provides strong consistency guarantees, transaction safety, and robust error handling under high concurrency.

## System Architecture

### High-Level Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Client Apps   │    │   Client Apps   │    │   Client Apps   │
└─────────┬───────┘    └─────────┬───────┘    └─────────┬───────┘
          │                      │                      │
          └──────────────────────┼──────────────────────┘
                                 │
                    ┌─────────────┴─────────────┐
                    │      Load Balancer        │
                    │        (Nginx)            │
                    └─────────────┬─────────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │      REST API Server      │
                    │     (HTTP Interface)      │
                    └─────────────┬─────────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │    File System Core       │
                    │  (Transaction Manager)    │
                    └─────────────┬─────────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │    Storage Layer          │
                    │  (Block Manager, Inodes)  │
                    └───────────────────────────┘
```

### Component Overview

1. **Client Layer**: Applications using the file system
2. **Load Balancer**: Distributes requests across multiple server instances
3. **REST API Server**: HTTP interface for file operations
4. **File System Core**: Core file system logic and transaction management
5. **Storage Layer**: Low-level storage management

## Core Components

### 1. SuperBlock

The SuperBlock contains essential file system metadata and configuration.

```cpp
struct SuperBlock {
    uint32_t magic_number;        // File system signature
    uint32_t block_size;          // Block size (typically 4KB)
    uint32_t total_blocks;        // Total number of blocks
    uint32_t free_blocks;         // Available blocks
    uint32_t inode_count;         // Total inodes
    uint32_t free_inodes;         // Available inodes
    uint32_t root_inode;          // Root directory inode
    uint64_t last_mount_time;     // Timestamp
    uint32_t checksum;            // Integrity verification
};
```

**Responsibilities**:
- File system identification and validation
- Block and inode allocation tracking
- Mount/unmount state management
- Integrity verification

### 2. Inode Table

Manages file metadata and provides concurrent access to inode information.

```cpp
struct Inode {
    uint16_t mode;                // File type and permissions
    uint16_t uid, gid;           // User/group ownership
    uint64_t size;               // File size
    uint64_t blocks;             // Number of blocks used
    uint64_t atime, mtime, ctime; // Timestamps
    uint32_t direct_blocks[12];   // Direct block pointers
    uint32_t indirect_block;      // Single indirect pointer
    uint32_t double_indirect;     // Double indirect pointer
    uint32_t triple_indirect;     // Triple indirect pointer
    uint32_t replication_count;   // For distributed storage
    uint32_t checksum;           // Data integrity
};
```

**Responsibilities**:
- File metadata storage and retrieval
- Concurrent access control
- Block pointer management
- Permission and ownership handling

### 3. Block Manager

Manages data block allocation and deallocation with thread-safe operations.

```cpp
class BlockManager {
private:
    std::vector<bool> block_bitmap_;  // Free block tracking
    std::mutex bitmap_mutex_;         // Thread safety
    
public:
    uint32_t allocate_block();        // Get free block
    void deallocate_block(uint32_t block_id);
    bool is_block_free(uint32_t block_id);
    void mark_block_used(uint32_t block_id);
};
```

**Responsibilities**:
- Block allocation and deallocation
- Free space tracking
- Thread-safe block management
- Block integrity verification

### 4. Transaction Manager

Provides ACID transaction support with write-ahead logging (WAL).

```cpp
class TransactionManager {
private:
    std::unordered_map<uint64_t, std::unique_ptr<Transaction>> active_transactions_;
    std::mutex transaction_mutex_;
    std::atomic<uint64_t> next_transaction_id_;
    std::string log_file_path_;
    
public:
    uint64_t begin_transaction();
    bool commit_transaction(uint64_t tx_id);
    bool rollback_transaction(uint64_t tx_id);
    void add_log_entry(uint64_t tx_id, const LogEntry& entry);
};
```

**Responsibilities**:
- Transaction lifecycle management
- Write-ahead logging
- Rollback and recovery
- Concurrency control

### 5. File System

Coordinates all file system components and provides high-level operations.

```cpp
class FileSystem {
private:
    std::unique_ptr<SuperBlock> superblock_;
    std::unique_ptr<InodeTable> inode_table_;
    std::unique_ptr<BlockManager> block_manager_;
    std::unique_ptr<TransactionManager> transaction_manager_;
    
public:
    bool create_file(const std::string& path, uint16_t permissions = 0644);
    std::vector<uint8_t> read_file(const std::string& path) const;
    bool write_file(const std::string& path, const std::vector<uint8_t>& data);
    bool delete_file(const std::string& path);
};
```

**Responsibilities**:
- High-level file operations
- Path resolution and validation
- Component coordination
- Error handling and recovery

## API Layer

### REST Server

Provides HTTP interface for file system operations.

```cpp
class RESTServer {
private:
    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<core::FileSystem> file_system_;
    std::unique_ptr<utils::ThreadPool> thread_pool_;
    std::unique_ptr<utils::RateLimiter> rate_limiter_;
    
public:
    void handle_file_operations(const httplib::Request& req, httplib::Response& res);
    void handle_directory_operations(const httplib::Request& req, httplib::Response& res);
    void handle_system_operations(const httplib::Request& req, httplib::Response& res);
};
```

**Responsibilities**:
- HTTP request handling
- JSON serialization/deserialization
- Request validation and sanitization
- Response formatting

### Client Library

Provides C++ client interface for file system operations.

```cpp
class ClientLibrary {
private:
    ClientConfig config_;
    std::unique_ptr<utils::ThreadPool> thread_pool_;
    std::unique_ptr<utils::RetryHandler> retry_handler_;
    
public:
    bool create_file(const std::string& path, const std::vector<uint8_t>& data);
    std::vector<uint8_t> read_file(const std::string& path);
    bool write_file(const std::string& path, const std::vector<uint8_t>& data);
    bool delete_file(const std::string& path);
};
```

**Responsibilities**:
- Client-side file operations
- Connection management
- Error handling and retry logic
- Async operation support

## Utility Components

### Thread Pool

Manages worker threads for concurrent task execution.

```cpp
class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::priority_queue<Task> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    
public:
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args, Priority priority = Priority::NORMAL)
        -> std::future<typename std::result_of<F(Args...)>::type>;
};
```

**Features**:
- Priority-based task scheduling
- Configurable thread count
- Work stealing for load balancing
- Graceful shutdown

### Rate Limiter

Implements token bucket algorithm for request rate limiting.

```cpp
class RateLimiter {
private:
    struct TokenBucket {
        std::atomic<uint32_t> tokens;
        std::chrono::steady_clock::time_point last_refill;
        std::mutex bucket_mutex;
    };
    
public:
    bool is_allowed(const std::string& client_id, uint32_t tokens_needed = 1);
    void update_config(const RateLimitConfig& new_config);
};
```

**Features**:
- Per-client rate limiting
- Configurable thresholds
- Burst capacity support
- Statistics tracking

### Retry Handler

Implements retry logic with exponential backoff and circuit breaker.

```cpp
class RetryHandler {
private:
    RetryConfig config_;
    std::atomic<CircuitState> circuit_state_;
    std::atomic<uint32_t> consecutive_failures_;
    
public:
    template<typename Func, typename... Args>
    auto execute_with_retry(Func&& func, Args&&... args) 
        -> decltype(func(args...));
};
```

**Features**:
- Exponential backoff with jitter
- Circuit breaker pattern
- Error classification
- Configurable retry policies

### Logger

Provides structured logging with multiple output destinations.

```cpp
class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR, CRITICAL };
    
    void log(Level level, const std::string& message);
    void log_transaction(uint64_t tx_id, const std::string& operation);
    void log_performance(const std::string& operation, std::chrono::milliseconds duration);
};
```

**Features**:
- Multiple log levels
- File rotation
- Async logging
- Performance metrics

## Concurrency Model

### Locking Strategy

The system uses a hierarchical locking strategy:

1. **File System Level**: Shared mutex for mount/unmount operations
2. **Inode Level**: Per-inode mutexes for concurrent access
3. **Block Level**: Mutex for block bitmap operations
4. **Transaction Level**: Mutex for transaction management

### Thread Safety

- **Reader-Writer Locks**: For inode access (multiple readers, single writer)
- **Atomic Operations**: For counters and flags
- **Lock-Free Structures**: Where possible for performance
- **Deadlock Prevention**: Ordered locking to prevent deadlocks

### Transaction Isolation

- **Read Committed**: Default isolation level
- **Serializable**: For critical operations
- **Optimistic Concurrency**: For high-throughput scenarios

## Data Flow

### File Creation Flow

```
1. Client Request → REST Server
2. REST Server → File System
3. File System → Transaction Manager (Begin Transaction)
4. File System → Inode Table (Allocate Inode)
5. File System → Block Manager (Allocate Blocks)
6. File System → Data Blocks (Write Content)
7. File System → Transaction Manager (Commit Transaction)
8. Transaction Manager → WAL (Write Log Entry)
9. File System → REST Server (Success Response)
10. REST Server → Client (Success Response)
```

### File Reading Flow

```
1. Client Request → REST Server
2. REST Server → File System
3. File System → Inode Table (Get Inode)
4. File System → Block Manager (Get Block Pointers)
5. File System → Data Blocks (Read Content)
6. File System → REST Server (File Content)
7. REST Server → Client (File Content)
```

### Transaction Flow

```
1. Begin Transaction
   ├── Generate Transaction ID
   ├── Create Transaction Object
   └── Add to Active Transactions

2. Execute Operations
   ├── Validate Transaction
   ├── Perform Operation
   ├── Log Operation
   └── Update State

3. Commit Transaction
   ├── Validate All Operations
   ├── Write to WAL
   ├── Apply Changes
   └── Remove from Active Transactions

4. Rollback Transaction
   ├── Read WAL Entries
   ├── Undo Operations
   └── Remove from Active Transactions
```

## Error Handling

### Exception Hierarchy

```
FileSystemException (Base)
├── InodeNotFoundException
├── BlockNotFoundException
├── FileNotFoundException
├── DirectoryNotFoundException
├── PermissionDeniedException
├── TransactionNotFoundException
├── TransactionAbortedException
├── ConcurrentAccessException
├── InsufficientSpaceException
└── FileSystemCorruptedException
```

### Error Recovery

1. **Transient Errors**: Automatic retry with exponential backoff
2. **Permanent Errors**: Return error to client
3. **Corruption Errors**: Attempt repair, fallback to backup
4. **Concurrency Errors**: Retry with different locking strategy

### Circuit Breaker

- **Closed**: Normal operation
- **Open**: Fail fast, no requests allowed
- **Half-Open**: Test if service is back

## Performance Considerations

### Optimization Strategies

1. **Memory Management**:
   - Smart pointers for automatic memory management
   - Memory pools for frequent allocations
   - Cache-friendly data structures

2. **I/O Optimization**:
   - Asynchronous I/O operations
   - Batch operations for multiple files
   - Memory-mapped files for large data

3. **Concurrency Optimization**:
   - Lock-free data structures where possible
   - Work stealing for load balancing
   - NUMA-aware thread placement

4. **Caching**:
   - Inode cache for frequently accessed files
   - Block cache for recently used data
   - Metadata cache for directory listings

### Scalability Features

1. **Horizontal Scaling**: Multiple server instances with load balancing
2. **Vertical Scaling**: Configurable thread pools and resource limits
3. **Partitioning**: File system partitioning for large deployments
4. **Replication**: Data replication for fault tolerance

## Security Architecture

### Authentication and Authorization

- **API Keys**: For client authentication
- **Role-Based Access Control**: For permission management
- **Audit Logging**: For security monitoring

### Data Protection

- **Encryption**: SSL/TLS for data in transit
- **Integrity**: Checksums for data verification
- **Backup**: Regular backup and recovery procedures

### Network Security

- **Rate Limiting**: DDoS protection
- **Input Validation**: Sanitization of all inputs
- **CORS**: Cross-origin resource sharing control

## Monitoring and Observability

### Metrics

- **Performance Metrics**: Latency, throughput, error rates
- **Resource Metrics**: CPU, memory, disk usage
- **Business Metrics**: File operations, user activity

### Logging

- **Structured Logging**: JSON format for easy parsing
- **Log Levels**: DEBUG, INFO, WARN, ERROR, CRITICAL
- **Log Rotation**: Automatic log file rotation

### Health Checks

- **Liveness**: Is the service running?
- **Readiness**: Is the service ready to accept requests?
- **Dependencies**: Are external dependencies available?

## Deployment Architecture

### Containerization

- **Multi-stage Builds**: Optimized Docker images
- **Health Checks**: Container health monitoring
- **Resource Limits**: CPU and memory constraints

### Orchestration

- **Docker Compose**: Local development and testing
- **Kubernetes**: Production deployment
- **Service Discovery**: Automatic service registration

### High Availability

- **Load Balancing**: Multiple server instances
- **Failover**: Automatic failover mechanisms
- **Backup**: Regular backup and recovery

## Future Enhancements

### Planned Features

1. **Distributed Consensus**: Raft algorithm for consistency
2. **Data Replication**: Multi-node replication
3. **Compression**: Built-in data compression
4. **Encryption**: At-rest data encryption
5. **Snapshot**: Point-in-time snapshots
6. **Cloning**: Fast file system cloning

### Performance Improvements

1. **SSD Optimization**: SSD-specific optimizations
2. **Memory Mapping**: Advanced memory mapping
3. **Parallel Processing**: GPU acceleration for certain operations
4. **Machine Learning**: Predictive caching and optimization

This architecture provides a solid foundation for a production-ready distributed file system with room for future enhancements and optimizations.
