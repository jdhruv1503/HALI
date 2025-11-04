#!/usr/bin/env python3
"""
Visualization script for HALI benchmark results
Generates Pareto frontier plots, CDF plots, and performance tables
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
from pathlib import Path

# Set style for publication-quality figures
plt.style.use('seaborn-v0_8-darkgrid')
plt.rcParams['figure.figsize'] = (10, 6)
plt.rcParams['font.size'] = 11
plt.rcParams['axes.labelsize'] = 12
plt.rcParams['axes.titlesize'] = 14
plt.rcParams['legend.fontsize'] = 10

# Color scheme for indexes
INDEX_COLORS = {
    'BTree': '#1f77b4',
    'HashTable': '#ff7f0e',
    'ART': '#2ca02c',
    'PGM-Index': '#d62728',
    'RMI': '#9467bd',
    'HALI': '#e377c2'
}

INDEX_MARKERS = {
    'BTree': 'o',
    'HashTable': 's',
    'ART': '^',
    'PGM-Index': 'D',
    'RMI': 'v',
    'HALI': '*'
}


def load_results(csv_path):
    """Load benchmark results from CSV"""
    if not Path(csv_path).exists():
        print(f"Error: Results file not found: {csv_path}")
        sys.exit(1)

    df = pd.read_csv(csv_path)
    print(f"Loaded {len(df)} benchmark results")
    print(f"Indexes: {df['Index'].unique()}")
    print(f"Datasets: {df['Dataset'].unique()}")
    print(f"Workloads: {df['Workload'].unique()}")

    return df


def plot_pareto_frontier(df, dataset_name, workload_name, output_dir):
    """
    Generate Pareto frontier plot: Memory Footprint vs Mean Lookup Latency
    Shows which indexes dominate in the space-time trade-off
    """
    # Filter data
    data = df[(df['Dataset'] == dataset_name) & (df['Workload'] == workload_name)]

    if len(data) == 0:
        print(f"Warning: No data for {dataset_name} + {workload_name}")
        return

    fig, ax = plt.subplots(figsize=(10, 7))

    # Plot each index
    for index in data['Index'].unique():
        idx_data = data[data['Index'] == index]

        ax.scatter(
            idx_data['Memory_MB'],
            idx_data['MeanLookup_ns'],
            s=200,
            marker=INDEX_MARKERS.get(index, 'o'),
            color=INDEX_COLORS.get(index, 'gray'),
            label=index,
            alpha=0.7,
            edgecolors='black',
            linewidth=1.5
        )

        # Add index name annotation
        for _, row in idx_data.iterrows():
            ax.annotate(
                index,
                (row['Memory_MB'], row['MeanLookup_ns']),
                xytext=(5, 5),
                textcoords='offset points',
                fontsize=9,
                alpha=0.8
            )

    ax.set_xlabel('Memory Footprint (MB)', fontweight='bold')
    ax.set_ylabel('Mean Lookup Latency (ns)', fontweight='bold')
    ax.set_title(f'Pareto Frontier: {dataset_name} - {workload_name}', fontweight='bold')
    ax.legend(loc='best', framealpha=0.9)
    ax.grid(True, alpha=0.3)

    # Log scale if range is large
    if data['MeanLookup_ns'].max() / data['MeanLookup_ns'].min() > 10:
        ax.set_yscale('log')

    plt.tight_layout()

    output_file = output_dir / f'pareto_{dataset_name}_{workload_name.replace("/", "_")}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"  Saved: {output_file}")
    plt.close()


def plot_latency_cdf(df, dataset_name, workload_name, output_dir):
    """
    Generate CDF plot for latency distribution
    Shows full latency distribution for tail performance analysis
    Note: This requires raw latency data, which we don't have in the CSV
    This is a placeholder for when we export raw latencies
    """
    # For now, we can show a bar chart of P95/P99
    data = df[(df['Dataset'] == dataset_name) & (df['Workload'] == workload_name)]

    if len(data) == 0:
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    indexes = data['Index'].unique()
    x = np.arange(len(indexes))
    width = 0.35

    p95_values = [data[data['Index'] == idx]['P95Lookup_ns'].values[0] for idx in indexes]
    p99_values = [data[data['Index'] == idx]['P99Lookup_ns'].values[0] for idx in indexes]

    bars1 = ax.bar(x - width/2, p95_values, width, label='P95 Latency', alpha=0.8)
    bars2 = ax.bar(x + width/2, p99_values, width, label='P99 Latency', alpha=0.8)

    ax.set_xlabel('Index', fontweight='bold')
    ax.set_ylabel('Latency (ns)', fontweight='bold')
    ax.set_title(f'Tail Latency: {dataset_name} - {workload_name}', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(indexes, rotation=45, ha='right')
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')

    # Add value labels on bars
    for bars in [bars1, bars2]:
        for bar in bars:
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height,
                   f'{int(height)}',
                   ha='center', va='bottom', fontsize=8)

    plt.tight_layout()

    output_file = output_dir / f'tail_latency_{dataset_name}_{workload_name.replace("/", "_")}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"  Saved: {output_file}")
    plt.close()


def plot_throughput_comparison(df, dataset_name, output_dir):
    """
    Generate throughput comparison bar chart
    """
    # Filter for write-heavy workload
    data = df[(df['Dataset'] == dataset_name) & (df['Workload'].str.contains('Write'))]

    if len(data) == 0:
        return

    fig, ax = plt.subplots(figsize=(10, 6))

    indexes = data['Index'].unique()
    throughputs = [data[data['Index'] == idx]['InsertThroughput_ops'].values[0] for idx in indexes]

    colors = [INDEX_COLORS.get(idx, 'gray') for idx in indexes]
    bars = ax.bar(indexes, throughputs, color=colors, alpha=0.7, edgecolor='black', linewidth=1.5)

    ax.set_xlabel('Index', fontweight='bold')
    ax.set_ylabel('Insert Throughput (ops/sec)', fontweight='bold')
    ax.set_title(f'Insert Throughput: {dataset_name}', fontweight='bold')
    ax.grid(True, alpha=0.3, axis='y')

    # Add value labels
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
               f'{int(height):,}',
               ha='center', va='bottom', fontsize=9)

    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()

    output_file = output_dir / f'throughput_{dataset_name}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"  Saved: {output_file}")
    plt.close()


def generate_performance_table(df, output_dir):
    """
    Generate formatted performance tables for the paper
    """
    # Summary table: Mean lookup latency across all workloads
    summary = df.groupby(['Index', 'Dataset']).agg({
        'MeanLookup_ns': 'mean',
        'Memory_MB': 'mean',
        'BytesPerKey': 'mean',
        'BuildTime_ms': 'mean'
    }).round(2)

    summary_file = output_dir / 'performance_summary.csv'
    summary.to_csv(summary_file)
    print(f"  Saved: {summary_file}")

    # Detailed table
    detailed_file = output_dir / 'performance_detailed.csv'
    df.to_csv(detailed_file, index=False)
    print(f"  Saved: {detailed_file}")

    # Print markdown table for paper
    print("\n" + "="*80)
    print("PERFORMANCE SUMMARY TABLE (Markdown format for paper)")
    print("="*80 + "\n")

    # Create pivot table
    pivot = df.pivot_table(
        values='MeanLookup_ns',
        index='Index',
        columns='Dataset',
        aggfunc='mean'
    ).round(1)

    print(pivot.to_markdown())
    print("\n")


def plot_memory_efficiency(df, output_dir):
    """
    Plot bytes per key for different indexes across datasets
    """
    fig, ax = plt.subplots(figsize=(12, 6))

    # Get unique indexes and datasets
    indexes = sorted(df['Index'].unique())
    datasets = sorted(df['Dataset'].unique())

    # Group by index and dataset, take mean across workloads
    memory_data = df.groupby(['Index', 'Dataset'])['BytesPerKey'].mean().unstack()

    x = np.arange(len(datasets))
    width = 0.12

    for i, index in enumerate(indexes):
        offset = (i - len(indexes)/2) * width
        values = [memory_data.loc[index, ds] if ds in memory_data.columns else 0 for ds in datasets]
        ax.bar(x + offset, values, width,
               label=index,
               color=INDEX_COLORS.get(index, 'gray'),
               alpha=0.7,
               edgecolor='black',
               linewidth=0.5)

    ax.set_xlabel('Dataset', fontweight='bold')
    ax.set_ylabel('Bytes per Key', fontweight='bold')
    ax.set_title('Memory Efficiency Comparison', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(datasets, rotation=45, ha='right')
    ax.legend(loc='upper left', ncol=2)
    ax.grid(True, alpha=0.3, axis='y')

    plt.tight_layout()

    output_file = output_dir / 'memory_efficiency.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"  Saved: {output_file}")
    plt.close()


def main():
    """Main visualization pipeline"""
    print("="*80)
    print("HALI Benchmark Results Visualization")
    print("="*80 + "\n")

    # Paths
    results_dir = Path('results')
    csv_path = results_dir / 'benchmark_results.csv'
    plots_dir = results_dir / 'plots'
    plots_dir.mkdir(exist_ok=True)

    # Load data
    df = load_results(csv_path)
    print()

    # Generate all plots
    print("Generating Pareto frontier plots...")
    for dataset in df['Dataset'].unique():
        for workload in df['Workload'].unique():
            plot_pareto_frontier(df, dataset, workload, plots_dir)

    print("\nGenerating tail latency plots...")
    for dataset in df['Dataset'].unique():
        for workload in df['Workload'].unique():
            plot_latency_cdf(df, dataset, workload, plots_dir)

    print("\nGenerating throughput comparisons...")
    for dataset in df['Dataset'].unique():
        plot_throughput_comparison(df, dataset, plots_dir)

    print("\nGenerating memory efficiency plot...")
    plot_memory_efficiency(df, plots_dir)

    print("\nGenerating performance tables...")
    generate_performance_table(df, results_dir)

    print("\n" + "="*80)
    print("Visualization complete!")
    print(f"All plots saved to: {plots_dir}")
    print("="*80)


if __name__ == '__main__':
    main()
