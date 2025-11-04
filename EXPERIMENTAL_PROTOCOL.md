# WT-HALI Experimental Protocol: Step-by-Step Execution Plan

**Objective:** Find optimal WT-HALI configurations for different dataset sizes and workloads, then build an automatic configuration selector.

**Timeline:** Systematic execution with continuous validation and adaptation.

---

## Phase 1: Infrastructure Setup (Day 1)

### Step 1.1: Review Current Codebase ✓
**Status:** COMPLETED
- Identified HALIv2Index with Config struct
- compression_level: 0.0-1.0
- merge_threshold: buffer size percentage
- adaptive_expert_count(): scales with dataset size

### Step 1.2: Create Experiment Configuration System
**Goal:** Parameterize all experiments for systematic testing

**Files to Create:**
1. `scripts/experiment_config.py` - Configuration generator
2. `scripts/run_experiment.sh` - Automated experiment runner
3. `scripts/analyze_results.py` - Result analyzer and visualizer

**Configuration Parameters:**
```python
DATASET_SIZES = [100_000, 500_000, 1_000_000]  # Start with these
COMPRESSION_LEVELS = [0.0, 0.25, 0.5, 0.75, 1.0]
BUFFER_SIZES = [0.005, 0.01, 0.02, 0.05, 0.10]  # 0.5% to 10%
WORKLOADS = ['read_heavy', 'mixed', 'write_heavy']
DATASETS = ['clustered', 'sequential', 'lognormal', 'uniform', 'mixed', 'zipfian']
```

### Step 1.3: Modify main.cpp for Parameterized Runs
**Goal:** Accept command-line arguments for configuration

**Required Changes:**
```cpp
// Add command-line argument parsing
int main(int argc, char* argv[]) {
    // Parse: --compression=0.5 --buffer=0.01 --dataset=clustered --size=500000
    double compression_level = parse_arg(argc, argv, "--compression", 0.5);
    double buffer_size = parse_arg(argc, argv, "--buffer", 0.01);
    std::string dataset_type = parse_arg(argc, argv, "--dataset", "clustered");
    size_t dataset_size = parse_arg(argc, argv, "--size", 500000);

    // Create HALIv2 with specific config
    auto hali = std::make_unique<HALIv2Index<uint64_t, uint64_t>>(
        compression_level, buffer_size);

    // Run benchmarks...
}
```

---

## Phase 2: Baseline Experiments (Days 2-3)

### Experiment 2.1: Compression Level Sweep (Priority 1)
**Hypothesis:** Different compression levels perform best on different dataset sizes

**Configuration Matrix:**
- **Compression Levels:** 0.0, 0.25, 0.5, 0.75, 1.0
- **Dataset Sizes:** 100K, 500K, 1M
- **Datasets:** clustered, sequential (best performing from initial tests)
- **Workload:** mixed (balanced evaluation)
- **Total Configs:** 5 × 3 × 2 × 1 = 30 runs

**Expected Runtime:** ~2-3 hours (assuming 5 min/config)

**Success Criteria:**
- Identify optimal compression level for each dataset size
- Measure: lookup latency, insert throughput, memory footprint

**Analysis:**
```python
# Create heatmap: Dataset Size (x) vs Compression Level (y) vs Lookup Latency (color)
# Expected pattern:
# - Small datasets (100K): compression=0.0 or 0.25 (fewer experts, less routing overhead)
# - Large datasets (1M): compression=0.5 or 0.75 (more experts, better approximation)
```

### Experiment 2.2: Buffer Size Sweep (Priority 2)
**Hypothesis:** Optimal buffer size depends on workload read/write ratio

**Configuration Matrix:**
- **Buffer Sizes:** 0.5%, 1%, 2%, 5%, 10%
- **Workloads:** read_heavy (95/5), mixed (50/50), write_heavy (10/90)
- **Dataset:** clustered 500K (baseline)
- **Compression:** 0.5 (balanced)
- **Total Configs:** 5 × 3 = 15 runs

**Expected Runtime:** ~1 hour

**Success Criteria:**
- Identify optimal buffer size for each workload type
- Measure impact on read and write performance

**Analysis:**
```python
# Plot: Buffer Size (x) vs Throughput (y), separate lines for each workload
# Expected pattern:
# - Read-heavy: Small buffer (0.5-1%) to minimize lookup overhead
# - Write-heavy: Large buffer (5-10%) to batch more writes
# - Mixed: Medium buffer (2-5%)
```

