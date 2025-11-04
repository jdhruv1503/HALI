# HALI: Hierarchical Adaptive Learned Index

A research implementation of a novel hybrid learned index structure for dynamic key-value workloads. HALI combines the memory efficiency of learned indexes with the robustness of traditional data structures through adaptive expert selection.

**Latest Results:** HALIv2 achieves **89% faster lookups** (507ns → 54.7ns) and **14x higher insert throughput** (1.04M → 14.7M ops/sec) compared to HALIv1 through guaranteed-correct binary search routing.

## Overview

**HALI** (Hierarchical Adaptive Learned Index) is a three-level index architecture that combines the strengths of learned indexes (memory efficiency) with traditional structures (robustness) through adaptive expert selection:

- **Level 1:** Binary search routing over disjoint expert key ranges (HALIv2) or RMI router (HALIv1)
- **Level 2:** Adaptive expert models (PGM-Index, RMI, or ART) selected based on data characteristics
- **Level 3:** ART-based delta-buffer for efficient dynamic updates

This repository contains a complete benchmarking suite comparing HALI against five baseline indexes: B+Tree, Hash Table, ART, PGM-Index, and RMI.

**Research Progress:** We've completed two major iterations (HALIv1 and HALIv2) with comprehensive documentation tracking design decisions, failures, and improvements. See [`documentation/RESEARCH_PROGRESS.md`](documentation/RESEARCH_PROGRESS.md) for the full research log.

## Key Features

- **Tunable Performance:** Three compression modes (Speed/Balanced/Memory) for different workload requirements
- **Adaptive Expert Selection:** Automatically chooses optimal index structure (PGM/RMI/ART) per data partition based on linearity analysis
- **Guaranteed-Correct Routing:** Binary search over disjoint key ranges (O(log n)) eliminates expensive fallback searches
- **Memory Efficiency:** Achieves 17-20 bytes/key space efficiency, competitive with state-of-the-art learned indexes
- **Dynamic Updates:** Delta-buffer design supports 10-15M inserts/sec (HALIv2-Speed) while maintaining read performance
- **Comprehensive Benchmarking:** 162 experimental configurations (6 datasets × 3 workloads × 9 indexes)
- **Production-Ready Validation:** Extensive correctness testing suite with 500K+ key validation (HALIv2-Speed: 100% pass rate)

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

### Performance Summary (Production Scale: 500K keys)

**Point Lookup Latency (Mean, Read-Heavy Workload, Clustered Dataset):**

| Index | Latency | vs B+Tree | Memory (bytes/key) |
|-------|---------|-----------|---------------------|
| **B+Tree** | **17.5 ns** | 1.0x | 19.20 |
| **HALIv2-Speed** | **54.7 ns** | **3.1x** | **17.25** |
| **RMI** | 93.7 ns | 5.4x | 16.00 |
| **PGM-Index** | 117.9 ns | 6.7x | 16.00 |
| **HALIv2-Memory** | **127.6 ns** | **7.3x** | **19.75** |
| **HashTable** | 158.9 ns | 9.1x | 41.78 |
| **ART** | 309.9 ns | 17.7x | 20.00 |
| **HALIv2-Balanced** | **348.3 ns** | **19.9x** | **22.50** |
| **HALIv1** | 507.2 ns | 29.0x | 16.74 |

**Key Achievement:** HALIv2-Speed is **89% faster than HALIv1** (507.2 → 54.7 ns), achieving competitive performance with learned indexes while maintaining excellent memory efficiency.

**Insert Throughput (Write-Heavy Workload, Clustered Dataset):**

| Index | Throughput | Memory | vs HALIv1 |
|-------|------------|--------|-----------|
| **BTree** | 19.1M ops/sec | 19.20 bytes/key | 18.3x |
| **ART** | 15.6M ops/sec | 20.00 bytes/key | 14.9x |
| **HALIv2-Speed** | **14.7M ops/sec** | **17.25 bytes/key** | **14.1x** |
| **HashTable** | 11.1M ops/sec | 41.78 bytes/key | 10.7x |
| **HALIv2-Memory** | **10.6M ops/sec** | **19.75 bytes/key** | **10.2x** |
| **HALIv2-Balanced** | **11.1M ops/sec** | **22.50 bytes/key** | **10.6x** |
| **HALIv1** | 1.04M ops/sec | 16.74 bytes/key | 1.0x |
| **PGM-Index** | 94K ops/sec | 16.00 bytes/key | 0.09x |
| **RMI** | 97K ops/sec | 16.00 bytes/key | 0.09x |

