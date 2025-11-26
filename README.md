# Learned Index Benchmark Suite

**ELL405 Operating Systems Course Project**
**IIT Delhi, Fall 2025**

A comprehensive benchmarking framework comparing traditional and learned index structures, featuring **WT-HALI** (Write-Through Hierarchical Adaptive Learned Index) - a novel hybrid approach.

---

## Overview

This project implements and benchmarks **7 index structures** across multiple datasets and workloads to understand the trade-offs between traditional and learned indexes.

### Indexes Benchmarked

**Traditional Indexes (Baselines):**
- **B+Tree** - Cache-optimized ordered index (`phmap::btree_map`)
- **Hash Table** - Fast unordered index (`phmap::flat_hash_map`)
- **ART** - Adaptive Radix Tree for space-efficient storage

**Learned Indexes (State-of-the-Art):**
- **PGM-Index** - Piecewise Geometric Model (VLDB 2020)
- **RMI** - Recursive Model Index (SIGMOD 2018)

**Our Contribution:**
- **WT-HALI** - Write-Through Hierarchical Adaptive Learned Index
  - Hybrid architecture combining learned models with traditional structures
  - Binary search routing + adaptive experts + write-through buffer
  - Goal: Memory efficiency of learned indexes with better write performance

---

## Quick Start

### Prerequisites
- **Docker Desktop** (for reproducible builds)
- **Python 3.8+** with `matplotlib`, `numpy`, `pandas` (for visualization)
- **Windows 11** with WSL 2 (or Linux/macOS)

### Running the Benchmarks

```bash
# 1. Clone repository
git clone https://github.com/jdhruv1503/HALI.git
cd HALI

# 2. Build Docker image
docker build -t hali-benchmark .

# 3. Run container with fixed resources (ensures reproducibility)
docker run --name hali-benchmark -it --rm \
  --cpus="16" --memory="8g" \
  -v "$(pwd):/workspace" \
  hali-benchmark bash

# 4. Inside container: Build the project
cd /workspace
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# 5. Run full benchmark suite
# (7 indexes × 6 datasets × 3 workloads = 126 experiments)
./simulator

# 6. Run validation tests (verify correctness)
./validate

# 7. Exit container and generate visualizations on host
exit
python scripts/visualize_results.py
```

Results will be saved to:
- **CSV data:** `results/benchmark_results.csv`
- **Plots:** `results/plots/` (43 visualizations)

### Quick Benchmark Examples

```bash
# Benchmark specific index
./simulator --index=btree --dataset=clustered --size=500000

# Benchmark WT-HALI with custom parameters
./simulator --index=wthali --compression=0.25 --buffer=0.005 --dataset=all

# Benchmark only read-heavy workload
./simulator --workload=read_heavy --dataset=all

# Quick test (smaller dataset)
./simulator --size=100000 --operations=10000
```

---

## Key Results

**Configuration:** 500K keys, 100K operations, AMD Ryzen 9 7940HS @ 3.99GHz, Docker Ubuntu 22.04, GCC 11.4 (`-O3`)

### Read-Heavy Workload (95% lookups, 5% inserts) - Clustered Dataset

| Index | Lookup (ns) | Insert (M/s) | Memory (B/key) | P99 Latency (ns) |
|-------|-------------|--------------|----------------|------------------|
| **B+Tree** | **22.7** | 9.4 | 19.2 | 31 |
| **Hash** | 183.3 | 10.4 | 41.8 | 621 |
| **ART** | 393.9 | 6.4 | 20.0 | 1182 |
| **PGM** | 155.7 | 2.7 | **16.0** | 461 |
| **RMI** | **73.1** | 3.1 | **16.0** | 230 |
| **WT-HALI** | 635.7 | 1.0 | 16.7 | 1654 |

**Key Findings:**
- Traditional indexes (B+Tree, Hash) achieve **fastest lookups** (22-183ns)
- Learned indexes (PGM, RMI) achieve **best memory efficiency** (16 bytes/key)
- WT-HALI maintains memory efficiency (16.7 B/key) but slower lookups (636ns)

