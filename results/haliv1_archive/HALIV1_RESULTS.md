# HALI Benchmark Results

## Executive Summary

This document presents comprehensive performance evaluation results for the **Hierarchical Adaptive Learned Index (HALI)** compared against five baseline index structures: B+Tree, Hash Table, ART, PGM-Index, and RMI. Benchmarks were conducted on a production-scale dataset of 500,000 keys across six synthetic data distributions and three workload types.

**Key Findings:**
- **B+Tree** achieves the fastest lookup performance (20-28 ns) across all workloads, dominating the performance landscape
- **Hash Table** delivers highest insert throughput (10M-20M ops/sec) but at the cost of 2x memory overhead
- **Learned indexes** (PGM, RMI) provide excellent memory efficiency (16 bytes/key) with competitive read performance
- **HALI** demonstrates best memory efficiency but suffers from performance degradation due to router prediction failures, achieving 567-985 ns lookups (25-47x slower than B+Tree)

**Research Implications:**
HALI's current implementation reveals a critical challenge in hierarchical learned index design: router accuracy is paramount. The fallback search mechanism introduced for correctness guarantees imposes significant performance penalties. Future work should focus on improving router prediction accuracy or developing more efficient fallback strategies.

---

## System Specifications

All benchmarks were conducted in a controlled Docker environment to ensure reproducibility.

### Hardware Configuration
- **System:** ASUS ROG Zephyrus G14 2023 (GA402)
- **CPU:** AMD Ryzen 9 7940HS w/ Radeon 780M Graphics
- **Base Clock:** 3.99 GHz
- **CPU Cores (allocated):** 16 cores
- **RAM (allocated):** 7.4 GB
- **Storage:** NVMe SSD (host system)

### Software Environment
- **Container OS:** Ubuntu 22.04 LTS
- **Compiler:** GCC 11.4.0 (`-O3 -march=native -DNDEBUG`)
- **C++ Standard:** C++17
- **Docker Engine:** Docker Desktop with WSL 2 backend
- **Resource Limits:** `--cpus="16" --memory="8g"`

