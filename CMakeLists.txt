cmake_minimum_required(VERSION 3.14)
project(HALI CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags as per specification
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_BUILD_TYPE Release)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external/libs)
include_directories(${CMAKE_SOURCE_DIR}/external/libs/parallel-hashmap)
include_directories(${CMAKE_SOURCE_DIR}/external/libs/art_map/include)
include_directories(${CMAKE_SOURCE_DIR}/external/libs/PGM-index/include)
include_directories(${CMAKE_SOURCE_DIR}/external/ALEX/src/core)

# Find threads library
find_package(Threads REQUIRED)

# Main simulator executable
add_executable(simulator src/main.cpp)

# Validation executable
add_executable(validate src/validate.cpp)

# Link libraries
target_link_libraries(simulator PRIVATE Threads::Threads)
target_link_libraries(validate PRIVATE Threads::Threads)

# Enable warnings
target_compile_options(simulator PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(validate PRIVATE -Wall -Wextra -Wpedantic)

# Print configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS_RELEASE}")
