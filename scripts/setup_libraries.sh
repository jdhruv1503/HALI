#!/bin/bash
# Script to download and setup header-only libraries for HALI project

set -e

LIBS_DIR="external/libs"
mkdir -p $LIBS_DIR

echo "==> Downloading header-only libraries..."

# 1. Parallel Hashmap (phmap) - for B+Tree and Hash Table
echo "Downloading parallel-hashmap..."
cd $LIBS_DIR
if [ ! -d "parallel-hashmap" ]; then
    git clone --depth 1 https://github.com/greg7mdp/parallel-hashmap.git
fi

# 2. ART Map - Adaptive Radix Tree
echo "Downloading art_map..."
if [ ! -d "art_map" ]; then
    git clone --depth 1 https://github.com/justinasvd/art_map.git
fi

# 3. PGM-Index
echo "Downloading PGM-index..."
if [ ! -d "PGM-index" ]; then
    git clone --depth 1 https://github.com/gvinciguerra/PGM-index.git
fi

# 4. RMI Tool (for code generation)
echo "Downloading RMI tool..."
if [ ! -d "RMI" ]; then
    git clone --depth 1 https://github.com/learnedsystems/RMI.git
fi

echo "==> All libraries downloaded successfully!"
echo "Library locations:"
echo "  - phmap: $LIBS_DIR/parallel-hashmap/parallel_hashmap/"
echo "  - art_map: $LIBS_DIR/art_map/"
echo "  - PGM-index: $LIBS_DIR/PGM-index/include/"
echo "  - RMI: $LIBS_DIR/RMI/"
