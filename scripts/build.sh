#!/bin/bash

# Build script for Distributed File System
# Usage: ./scripts/build.sh [options]

set -e

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
INSTALL_PREFIX=""
ENABLE_TESTS="ON"
ENABLE_COVERAGE="OFF"
ENABLE_STATIC_ANALYSIS="OFF"
CLEAN_BUILD="OFF"
VERBOSE="OFF"

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
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -t, --type TYPE          Build type (Debug, Release, RelWithDebInfo, MinSizeRel)"
    echo "  -d, --dir DIR            Build directory (default: build)"
    echo "  -p, --prefix PREFIX      Install prefix"
    echo "  -c, --clean              Clean build directory before building"
    echo "  -v, --verbose            Verbose output"
    echo "  --no-tests               Disable tests"
    echo "  --coverage               Enable code coverage"
    echo "  --static-analysis        Enable static analysis"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                       # Default build"
    echo "  $0 -t Debug -c           # Debug build with clean"
    echo "  $0 --coverage --static-analysis  # Build with coverage and analysis"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -d|--dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -p|--prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD="ON"
            shift
            ;;
        -v|--verbose)
            VERBOSE="ON"
            shift
            ;;
        --no-tests)
            ENABLE_TESTS="OFF"
            shift
            ;;
        --coverage)
            ENABLE_COVERAGE="ON"
            shift
            ;;
        --static-analysis)
            ENABLE_STATIC_ANALYSIS="ON"
            shift
            ;;
        -h|--help)
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

# Validate build type
case $BUILD_TYPE in
    Debug|Release|RelWithDebInfo|MinSizeRel)
        ;;
    *)
        print_error "Invalid build type: $BUILD_TYPE"
        exit 1
        ;;
esac

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

print_info "Building Distributed File System"
print_info "Project root: $PROJECT_ROOT"
print_info "Build type: $BUILD_TYPE"
print_info "Build directory: $BUILD_DIR"

# Change to project root
cd "$PROJECT_ROOT"

# Clean build directory if requested
if [[ "$CLEAN_BUILD" == "ON" ]]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
print_info "Configuring CMake..."

CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DENABLE_TESTS="$ENABLE_TESTS"
    -DENABLE_COVERAGE="$ENABLE_COVERAGE"
    -DENABLE_STATIC_ANALYSIS="$ENABLE_STATIC_ANALYSIS"
)

if [[ -n "$INSTALL_PREFIX" ]]; then
    CMAKE_ARGS+=(-DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX")
fi

if [[ "$VERBOSE" == "ON" ]]; then
    CMAKE_ARGS+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
fi

cmake "${CMAKE_ARGS[@]}" ..

# Build
print_info "Building..."
if [[ "$VERBOSE" == "ON" ]]; then
    make -j$(nproc) VERBOSE=1
else
    make -j$(nproc)
fi

# Run tests if enabled
if [[ "$ENABLE_TESTS" == "ON" ]]; then
    print_info "Running tests..."
    ctest --output-on-failure
fi

# Run static analysis if enabled
if [[ "$ENABLE_STATIC_ANALYSIS" == "ON" ]]; then
    print_info "Running static analysis..."
    make cppcheck
fi

# Generate coverage report if enabled
if [[ "$ENABLE_COVERAGE" == "ON" ]]; then
    print_info "Generating coverage report..."
    make coverage
fi

# Install if prefix is specified
if [[ -n "$INSTALL_PREFIX" ]]; then
    print_info "Installing to $INSTALL_PREFIX..."
    make install
fi

print_success "Build completed successfully!"

# Show build summary
echo ""
print_info "Build Summary:"
echo "  Build type: $BUILD_TYPE"
echo "  Build directory: $BUILD_DIR"
echo "  Tests enabled: $ENABLE_TESTS"
echo "  Coverage enabled: $ENABLE_COVERAGE"
echo "  Static analysis enabled: $ENABLE_STATIC_ANALYSIS"

if [[ -n "$INSTALL_PREFIX" ]]; then
    echo "  Install prefix: $INSTALL_PREFIX"
fi

echo ""
print_info "Executables:"
echo "  Server: $BUILD_DIR/bin/dfs-server"
echo "  Client: $BUILD_DIR/bin/dfs-client"

echo ""
print_info "Libraries:"
echo "  Core: $BUILD_DIR/lib/libdfs_core.a"
echo "  Utils: $BUILD_DIR/lib/libdfs_utils.a"
echo "  API: $BUILD_DIR/lib/libdfs_api.a"
echo "  Shared: $BUILD_DIR/lib/libdfs.so"

if [[ "$ENABLE_COVERAGE" == "ON" ]]; then
    echo ""
    print_info "Coverage report: $BUILD_DIR/coverage/index.html"
fi

if [[ "$ENABLE_STATIC_ANALYSIS" == "ON" ]]; then
    echo ""
    print_info "Static analysis completed"
fi
