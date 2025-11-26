#!/usr/bin/env python3
"""
Comprehensive Benchmark Analysis Script
Generates Pareto frontiers, latency distributions, and performance comparisons
"""

import pandas as pd
import json
import sys
from pathlib import Path

# Plotting setup
try:
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    import matplotlib.pyplot as plt
    import numpy as np
    PLOTTING_AVAILABLE = True
except ImportError:
    PLOTTING_AVAILABLE = False
    print("Warning: matplotlib not available, skipping visualizations")


def load_results(csv_path):
    """Load benchmark results from CSV file"""
    df = pd.read_csv(csv_path)
    print(f"Loaded {len(df)} benchmark results")
    print(f"Indexes: {df['Index'].unique()}")
    print(f"Datasets: {df['Dataset'].unique()}")
    print(f"Workloads: {df['Workload'].unique()}")
    return df


def generate_pareto_frontier(df, output_dir):
    """Generate Pareto frontier plots: Memory vs Lookup Latency"""
    if not PLOTTING_AVAILABLE:
        return

    # Calculate bytes per key (already in CSV as BytesPerKey)
    df['bytes_per_key'] = df['BytesPerKey']

    # Create Pareto frontier for each workload
    workloads = df['Workload'].unique()

    for workload in workloads:
        workload_df = df[df['Workload'] == workload]

        fig, ax = plt.subplots(figsize=(12, 8))

        # Plot each index
        for index_name in workload_df['Index'].unique():
            index_df = workload_df[workload_df['Index'] == index_name]

            # Average across datasets
            avg_memory = index_df['bytes_per_key'].mean()
            avg_lookup = index_df['MeanLookup_ns'].mean()

            ax.scatter(avg_memory, avg_lookup, s=200, alpha=0.7, label=index_name)
            ax.annotate(index_name, (avg_memory, avg_lookup),
                       xytext=(5, 5), textcoords='offset points', fontsize=9)

        ax.set_xlabel('Memory Footprint (bytes/key)', fontsize=12)
        ax.set_ylabel('Mean Lookup Latency (ns)', fontsize=12)
        ax.set_title(f'Pareto Frontier: Memory vs Lookup Latency\\n{workload} Workload',
                    fontsize=14, fontweight='bold')
        ax.legend(loc='best', fontsize=10)
        ax.grid(True, alpha=0.3)

        # Log scale if range is large
        if workload_df['MeanLookup_ns'].max() / workload_df['MeanLookup_ns'].min() > 10:
            ax.set_yscale('log')

        plt.tight_layout()
        plt.savefig(output_dir / f'pareto_frontier_{workload.replace(" ", "_").replace("/", "_")}.png',
                   dpi=300, bbox_inches='tight')
        plt.close()

    print(f"✓ Generated Pareto frontier plots for {len(workloads)} workloads")


