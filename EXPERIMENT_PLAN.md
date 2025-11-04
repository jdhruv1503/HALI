# WT-HALI Experimental Plan: Finding the Sweet Spot

**Goal:** Identify specific scenarios where WT-HALI dominates the Pareto frontier and outperforms all baseline indexes.

**Motivation:** Current results show WT-HALI is competitive, but we want to find workloads where it's the **clear winner**.

---

## Experiment 1: Write-Through Buffer Size Sweep

### Hypothesis
Larger buffers improve write-heavy workloads but hurt read-heavy ones. Optimal size depends on read/write ratio.

### Configuration Matrix
- **Buffer Sizes:** 0.5%, 1%, 2%, 5%, 10% of main index
- **Workloads:** Read-Heavy (95/5), Mixed (50/50), Write-Heavy (10/90)
- **Datasets:** Clustered, Sequential (most promising from initial results)
- **Metrics:** Lookup latency, insert throughput, memory footprint

### Expected Outcome
Find optimal buffer size per workload:
- Read-Heavy → Small buffer (0.5-1%)
- Write-Heavy → Large buffer (5-10%)
- Mixed → Medium buffer (2-5%)

### Implementation
```cpp
// In wt_hali_index.h
const double BUFFER_SIZE_PERCENT = 0.01; // Currently 1%
// Test: 0.005, 0.01, 0.02, 0.05, 0.10
```

---

## Experiment 2: Compression Level Grid Search

### Hypothesis
Different datasets benefit from different compression levels. Sequential/clustered data → high compression works well.

### Configuration Matrix
- **Compression Levels:** 0.0, 0.25, 0.5, 0.75, 1.0
- **Datasets:** All 6 (Clustered, Lognormal, Sequential, Uniform, Mixed, Zipfian)
- **Workload:** Mixed (50/50) for balanced evaluation
- **Metrics:** Full performance profile

### Expected Outcome
Dataset-specific recommendations:
- Clustered → compression=0.0 (Speed) should dominate
- Sequential → compression=0.5 (Balanced) might excel
- Uniform → compression doesn't matter much

### Implementation
```cpp
// Test configs
WT_HALI_Index idx_speed(0.0);
WT_HALI_Index idx_025(0.25);
WT_HALI_Index idx_bal(0.5);
WT_HALI_Index idx_075(0.75);
WT_HALI_Index idx_mem(1.0);
```

---

## Experiment 3: Expert Count Scaling

### Hypothesis
Current formula ($\sqrt{n}/100$) may not be optimal. More experts → better fit, but diminishing returns.

### Configuration Matrix
- **Scaling Factors:** 0.5x, 1.0x, 1.5x, 2.0x of current formula
- **Dataset Sizes:** 100K, 500K, 1M, 5M keys
- **Dataset:** Clustered (best WT-HALI performance)
- **Metrics:** Lookup latency, memory, build time

### Expected Outcome
Identify optimal expert count scaling that maximizes performance while minimizing routing overhead.

### Implementation
```cpp
// Current formula
size_t base_experts = max(4, sqrt(n) / 100);
size_t num_experts = base_experts * (0.5 + 1.5 * compression_level);

// Test scaling
size_t num_experts = base_experts * SCALING_FACTOR * (0.5 + 1.5 * compression_level);
// SCALING_FACTOR: 0.5, 1.0, 1.5, 2.0
```

---

## Experiment 4: Large-Scale Validation

### Hypothesis
WT-HALI's advantage increases with dataset size due to better amortization of routing overhead.

### Configuration Matrix
- **Dataset Sizes:** 1M, 2M, 5M, 10M keys
- **Datasets:** Clustered, Sequential
- **Config:** WT-HALI-Speed (best performer)
- **Baselines:** B+Tree, RMI, PGM-Index
- **Metrics:** Lookup latency, throughput, memory

### Expected Outcome
WT-HALI gap vs RMI/PGM widens at larger scales. Find the "breakpoint" where WT-HALI becomes dominant.

### Success Criteria
- WT-HALI-Speed < RMI lookup latency at 5M+ keys
- WT-HALI-Speed maintains 10M+ inserts/sec at all scales
- Memory footprint stays < 20 bytes/key

---

## Experiment 5: Workload-Specific Optimization

### Hypothesis
Extreme workloads (99% read or 99% write) reveal WT-HALI's strengths/weaknesses.

### Configuration Matrix
- **Workloads:**
  - 99% Read, 1% Write
  - 90% Read, 10% Write
  - 50% Read, 50% Write
  - 10% Read, 90% Write
  - 1% Read, 99% Write
