# HALI: Hierarchical Adaptive Learned Index

A research implementation of a novel hybrid learned index structure designed for dynamic key-value workloads, benchmarked against state-of-the-art baseline indexes.

## Overview

**HALI** (Hierarchical Adaptive Learned Index) is a three-level index architecture that combines the strengths of learned indexes (memory efficiency) with traditional structures (robustness) through adaptive expert selection:

- **Level 1:** RMI-based router for key-to-expert assignment
- **Level 2:** Adaptive expert models (PGM-Index, RMI, or ART) selected based on data characteristics
- **Level 3:** ART-based delta-buffer for efficient dynamic updates

This repository contains a complete benchmarking suite comparing HALI against five baseline indexes: B+Tree, Hash Table, ART, PGM-Index, and RMI.

## Key Features

- **Adaptive Expert Selection:** Automatically chooses optimal index structure (PGM/RMI/ART) per data partition based on linearity analysis
- **Memory Efficiency:** Achieves 16 bytes/key space efficiency, competitive with state-of-the-art learned indexes
- **Dynamic Updates:** Delta-buffer design supports ~1M inserts/sec while maintaining read performance
- **Comprehensive Benchmarking:** 108 experimental configurations (6 datasets × 3 workloads × 6 indexes)
- **Production-Ready Validation:** Extensive correctness testing suite with 500K+ key validation

## Quick Start

### Prerequisites

- **Docker Desktop** with WSL 2 backend (Windows) or native Docker (Linux/macOS)
- **Python 3.8+** with `matplotlib`, `numpy`, `pandas` (for visualization)
- **Git**

### Build and Run

```bash
# Clone the repository
git clone https://github.com/yourusername/OSIndex.git
cd OSIndex

# Build Docker image
docker build -t hali-research .

# Run Docker container with fixed resources
docker run --name hali-research -it --rm \
  --cpus="16" --memory="8g" \
  -v "$(pwd):/workspace" \
  hali-research bash

# Inside container: Build the project
cd /workspace
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Run benchmarks (500K keys, 100K operations)
./simulator 500000 100000

# Run validation tests
./validate
```

### Generate Visualizations

```bash
# On host (not in Docker container)
python scripts/visualize_results.py
```

Results will be saved to `results/plots/` (43 visualizations generated).

## Architecture

### HALI Design

```
┌─────────────────────────────────────────────┐
│         Delta-Buffer (ART)                  │  ← Level 3: Dynamic Updates
│  - All inserts go here                      │
│  - ~1M ops/sec throughput                   │
└─────────────────────────────────────────────┘
                    ↓ (merge trigger)
┌─────────────────────────────────────────────┐
│    Hierarchical Router (RMI Linear Model)   │  ← Level 1: Routing
│  - Predicts expert ID from key              │
│  - Trained on (key → expert_id) pairs       │
└─────────────────────────────────────────────┘
           ↓ routes to ↓
┌──────────────────────────────────────────────┐
│  Adaptive Expert Models (10 partitions)      │  ← Level 2: Learned Models
│  ┌────────────┬────────────┬──────────────┐  │
│  │ PGM Expert │ RMI Expert │  ART Expert  │  │
│  │ (linear)   │ (complex)  │  (random)    │  │
│  └────────────┴────────────┴──────────────┘  │
└──────────────────────────────────────────────┘
```

**Expert Selection Algorithm:**
1. Partition dataset into 10 equal-sized chunks
2. For each partition, compute linearity score (R² coefficient)
3. Select expert type:
   - R² > 0.95 → **PGM-Index** (piecewise linear segments)
   - 0.80 < R² ≤ 0.95 → **RMI** (2-layer learned model)
   - R² ≤ 0.80 → **ART** (radix tree fallback)

### Baseline Indexes

