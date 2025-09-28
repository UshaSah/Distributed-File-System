#!/bin/bash

# Entrypoint script for Distributed File System containers
# This script handles different container modes and configurations

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  server              Start the DFS server"
    echo "  client              Start the DFS client"
    echo "  health-check        Perform health check"
    echo "  init                Initialize the file system"
    echo "  format              Format the file system"
    echo "  backup              Create backup"
    echo "  restore             Restore from backup"
    echo "  shell               Start interactive shell"
    echo ""
    echo "Options:"
    echo "  --config PATH       Configuration file path"
    echo "  --log-level LEVEL   Log level (DEBUG, INFO, WARN, ERROR)"
    echo "  --help              Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  DFS_CONFIG_PATH     Configuration file path"
    echo "  DFS_LOG_LEVEL       Log level"
    echo "  DFS_DATA_PATH       Data directory path"
    echo "  DFS_LOG_PATH        Log directory path"
}

# Default values
COMMAND=""
CONFIG_PATH="${DFS_CONFIG_PATH:-/etc/dfs/config.json}"
LOG_LEVEL="${DFS_LOG_LEVEL:-INFO}"
DATA_PATH="${DFS_DATA_PATH:-/var/lib/dfs}"
LOG_PATH="${DFS_LOG_PATH:-/var/log/dfs}"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        server|client|health-check|init|format|backup|restore|shell)
            COMMAND="$1"
            shift
            ;;
        --config)
            CONFIG_PATH="$2"
            shift 2
            ;;
        --log-level)
            LOG_LEVEL="$2"
            shift 2
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# If no command specified, use first argument or default to server
if [[ -z "$COMMAND" ]]; then
    COMMAND="${1:-server}"
fi

# Function to check if configuration file exists
check_config() {
    if [[ ! -f "$CONFIG_PATH" ]]; then
        print_warning "Configuration file not found: $CONFIG_PATH"
        print_info "Using default configuration"
        return 1
    fi
    return 0
}

# Function to create necessary directories
create_directories() {
    print_info "Creating necessary directories..."
    
    mkdir -p "$DATA_PATH"
    mkdir -p "$LOG_PATH"
    mkdir -p "$(dirname "$CONFIG_PATH")"
    
    # Set proper permissions
    chmod 755 "$DATA_PATH"
    chmod 755 "$LOG_PATH"
    
    print_success "Directories created successfully"
}

# Function to initialize the file system
init_filesystem() {
    print_info "Initializing file system..."
    
    if [[ -f "$DATA_PATH/device" ]]; then
        print_warning "File system device already exists"
        return 0
    fi
    
    # Create device file
    dd if=/dev/zero of="$DATA_PATH/device" bs=1M count=100 2>/dev/null
    
    # Format the file system
    /usr/local/bin/dfs-server --format --device "$DATA_PATH/device" --config "$CONFIG_PATH"
    
    print_success "File system initialized successfully"
}

# Function to start the server
start_server() {
    print_info "Starting DFS server..."
    
    # Check configuration
    check_config
    
    # Create directories
    create_directories
    
    # Initialize file system if needed
    if [[ ! -f "$DATA_PATH/device" ]]; then
        print_info "File system device not found, initializing..."
        init_filesystem
    fi
    
    # Start the server
    exec /usr/local/bin/dfs-server \
        --config "$CONFIG_PATH" \
        --log-level "$LOG_LEVEL" \
        --data-path "$DATA_PATH" \
        --log-path "$LOG_PATH"
}

# Function to start the client
start_client() {
    print_info "Starting DFS client..."
    
    # Check configuration
    check_config
    
    # Start the client
    exec /usr/local/bin/dfs-client \
        --config "$CONFIG_PATH" \
        --log-level "$LOG_LEVEL"
}

# Function to perform health check
health_check() {
    print_info "Performing health check..."
    
    # Check if server is running
    if ! pgrep -f "dfs-server" > /dev/null; then
        print_error "DFS server is not running"
        exit 1
    fi
    
    # Check if client can connect
    if ! /usr/local/bin/dfs-client health-check; then
        print_error "Health check failed"
        exit 1
    fi
    
    print_success "Health check passed"
    exit 0
}

# Function to format the file system
format_filesystem() {
    print_info "Formatting file system..."
    
    if [[ ! -f "$DATA_PATH/device" ]]; then
        print_error "File system device not found: $DATA_PATH/device"
        exit 1
    fi
    
    # Format the file system
    /usr/local/bin/dfs-server --format --device "$DATA_PATH/device" --config "$CONFIG_PATH"
    
    print_success "File system formatted successfully"
}

# Function to create backup
create_backup() {
    print_info "Creating backup..."
    
    BACKUP_DIR="${BACKUP_DIR:-/var/lib/dfs/backups}"
    BACKUP_NAME="dfs_backup_$(date +%Y%m%d_%H%M%S)"
    
    mkdir -p "$BACKUP_DIR"
    
    # Create backup
    tar -czf "$BACKUP_DIR/$BACKUP_NAME.tar.gz" -C "$DATA_PATH" .
    
    print_success "Backup created: $BACKUP_DIR/$BACKUP_NAME.tar.gz"
}

# Function to restore from backup
restore_backup() {
    print_info "Restoring from backup..."
    
    BACKUP_FILE="${BACKUP_FILE:-/var/lib/dfs/backups/latest.tar.gz}"
    
    if [[ ! -f "$BACKUP_FILE" ]]; then
        print_error "Backup file not found: $BACKUP_FILE"
        exit 1
    fi
    
    # Stop server if running
    if pgrep -f "dfs-server" > /dev/null; then
        print_info "Stopping server for restore..."
        pkill -f "dfs-server"
        sleep 2
    fi
    
    # Restore backup
    tar -xzf "$BACKUP_FILE" -C "$DATA_PATH"
    
    print_success "Backup restored successfully"
}

# Function to start interactive shell
start_shell() {
    print_info "Starting interactive shell..."
    exec /bin/bash
}

# Main execution
case "$COMMAND" in
    server)
        start_server
        ;;
    client)
        start_client
        ;;
    health-check)
        health_check
        ;;
    init)
        create_directories
        init_filesystem
        ;;
    format)
        format_filesystem
        ;;
    backup)
        create_backup
        ;;
    restore)
        restore_backup
        ;;
    shell)
        start_shell
        ;;
    *)
        print_error "Unknown command: $COMMAND"
        show_usage
        exit 1
        ;;
esac
