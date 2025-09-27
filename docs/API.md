# API Documentation

## Overview

The Distributed File System provides a RESTful API for file and directory operations. The API is designed to be simple, consistent, and efficient.

## Base URL

```
http://localhost:8080/api
```

## Authentication

Currently, the API does not require authentication. In production, API keys or other authentication mechanisms should be implemented.

## Content Types

- **Request**: `application/json`
- **Response**: `application/json`

## Error Handling

All API responses follow a consistent format:

```json
{
  "status": "success|error",
  "message": "Human-readable message",
  "transaction_id": "Unique transaction identifier",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "key": "value"
  }
}
```

### Error Codes

- `400` - Bad Request
- `401` - Unauthorized
- `403` - Forbidden
- `404` - Not Found
- `409` - Conflict
- `429` - Too Many Requests
- `500` - Internal Server Error

## File Operations

### Create File

Create a new file with content.

**Endpoint**: `POST /api/files`

**Request Body**:
```json
{
  "path": "/path/to/file.txt",
  "content": "File content",
  "permissions": 0644
}
```

**Response**:
```json
{
  "status": "success",
  "message": "File created successfully",
  "transaction_id": "tx_abc123",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "path": "/path/to/file.txt",
    "size": 12,
    "inode": 12345
  }
}
```

### Read File

Read the contents of a file.

**Endpoint**: `GET /api/files/{path}`

**Parameters**:
- `path` (path parameter): File path

**Response**:
```json
{
  "status": "success",
  "message": "File read successfully",
  "transaction_id": "tx_def456",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "path": "/path/to/file.txt",
    "content": "File content",
    "size": 12,
    "permissions": "rw-r--r--",
    "created": "2024-05-15T10:30:00Z",
    "modified": "2024-05-15T10:30:00Z"
  }
}
```

### Update File

Update the contents of an existing file.

**Endpoint**: `PUT /api/files/{path}`

**Parameters**:
- `path` (path parameter): File path

**Request Body**:
```json
{
  "content": "Updated file content",
  "permissions": 0644
}
```

**Response**:
```json
{
  "status": "success",
  "message": "File updated successfully",
  "transaction_id": "tx_ghi789",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "path": "/path/to/file.txt",
    "size": 20,
    "modified": "2024-05-15T10:30:00Z"
  }
}
```

### Delete File

Delete a file.

**Endpoint**: `DELETE /api/files/{path}`

**Parameters**:
- `path` (path parameter): File path

**Response**:
```json
{
  "status": "success",
  "message": "File deleted successfully",
  "transaction_id": "tx_jkl012",
  "timestamp": "2024-05-15T10:30:00Z"
}
```

### Get File Info

Get metadata information about a file.

**Endpoint**: `GET /api/metadata/{path}`

**Parameters**:
- `path` (path parameter): File path

**Response**:
```json
{
  "status": "success",
  "message": "File metadata retrieved successfully",
  "transaction_id": "tx_mno345",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "path": "/path/to/file.txt",
    "size": 12,
    "permissions": "rw-r--r--",
    "uid": 1000,
    "gid": 1000,
    "created": "2024-05-15T10:30:00Z",
    "modified": "2024-05-15T10:30:00Z",
    "accessed": "2024-05-15T10:30:00Z",
    "is_directory": false,
    "is_file": true,
    "is_symlink": false
  }
}
```

## Directory Operations

### Create Directory

Create a new directory.

**Endpoint**: `POST /api/directories`

**Request Body**:
```json
{
  "path": "/path/to/directory",
  "permissions": 0755
}
```

**Response**:
```json
{
  "status": "success",
  "message": "Directory created successfully",
  "transaction_id": "tx_pqr678",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "path": "/path/to/directory",
    "inode": 12346
  }
}
```

### List Directory

List the contents of a directory.

**Endpoint**: `GET /api/directories/{path}`

**Parameters**:
- `path` (path parameter): Directory path