### Write-Heavy Workload (10% lookups, 90% inserts) - Clustered Dataset

| Index | Lookup (ns) | Insert (M/s) | Memory (B/key) |
|-------|-------------|--------------|----------------|
| **B+Tree** | 23.8 | **17.4** | 19.2 |
| **Hash** | 158.3 | **13.9** | 41.8 |
| **ART** | 678.0 | **13.7** | 20.0 |
| **PGM** | 431.5 | 0.18 | 16.0 |
| **RMI** | 411.9 | 0.19 | 16.0 |
| **WT-HALI** | 744.5 | 1.0 | 16.7 |

**Critical Finding:**
- **Traditional indexes dominate writes:** 13-17M inserts/sec
- **Learned indexes struggle:** PGM/RMI only ~180K inserts/sec (**100x slower!**)
- **WT-HALI improves over pure learned:** 1M inserts/sec (5x better than PGM/RMI)
- **But still lags traditional:** 17x slower than B+Tree

### Overall Assessment

✅ **What Works:**
- B+Tree: Best all-around performance (22ns lookups, 17M inserts/sec)
- Hash: Fast lookups, excellent write performance
- PGM/RMI: Best memory efficiency for **read-only** workloads

❌ **What Doesn't Work:**
- Learned indexes (PGM/RMI) for any write-heavy workload
- WT-HALI for production use (still 17x slower writes than B+Tree)

🎓 **Academic Contribution:**
- WT-HALI demonstrates hybrid approach is possible
- Write-through buffer improves learned index writes by 5x
- But fundamental gap remains between learned and traditional indexes

---

## Datasets

Six synthetic distributions (500K keys each):

1. **Clustered** - 5 normal distributions with gaps (models fragmented data)
2. **Lognormal** - μ=10, σ=2 (models file size distributions)
3. **Sequential** - Monotonic with periodic gaps (models timestamps)
4. **Uniform** - Pure random (worst-case for learned indexes)
5. **Mixed** - 40% uniform + 40% normal + 20% exponential
6. **Zipfian** - Power-law α=1.5 (models access frequency)

## Workloads

1. **Read-Heavy:** 95% find, 5% insert (OLAP-style analytics)
2. **Write-Heavy:** 10% find, 90% insert (log ingestion)
3. **Mixed:** 50% find, 50% insert (OLTP-style transactions)

---

## WT-HALI Architecture

### Design Philosophy

**Problem:** Pure learned indexes suffer from expensive model retraining on writes
**Solution:** Decouple writes from learned structure using write-through architecture

### Three-Level Design

```
┌─────────────────────────────────────┐
│  Level 3: Write-Through Buffer      │  ← All inserts go here (ART/HashMap)
│  • 10M+ inserts/sec                 │
│  • No retraining needed             │
└─────────────────────────────────────┘
              ↓ Lookups check buffer first
┌─────────────────────────────────────┐
│  Level 1: Binary Search Router      │  ← Guaranteed O(log m) routing
│  • Disjoint key ranges              │
│  • 100% accuracy                    │
└─────────────────────────────────────┘
              ↓ Routes to correct expert
┌─────────────────────────────────────┐
│  Level 2: Adaptive Experts          │  ← PGM/RMI/ART based on data
│  • PGM: R² > 0.95 (linear data)     │
│  • RMI: R² > 0.80 (complex data)    │
│  • ART: R² ≤ 0.80 (random data)     │
└─────────────────────────────────────┘
```

### Performance

- **Lookup:** 636ns (28x slower than B+Tree)
- **Insert:** 1M ops/sec (17x slower than B+Tree, 5x better than PGM/RMI)
- **Memory:** 16.7 bytes/key (13% better than B+Tree, similar to PGM/RMI)

### When to Use WT-HALI

✅ **Consider if:**
- Memory is extremely constrained (every byte matters)
- Workload is read-mostly with occasional writes
- 600-700ns lookup latency is acceptable

❌ **Do NOT use if:**
- Need high-performance OLTP (use B+Tree or Hash instead)
- Require >10M inserts/sec (use traditional indexes)
- Latency-critical application (<100ns lookups needed)

---

## Project Structure

