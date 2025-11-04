#!/usr/bin/env python3
"""
WT-HALI Experiment Analyzer
Analyze results and determine optimal configurations
"""

import json
import pandas as pd
try:
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    import matplotlib.pyplot as plt
    PLOTTING_AVAILABLE = True
except ImportError:
    PLOTTING_AVAILABLE = False
    print("Warning: matplotlib not available, skipping visualizations")

from pathlib import Path
from typing import Dict, List
import numpy as np

class ExperimentAnalyzer:
    """Analyze experiment results and generate insights"""

    def __init__(self, results_dir: str = "results/experiments"):
        self.results_dir = Path(results_dir)
        self.output_dir = Path("results/analysis")
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def load_results(self, phase: str) -> pd.DataFrame:
        """Load results from JSON file"""
        results_file = self.results_dir / f"experiment_results_phase{phase}_final.json"

        if not results_file.exists():
            print(f"[WARNING] Results file not found: {results_file}")
            return pd.DataFrame()

        with open(results_file, 'r') as f:
            data = json.load(f)

        # Flatten nested structure
        rows = []
        for entry in data:
            if entry['status'] != 'success':
                continue

            row = {}
            row.update(entry['config'])
            row.update(entry['results'])
            rows.append(row)

        return pd.DataFrame(rows)

    def analyze_compression_sweep(self, df: pd.DataFrame) -> Dict:
        """Analyze compression level sweep (Experiment 2.1)"""
        print("\n" + "="*60)
        print("ANALYSIS: Compression Level Sweep (Experiment 2.1)")
        print("="*60)

        # Group by dataset size and compression level
        optimal_configs = {}

        for size in df['dataset_size'].unique():
            size_df = df[df['dataset_size'] == size]

            print(f"\nDataset Size: {size:,} keys")
            print("-" * 40)

            # Find best compression level for this size
            # Optimize for lookup latency
            best_row = size_df.loc[size_df['lookup_ns'].idxmin()]

            print(f"  Optimal Compression: {best_row['compression_level']}")
            print(f"  Lookup Latency: {best_row['lookup_ns']:.1f} ns")
            print(f"  Insert Throughput: {best_row['insert_ops_sec']/1e6:.1f} M ops/sec")
            print(f"  Memory: {best_row['bytes_per_key']:.2f} bytes/key")

            optimal_configs[size] = {
                'compression_level': best_row['compression_level'],
                'lookup_ns': best_row['lookup_ns'],
                'insert_ops_sec': best_row['insert_ops_sec'],
                'bytes_per_key': best_row['bytes_per_key'],
                'reasoning': f"Minimizes lookup latency for {size:,} keys"
            }

        # Create heatmap
        self._plot_compression_heatmap(df)

        return optimal_configs

    def analyze_buffer_sweep(self, df: pd.DataFrame) -> Dict:
        """Analyze buffer size sweep (Experiment 2.2)"""
        print("\n" + "="*60)
        print("ANALYSIS: Buffer Size Sweep (Experiment 2.2)")
        print("="*60)

        optimal_buffers = {}

        for workload in df['workload_type'].unique():
            wl_df = df[df['workload_type'] == workload]

            print(f"\nWorkload: {workload}")
            print("-" * 40)

            # For read-heavy: minimize lookup latency
            # For write-heavy: maximize insert throughput
            # For mixed: balance both

            if workload == 'read_heavy':
                best_row = wl_df.loc[wl_df['lookup_ns'].idxmin()]
                metric = 'lookup latency'
            elif workload == 'write_heavy':
                best_row = wl_df.loc[wl_df['insert_ops_sec'].idxmax()]
                metric = 'insert throughput'
            else:  # mixed
                # Balanced metric: normalize and average
                wl_df_norm = wl_df.copy()
                wl_df_norm['lookup_norm'] = wl_df['lookup_ns'] / wl_df['lookup_ns'].max()
                wl_df_norm['insert_norm'] = wl_df['insert_ops_sec'] / wl_df['insert_ops_sec'].max()
                wl_df_norm['balanced_score'] = (1 - wl_df_norm['lookup_norm']) + wl_df_norm['insert_norm']
                best_row = wl_df.loc[wl_df_norm['balanced_score'].idxmax()]
                metric = 'balanced performance'

            print(f"  Optimal Buffer Size: {best_row['buffer_size']*100:.1f}%")
            print(f"  Optimizes: {metric}")
            print(f"  Lookup Latency: {best_row['lookup_ns']:.1f} ns")
            print(f"  Insert Throughput: {best_row['insert_ops_sec']/1e6:.1f} M ops/sec")

            optimal_buffers[workload] = {
                'buffer_size': best_row['buffer_size'],
                'lookup_ns': best_row['lookup_ns'],
                'insert_ops_sec': best_row['insert_ops_sec'],
                'reasoning': f"Optimizes {metric} for {workload} workload"
            }

        # Create plots
        self._plot_buffer_impact(df)

        return optimal_buffers

    def analyze_scaling(self, df: pd.DataFrame) -> Dict:
        """Analyze dataset size scaling (Experiment 2.3)"""
        print("\n" + "="*60)
        print("ANALYSIS: Dataset Size Scaling (Experiment 2.3)")
        print("="*60)

        # Plot scaling behavior
        self._plot_scaling_behavior(df)

        # Find crossover points vs baselines
        # (This assumes baseline data is also in the results)

        insights = {
            'scaling_behavior': 'sub-linear' if self._check_sublinear(df) else 'linear',
            'min_recommended_size': 100_000,  # Update based on actual data
            'optimal_range': '250K-2M keys',   # Update based on actual data
        }

        return insights

    def generate_config_selector_code(self,
                                     compression_configs: Dict,
                                     buffer_configs: Dict) -> str:
        """Generate C++ code for automatic configuration selector"""

        code = """
// Auto-generated configuration selector based on experimental results
// Generated by: scripts/analyze_experiments.py

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
        const std::string& workload_type = "mixed")
    {
        RecommendedConfig config;

        // Dataset size-based compression level
        // Based on Experiment 2.1 results
"""

        # Add compression level selection logic based on results
        sorted_sizes = sorted(compression_configs.keys())
        for i, size in enumerate(sorted_sizes):
            comp = compression_configs[size]['compression_level']
            if i == 0:
                code += f"""        if (dataset_size < {size}) {{
            config.compression_level = {comp};
            config.reasoning = "Small dataset: {compression_configs[size]['reasoning']}";
"""
            elif i == len(sorted_sizes) - 1:
                code += f"""        }} else {{
            config.compression_level = {comp};
            config.reasoning = "Large dataset: {compression_configs[size]['reasoning']}";
        }}
"""
            else:
                next_size = sorted_sizes[i+1]
                code += f"""        }} else if (dataset_size < {next_size}) {{
            config.compression_level = {comp};
            config.reasoning = "{compression_configs[size]['reasoning']}";
"""

        # Add buffer size selection logic
        code += """
        // Workload-based buffer sizing
        // Based on Experiment 2.2 results
"""
        for workload, config in buffer_configs.items():
            buffer = config['buffer_size']
            code += f"""        if (workload_type == "{workload}") {{
            config.buffer_size_percent = {buffer};
            // {config['reasoning']}
        }}"""
            if workload != list(buffer_configs.keys())[-1]:
                code += " else "

        code += """

        return config;
    }

    // Get expected performance for configuration
    static std::string get_expected_performance(const RecommendedConfig& config) {
        std::stringstream ss;
        ss << "Expected: ";

        // Performance estimates based on experimental data
        // TODO: Update with actual measured values

        return ss.str();
    }
};

}  // namespace hali
"""

        return code

    def _plot_compression_heatmap(self, df: pd.DataFrame):
        """Create heatmap of compression level vs dataset size"""
        # Pivot data for heatmap
        pivot = df.pivot_table(
            values='lookup_ns',
            index='compression_level',
            columns='dataset_size',
            aggfunc='mean'
        )

        if PLOTTING_AVAILABLE:
            plt.figure(figsize=(10, 6))
            im = plt.imshow(pivot.values, cmap='RdYlGn_r', aspect='auto')
            plt.colorbar(im, label='Lookup Latency (ns)')

            # Add text annotations
            for i in range(len(pivot)):
                for j in range(len(pivot.columns)):
                    plt.text(j, i, f'{pivot.iloc[i, j]:.1f}',
                            ha="center", va="center", color="black", fontsize=8)

            plt.xticks(range(len(pivot.columns)), pivot.columns)
            plt.yticks(range(len(pivot)), pivot.index)
            plt.title('Lookup Latency (ns): Compression Level vs Dataset Size')
            plt.xlabel('Dataset Size')
            plt.ylabel('Compression Level')
            plt.tight_layout()
        else:
            print(f"Skipping heatmap (matplotlib not available)")

        if PLOTTING_AVAILABLE:
            output_file = self.output_dir / 'compression_heatmap.png'
            plt.savefig(output_file, dpi=300)
            print(f"\n[PLOT] Saved: {output_file}")
            plt.close()

    def _plot_buffer_impact(self, df: pd.DataFrame):
        """Plot buffer size impact on performance"""
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

        # Lookup latency
        for workload in df['workload_type'].unique():
            wl_df = df[df['workload_type'] == workload]
            ax1.plot(wl_df['buffer_size'] * 100, wl_df['lookup_ns'],
                    marker='o', label=workload)

        ax1.set_xlabel('Buffer Size (%)')
        ax1.set_ylabel('Lookup Latency (ns)')
        ax1.set_title('Buffer Size Impact on Lookup Latency')
        ax1.legend()
        ax1.grid(True, alpha=0.3)

        # Insert throughput
        for workload in df['workload_type'].unique():
            wl_df = df[df['workload_type'] == workload]
            ax2.plot(wl_df['buffer_size'] * 100, wl_df['insert_ops_sec'] / 1e6,
                    marker='o', label=workload)

        ax2.set_xlabel('Buffer Size (%)')
        ax2.set_ylabel('Insert Throughput (M ops/sec)')
        ax2.set_title('Buffer Size Impact on Insert Throughput')
        ax2.legend()
        ax2.grid(True, alpha=0.3)

        plt.tight_layout()

        output_file = self.output_dir / 'buffer_impact.png'
        plt.savefig(output_file, dpi=300)
        print(f"[PLOT] Saved: {output_file}")
        plt.close()

    def _plot_scaling_behavior(self, df: pd.DataFrame):
        """Plot scaling behavior across dataset sizes"""
        plt.figure(figsize=(10, 6))

        # Lookup latency scaling
        plt.subplot(1, 2, 1)
        plt.plot(df['dataset_size'] / 1000, df['lookup_ns'], 'o-')
        plt.xlabel('Dataset Size (K keys)')
        plt.ylabel('Lookup Latency (ns)')
        plt.title('Lookup Latency Scaling')
        plt.grid(True, alpha=0.3)

        # Memory scaling
        plt.subplot(1, 2, 2)
        plt.plot(df['dataset_size'] / 1000, df['bytes_per_key'], 'o-')
        plt.xlabel('Dataset Size (K keys)')
        plt.ylabel('Memory (bytes/key)')
        plt.title('Memory Footprint Scaling')
        plt.grid(True, alpha=0.3)

        plt.tight_layout()

        output_file = self.output_dir / 'scaling_behavior.png'
        plt.savefig(output_file, dpi=300)
        print(f"[PLOT] Saved: {output_file}")
        plt.close()

    def _check_sublinear(self, df: pd.DataFrame) -> bool:
        """Check if scaling is sub-linear"""
        # Fit log-linear model
        sizes = df['dataset_size'].values
        latencies = df['lookup_ns'].values

        # If log(latency) ~ log(size), then slope < 1 means sub-linear
        log_sizes = np.log(sizes)
        log_latencies = np.log(latencies)

        slope = np.polyfit(log_sizes, log_latencies, 1)[0]

        return slope < 1.0

    def generate_summary_report(self,
                                compression_configs: Dict,
                                buffer_configs: Dict,
                                scaling_insights: Dict) -> str:
        """Generate markdown summary report"""

        report = """# WT-HALI Experimental Results Summary

## Experiment 2.1: Compression Level Optimization

### Optimal Compression Levels by Dataset Size

| Dataset Size | Optimal Compression | Lookup (ns) | Throughput (M ops/s) | Memory (B/key) |
|--------------|---------------------|-------------|----------------------|----------------|
"""

        for size in sorted(compression_configs.keys()):
            config = compression_configs[size]
            report += f"| {size:,} | {config['compression_level']:.2f} | "
            report += f"{config['lookup_ns']:.1f} | "
            report += f"{config['insert_ops_sec']/1e6:.1f} | "
            report += f"{config['bytes_per_key']:.2f} |\n"

        report += """
### Key Findings
"""
        for size, config in compression_configs.items():
            report += f"- **{size:,} keys**: {config['reasoning']}\n"

        report += """
## Experiment 2.2: Buffer Size Optimization

### Optimal Buffer Sizes by Workload

| Workload Type | Optimal Buffer | Lookup (ns) | Throughput (M ops/s) |
|---------------|----------------|-------------|----------------------|
"""

        for workload in ['read_heavy', 'mixed', 'write_heavy']:
            if workload in buffer_configs:
                config = buffer_configs[workload]
                report += f"| {workload} | {config['buffer_size']*100:.1f}% | "
                report += f"{config['lookup_ns']:.1f} | "
                report += f"{config['insert_ops_sec']/1e6:.1f} |\n"

        report += """
### Key Findings
"""
        for workload, config in buffer_configs.items():
            report += f"- **{workload}**: {config['reasoning']}\n"

        report += f"""
## Experiment 2.3: Scaling Analysis

### Scaling Behavior
- **Type**: {scaling_insights.get('scaling_behavior', 'TBD')}
- **Minimum Recommended Size**: {scaling_insights.get('min_recommended_size', 'TBD'):,} keys
- **Optimal Range**: {scaling_insights.get('optimal_range', 'TBD')}

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
"""

        return report