**Response**:
```json
{
  "status": "success",
  "message": "Directory listed successfully",
  "transaction_id": "tx_stu901",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "path": "/path/to/directory",
    "entries": [
      {
        "name": "file1.txt",
        "type": "file",
        "size": 1024,
        "permissions": "rw-r--r--",
        "modified": "2024-05-15T10:30:00Z"
      },
      {
        "name": "subdirectory",
        "type": "directory",
        "size": 0,
        "permissions": "rwxr-xr-x",
        "modified": "2024-05-15T10:30:00Z"
      }
    ]
  }
}
```

### Delete Directory

Delete a directory.

**Endpoint**: `DELETE /api/directories/{path}`

**Parameters**:
- `path` (path parameter): Directory path

**Response**:
```json
{
  "status": "success",
  "message": "Directory deleted successfully",
  "transaction_id": "tx_vwx234",
  "timestamp": "2024-05-15T10:30:00Z"
}
```

### Rename/Move

Rename or move a file or directory.

**Endpoint**: `PUT /api/rename`

**Request Body**:
```json
{
  "old_path": "/path/to/old_name",
  "new_path": "/path/to/new_name"
}
```

**Response**:
```json
{
  "status": "success",
  "message": "File renamed successfully",
  "transaction_id": "tx_yza567",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "old_path": "/path/to/old_name",
    "new_path": "/path/to/new_name"
  }
}
```

## System Operations

### Get System Info

Get information about the file system.

**Endpoint**: `GET /api/system/info`

**Response**:
```json
{
  "status": "success",
  "message": "System info retrieved successfully",
  "transaction_id": "tx_bcd890",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "total_blocks": 1000000,
    "free_blocks": 750000,
    "total_inodes": 100000,
    "free_inodes": 50000,
    "block_size": 4096,
    "usage_percentage": 25.0,
    "version": "1.0.0",
    "uptime": "2h 30m 15s"
  }
}
```

### Get System Stats

Get detailed system statistics.

**Endpoint**: `GET /api/system/stats`

**Response**:
```json
{
  "status": "success",
  "message": "System stats retrieved successfully",
  "transaction_id": "tx_efg123",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "total_files": 1500,
    "total_directories": 200,
    "total_data_size": 104857600,
    "active_transactions": 5,
    "request_count": 10000,
    "error_count": 25,
    "average_response_time": "15ms"
  }
}
```

### Health Check

Check the health of the system.

**Endpoint**: `GET /api/health`

**Response**:
```json
{
  "status": "success",
  "message": "System is healthy",
  "transaction_id": "tx_hij456",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "status": "healthy",
    "version": "1.0.0",
    "uptime": "2h 30m 15s",
    "memory_usage": "45%",
    "disk_usage": "25%",
    "active_connections": 10
  }
}
```

## Transaction Operations

### Begin Transaction

Start a new transaction.

**Endpoint**: `POST /api/transactions`

**Response**:
```json
{
  "status": "success",
  "message": "Transaction started successfully",
  "transaction_id": "tx_klm789",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "transaction_id": "tx_klm789",
    "timeout": 30
  }
}
```

### Commit Transaction

Commit a transaction.

**Endpoint**: `PUT /api/transactions/{transaction_id}/commit`

**Parameters**:
- `transaction_id` (path parameter): Transaction ID

**Response**:
```json
{
  "status": "success",
  "message": "Transaction committed successfully",
  "transaction_id": "tx_klm789",
  "timestamp": "2024-05-15T10:30:00Z"
}
```

### Rollback Transaction

Rollback a transaction.

**Endpoint**: `PUT /api/transactions/{transaction_id}/rollback`

**Parameters**:
- `transaction_id` (path parameter): Transaction ID

**Response**:
```json
{
  "status": "success",
  "message": "Transaction rolled back successfully",
  "transaction_id": "tx_klm789",
  "timestamp": "2024-05-15T10:30:00Z"
}
```

## Rate Limiting