- **Dataset:** Clustered (500K keys)
- **Configs:** All 3 WT-HALI variants + baselines
- **Metrics:** Throughput (ops/sec), latency distribution

### Expected Outcome
Identify WT-HALI's "sweet spot":
- Likely: 70-95% read workloads
- Where: Write throughput matters but reads still dominate

---

## Experiment 6: Bloom Filter Impact

### Hypothesis
Bloom filters help most on uniform/random data where false positives are rare.

### Configuration Matrix
- **Variants:** WT-HALI with/without Bloom filters
- **Datasets:** Uniform, Zipfian (worst case for learned indexes)
- **Config:** WT-HALI-Speed
- **Metrics:** Lookup latency, memory overhead

### Expected Outcome
- Bloom filters: 10-15% speedup on negative lookups
- Memory overhead: +1.5-2 bytes/key (10-15 bits)
- Best on uniform/random data

---

## Experiment 7: ALEX Comparison

### Hypothesis
WT-HALI should outperform ALEX on write-heavy workloads due to simpler write-through design.

### Configuration Matrix
- **Indexes:** WT-HALI-Speed, WT-HALI-Memory, ALEX
- **Workloads:** Mixed (50/50), Write-Heavy (10/90)
- **Datasets:** All 6
- **Metrics:** Full comparison

### Expected Outcome
- WT-HALI faster inserts than ALEX
- Competitive or better lookup latency
- Similar memory footprint

### Dependencies
- Need to integrate ALEX library
- Estimated effort: 2-3 days

---

## Success Metrics: When is WT-HALI the Winner?

### Primary Goals
1. **Pareto Dominance:** WT-HALI-Speed on Pareto frontier for ≥3 datasets
2. **Write Throughput:** 10M+ inserts/sec maintained across all configs
3. **Memory Efficiency:** ≤20 bytes/key for Speed/Memory configs
4. **Correctness:** 100% validation pass rate on all configs

### Stretch Goals
1. **Absolute Dominance:** WT-HALI beats ALL baselines on clustered+mixed workload
2. **RMI Killer:** WT-HALI-Speed faster lookups than RMI on ≥2 datasets
3. **Write King:** WT-HALI highest insert throughput among learned indexes

---

## Execution Plan

### Phase 1: Buffer Size Experiments (2-3 days)
1. Modify buffer size parameter
2. Run benchmarks for 5 buffer sizes × 3 workloads × 2 datasets = 30 configs
3. Analyze and identify optimal buffer sizes

### Phase 2: Compression Level Grid (2-3 days)
1. Run 5 compression levels × 6 datasets × 1 workload = 30 configs
2. Create heatmap of performance
3. Identify dataset-specific recommendations

### Phase 3: Scaling Experiments (3-4 days)
1. Generate large datasets (1M-10M keys)
2. Run expert count scaling tests
3. Validate large-scale performance

### Phase 4: ALEX Integration (3-5 days)
1. Integrate ALEX library
2. Implement ALEX wrapper
3. Run comparative benchmarks

### Phase 5: Analysis & Documentation (2-3 days)
1. Analyze all results
2. Identify scenarios where WT-HALI dominates
3. Update documentation and presentation
4. Generate final visualizations

**Total Estimated Time:** 12-18 days

---

## Expected Deliverables

1. **Complete Benchmark Results:** CSV files for all experiments
2. **Performance Heatmaps:** Compression level × Dataset, Buffer size × Workload
3. **Updated Documentation:**
   - README.md with "When to Use WT-HALI" section
   - PRODUCTION_RESULTS.md with full analysis
   - EXPERIMENT_RESULTS.md (new file)
4. **Updated Presentation:** Final slides with best results
5. **Configuration Guide:** Recommended settings for different use cases

---

## Risk Mitigation

### Risk: WT-HALI doesn't dominate any scenario
**Mitigation:** Focus on "competitive but simpler" narrative. Highlight 100% correctness + ease of implementation.

### Risk: Experiments take too long
**Mitigation:** Parallelize workloads, use smaller key counts (100K) for grid search.

### Risk: ALEX outperforms WT-HALI everywhere
**Mitigation:** Emphasize WT-HALI's simplicity (no gapped arrays, no complex tuning), focus on write-heavy scenarios.

---

## Notes

- All experiments run in Docker with fixed resources (16 cores, 7.4GB RAM)
- Benchmark harness already supports configuration parameters
- Validation tests must pass before claiming production-ready
- Focus on clustered/sequential datasets where initial results are strongest