### Experiment 2.3: Dataset Size Scaling (Priority 3)
**Hypothesis:** WT-HALI advantage grows with dataset size

**Configuration Matrix:**
- **Dataset Sizes:** 100K, 250K, 500K, 750K, 1M, 2M (if time permits)
- **Config:** WT-HALI-Speed (compression=0.0, buffer=0.01)
- **Dataset:** clustered
- **Workload:** mixed
- **Baselines:** BTree, RMI, PGM-Index
- **Total Configs:** 6 sizes × 4 indexes = 24 runs

**Expected Runtime:** ~2 hours

**Success Criteria:**
- Find dataset size where WT-HALI becomes faster than RMI
- Measure scaling behavior of routing overhead

**Analysis:**
```python
# Plot: Dataset Size (x) vs Lookup Latency (y), lines for each index
# Expected crossover point: WT-HALI < RMI at ~1M keys
```

---

## Phase 3: Fine-Tuning (Days 4-5)

### Experiment 3.1: Expert Count Manual Override
**Hypothesis:** Current formula (sqrt(n)/100) may not be optimal

**Configuration Matrix:**
- **Expert Count Multipliers:** 0.5x, 1.0x, 1.5x, 2.0x
- **Dataset Size:** 500K
- **Dataset:** clustered
- **Compression:** Fixed at 0.5
- **Total Configs:** 4 runs

**Success Criteria:**
- Determine if current formula is near-optimal
- Measure routing overhead vs approximation quality trade-off

### Experiment 3.2: Workload Spectrum
**Hypothesis:** WT-HALI sweet spot is 70-95% read workloads

**Configuration Matrix:**
- **Read Percentages:** 50%, 70%, 80%, 90%, 95%, 99%
- **Dataset:** clustered 500K
- **Config:** WT-HALI-Speed
- **Total Configs:** 6 runs

**Success Criteria:**
- Find exact workload ratio where WT-HALI outperforms B+Tree
- Identify optimal use cases

---

## Phase 4: Validation & Correctness (Day 6)

### Experiment 4.1: Correctness Validation for All Optimal Configs
**Requirement:** 100% validation pass rate

**Test Matrix:**
- Run validation suite on all "optimal" configurations found in Phase 2-3
- Test datasets: clustered, sequential, uniform
- Each test: 500K keys

**Success Criteria:**
- All optimal configs must pass 100% validation
- If failures: Debug, fix, re-benchmark

### Experiment 4.2: Large-Scale Stress Test
**Hypothesis:** Configurations remain valid at scale

**Configuration Matrix:**
- **Dataset Sizes:** 1M, 5M (if feasible)
- **Configs:** Top 3 optimal from Phase 2
- **Dataset:** clustered
- **Workload:** mixed

**Success Criteria:**
- Maintain 100% correctness
- Performance doesn't degrade unexpectedly

---

## Phase 5: Configuration Selector (Days 7-8)

### Step 5.1: Analyze All Experimental Data
**Goal:** Build decision tree for optimal configuration selection

**Analysis Tasks:**
1. **Per Dataset Size Analysis:**
   ```python
   # Group results by dataset size
   # Find best compression_level for each size
   optimal_configs = {
       100_000: {'compression': 0.0, 'buffer': 0.005, 'reason': '...'},
       500_000: {'compression': 0.25, 'buffer': 0.01, 'reason': '...'},
       1_000_000: {'compression': 0.5, 'buffer': 0.02, 'reason': '...'},
   }
   ```

2. **Per Workload Analysis:**
   ```python
   buffer_recommendations = {
       'read_heavy': 0.005,   # 0.5%
       'mixed': 0.01,          # 1%
       'write_heavy': 0.05,    # 5%
   }
   ```

3. **Performance Boundaries:**
   ```python
   # Where does WT-HALI beat each baseline?
   beats_rmi_at = {'clustered': 500_000, 'sequential': 750_000, ...}
   beats_btree_at = None  # Probably never on pure lookup latency
   optimal_niche = "Mixed workload, clustered data, 500K-2M keys"
   ```

### Step 5.2: Implement Automatic Configuration Selector

**Create:** `include/wt_hali_config_selector.h`