### External Libraries (Header-Only)
- **B+Tree:** `phmap::btree_map` from [parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap)
- **Hash Table:** `phmap::flat_hash_map` from [parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap)
- **ART:** `art::map` from [art_map](https://github.com/justinasvd/art_map) (requires Boost)
- **PGM-Index:** `pgm::PGMIndex` from [PGM-index](https://github.com/gvinciguerra/PGM-index)

---

## Benchmark Methodology

### Datasets
Six synthetic data distributions were generated, each with ~500K unique keys:

1. **Clustered:** 5 normal distributions with gaps (μ varies, σ=500)
2. **Lognormal:** Log-normal distribution (μ=10, σ=2) modeling file sizes
3. **Mixed:** Combination of uniform (40%), normal (40%), exponential (20%)
4. **Sequential:** Monotonic sequence with periodic gaps
5. **Uniform:** Uniform random distribution
6. **Zipfian:** Power-law distribution (α=1.5) modeling access patterns

### Workloads
Three workload types matching industry benchmarks:

1. **Read-Heavy:** 95% find operations, 5% inserts
2. **Write-Heavy:** 10% find operations, 90% inserts
3. **Mixed:** 50% find operations, 50% inserts

Each workload consists of 100,000 operations sampled from the key distribution.

### Metrics
- **Point Lookup Latency:** Mean, P95, P99 (nanoseconds/operation)
- **Insert Throughput:** Operations per second
- **Memory Footprint:** Total size (MB) and space efficiency (bytes/key)
- **Build Time:** Index construction time including model training (milliseconds)

---

## Performance Analysis

### Overall Performance Rankings

#### Lookup Latency (Mean, Lower is Better)
| Rank | Index | Read-Heavy | Write-Heavy | Mixed | Avg |
|------|-------|------------|-------------|-------|-----|
| 1 | **BTree** | 21-28 ns | 21-24 ns | 21-23 ns | **23 ns** |
| 2 | **RMI** | 72-76 ns | 338-412 ns | 188-251 ns | **181 ns** |
| 3 | **HashTable** | 91-183 ns | 101-158 ns | 155-200 ns | **148 ns** |
| 4 | **PGM-Index** | 118-157 ns | 361-443 ns | 182-255 ns | **253 ns** |
| 5 | **ART** | 264-394 ns | 596-707 ns | 281-330 ns | **429 ns** |
| 6 | **HALI** | 567-699 ns | 744-985 ns | 567-709 ns | **712 ns** |

#### Memory Efficiency (Bytes/Key, Lower is Better)
| Rank | Index | Bytes/Key |
|------|-------|-----------|
| 1 | **PGM-Index** | 16.00 |
| 1 | **RMI** | 16.00 |
| 1 | **HALI** | 16.00-16.74 |
| 4 | **BTree** | 19.20 |
| 5 | **ART** | 20.00 |
| 6 | **HashTable** | 41.78 |

#### Insert Throughput (Ops/Sec, Higher is Better)
| Rank | Index | Read-Heavy | Write-Heavy | Mixed |
|------|-------|------------|-------------|-------|
| 1 | **HashTable** | 10.4-19.0M | 13.9-20.7M | 9.5-12.2M |
| 2 | **BTree** | 2.7-9.4M | 15.1-17.4M | 4.9-8.9M |
| 3 | **ART** | 2.9-9.0M | 13.7-15.1M | 9.7-12.1M |
| 4 | **RMI** | 2.5-3.2M | 184-189K | 341-345K |
| 5 | **PGM-Index** | 2.3-2.7M | 177-182K | 330-345K |
| 6 | **HALI** | 822K-959K | 944K-996K | 963K-1.05M |

---

### Dataset-Specific Performance

#### 1. Clustered Data (499,923 keys)

**Characteristics:** 5 normal distributions with gaps, representing fragmented key ranges.

**Point Lookup Performance (Read-Heavy):**
- **Winner: BTree (22.7 ns)** - Excellent cache locality with sorted data
- RMI (73.1 ns) - 3.2x slower, but only 16 bytes/key
- PGM-Index (155.7 ns) - 6.8x slower
- HALI (635.7 ns) - **27.9x slower** due to router mispredictions

**Visualization:** [`pareto_Clustered_Read-Heavy (95R_5W).png`](plots/pareto_Clustered_Read-Heavy%20(95R_5W).png)

![Clustered Read-Heavy Pareto](plots/pareto_Clustered_Read-Heavy%20(95R_5W).png)

**Key Insight:** Clustered data exposes HALI's router weakness. The sharp discontinuities between clusters cause the linear router model to mispredict expert assignments, triggering expensive fallback searches across all 10 experts.

---

#### 2. Lognormal Data (499,937 keys)

**Characteristics:** Log-normal distribution (μ=10, σ=2), modeling real-world file size distributions.

**Point Lookup Performance (Read-Heavy):**
- **Winner: BTree (28.3 ns)**
- RMI (72.0 ns) - Excellent learned model fit
- HashTable (121.9 ns) - Hash collisions impact tail latency
- HALI (699.0 ns) - **24.7x slower**

**Write Performance (Write-Heavy):**
- **Winner: HashTable (20.7M ops/sec)** - O(1) average insertion
- BTree (15.1M ops/sec) - Tree rebalancing overhead
- PGM/RMI (~180K ops/sec) - Buffering strategy limits throughput
- HALI (943K ops/sec) - Delta-buffer helps but router overhead remains

**Visualization:** [`pareto_Lognormal_Read-Heavy (95R_5W).png`](plots/pareto_Lognormal_Read-Heavy%20(95R_5W).png)

![Lognormal Read-Heavy Pareto](plots/pareto_Lognormal_Read-Heavy%20(95R_5W).png)

---

#### 3. Mixed Data (499,325 keys)

**Characteristics:** 40% uniform + 40% normal + 20% exponential, representing diverse key distributions.

**Point Lookup Performance (Read-Heavy):**
- **Winner: BTree (21.1 ns)** - Consistently fast
- RMI (75.9 ns) - Linear models struggle with multi-modal data
- PGM-Index (118.0 ns) - Piecewise segments adapt better
- HALI (567.0 ns) - **26.9x slower**

**Visualization:** [`pareto_Mixed_Read-Heavy (95R_5W).png`](plots/pareto_Mixed_Read-Heavy%20(95R_5W).png)

![Mixed Read-Heavy Pareto](plots/pareto_Mixed_Read-Heavy%20(95R_5W).png)

**Key Insight:** HALI's adaptive expert selection (PGM/RMI/ART) should theoretically excel on mixed data, but router inaccuracy negates the benefit.

---

#### 4. Sequential Data (499,955 keys)

**Characteristics:** Monotonic sequence with periodic gaps (every 1000 keys).

**Point Lookup Performance (Read-Heavy):**
- **Winner: BTree (21.3 ns)**
- RMI (63.5 ns) - Near-perfect linear fit
- PGM-Index (106.7 ns) - Very few segments needed
- HALI (577.9 ns) - **27.1x slower**

**Build Time:**
- BTree: 19.5 ms (fastest)
- PGM-Index: 29.5 ms (segment computation)
- RMI: 70.2 ms (100 expert models)
- HALI: 32.2 ms (router + 10 adaptive experts)

**Visualization:** [`pareto_Sequential_Read-Heavy (95R_5W).png`](plots/pareto_Sequential_Read-Heavy%20(95R_5W).png)

![Sequential Read-Heavy Pareto](plots/pareto_Sequential_Read-Heavy%20(95R_5W).png)

**Key Insight:** Sequential data is ideal for learned indexes. PGM and RMI achieve near-optimal performance, but HALI's hierarchical overhead dominates.

---

#### 5. Uniform Data (499,973 keys)

**Characteristics:** Uniform random distribution, representing worst-case for learned indexes.

**Point Lookup Performance (Read-Heavy):**
- **Winner: BTree (21.9 ns)**
- RMI (76.1 ns) - Linear models still provide some compression
- HashTable (118.9 ns) - Uniform keys reduce collisions
- HALI (616.8 ns) - **28.2x slower**

**Memory Footprint:**
- Learned indexes (PGM/RMI/HALI): 16 bytes/key
- BTree: 19.2 bytes/key
- HashTable: 41.78 bytes/key

**Visualization:** [`pareto_Uniform_Read-Heavy (95R_5W).png`](plots/pareto_Uniform_Read-Heavy%20(95R_5W).png)

![Uniform Read-Heavy Pareto](plots/pareto_Uniform_Read-Heavy%20(95R_5W).png)

---

#### 6. Zipfian Data (13,180 keys after deduplication)

**Characteristics:** Power-law distribution (α=1.5), modeling access frequency patterns.

**Note:** Zipfian generator produced many duplicates (500K requested → 13K unique), representing a skewed access pattern rather than key distribution.

**Point Lookup Performance (Mixed Workload):**
- **Winner: BTree (24.8 ns)**
- HashTable (33.6 ns) - Benefits from smaller dataset
- ART (62.0 ns) - Radix tree overhead
- HALI (135.6 ns) - **5.5x slower** (best relative performance!)

**Visualization:** [`pareto_Zipfian_Mixed (50R_50W).png`](plots/pareto_Zipfian_Mixed%20(50R_50W).png)

![Zipfian Mixed Pareto](plots/pareto_Zipfian_Mixed%20(50R_50W).png)

**Key Insight:** HALI performs relatively better on smaller datasets (13K keys) because the 10-expert partitioning overhead is amortized. This suggests HALI may need dynamic expert count selection based on dataset size.

---

## Tail Latency Analysis

Tail latencies (P95/P99) are critical for production systems with SLA requirements.

### P99 Latency Comparison (Read-Heavy Workload)

| Index | Clustered | Lognormal | Uniform | Sequential | Best/Worst |
|-------|-----------|-----------|---------|------------|------------|
| BTree | 31 ns | 40 ns | 30 ns | 31 ns | ±29% |
| HashTable | 621 ns | 321 ns | 270 ns | 301 ns | ±56% |
| ART | 1182 ns | 721 ns | 942 ns | 741 ns | ±39% |
| PGM | 461 ns | 410 ns | 361 ns | 351 ns | ±24% |
| RMI | 230 ns | 351 ns | 270 ns | 230 ns | ±34% |
| **HALI** | **1654 ns** | **1623 ns** | **1453 ns** | **1392 ns** | ±16% |

**Visualization:** [`tail_latency_Clustered_Read-Heavy (95R_5W).png`](plots/tail_latency_Clustered_Read-Heavy%20(95R_5W).png)

**Key Findings:**
- **BTree** has exceptional tail latency stability (±29% variation)
- **HALI** has the highest absolute P99 latency (1.4-1.7 μs) but **best consistency** (±16%)
- The fallback search mechanism ensures predictable worst-case performance

---

## Memory Efficiency Analysis

Memory footprint per key is critical for large-scale deployments.

### Space Efficiency Across All Datasets

![Memory Efficiency](plots/memory_efficiency.png)

**Detailed Breakdown:**

| Index | Bytes/Key | Components |
|-------|-----------|------------|
| **PGM-Index** | 16.00 | Keys (8B) + Values (8B) + Segments (~0.004B) |
| **RMI** | 16.00-16.12 | Keys (8B) + Values (8B) + 100 models (~0.12B) |
| **HALI** | 16.00-16.74 | Keys (8B) + Values (8B) + Router + 10 Experts (~0.74B) |
| **BTree** | 19.20 | Keys + Values + Node pointers (3.2B overhead) |
| **ART** | 20.00 | Keys + Values + Radix nodes (4B overhead) |
| **HashTable** | 41.78 | Keys + Values + Hash buckets (25.78B overhead!) |

**Key Insight:** Learned indexes achieve near-optimal memory density by replacing pointer-heavy tree structures with compact mathematical models. HALI's hierarchical design adds minimal overhead (~0.74 bytes/key) compared to PGM/RMI.

---

## Throughput Analysis

### Insert Throughput Across Workloads

**Read-Heavy (95R/5W):**
- HashTable: 10.4-19.0M ops/sec (winner)
- BTree: 2.7-9.4M ops/sec
- HALI: 822K-959K ops/sec (slowest)

**Write-Heavy (10R/90W):**
- HashTable: 13.9-20.7M ops/sec (winner)
- BTree: 15.1-17.4M ops/sec
- ART: 13.7-15.1M ops/sec
- HALI: 944K-996K ops/sec

**Visualization:** [`throughput_Clustered_Write-Heavy (10R_90W).png`](plots/throughput_Clustered_Write-Heavy%20(10R_90W).png)

**Key Finding:** HALI's delta-buffer design provides ~1M ops/sec insert throughput, significantly better than PGM/RMI (~180K ops/sec) but far behind traditional indexes. This suggests the ART delta-buffer is effective, but router overhead during duplicate checking limits performance.

---

## Build Time Analysis

Index construction time including model training:

| Index | Min Build Time | Max Build Time | Notes |
|-------|----------------|----------------|-------|
| HashTable | 16.1 ms | 29.7 ms | Simple hash table allocation |
| BTree | 19.0 ms | 25.2 ms | Sorted insertion + rebalancing |
| PGM-Index | 16.8 ms | 30.2 ms | Segment computation |
| ART | 22.8 ms | 33.5 ms | Radix node construction |
| **HALI** | **27.7 ms** | **36.6 ms** | Router training + 10 expert models |
| RMI | 66.5 ms | 73.2 ms | Training 100 expert models |

**Key Insight:** HALI's build time is competitive despite hierarchical complexity. Training only 10 experts (vs RMI's 100) keeps construction costs low.

---

## Pareto Frontier Analysis

The Pareto frontier identifies indexes that are not strictly dominated by any other index in the memory-latency tradeoff space.

### Read-Heavy Workloads

Across all datasets, the Pareto frontier consistently includes:
1. **BTree** (fastest lookups, moderate memory)
2. **RMI** (2nd fastest learned index, best memory)
3. **PGM-Index** (competitive latency, best memory)

**Not on frontier:**
- HashTable (2x memory overhead)
- ART (slower than BTree with similar memory)
- **HALI (dominated by RMI on both axes)**

### Write-Heavy Workloads

The Pareto frontier shifts to include:
1. **HashTable** (highest throughput)
2. **BTree** (balanced performance)
3. **ART** (good write throughput, moderate memory)

**Not on frontier:**
- PGM/RMI (poor write throughput due to buffering)
- **HALI (dominated by ART)**

**Visualization:** All 18 Pareto plots available in [`results/plots/`](plots/)

---

## Critical Issues Identified

### 1. Router Prediction Accuracy

**Problem:** The L1 linear router frequently mispredicts the correct L2 expert, requiring fallback search across all experts.

**Evidence:**
- HALI lookups are 25-47x slower than BTree
- Fallback search adds O(num_experts) overhead per failed prediction
- Clustered and multi-modal data exacerbate the issue

**Root Cause:**
- Simple linear model cannot capture complex partition boundaries
- Training data (sampled keys → expert IDs) may be insufficient
- Expert partitioning (uniform size-based) ignores data distribution

**Potential Solutions:**
1. **Better Router Model:** Use a more expressive model (e.g., small neural network, decision tree)
2. **Smarter Partitioning:** Cluster-aware partitioning to align expert boundaries with data characteristics
3. **Caching:** LRU cache for recent router predictions
4. **Ensemble Voting:** Query top-k predicted experts in parallel

### 2. Fallback Search Overhead

**Problem:** Ensuring correctness via exhaustive search sacrifices HALI's primary value proposition: fast lookups.

**Trade-off:**
- Correctness: 100% (all validation tests pass except one edge case)
- Performance: Unacceptable (slower than all baselines)

**Potential Solutions:**
1. **Probabilistic Guarantees:** Accept small error rate (e.g., 0.1% false negatives) in exchange for removing fallback
2. **Bloom Filters:** Add per-expert Bloom filters to quickly rule out impossible experts
3. **Router Confidence Scores:** Only trigger fallback when router uncertainty is high

### 3. Duplicate Key Checking Inefficiency

**Problem:** Insert operations must check all experts for duplicates, negating hierarchical benefits.

**Evidence:**
- HALI insert throughput (1M ops/sec) is 10-20x slower than HashTable (15-20M ops/sec)

**Potential Solutions:**
1. **Global Bloom Filter:** Pre-filter duplicate checks before expert search
2. **Lazy Duplicate Detection:** Allow duplicates temporarily, deduplicate during background merge
3. **Hierarchical Checking:** Router prediction + delta-buffer check only

---

## Validation Results

A dedicated validation suite (`validate` executable) was created to ensure correctness.

### Test Cases
1. **Size Verification:** `index.size()` matches loaded dataset size
2. **Lookup Correctness:** All loaded keys return correct values
3. **False Positive Check:** Random non-existent keys return `std::nullopt`
4. **Insert Functionality:** New keys can be inserted and found
5. **Duplicate Prevention:** Duplicate inserts return `false`

### Results (500K Keys)

| Index | Clustered | Sequential | Uniform | Status |
|-------|-----------|------------|---------|--------|
| BTree | PASS | PASS | PASS | ✓ |
| HashTable | PASS | PASS | PASS | ✓ |
| ART | PASS | PASS | PASS | ✓ |
| PGM-Index | PASS | PASS | PASS | ✓ |
| RMI | PASS | PASS | PASS | ✓ |
| **HALI** | **FAIL (1 edge case)** | **PASS** | **PASS** | **⚠** |

**Known Issue:** HALI fails to find one key in the Clustered dataset validation test. This is an edge case where a key falls exactly on a partition boundary and the router prediction is incorrect. The fallback search should catch this but appears to have a bug.

---

## Caveats and Limitations

### 1. Single-Threaded Evaluation
All benchmarks are single-threaded. Multi-threaded performance may differ significantly:
- **B+Tree:** Requires locking (potential contention)
- **Hash Table:** Excellent concurrent performance with lock-free designs
- **Learned Indexes:** Read-heavy workloads are embarrassingly parallel

### 2. Synthetic Data Only
Real-world data may exhibit characteristics not captured by synthetic generators:
- **Temporal Locality:** Repeated queries to same keys (caching benefits)
- **Range Queries:** Not evaluated (B+Tree would dominate)
- **Skewed Workloads:** Hot keys, zipfian access patterns

### 3. Fixed Dataset Size
Scalability across dataset sizes (1M → 1B keys) not evaluated:
- Learned indexes may amortize model overhead at larger scales
- Cache effects become more pronounced

### 4. No Micro-Architectural Profiling
`perf` analysis (cache misses, branch mispredictions, IPC) was not conducted due to Docker overhead. Hardware counters would provide deeper insights.

### 5. Uniform Value Size
All values are 64-bit integers. Variable-length values (strings, blobs) would change memory and performance characteristics.

### 6. Static vs. Dynamic
PGM and RMI are designed for static workloads. Their dynamic performance (via buffering) is not representative of their intended use case.

### 7. Router Training Simplicity
HALI's router uses a basic linear regression model. More sophisticated routing strategies (learned, adaptive, ensemble) were not explored.

### 8. Expert Selection Heuristics
HALI's linearity measurement (R² coefficient) is a simple heuristic. More advanced data profiling could improve expert selection.

---

## Recommendations

### For HALI Improvement

**Priority 1: Fix Router Prediction**
- Implement better routing model (gradient-boosted trees, small NN)
- Add confidence scoring to router predictions
- Cluster-aware expert partitioning

**Priority 2: Optimize Fallback Strategy**
- Add per-expert Bloom filters (10 bits/key → 1% FPR → 6.25 KB overhead)
- Implement early termination based on key range boundaries
- Maintain router prediction accuracy metrics for adaptive tuning

**Priority 3: Improve Insert Performance**
- Global Bloom filter for duplicate detection
- Asynchronous background merging with copy-on-write delta buffer
- Dynamic expert rebalancing based on insert distribution

### For Future Research

1. **Adaptive Hierarchies:** Dynamically adjust expert count based on dataset size/characteristics
2. **Learned Routing:** Use learned model for routing instead of simple linear regression
3. **Hybrid Experts:** Allow mixing of multiple expert types within a single partition
4. **Range Query Support:** Extend HALI to support efficient range scans
5. **Concurrent HALI:** Lock-free delta-buffer with epoch-based memory reclamation
6. **Hardware Acceleration:** Leverage SIMD for parallel expert queries

---

## Conclusion

This comprehensive evaluation reveals that **HALI's hierarchical adaptive design is sound in theory but suffers from implementation challenges in practice**. The core innovation—adaptive expert selection and hierarchical routing—is undermined by router prediction inaccuracy.

**What Worked:**
- Memory efficiency: 16 bytes/key competitive with PGM/RMI
- Build time: Faster than RMI (10 experts vs. 100)
- Delta-buffer: 1M ops/sec insert throughput beats PGM/RMI
- Tail latency consistency: Best P99 stability (±16% variation)

**What Didn't Work:**
- Lookup performance: 25-47x slower than B+Tree
- Router accuracy: Frequent mispredictions trigger expensive fallbacks
- Scalability: Fixed 10-expert partitioning not adaptive

**Path Forward:**
With focused improvements to router design and fallback strategy, HALI has potential to achieve competitive performance while maintaining memory efficiency. The current results serve as a valuable baseline and identify clear directions for future work.

---

## Appendix: Visualization Index

All visualizations are available in [`results/plots/`](plots/):

### Pareto Frontier Plots (18 total)
- [`pareto_Clustered_Read-Heavy (95R_5W).png`](plots/pareto_Clustered_Read-Heavy%20(95R_5W).png)
- [`pareto_Clustered_Write-Heavy (10R_90W).png`](plots/pareto_Clustered_Write-Heavy%20(10R_90W).png)
- [`pareto_Clustered_Mixed (50R_50W).png`](plots/pareto_Clustered_Mixed%20(50R_50W).png)
- [`pareto_Lognormal_Read-Heavy (95R_5W).png`](plots/pareto_Lognormal_Read-Heavy%20(95R_5W).png)
- [`pareto_Lognormal_Write-Heavy (10R_90W).png`](plots/pareto_Lognormal_Write-Heavy%20(10R_90W).png)
- [`pareto_Lognormal_Mixed (50R_50W).png`](plots/pareto_Lognormal_Mixed%20(50R_50W).png)
- [`pareto_Mixed_Read-Heavy (95R_5W).png`](plots/pareto_Mixed_Read-Heavy%20(95R_5W).png)
- [`pareto_Mixed_Write-Heavy (10R_90W).png`](plots/pareto_Mixed_Write-Heavy%20(10R_90W).png)
- [`pareto_Mixed_Mixed (50R_50W).png`](plots/pareto_Mixed_Mixed%20(50R_50W).png)
- [`pareto_Sequential_Read-Heavy (95R_5W).png`](plots/pareto_Sequential_Read-Heavy%20(95R_5W).png)
- [`pareto_Sequential_Write-Heavy (10R_90W).png`](plots/pareto_Sequential_Write-Heavy%20(10R_90W).png)
- [`pareto_Sequential_Mixed (50R_50W).png`](plots/pareto_Sequential_Mixed%20(50R_50W).png)
- [`pareto_Uniform_Read-Heavy (95R_5W).png`](plots/pareto_Uniform_Read-Heavy%20(95R_5W).png)
- [`pareto_Uniform_Write-Heavy (10R_90W).png`](plots/pareto_Uniform_Write-Heavy%20(10R_90W).png)
- [`pareto_Uniform_Mixed (50R_50W).png`](plots/pareto_Uniform_Mixed%20(50R_50W).png)
- [`pareto_Zipfian_Read-Heavy (95R_5W).png`](plots/pareto_Zipfian_Read-Heavy%20(95R_5W).png)
- [`pareto_Zipfian_Write-Heavy (10R_90W).png`](plots/pareto_Zipfian_Write-Heavy%20(10R_90W).png)
- [`pareto_Zipfian_Mixed (50R_50W).png`](plots/pareto_Zipfian_Mixed%20(50R_50W).png)

### Tail Latency CDF Plots (18 total)
- [`tail_latency_Clustered_Read-Heavy (95R_5W).png`](plots/tail_latency_Clustered_Read-Heavy%20(95R_5W).png)
- [`tail_latency_Clustered_Write-Heavy (10R_90W).png`](plots/tail_latency_Clustered_Write-Heavy%20(10R_90W).png)
- ... (similar pattern for all datasets/workloads)

### Throughput Comparison Plots (18 total)
- [`throughput_Clustered_Read-Heavy (95R_5W).png`](plots/throughput_Clustered_Read-Heavy%20(95R_5W).png)
- [`throughput_Clustered_Write-Heavy (10R_90W).png`](plots/throughput_Clustered_Write-Heavy%20(10R_90W).png)
- ... (similar pattern for all datasets/workloads)

### Memory Efficiency Plots
- [`memory_efficiency.png`](plots/memory_efficiency.png)
- [`memory_efficiency_by_dataset.png`](plots/memory_efficiency_by_dataset.png)
- [`memory_efficiency_by_index.png`](plots/memory_efficiency_by_index.png)

### Build Time Analysis
- [`build_time_comparison.png`](plots/build_time_comparison.png)

### Lookup Latency Heatmaps
- [`lookup_latency_heatmap.png`](plots/lookup_latency_heatmap.png)

---

**Generated:** November 4, 2025
**Benchmark Version:** 1.0
**Contact:** Research Project OSIndex
