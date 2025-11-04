#!/bin/bash
# Build script for HALI simulator

set -e

echo "==> Building HALI Simulator..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build with all available cores
make -j$(nproc)

echo "==> Build complete! Executable: build/simulator"
