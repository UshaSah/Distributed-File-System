# Unit tests CMakeLists.txt

# Find Google Test
find_package(GTest REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Test source files
set(UNIT_TEST_SOURCES
    test_superblock.cpp
    test_inode.cpp
    test_block_manager.cpp
    test_utilities.cpp
)

# Create test executable
add_executable(unit_tests ${UNIT_TEST_SOURCES})

# Link libraries
target_link_libraries(unit_tests
    dfs_core
    dfs_utils
    GTest::GTest
    GTest::Main
    Threads::Threads
)

# Add test to CTest
enable_testing()
add_test(NAME UnitTests COMMAND unit_tests)

# Set test properties
set_tests_properties(UnitTests PROPERTIES
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