def generate_latency_comparison(df, output_dir):
    """Generate latency comparison charts (mean, p95, p99)"""
    if not PLOTTING_AVAILABLE:
        return

    workloads = df['Workload'].unique()

    for workload in workloads:
        workload_df = df[df['Workload'] == workload]

        # Group by index and average across datasets
        grouped = workload_df.groupby('Index').agg({
            'MeanLookup_ns': 'mean',
            'P95Lookup_ns': 'mean',
            'P99Lookup_ns': 'mean'
        }).reset_index()

        # Sort by mean lookup
        grouped = grouped.sort_values('MeanLookup_ns')

        fig, ax = plt.subplots(figsize=(14, 8))

        x = np.arange(len(grouped))
        width = 0.25

        ax.bar(x - width, grouped['MeanLookup_ns'], width, label='Mean', alpha=0.8)
        ax.bar(x, grouped['P95Lookup_ns'], width, label='P95', alpha=0.8)
        ax.bar(x + width, grouped['P99Lookup_ns'], width, label='P99', alpha=0.8)

        ax.set_xlabel('Index', fontsize=12)
        ax.set_ylabel('Lookup Latency (ns)', fontsize=12)
        ax.set_title(f'Lookup Latency Comparison: {workload} Workload',
                    fontsize=14, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(grouped['Index'], rotation=45, ha='right')
        ax.legend(fontsize=11)
        ax.grid(True, alpha=0.3, axis='y')

        plt.tight_layout()
        plt.savefig(output_dir / f'latency_comparison_{workload.replace(" ", "_").replace("/", "_")}.png',
                   dpi=300, bbox_inches='tight')
        plt.close()

    print(f"✓ Generated latency comparison charts for {len(workloads)} workloads")


def generate_throughput_comparison(df, output_dir):
    """Generate insert throughput comparison charts"""
    if not PLOTTING_AVAILABLE:
        return

    workloads = df['Workload'].unique()

    for workload in workloads:
        workload_df = df[df['Workload'] == workload]

        # Group by index and average across datasets
        grouped = workload_df.groupby('Index').agg({
            'InsertThroughput_ops': 'mean'
        }).reset_index()

        # Sort by throughput
        grouped = grouped.sort_values('InsertThroughput_ops', ascending=False)

        fig, ax = plt.subplots(figsize=(12, 8))

        bars = ax.barh(grouped['Index'], grouped['InsertThroughput_ops'],
                      color='steelblue', alpha=0.8)

        ax.set_xlabel('Insert Throughput (ops/sec)', fontsize=12)
        ax.set_ylabel('Index', fontsize=12)
        ax.set_title(f'Insert Throughput Comparison: {workload} Workload',
                    fontsize=14, fontweight='bold')
        ax.grid(True, alpha=0.3, axis='x')

        # Add value labels
        for i, bar in enumerate(bars):
            width = bar.get_width()
            ax.text(width, bar.get_y() + bar.get_height()/2,
                   f'{width:,.0f}',
                   ha='left', va='center', fontsize=10, fontweight='bold')

        plt.tight_layout()
        plt.savefig(output_dir / f'throughput_comparison_{workload.replace(" ", "_").replace("/", "_")}.png',
                   dpi=300, bbox_inches='tight')
        plt.close()

    print(f"✓ Generated throughput comparison charts for {len(workloads)} workloads")


def generate_dataset_heatmap(df, output_dir):
    """Generate heatmap showing performance across datasets"""
    if not PLOTTING_AVAILABLE:
        return

    # Create pivot table: Index × Dataset, value = Mean Lookup
    pivot = df.pivot_table(values='MeanLookup_ns',
                          index='Index',
                          columns='Dataset',
                          aggfunc='mean')

    fig, ax = plt.subplots(figsize=(14, 10))

    im = ax.imshow(pivot.values, cmap='RdYlGn_r', aspect='auto')

    # Set ticks
    ax.set_xticks(np.arange(len(pivot.columns)))
    ax.set_yticks(np.arange(len(pivot.index)))
    ax.set_xticklabels(pivot.columns, rotation=45, ha='right')
    ax.set_yticklabels(pivot.index)

    # Add colorbar
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Mean Lookup Latency (ns)', fontsize=12)

    # Add text annotations
    for i in range(len(pivot.index)):
        for j in range(len(pivot.columns)):
            text = ax.text(j, i, f'{pivot.values[i, j]:.1f}',
                         ha="center", va="center", color="black", fontsize=9)

    ax.set_title('Lookup Latency Heatmap: All Indexes × All Datasets',
                fontsize=14, fontweight='bold')

    plt.tight_layout()
    plt.savefig(output_dir / 'dataset_heatmap.png', dpi=300, bbox_inches='tight')
    plt.close()

    print(f"✓ Generated dataset performance heatmap")


def generate_memory_comparison(df, output_dir):
    """Generate memory footprint comparison"""
    if not PLOTTING_AVAILABLE:
        return

    # Calculate bytes per key
    df['bytes_per_key'] = df['Memory (bytes)'] / df['Dataset Size']

    # Group by index and average across all workloads and datasets
    grouped = df.groupby('Index').agg({
        'bytes_per_key': 'mean'
    }).reset_index()

    # Sort by memory
    grouped = grouped.sort_values('bytes_per_key')

    fig, ax = plt.subplots(figsize=(12, 8))

    bars = ax.barh(grouped['Index'], grouped['bytes_per_key'],
                  color='coral', alpha=0.8)

    ax.set_xlabel('Memory Footprint (bytes/key)', fontsize=12)
    ax.set_ylabel('Index', fontsize=12)
    ax.set_title('Memory Footprint Comparison (Average Across All Workloads)',
                fontsize=14, fontweight='bold')
    ax.grid(True, alpha=0.3, axis='x')

    # Add value labels
    for i, bar in enumerate(bars):
        width = bar.get_width()
        ax.text(width, bar.get_y() + bar.get_height()/2,
               f'{width:.2f}',
               ha='left', va='center', fontsize=10, fontweight='bold')

    plt.tight_layout()
    plt.savefig(output_dir / 'memory_comparison.png', dpi=300, bbox_inches='tight')
    plt.close()

    print(f"✓ Generated memory comparison chart")


def generate_summary_report(df, output_path):
    """Generate comprehensive summary report"""

    df['bytes_per_key'] = df['BytesPerKey']

    # Overall summary by index
    summary = df.groupby('Index').agg({
        'MeanLookup_ns': ['mean', 'min', 'max'],
        'P95Lookup_ns': ['mean', 'min', 'max'],
        'P99Lookup_ns': ['mean', 'min', 'max'],
        'InsertThroughput_ops': ['mean', 'min', 'max'],
        'bytes_per_key': ['mean', 'min', 'max'],
        'BuildTime_ms': ['mean', 'min', 'max']
    }).reset_index()

    # Flatten column names
    summary.columns = ['_'.join(col).strip('_') for col in summary.columns.values]

    # Find best performers
    best_lookup = df.loc[df['MeanLookup_ns'].idxmin(), 'Index']
    best_throughput = df.loc[df['InsertThroughput_ops'].idxmax(), 'Index']
    best_memory = df.loc[df['bytes_per_key'].idxmin(), 'Index']

    report = f"""# Comprehensive Benchmark Results Summary

## Dataset Configuration
- **Dataset Size:** {df['DatasetSize'].iloc[0]:,} keys
- **Operations per Benchmark:** {len(df)} total benchmarks
- **Indexes Tested:** {', '.join(df['Index'].unique())}
- **Datasets:** {', '.join(df['Dataset'].unique())}
- **Workloads:** {', '.join(df['Workload'].unique())}

## Overall Best Performers

- **Fastest Lookup:** {best_lookup} ({df[df['Index'] == best_lookup]['MeanLookup_ns'].min():.1f} ns)
- **Highest Throughput:** {best_throughput} ({df[df['Index'] == best_throughput]['InsertThroughput_ops'].max():,.0f} ops/s)
- **Lowest Memory:** {best_memory} ({df[df['Index'] == best_memory]['bytes_per_key'].min():.2f} bytes/key)

## Performance Summary by Index

"""

    # Add summary table
    for index_name in df['Index'].unique():
        index_df = df[df['Index'] == index_name]
        report += f"### {index_name}\n\n"
        report += f"- **Mean Lookup:** {index_df['MeanLookup_ns'].mean():.1f} ns "
        report += f"(range: {index_df['MeanLookup_ns'].min():.1f} - {index_df['MeanLookup_ns'].max():.1f})\n"
        report += f"- **P99 Lookup:** {index_df['P99Lookup_ns'].mean():.1f} ns\n"
        report += f"- **Insert Throughput:** {index_df['InsertThroughput_ops'].mean():,.0f} ops/s\n"
        report += f"- **Memory:** {index_df['bytes_per_key'].mean():.2f} bytes/key\n"
        report += f"- **Build Time:** {index_df['BuildTime_ms'].mean():.1f} ms\n\n"

    # Write report
    with open(output_path, 'w') as f:
        f.write(report)

    print(f"✓ Generated summary report: {output_path}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python analyze_comprehensive_benchmarks.py <results_csv>")
        sys.exit(1)

    csv_path = Path(sys.argv[1])
    if not csv_path.exists():
        print(f"Error: {csv_path} does not exist")
        sys.exit(1)

    # Load results
    df = load_results(csv_path)

    # Create output directory
    output_dir = Path('results/analysis_comprehensive')
    output_dir.mkdir(parents=True, exist_ok=True)

    print("\nGenerating visualizations...")

    # Generate all plots
    generate_pareto_frontier(df, output_dir)
    generate_latency_comparison(df, output_dir)
    generate_throughput_comparison(df, output_dir)
    generate_dataset_heatmap(df, output_dir)
    generate_memory_comparison(df, output_dir)

    # Generate summary report
    generate_summary_report(df, output_dir / 'COMPREHENSIVE_RESULTS.md')

    print(f"\n✓ Analysis complete! Results saved to {output_dir}")
    print(f"\nGenerated files:")
    print(f"  - Pareto frontier plots (3)")
    print(f"  - Latency comparison charts (3)")
    print(f"  - Throughput comparison charts (3)")
    print(f"  - Dataset heatmap (1)")
    print(f"  - Memory comparison chart (1)")
    print(f"  - Summary report (COMPREHENSIVE_RESULTS.md)")
    print(f"\nTotal: ~11 visualization files + 1 report")


if __name__ == '__main__':
    main()