```cpp
namespace hali {

class WTHALIConfigSelector {
public:
    struct RecommendedConfig {
        double compression_level;
        double buffer_size_percent;
        std::string reasoning;
    };

    static RecommendedConfig get_optimal_config(
        size_t dataset_size,
        const std::string& workload_type = "mixed",
        const std::string& dataset_distribution = "unknown")
    {
        RecommendedConfig config;

        // Dataset size-based recommendations
        if (dataset_size < 250'000) {
            config.compression_level = 0.0;  // Speed mode
            config.reasoning = "Small dataset: minimize routing overhead";
        } else if (dataset_size < 1'000'000) {
            config.compression_level = 0.25;  // Light compression
            config.reasoning = "Medium dataset: balance speed and memory";
        } else {
            config.compression_level = 0.5;   // Balanced
            config.reasoning = "Large dataset: more experts for better approximation";
        }

        // Workload-based buffer sizing
        if (workload_type == "read_heavy") {
            config.buffer_size_percent = 0.005;  // 0.5%
        } else if (workload_type == "write_heavy") {
            config.buffer_size_percent = 0.05;   // 5%
        } else {
            config.buffer_size_percent = 0.01;   // 1%
        }

        return config;
    }
};

}  // namespace hali
```

### Step 5.3: Update HALIv2Index Constructor

```cpp
// Add default constructor that auto-selects configuration
explicit HALIv2Index(size_t expected_size,
                     const std::string& workload_hint = "mixed")
    : config_(WTHALIConfigSelector::get_optimal_config(expected_size, workload_hint))
{
    // Log selected configuration
    std::cout << "[WT-HALI] Auto-selected config for " << expected_size
              << " keys: compression=" << config_.compression_level
              << ", buffer=" << config_.buffer_size_percent
              << " (" << config_.reasoning << ")\n";
}
```

---

## Phase 6: Documentation Update (Day 9)

### Step 6.1: Update README.md

**Add Section:** "Optimal Configuration Guide"

```markdown
## Optimal Configuration Guide

Based on extensive experimentation, here are recommended WT-HALI configurations:

### By Dataset Size

| Dataset Size | Compression Level | Buffer Size | Expected Performance |
|--------------|------------------|-------------|---------------------|
| < 250K keys  | 0.0 (Speed)      | 0.5%       | 50-60 ns, 15M ops/sec |
| 250K-1M keys | 0.25 (Light)     | 1%         | 60-80 ns, 12M ops/sec |
| 1M-5M keys   | 0.5 (Balanced)   | 2%         | 80-120 ns, 10M ops/sec |
| > 5M keys    | 0.75 (Memory)    | 5%         | 120-180 ns, 8M ops/sec |

### By Workload

| Workload Type | Buffer Size | Trade-off |
|---------------|-------------|-----------|
| Read-Heavy (>90% reads) | 0.5% | Minimize lookup overhead |
| Mixed (50-90% reads) | 1-2% | Balanced |
| Write-Heavy (>50% writes) | 5-10% | Batch more writes |

### Automatic Configuration

```cpp
// Recommended: Let WT-HALI auto-select configuration
auto index = std::make_unique<HALIv2Index<uint64_t, uint64_t>>(
    expected_size,      // Dataset size hint
    "mixed"             // Workload hint: "read_heavy", "mixed", "write_heavy"
);

// Advanced: Manual configuration
auto index = std::make_unique<HALIv2Index<uint64_t, uint64_t>>(
    0.25,    // compression_level
    0.01     // buffer_size_percent
);
```
```

### Step 6.2: Create EXPERIMENT_RESULTS.md

**Full experimental findings with:**
- Heatmaps of compression level vs dataset size
- Buffer size impact charts
- Scaling behavior graphs
- Optimal configuration decision tree
- Validation results

### Step 6.3: Update Presentation

**Add Slides:**
1. "Experimental Methodology" (Phase 2-3 overview)
2. "Optimal Configurations: What We Learned"
3. "Configuration Selector Demo"
4. "Performance Sweet Spots" (updated Pareto frontier with optimal configs)

**Update Existing Slides:**
- Replace placeholder "TODO" items with actual results
- Update performance tables with optimal configuration results
- Add configuration recommendation flowchart

---

## Phase 7: Final Validation & Commit (Day 10)

