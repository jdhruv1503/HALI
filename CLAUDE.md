# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a research project implementing the **Hierarchical Adaptive Learned Index (HALI)** - a novel hybrid learned index structure designed for dynamic file system metadata workloads. The project consists of:

1. A C++17 simulator with pluggable index backends
2. Multiple baseline index implementations (B+Tree, Hash Table, ART, PGM-Index, RMI)
3. The novel HALI implementation
4. Comprehensive benchmarking infrastructure
5. A research paper documenting the findings

## Development Environment

### Docker-based Build Environment

All compilation and benchmarking MUST be conducted within Docker on WSL 2:

- **Host OS:** Windows with WSL 2
- **Container OS:** Ubuntu 22.04 LTS
- **Compiler:** GCC 11.4.0+ with `-O3` optimization
- **Resource Allocation:** Fixed CPU cores and RAM for reproducibility

```bash
# Launch container with fixed resources
docker run --cpus="4" --memory="16g" your_image_name
```

### Required Libraries (Header-Only)

All dependencies are header-only for ease of integration:

| Component | Library | Repository |
|-----------|---------|------------|
| B+Tree | `phmap::btree_map` | [greg7mdp/parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) |
| Hash Table | `phmap::flat_hash_map` | [greg7mdp/parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) |
| Adaptive Radix Tree | `art_map` | [justinasvd/art_map](https://github.com/justinasvd/art_map) |
| PGM-Index | `pgm::PGMIndex` | [gvinciguerra/PGM-index](https://github.com/gvinciguerra/PGM-index) |
| RMI | RMI Codegen Tool | [learnedsystems/RMI](https://github.com/learnedsystems/RMI) |

## Architecture

### Simulator Design

The simulator uses a **pluggable backend architecture** with a standardized interface:

- Abstract base class with `std::map`-like API: `insert(key, value)`, `find(key)`, `erase(key)`, `load(dataset)`
- Each index wrapped in a class implementing this common interface
- Benchmarking loop uses interface for fair comparison
- Timing: `std::chrono::high_resolution_clock` for nanosecond-level measurements

### HALI Architecture (Three Levels)

1. **Level 1: RMI Router**
   - Small, static 2-layer RMI trained on dataset sample
   - Routes keys to appropriate L2 expert by partition index

2. **Level 2: Adaptive Expert Models**
   - **PGM-Index Expert:** For nearly linear data partitions
   - **RMI Expert:** For complex but learnable non-linear data
   - **ART Fallback:** For unlearnable/random data (robustness guarantee)

3. **Dynamic Update Layer: ART Delta-Buffer**
   - All inserts go directly to global `art_map` delta-buffer
   - Lookups query static structure first, then delta-buffer
   - Merge triggered when buffer exceeds threshold (e.g., 1% of main index)
   - Background retraining after merge

4. **Optimization: Adaptive Last-Mile Search**
   - Small error (`< 64`): Branchless linear scan
   - Large error: Binary search

## Datasets

### Primary: SOSD Benchmark Suite

Using industry-standard datasets from the original RMI and ALEX papers:

- **Source:** [SOSD: A Benchmark for Learned Indexes](https://learned.systems/sosd)
- **Key Datasets:**
  - `books` - 200M Amazon book popularity (high variance)
  - `fb` - 200M Facebook user IDs (clustered)
  - `osmc` - 200M OSM Cell IDs (geospatial data)
  - `wiki` - 200M Wikipedia edit timestamps (temporal)
- **Format:** 64-bit unsigned integers, pre-sorted for static evaluation

### Synthetic Data Generators

Robust variety of realistic distributions for comprehensive testing:

1. **Lognormal Distribution:** Models file sizes, network traffic
2. **Zipfian Distribution:** Models access patterns, popularity
3. **Clustered Data:** Multiple normal distributions with gaps
4. **Sequential with Gaps:** Monotonic with periodic discontinuities
5. **Mixed Workload:** Combination of uniform, normal, and exponential

## Workloads

Three workload types matching RMI/ALEX benchmarks:

1. **Read-Heavy (95% find, 5% insert):** Static workload with occasional updates
2. **Write-Heavy (90% insert, 10% find):** Dynamic ingestion with lookups
3. **Mixed (50% find, 50% insert):** Balanced read-write workload

## Performance Evaluation

### Metrics to Collect

- **Point Lookup:** Mean latency (ns/op), P95 & P99 tail latency
- **Update Performance:** Insertion throughput (ops/sec)
- **Memory Footprint:** Total size (MB), space efficiency (bytes/key)
- **Build Cost:** Wall-clock time including model training

### Micro-architectural Analysis

Use Linux `perf` inside Docker container:

```bash
perf stat -e cycles,instructions,cache-misses,branch-misses ./simulator --index=BTree --workload=E1
```

Measure:
- Cache misses (L1, L2, LLC)
- Branch mispredictions
- Instructions Per Cycle (IPC)

### Visualization Requirements

1. **Pareto Frontier Plots:** Memory footprint vs. mean lookup latency
2. **Latency CDF Plots:** Full latency distribution for dynamic workloads
3. **Performance Tables:** Experimental results and micro-architectural metrics

## Code Quality Standards

- **Single-threaded:** Current implementation is single-threaded
- **Performance Critical:** All code must be cache-aware and minimize pointer chasing
- **Fair Comparison:** All indexes must use identical benchmarking harness
- **Nanosecond Precision:** Use high-resolution timers for all measurements

## Key Implementation Principles

1. **Cache Consciousness:** Design for CPU cache hierarchy (L1/L2/LLC)
2. **Minimize Memory Indirection:** Avoid pointer chasing; prefer contiguous arrays
3. **SIMD When Possible:** Leverage vectorization for parallel operations
4. **Adaptive Design:** Different structures for different data characteristics
5. **Robustness First:** HALI must never perform worse than ART fallback

## RMI Code Generation

RMI uses a code generator workflow:
1. Prepare sorted binary file of keys
2. Run RMI tool with model config (e.g., `linear,linear_spline 100000`)
3. Tool generates optimized `.h` and `.cpp` files
4. Simulator links against generated code

## Performance Expectations

Based on architectural analysis:

- **B+Tree:** High LLC misses (~12/lookup), pointer chasing, lower IPC (~0.85)
- **Hash Table:** Fastest point lookups (O(1) average), unordered
- **ART:** Best for string keys (O(k)), low cache misses (~4/lookup)
- **RMI/PGM:** Very low cache misses (~2-3/lookup), high IPC (~2.0)
- **HALI:** Should dominate Pareto frontier with adaptive structure selection

## Testing Approach

Five core experiments:

- **E1:** Static lookup on FSL Homes (read-heavy)
- **E2:** Dynamic throughput on MSR-Synthetic (write-heavy)
- **E3:** Mixed workload on FSL Homes
- **E4:** Scalability (10M to 200M keys)
- **E5:** Robustness on uniform random data

## Project Structure Notes

Current repository contains only documentation:
- `Documentation/SPECIFICATIONS.md` - Detailed technical specification
- `Documentation/RESEARCH.md` - Background research and literature review

Implementation will need:
- `src/` directory for simulator and index implementations
- `include/` for headers
- `benchmarks/` for workload generators
- `data/` for datasets
- `scripts/` for Docker setup and experiment automation
- The way to benchmark will be, because the host is Windows, to use WSL and use a Docker container with pre allocated cores and RAM. This is a Windows 11 environment.
- Never any mock or simplified implementations. If you have said to the user that something has been implemented, the user expects a COMPLETE and working implementation, or a response from you that it is blocked due to something that must be unblocked by the user. Do not make a simplified implementation of anything that is not feature-complete and is mocked, as this leads to confusion for the user.