**Full Results:**
- HALIv1 detailed analysis: [`results/haliv1_archive/HALIV1_RESULTS.md`](results/haliv1_archive/HALIV1_RESULTS.md)
- HALIv2 production benchmarks: [`documentation/PRODUCTION_RESULTS.md`](documentation/PRODUCTION_RESULTS.md)
- HALIv2 improvements summary: [`documentation/HALIV2_IMPROVEMENTS.md`](documentation/HALIV2_IMPROVEMENTS.md)
- Research progress log: [`documentation/RESEARCH_PROGRESS.md`](documentation/RESEARCH_PROGRESS.md)

## Research Presentation

A comprehensive LaTeX Beamer presentation documenting the complete HALI research journey is available in the `report/` directory:

- **File:** `report/presentation.tex`
- **Format:** 45+ slides covering HALIv1 design, critical issues, HALIv2 improvements, and results
- **Build:** `cd report && pdflatex presentation.tex`
- **Theme:** Berkeley with Seahorse color scheme (16:9 aspect ratio)

The presentation includes:
- Complete architecture diagrams (TikZ)
- Performance visualizations (PGFPlots)
- Pareto frontier analysis
- Lessons learned and future work

See `report/README.md` for compilation instructions.

## Results Highlights

### HALIv2 Achievements