```
HALI/
├── include/
│   ├── index_interface.h         # Abstract base class
│   ├── timing_utils.h             # High-resolution timers
│   ├── data_generator.h           # Synthetic datasets
│   ├── workload_generator.h       # Workload generators
│   └── indexes/
│       ├── btree_index.h          # B+Tree
│       ├── hash_index.h           # Hash Table
│       ├── art_index.h            # Adaptive Radix Tree
│       ├── pgm_index.h            # PGM-Index
│       ├── rmi_index.h            # RMI (2-layer, 100 experts)
│       └── haliv2_index.h         # WT-HALI (our contribution)
├── src/
│   ├── main.cpp                   # Benchmark harness
│   └── validate.cpp               # Correctness validation
├── external/
│   └── libs/                      # Git submodules (header-only)
│       ├── parallel-hashmap/      # B+Tree and Hash
│       ├── art_map/               # ART
│       └── PGM-index/             # PGM
├── scripts/
│   ├── build.sh                   # Build script
│   ├── run_docker.sh              # Docker launcher
│   └── visualize_results.py       # Generate plots
├── results/
│   ├── benchmark_results.csv      # Raw data
│   └── plots/                     # Visualizations
├── report/
│   └── presentation.tex           # LaTeX Beamer (20+ slides)
├── CMakeLists.txt                 # Build configuration
├── Dockerfile                     # Reproducible environment
└── README.md                      # This file
```

---

## Validation

All indexes pass correctness tests on 3 datasets:

```bash
./validate
```

**Expected output:**
```
Testing with Clustered data:
Validating BTree... PASS (verified 5000 keys)
Validating HashTable... PASS (verified 5000 keys)
Validating ART... PASS (verified 5000 keys)
Validating PGM-Index... PASS (verified 5000 keys)
Validating RMI... PASS (verified 5000 keys)
Validating WT-HALI... PASS (verified 5000 keys)

✓ ALL VALIDATION TESTS PASSED
```

---

## Build Configuration

**CMake flags:**
- C++17 standard
- Release mode: `-O3 -march=native -DNDEBUG`
- Compiler: GCC 11.4.0+

**Docker configuration:**
- Base: Ubuntu 22.04 LTS
- Allocated: 16 CPU cores, 8GB RAM (configurable)

**Dependencies:** All header-only libraries via git submodules

---

## Customization

### Tuning WT-HALI

```bash
# Compression level: 0.0 (speed) to 1.0 (memory)
# Buffer size: 0.001 (0.1%) to 0.1 (10%)
./simulator --index=wthali --compression=0.25 --buffer=0.005
```

### Adding Custom Datasets

Edit `src/main.cpp`:
```cpp
auto my_data = DataGenerator::generate_lognormal(1000000, 12.0, 3.0);
datasets["MyDataset"] = my_data;
```

---

## Limitations & Future Work

### Current Limitations

1. **WT-HALI write performance still lags:** 1M vs 17M inserts/sec (17x slower than B+Tree)
2. **Memory advantage is minimal:** Only 13% better than B+Tree (16.7 vs 19.2 bytes/key)
3. **Lookup latency trade-off:** 636ns vs 23ns for B+Tree (28x slower)
4. **Single-threaded only:** No concurrent access support
5. **No range queries:** Point lookups and inserts only
6. **Synthetic data only:** Not tested on real-world datasets (SOSD benchmark pending)

### Future Work

**Immediate:**
- Test on SOSD real-world datasets (books, fb, osmc, wiki)
- Scale to larger datasets (1M-10M keys)

**Long-term:**
- Concurrent/lock-free WT-HALI
- Range query support
- Optimize write-through buffer strategies
- Hardware-specific optimizations (SIMD, GPU)

---

## References

1. **RMI:** Kraska et al., "The Case for Learned Index Structures," SIGMOD 2018
2. **PGM-Index:** Ferragina & Vinciguerra, "The PGM-index," VLDB 2020
3. **ART:** Leis et al., "The Adaptive Radix Tree," ICDE 2013
4. **SOSD:** Marcus et al., "Benchmarking Learned Indexes," VLDB 2020

---