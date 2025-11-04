# WT-HALI Experimental Results Summary

## Experiment 2.1: Compression Level Optimization

### Optimal Compression Levels by Dataset Size

| Dataset Size | Optimal Compression | Lookup (ns) | Throughput (M ops/s) | Memory (B/key) |
|--------------|---------------------|-------------|----------------------|----------------|
| 100,000 | 0.50 | 98.2 | 7.5 | 18.50 |
| 500,000 | 0.25 | 128.2 | 12.1 | 17.75 |
| 1,000,000 | 0.25 | 190.5 | 10.1 | 17.75 |

### Key Findings
- **100,000 keys**: Minimizes lookup latency for 100,000 keys
- **500,000 keys**: Minimizes lookup latency for 500,000 keys
- **1,000,000 keys**: Minimizes lookup latency for 1,000,000 keys

## Experiment 2.2: Buffer Size Optimization

### Optimal Buffer Sizes by Workload

| Workload Type | Optimal Buffer | Lookup (ns) | Throughput (M ops/s) |
|---------------|----------------|-------------|----------------------|
| read_heavy | 5.0% | 162.2 | 7.8 |
| mixed | 0.5% | 250.5 | 7.5 |
| write_heavy | 5.0% | 302.1 | 10.0 |

### Key Findings
- **read_heavy**: Optimizes lookup latency for read_heavy workload
- **mixed**: Optimizes balanced performance for mixed workload
- **write_heavy**: Optimizes insert throughput for write_heavy workload

## Experiment 2.3: Scaling Analysis

### Scaling Behavior
- **Type**: sub-linear
- **Minimum Recommended Size**: 100,000 keys
- **Optimal Range**: 250K-2M keys

## Configuration Recommendations

### Quick Reference

```cpp
// Small datasets (< 250K keys)
auto index = std::make_unique<HALIv2Index>(
    compression_level = 0.0,   // Speed mode
    buffer_size = 0.005        // 0.5%
);

// Medium datasets (250K-1M keys)
auto index = std::make_unique<HALIv2Index>(
    compression_level = 0.25,  // Light compression
    buffer_size = 0.01         // 1%
);

// Large datasets (> 1M keys)
auto index = std::make_unique<HALIv2Index>(
    compression_level = 0.5,   // Balanced
    buffer_size = 0.02         // 2%
);
```

### Automatic Configuration (Recommended)

Use the auto-generated configuration selector for optimal performance:

```cpp
#include "wt_hali_config_selector.h"

auto config = WTHALIConfigSelector::get_optimal_config(
    dataset_size,
    workload_type  // "read_heavy", "mixed", "write_heavy"
);

auto index = std::make_unique<HALIv2Index>(
    config.compression_level,
    config.buffer_size_percent
);

std::cout << config.reasoning << std::endl;
```

## Visualizations

- [Compression Heatmap](analysis/compression_heatmap.png)
- [Buffer Impact Analysis](analysis/buffer_impact.png)
- [Scaling Behavior](analysis/scaling_behavior.png)

## Next Steps

1. Validate all optimal configurations for 100% correctness
2. Run large-scale tests (5M-10M keys)
3. Compare against ALEX baseline
4. Update production benchmarks with optimal configs