The API implements rate limiting to prevent abuse. Rate limits are applied per client IP address.

### Rate Limit Headers

Responses include rate limiting information:

```
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1640995200
```

### Rate Limit Exceeded

When rate limits are exceeded, a 429 status code is returned:

```json
{
  "status": "error",
  "message": "Rate limit exceeded",
  "transaction_id": "tx_nop012",
  "timestamp": "2024-05-15T10:30:00Z",
  "data": {
    "error_code": 429,
    "retry_after": 60
  }
}
```

## Examples

### Complete File Workflow

```bash
# 1. Create a file
curl -X POST http://localhost:8080/api/files \
  -H "Content-Type: application/json" \
  -d '{"path": "/test.txt", "content": "Hello, World!"}'

# 2. Read the file
curl http://localhost:8080/api/files/test.txt

# 3. Update the file
curl -X PUT http://localhost:8080/api/files/test.txt \
  -H "Content-Type: application/json" \
  -d '{"content": "Hello, Distributed File System!"}'

# 4. Get file info
curl http://localhost:8080/api/metadata/test.txt

# 5. Delete the file
curl -X DELETE http://localhost:8080/api/files/test.txt
```

### Directory Operations

```bash
# 1. Create a directory
curl -X POST http://localhost:8080/api/directories \
  -H "Content-Type: application/json" \
  -d '{"path": "/test_dir", "permissions": 0755}'

# 2. List directory contents
curl http://localhost:8080/api/directories/test_dir

# 3. Create a file in the directory
curl -X POST http://localhost:8080/api/files \
  -H "Content-Type: application/json" \
  -d '{"path": "/test_dir/file.txt", "content": "Test content"}'

# 4. List directory again
curl http://localhost:8080/api/directories/test_dir

# 5. Delete the directory
curl -X DELETE http://localhost:8080/api/directories/test_dir
```

### Transaction Example

```bash
# 1. Begin transaction
TRANSACTION_ID=$(curl -X POST http://localhost:8080/api/transactions | jq -r '.transaction_id')

# 2. Perform operations within transaction
curl -X POST http://localhost:8080/api/files \
  -H "Content-Type: application/json" \
  -H "X-Transaction-ID: $TRANSACTION_ID" \
  -d '{"path": "/tx_test.txt", "content": "Transaction test"}'

# 3. Commit transaction
curl -X PUT http://localhost:8080/api/transactions/$TRANSACTION_ID/commit
```

## SDKs and Client Libraries

### C++ Client

```cpp
#include "dfs/api/client_library.h"

// Create client
auto client = dfs::api::ClientFactory::create_client();

// Connect
client->connect();

// Create file
client->create_file("/test.txt", "Hello, World!");

// Read file
auto content = client->read_file_as_string("/test.txt");

// Disconnect
client->disconnect();
```

### Python Client

```python
import requests

# Create file
response = requests.post('http://localhost:8080/api/files', 
                        json={'path': '/test.txt', 'content': 'Hello, World!'})

# Read file
response = requests.get('http://localhost:8080/api/files/test.txt')
print(response.json()['data']['content'])
```

### JavaScript Client

```javascript
// Create file
fetch('http://localhost:8080/api/files', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({path: '/test.txt', content: 'Hello, World!'})
})
.then(response => response.json())
.then(data => console.log(data));

// Read file
fetch('http://localhost:8080/api/files/test.txt')
.then(response => response.json())
.then(data => console.log(data.data.content));
```

## Best Practices

1. **Use Transactions**: For multiple related operations, use transactions to ensure consistency
2. **Handle Errors**: Always check response status and handle errors appropriately
3. **Respect Rate Limits**: Implement exponential backoff for rate limit errors
4. **Use Async Operations**: For better performance, use async operations when possible
5. **Monitor Performance**: Use the metrics endpoint to monitor system performance
6. **Backup Data**: Regularly backup important data using the backup endpoints
7. **Security**: Use HTTPS in production and implement proper authentication
