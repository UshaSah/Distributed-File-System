# Multi-stage Dockerfile for Distributed File System
# Stage 1: Build environment
FROM ubuntu:22.04 AS builder

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_BUILD_TYPE=Release
ENV CXX_STANDARD=17

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    libjsoncpp-dev \
    libcurl4-openssl-dev \
    libboost-all-dev \
    clang \
    clang-tools \
    cppcheck \
    gcovr \
    lcov \
    doxygen \
    graphviz \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Create build directory
RUN mkdir -p build

# Build the project
WORKDIR /app/build
RUN cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
          -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
          -DENABLE_TESTS=ON \
          -DENABLE_COVERAGE=OFF \
          -DENABLE_STATIC_ANALYSIS=OFF \
          ..

RUN make -j$(nproc)

# Run tests
RUN ctest --output-on-failure

# Stage 2: Runtime environment
FROM ubuntu:22.04 AS runtime

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV DFS_LOG_LEVEL=INFO
ENV DFS_CONFIG_PATH=/etc/dfs/config.json

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libjsoncpp25 \
    libcurl4 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-chrono1.74.0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN groupadd -r dfs && useradd -r -g dfs dfs

# Create necessary directories
RUN mkdir -p /var/lib/dfs \
             /var/log/dfs \
             /etc/dfs \
             /tmp/dfs

# Copy binaries from builder stage
COPY --from=builder /app/build/bin/dfs-server /usr/local/bin/
COPY --from=builder /app/build/bin/dfs-client /usr/local/bin/
COPY --from=builder /app/build/lib/libdfs.so /usr/local/lib/

# Copy configuration files
COPY config/config.json /etc/dfs/
COPY docker/entrypoint.sh /usr/local/bin/

# Set permissions
RUN chmod +x /usr/local/bin/entrypoint.sh \
    && chown -R dfs:dfs /var/lib/dfs \
    && chown -R dfs:dfs /var/log/dfs \
    && chown -R dfs:dfs /tmp/dfs

# Create symlinks for library
RUN ldconfig

# Expose ports
EXPOSE 8080 9090

# Set working directory
WORKDIR /var/lib/dfs

# Switch to non-root user
USER dfs

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD /usr/local/bin/dfs-client health-check || exit 1

# Default command
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["server"]

# Stage 3: Development environment
FROM builder AS development

# Install additional development tools
RUN apt-get update && apt-get install -y \
    vim \
    nano \
    htop \
    strace \
    gdb \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Expose ports
EXPOSE 8080 9090

# Default command for development
CMD ["/bin/bash"]

# Stage 4: Testing environment
FROM builder AS testing

# Install additional testing tools
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    curl \
    jq \
    && rm -rf /var/lib/apt/lists/*

# Install Python testing dependencies
RUN pip3 install requests pytest pytest-cov

# Copy test scripts
COPY tests/ /app/tests/
COPY scripts/ /app/scripts/

# Set working directory
WORKDIR /app

# Default command for testing
CMD ["python3", "-m", "pytest", "tests/", "-v"]