### Step 7.1: Verify All Optimal Configurations
- Run validation suite on all recommended configs
- Ensure 100% correctness

### Step 7.2: Generate Final Visualizations
- Create all plots for EXPERIMENT_RESULTS.md
- Update presentation charts

### Step 7.3: Final Commit
```bash
git add -A
git commit -m "Complete experimental analysis with optimal configuration selector

Experimental Results:
- 90+ benchmark configurations tested
- Optimal compression levels identified for each dataset size
- Buffer size recommendations per workload type
- Automatic configuration selector implemented

Key Findings:
- Small datasets (<250K): compression=0.0, buffer=0.5%
- Medium datasets (250K-1M): compression=0.25, buffer=1%
- Large datasets (>1M): compression=0.5+, buffer=2-5%
- WT-HALI dominates on clustered/sequential data with mixed workloads

Documentation:
- Updated README.md with configuration guide
- Added EXPERIMENT_RESULTS.md with full analysis
- Updated presentation with experimental findings
- Implemented WTHALIConfigSelector for automatic tuning

100% Correctness: All optimal configurations validated
"
git push
```

---

## Experimental Execution Checklist

### Pre-Experiment
- [ ] Review codebase and existing configurations
- [ ] Create experiment runner scripts
- [ ] Modify main.cpp for parameterized runs
- [ ] Test single configuration end-to-end

### During Experiments
- [ ] Run Experiment 2.1: Compression level sweep
- [ ] Analyze results and adapt if needed
- [ ] Run Experiment 2.2: Buffer size sweep
- [ ] Run Experiment 2.3: Dataset size scaling
- [ ] Run Experiment 3.1: Expert count tuning
- [ ] Run Experiment 3.2: Workload spectrum

### Validation
- [ ] Run correctness tests on all optimal configs
- [ ] Verify 100% pass rate
- [ ] Run stress tests at scale

### Implementation
- [ ] Build configuration selector
- [ ] Integrate into HALIv2Index
- [ ] Test automatic configuration

### Documentation
- [ ] Update README.md
- [ ] Create EXPERIMENT_RESULTS.md
- [ ] Update presentation
- [ ] Generate all visualizations

### Finalization
- [ ] Verify all changes
- [ ] Run final validation
- [ ] Commit and push

---

## Adaptation Strategy

**If Results Don't Meet Goals:**

1. **If WT-HALI doesn't dominate any scenario:**
   - Pivot to "competitive but simpler" narrative
   - Emphasize 100% correctness + ease of configuration
   - Focus on specific niche (e.g., "file system metadata")

2. **If experiments take too long:**
   - Reduce dataset sizes (use 50K, 250K, 500K instead of 100K, 500K, 1M)
   - Parallelize workloads if possible
   - Focus on clustered/sequential datasets only

3. **If optimal configs fail validation:**
   - Debug and fix issues
   - Re-run affected experiments
   - Document edge cases and mitigations

4. **If compression level doesn't matter:**
   - Simplify to 2-3 configs (Speed, Balanced, Memory)
   - Focus on buffer size tuning instead

---

## Expected Timeline

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| 1. Infrastructure | 0.5 day | Experiment scripts ready |
| 2. Baseline Experiments | 2 days | 45 benchmark results |
| 3. Fine-Tuning | 1 day | 10 additional results |
| 4. Validation | 0.5 day | 100% correctness verified |
| 5. Config Selector | 1 day | Automatic configuration implemented |
| 6. Documentation | 1 day | All docs updated |
| 7. Final Validation | 0.5 day | Ready to commit |
| **TOTAL** | **6-7 days** | **Production-ready WT-HALI** |

---

## Success Metrics

**Must Have:**
- [ ] Identified optimal compression level for 3+ dataset sizes
- [ ] Identified optimal buffer size for 3 workload types
- [ ] 100% correctness on all optimal configurations
- [ ] Automatic configuration selector implemented

**Nice to Have:**
- [ ] WT-HALI faster than RMI on ≥1 dataset
- [ ] WT-HALI on Pareto frontier for ≥2 scenarios
- [ ] Scaling validated up to 5M keys

**Documentation:**
- [ ] README.md has clear configuration guide
- [ ] EXPERIMENT_RESULTS.md has full analysis
- [ ] Presentation updated with findings
- [ ] All claims backed by data