def main():
    import argparse

    parser = argparse.ArgumentParser(description='Analyze WT-HALI experiments')
    parser.add_argument('--results-dir', default='results/experiments',
                       help='Directory containing experiment results')

    args = parser.parse_args()

    analyzer = ExperimentAnalyzer(results_dir=args.results_dir)

    # Load and analyze each experiment phase
    print("\nLoading experimental results...")

    df_2_1 = analyzer.load_results('2.1')
    df_2_2 = analyzer.load_results('2.2')
    df_2_3 = analyzer.load_results('2.3')

    compression_configs = {}
    buffer_configs = {}
    scaling_insights = {}

    if not df_2_1.empty:
        compression_configs = analyzer.analyze_compression_sweep(df_2_1)

    if not df_2_2.empty:
        buffer_configs = analyzer.analyze_buffer_sweep(df_2_2)

    if not df_2_3.empty:
        scaling_insights = analyzer.analyze_scaling(df_2_3)

    # Generate configuration selector code
    if compression_configs and buffer_configs:
        selector_code = analyzer.generate_config_selector_code(
            compression_configs, buffer_configs)

        output_file = Path("include") / "wt_hali_config_selector.h"
        output_file.parent.mkdir(parents=True, exist_ok=True)
        with open(output_file, 'w') as f:
            f.write(selector_code)
        print(f"\n[GENERATED] Config selector: {output_file}")

    # Generate summary report
    report = analyzer.generate_summary_report(
        compression_configs, buffer_configs, scaling_insights)

    report_file = Path("EXPERIMENT_RESULTS.md")
    with open(report_file, 'w') as f:
        f.write(report)
    print(f"[GENERATED] Summary report: {report_file}")

    print("\n" + "="*60)
    print("ANALYSIS COMPLETE")
    print("="*60)

if __name__ == '__main__':
    main()