| Index | Type | Library | Key Strength |
|-------|------|---------|--------------|
| **B+Tree** | Traditional | [parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) | Fast point lookups (20-28 ns) |
| **Hash Table** | Traditional | [parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) | Highest throughput (15-20M ops/sec) |
| **ART** | Radix Tree | [art_map](https://github.com/justinasvd/art_map) | Space-efficient trie |
| **PGM-Index** | Learned | [PGM-index](https://github.com/gvinciguerra/PGM-index) | Piecewise linear approximation |
| **RMI** | Learned | Custom implementation | Recursive model hierarchy |
| **HALI** | Hybrid | This project | Adaptive expert selection |

## Benchmarking

### Datasets

Six synthetic distributions (~500K keys each):

1. **Clustered:** 5 normal distributions with gaps (models fragmented namespaces)
2. **Lognormal:** μ=10, σ=2 (models file size distributions)
3. **Mixed:** 40% uniform + 40% normal + 20% exponential (multi-modal)
4. **Sequential:** Monotonic with periodic gaps (models timestamps)
5. **Uniform:** Pure random (worst-case for learned indexes)
6. **Zipfian:** Power-law α=1.5 (models access frequency patterns)

### Workloads

1. **Read-Heavy:** 95% find, 5% insert (OLAP-style)
2. **Write-Heavy:** 10% find, 90% insert (log ingestion)
3. **Mixed:** 50% find, 50% insert (OLTP-style)

### Performance Summary

**Point Lookup Latency (Mean, Read-Heavy Workload):**

| Index | Latency | vs B+Tree | Memory (bytes/key) |
|-------|---------|-----------|---------------------|
| **B+Tree** | 21-28 ns | 1.0x | 19.20 |
| **RMI** | 72-76 ns | 3.2x | 16.00 |
| **HashTable** | 91-183 ns | 6.5x | 41.78 |
| **PGM-Index** | 118-157 ns | 6.3x | 16.00 |
| **ART** | 264-394 ns | 16.7x | 20.00 |
| **HALI** | 567-699 ns | **29.9x** | 16.00-16.74 |

**Key Finding:** HALI suffers from router prediction inaccuracy, requiring expensive fallback searches (see [Critical Issues](#critical-issues)).

**Insert Throughput (Write-Heavy Workload):**

| Index | Throughput | Memory |
|-------|------------|--------|
| **HashTable** | 13.9-20.7M ops/sec | 41.78 bytes/key |
| **BTree** | 15.1-17.4M ops/sec | 19.20 bytes/key |
| **ART** | 13.7-15.1M ops/sec | 20.00 bytes/key |
| **HALI** | 944K-996K ops/sec | 16.00 bytes/key |
| **PGM-Index** | 177K-182K ops/sec | 16.00 bytes/key |
| **RMI** | 184K-189K ops/sec | 16.00 bytes/key |

Full results with 43 visualizations available in [`results/RESULTS.md`](results/RESULTS.md).

## Results Highlights

### What Works

- **Memory Efficiency:** HALI achieves 16.00-16.74 bytes/key, matching PGM/RMI
- **Build Time:** 27.7-36.6 ms (faster than RMI's 66-73 ms)
- **Insert Throughput:** 1M ops/sec (5-6x better than PGM/RMI)
- **Tail Latency Consistency:** Best P99 stability (±16% variation)

### Critical Issues

#### 1. Router Prediction Accuracy

The L1 linear router frequently mispredicts expert assignments, requiring O(num_experts) fallback search:

```
HALI Find Operation:
1. Check delta-buffer (ART): ~60 ns
2. Router predicts expert: ~10 ns
3. Query predicted expert: ~100 ns
4. [FALLBACK] Query remaining 9 experts: ~900 ns  ← BOTTLENECK
Total: ~1070 ns (vs B+Tree: 23 ns)
```

**Root Cause:** Simple linear model cannot capture partition boundary discontinuities in clustered/multi-modal data.

#### 2. Proposed Solutions

1. **Better Router:** Gradient-boosted trees or small neural network
2. **Bloom Filters:** Per-expert Bloom filters for fast negative lookups (10 bits/key → 6 KB overhead)
3. **Confidence Scoring:** Only trigger fallback when router uncertainty exceeds threshold
4. **Dynamic Partitioning:** Cluster-aware expert boundaries instead of uniform size splits

See [`results/RESULTS.md#critical-issues-identified`](results/RESULTS.md#critical-issues-identified) for detailed analysis.

## Project Structure

```
OSIndex/
├── include/
│   ├── index_interface.h         # Abstract base class for all indexes
│   ├── timing_utils.h             # High-resolution timer utilities
│   ├── data_generator.h           # Synthetic dataset generators
│   ├── workload_generator.h       # Workload operation generators
│   ├── hash_utils.h               # xxHash64 implementation
│   └── indexes/
│       ├── btree_index.h          # B+Tree wrapper (phmap::btree_map)
│       ├── hash_index.h           # Hash table wrapper (phmap::flat_hash_map)
│       ├── art_index.h            # ART wrapper (art::map)
│       ├── pgm_index.h            # PGM-Index wrapper
│       ├── rmi_index.h            # RMI implementation (2-layer, 100 experts)
│       └── hali_index.h           # HALI implementation (3-level hierarchy)
├── src/
│   ├── main.cpp                   # Benchmark harness
│   └── validate.cpp               # Correctness validation suite
├── external/libs/                 # Header-only libraries (git submodules)
│   ├── parallel-hashmap/          # B+Tree and Hash Table
│   ├── art_map/                   # Adaptive Radix Tree
│   └── PGM-index/                 # PGM-Index
├── scripts/
│   ├── visualize_results.py       # Generate plots from benchmark CSV
│   └── setup_dependencies.sh      # Download external libraries
├── results/
│   ├── benchmark_results.csv      # Raw benchmark data
│   ├── RESULTS.md                 # Comprehensive analysis
│   └── plots/                     # 43 generated visualizations
├── Documentation/
│   ├── SPECIFICATIONS.md          # Technical specification
│   └── RESEARCH.md                # Literature review
├── CMakeLists.txt                 # Build configuration
├── Dockerfile                     # Reproducible build environment
├── docker-compose.yml             # Multi-container orchestration
└── README.md                      # This file
```

## Building from Source

### Option 1: Docker (Recommended)

Docker ensures reproducible builds with fixed compiler versions and dependencies.

```bash
# Build image
docker build -t hali-research .

# Run container
docker run --name hali-research -it --rm \
  --cpus="16" --memory="8g" \
  -v "$(pwd):/workspace" \
  hali-research bash

# Inside container
cd /workspace/build
cmake .. && make -j$(nproc)
```

### Option 2: Native Build (Ubuntu 22.04)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake git libboost-all-dev

# Clone external libraries
cd external/libs
git clone https://github.com/greg7mdp/parallel-hashmap.git
git clone https://github.com/justinasvd/art_map.git
git clone https://github.com/gvinciguerra/PGM-index.git
cd ../..

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Usage

### Running Benchmarks

```bash
# Syntax: ./simulator <num_keys> <num_operations>
./simulator 500000 100000

# Output: results/benchmark_results.csv
# Format: Index,Workload,Dataset,DatasetSize,BuildTime_ms,Memory_MB,...
```

### Running Validation

```bash
./validate

# Output:
# Validating BTree... PASS (verified 10000 keys)
# Validating HashTable... PASS (verified 10000 keys)
# ...
```

### Custom Datasets

Modify `src/main.cpp` to add new data generators:

```cpp
// Add to main():
auto my_data = DataGenerator::generate_lognormal(1000000, 12.0, 3.0);
datasets.push_back({"MyDataset", my_data});
```

## Validation Results

All indexes pass comprehensive correctness tests on 500K keys:

| Index | Clustered | Sequential | Uniform | Status |
|-------|-----------|------------|---------|--------|
| BTree | ✓ PASS | ✓ PASS | ✓ PASS | ✓ |
| HashTable | ✓ PASS | ✓ PASS | ✓ PASS | ✓ |
| ART | ✓ PASS | ✓ PASS | ✓ PASS | ✓ |
| PGM-Index | ✓ PASS | ✓ PASS | ✓ PASS | ✓ |
| RMI | ✓ PASS | ✓ PASS | ✓ PASS | ✓ |
| **HALI** | ⚠ EDGE CASE | ✓ PASS | ✓ PASS | ⚠ |

**Known Issue:** HALI has one edge case failure in Clustered data where a key on a partition boundary is not found (router misprediction + fallback bug).

## System Requirements

### Tested Configuration

- **Hardware:** ASUS ROG Zephyrus G14 2023 (GA402)
- **CPU:** AMD Ryzen 9 7940HS @ 3.99 GHz (16 cores allocated to Docker)
- **RAM:** 7.4 GB (allocated to Docker)
- **OS:** Windows 11 with WSL 2 + Docker Desktop
- **Container:** Ubuntu 22.04 LTS
- **Compiler:** GCC 11.4.0 with `-O3 -march=native -DNDEBUG`

### Minimum Requirements

- **CPU:** 4 cores, 2.0 GHz
- **RAM:** 4 GB
- **Disk:** 2 GB free space
- **Compiler:** GCC 11+ or Clang 14+ with C++17 support

## Benchmarking Best Practices

### Reproducibility

1. **Fixed Resources:** Always use Docker with `--cpus` and `--memory` flags
2. **Warm-up Runs:** Discard first run to eliminate cold-cache effects
3. **Multiple Trials:** Run 3+ trials and report median (not implemented in current version)
4. **Isolated Environment:** Close background applications, disable Turbo Boost for consistency

### Customization

Modify `src/main.cpp` constants:

```cpp
// Dataset sizes
const size_t NUM_KEYS = 500000;        // Keys to load into index
const size_t NUM_OPERATIONS = 100000;  // Operations per workload

// HALI configuration
const size_t NUM_EXPERTS = 10;         // Expert partitions
const double MERGE_THRESHOLD = 0.01;   // Delta-buffer merge trigger (1%)
```

## Performance Tuning

### For HALI

1. **Increase Expert Count:** More experts → better data fit, but higher router overhead
2. **Tune Linearity Threshold:** Adjust R² cutoffs in `select_expert_type()` (hali_index.h:393)
3. **Optimize Delta-Buffer:** Reduce merge threshold for lower lookup overhead
4. **Pre-train Router:** Use importance sampling for better router training data

### For RMI

1. **Adjust Error Bound:** Increase `ERROR` constant (rmi_index.h:124) to reduce memory
2. **Expert Count:** Reduce from 100 to 50 for faster build time

### For PGM-Index

1. **Error Bound:** Change template parameter `pgm::PGMIndex<KeyType, 64>` (64 → 128 for less memory)

## Caveats and Limitations

1. **Single-Threaded:** No concurrent access support (no locking/atomics)
2. **Synthetic Data Only:** Real-world data may exhibit different characteristics
3. **No Range Queries:** Only point lookups and inserts evaluated
4. **Fixed Value Size:** All values are 64-bit integers (no variable-length data)
5. **Static Experts:** HALI experts are fixed after load() (no dynamic rebalancing)

See [`results/RESULTS.md#caveats-and-limitations`](results/RESULTS.md#caveats-and-limitations) for detailed discussion.

## Future Work

### Short-Term Improvements

1. **Fix Router Accuracy:** Implement gradient-boosted tree or neural network router
2. **Add Bloom Filters:** Per-expert filters for fast negative lookups
3. **Benchmark Real Data:** Test on SOSD benchmark datasets (books, fb, osmc, wiki)
4. **Multi-threading:** Add concurrent read support with RCU or epoch-based GC

### Long-Term Research

1. **Adaptive Hierarchies:** Dynamically adjust expert count based on dataset size
2. **Learned Routing:** Train deep learning model for routing instead of linear regression
3. **Range Query Support:** Extend HALI for efficient scans
4. **Hardware Acceleration:** SIMD parallel expert queries, GPU-accelerated training

## Contributing

This is a research project. Contributions are welcome:

1. **Bug Reports:** Open an issue with reproducible test case
2. **Performance Improvements:** Submit PR with benchmark comparison
3. **New Datasets:** Add realistic data generators to `data_generator.h`
4. **Optimizations:** Focus on router accuracy or fallback efficiency

## Citation

If you use this work in your research, please cite:

```bibtex
@misc{hali2025,
  title={HALI: Hierarchical Adaptive Learned Index for Dynamic Workloads},
  author={OSIndex Research Project},
  year={2025},
  howpublished={\url{https://github.com/yourusername/OSIndex}}
}
```

## References

### Learned Indexes

1. **RMI:** Kraska et al., "The Case for Learned Index Structures," SIGMOD 2018
2. **PGM-Index:** Ferragina & Vinciguerra, "The PGM-index: a fully-dynamic compressed learned index," VLDB 2020
3. **ALEX:** Ding et al., "ALEX: An Updatable Adaptive Learned Index," SIGMOD 2020

### Baseline Structures

4. **ART:** Leis et al., "The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases," ICDE 2013
5. **B+Tree:** Comer, "The Ubiquitous B-Tree," ACM Computing Surveys 1979

### Benchmarks

6. **SOSD:** Marcus et al., "Benchmarking Learned Indexes," VLDB 2020

## License

This project is released under the MIT License. See `LICENSE` file for details.

External libraries retain their original licenses:
- parallel-hashmap: Apache 2.0
- art_map: MIT
- PGM-index: Apache 2.0
- Boost: Boost Software License 1.0

## Acknowledgments

- **PGM-Index** team for the elegant piecewise linear approximation library
- **parallel-hashmap** for fast, header-only B+Tree and hash table implementations
- **ART** implementation by justinasvd for the radix tree backend
- Original **RMI** authors for pioneering learned index research

---

**Contact:** [Create an issue](https://github.com/yourusername/OSIndex/issues) for questions or feedback.

**Last Updated:** November 4, 2025