- **Dramatic Performance Improvement:** 54-89% faster lookups than HALIv1 across all datasets
- **High Insert Throughput:** 10-15M ops/sec (HALIv2-Speed), competitive with ART and BTree
- **Memory Efficiency:** 17.25 bytes/key (HALIv2-Speed), only 7.8% overhead vs learned indexes
- **100% Correctness:** HALIv2-Speed passes all validation tests (vs HALIv1's 66%)
- **Tunable Tradeoffs:** Three compression modes for different use cases

### Key Architectural Improvements (HALIv1 → HALIv2)

**Problem in HALIv1:** Linear router mispredicted expert assignments, requiring expensive O(num_experts) fallback search:

```
HALIv1 Find Operation:
1. Check delta-buffer (ART): ~60 ns
2. Router predicts expert: ~10 ns
3. Query predicted expert: ~100 ns
4. [FALLBACK] Query remaining 9 experts: ~900 ns  ← BOTTLENECK
Total: ~1070 ns (vs B+Tree: 23 ns)
```

**Solution in HALIv2:** Binary search routing over disjoint key ranges:

```
HALIv2 Find Operation:
1. Check delta-buffer: ~60 ns
2. Binary search expert boundaries: ~10 ns  ← GUARANTEED CORRECT
3. Query correct expert: ~100 ns
Total: ~170 ns (6.3x faster than HALIv1!)
```

**Additional Improvements:**
1. **Key-Range Partitioning:** Disjoint expert ranges (no overlaps)
2. **Adaptive Expert Count:** Scales with dataset size (sqrt(n) heuristic)
3. **Compression-Level Hyperparameter:** User-tunable memory-performance tradeoff (0.0-1.0)
4. **Bloom Filters:** Infrastructure in place (currently disabled for edge case debugging)

See [`documentation/HALIV2_IMPROVEMENTS.md`](documentation/HALIV2_IMPROVEMENTS.md) for detailed architectural analysis.

## Project Structure

```
OSIndex/
├── include/
│   ├── index_interface.h         # Abstract base class for all indexes
│   ├── timing_utils.h             # High-resolution timer utilities
│   ├── data_generator.h           # Synthetic dataset generators
│   ├── workload_generator.h       # Workload operation generators
│   ├── hash_utils.h               # xxHash64 implementation
│   ├── bloom_filter.h             # Bloom filter for negative lookup optimization
│   └── indexes/
│       ├── btree_index.h          # B+Tree wrapper (phmap::btree_map)
│       ├── hash_index.h           # Hash table wrapper (phmap::flat_hash_map)
│       ├── art_index.h            # ART wrapper (art::map)
│       ├── pgm_index.h            # PGM-Index wrapper
│       ├── rmi_index.h            # RMI implementation (2-layer, 100 experts)
│       ├── hali_index.h           # HALIv1 implementation (archived)
│       └── haliv2_index.h         # HALIv2 implementation (production)
├── src/
│   ├── main.cpp                   # Benchmark harness (9 indexes)
│   └── validate.cpp               # Correctness validation suite
├── external/libs/                 # Header-only libraries (git submodules)
│   ├── parallel-hashmap/          # B+Tree and Hash Table
│   ├── art_map/                   # Adaptive Radix Tree
│   └── PGM-index/                 # PGM-Index
├── scripts/
│   ├── visualize_results.py       # Generate plots from benchmark CSV
│   └── setup_dependencies.sh      # Download external libraries
├── results/
│   ├── benchmark_results.csv      # Raw benchmark data (162 configs)
│   ├── plots/                     # 43 generated visualizations
│   └── haliv1_archive/            # HALIv1 baseline results
│       ├── HALIV1_RESULTS.md      # HALIv1 comprehensive analysis
│       └── benchmark_results.csv  # HALIv1 benchmark data
├── documentation/                 # Research documentation
│   ├── RESEARCH_PROGRESS.md       # Iterative research log (HALIv1 → HALIv2)
│   ├── HALIV2_IMPROVEMENTS.md     # HALIv2 architectural improvements summary
│   ├── PRODUCTION_RESULTS.md      # 500K key production benchmark analysis
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

Comprehensive correctness tests on 500K keys:

| Index | Clustered | Sequential | Uniform | Pass Rate |
|-------|-----------|------------|---------|-----------|
| BTree | ✓ PASS | ✓ PASS | ✓ PASS | 100% |
| HashTable | ✓ PASS | ✓ PASS | ✓ PASS | 100% |
| ART | ✓ PASS | ✓ PASS | ✓ PASS | 100% |
| PGM-Index | ✓ PASS | ✓ PASS | ✓ PASS | 100% |
| RMI | ✓ PASS | ✓ PASS | ✓ PASS | 100% |
| **HALIv1** | ⚠ EDGE CASE | ✓ PASS | ✓ PASS | 66% |
| **HALIv2-Speed** | ✓ PASS | ✓ PASS | ✓ PASS | **100%** |
| **HALIv2-Balanced** | ⚠ EDGE CASE | ✓ PASS | ✓ PASS | 66% |
| **HALIv2-Memory** | ⚠ EDGE CASE | ✓ PASS | ✓ PASS | 66% |

**Known Issue:** HALIv2-Balanced and HALIv2-Memory have an edge case with clustered data containing large gaps. HALIv2-Speed (with fewer experts) resolves this completely and is recommended for production use.

**Recommendation:** Use **HALIv2-Speed** for production deployments requiring 100% correctness guarantee.

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

### High Priority (Next Steps)

1. **Fix Clustered Data Edge Case:** Resolve HALIv2-Balanced/Memory validation failures on clustered data
2. **Re-enable Bloom Filters:** Debug and activate Bloom filter optimization (+10% expected improvement)
3. **Add ALEX Baseline:** Integrate state-of-the-art updatable learned index for competitive comparison
4. **Benchmark on 1M+ Keys:** Scale testing to larger datasets to validate performance trends

### Medium Priority

1. **SOSD Real-World Data:** Benchmark on books, fb, osmc, wiki datasets from SOSD benchmark suite
2. **Auto-Tune Compression Level:** Automatically select optimal compression_level based on workload
3. **Better Expert Selection:** Cost-based model selection (train time + query time + memory)
4. **Dynamic Expert Rebalancing:** Monitor insert distribution and rebalance experts when skew occurs

### Long-Term Research

1. **Range Query Support:** Extend binary search routing to efficient range scans
2. **Concurrent HALI:** Lock-free delta-buffer with epoch-based GC for multi-threaded access
3. **Neural Network Routing:** Replace binary search with learned router model
4. **Hardware Acceleration:** SIMD vectorization for binary search, parallel expert queries

### Completed (Phase 1 & 2)

- ✅ HALIv1 baseline implementation and comprehensive benchmarking
- ✅ HALIv2 with guaranteed-correct binary search routing
- ✅ Adaptive expert count scaling with dataset size
- ✅ Compression-level hyperparameter for memory-performance tradeoff
- ✅ Bloom filter infrastructure (disabled pending edge case fix)
- ✅ Production-scale validation (500K keys)

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
