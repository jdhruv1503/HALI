#!/usr/bin/env python3
"""
WT-HALI Experiment Runner
Systematic benchmarking to find optimal configurations
"""

import subprocess
import json
import csv
import sys
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict
import time

@dataclass
class ExperimentConfig:
    """Single experiment configuration"""
    name: str
    compression_level: float
    buffer_size: float
    dataset_type: str
    dataset_size: int
    workload_type: str
    num_operations: int = 100_000

    def to_args(self) -> List[str]:
        """Convert to command-line arguments"""
        return [
            f"--compression={self.compression_level}",
            f"--buffer={self.buffer_size}",
            f"--dataset={self.dataset_type}",
            f"--size={self.dataset_size}",
            f"--workload={self.workload_type}",
            f"--operations={self.num_operations}",
        ]

class ExperimentSuite:
    """Collection of experiments to run"""

    # Experiment parameters
    COMPRESSION_LEVELS = [0.0, 0.25, 0.5, 0.75, 1.0]
    BUFFER_SIZES = [0.005, 0.01, 0.02, 0.05, 0.10]  # 0.5% to 10%
    DATASET_SIZES = [100_000, 500_000, 1_000_000]
    WORKLOADS = ['read_heavy', 'mixed', 'write_heavy']
    DATASETS = ['clustered', 'sequential', 'lognormal', 'uniform', 'mixed', 'zipfian']

    def __init__(self, simulator_path: str = "./simulator"):
        self.simulator_path = Path(simulator_path)
        self.results_dir = Path("results/experiments")
        self.results_dir.mkdir(parents=True, exist_ok=True)

    def experiment_2_1_compression_sweep(self) -> List[ExperimentConfig]:
        """
        Experiment 2.1: Compression Level Sweep
        Goal: Find optimal compression for each dataset size
        """
        experiments = []
        for compression in self.COMPRESSION_LEVELS:
            for size in self.DATASET_SIZES:
                for dataset in ['clustered', 'sequential']:  # Best performing
                    exp = ExperimentConfig(
                        name=f"exp2.1_comp{compression}_size{size//1000}k_{dataset}",
                        compression_level=compression,
                        buffer_size=0.01,  # Fixed 1%
                        dataset_type=dataset,
                        dataset_size=size,
                        workload_type='mixed'
                    )
                    experiments.append(exp)
        return experiments

    def experiment_2_2_buffer_sweep(self) -> List[ExperimentConfig]:
        """
        Experiment 2.2: Buffer Size Sweep
        Goal: Find optimal buffer size for each workload
        """
        experiments = []
        for buffer in self.BUFFER_SIZES:
            for workload in self.WORKLOADS:
                exp = ExperimentConfig(
                    name=f"exp2.2_buffer{int(buffer*1000)}_wl{workload}",
                    compression_level=0.5,  # Fixed balanced
                    buffer_size=buffer,
                    dataset_type='clustered',
                    dataset_size=500_000,
                    workload_type=workload
                )
                experiments.append(exp)
        return experiments

    def experiment_2_3_scaling(self) -> List[ExperimentConfig]:
        """
        Experiment 2.3: Dataset Size Scaling
        Goal: Validate scaling behavior
        """
        experiments = []
        sizes = [100_000, 250_000, 500_000, 750_000, 1_000_000]
        for size in sizes:
            exp = ExperimentConfig(
                name=f"exp2.3_scaling_{size//1000}k",
                compression_level=0.0,  # WT-HALI-Speed
                buffer_size=0.01,
                dataset_type='clustered',
                dataset_size=size,
                workload_type='mixed'
            )
            experiments.append(exp)
        return experiments

    def experiment_3_1_expert_count(self) -> List[ExperimentConfig]:
        """
        Experiment 3.1: Expert Count Tuning
        Goal: Validate current formula
        """
        # Note: This requires code modification to override expert count
        # For now, we'll test with different compression levels which affects expert count
        experiments = []
        for compression in [0.0, 0.25, 0.5, 0.75, 1.0]:
            exp = ExperimentConfig(
                name=f"exp3.1_experts_comp{compression}",
                compression_level=compression,
                buffer_size=0.01,
                dataset_type='clustered',
                dataset_size=500_000,
                workload_type='mixed'
            )
            experiments.append(exp)
        return experiments

    def experiment_3_2_workload_spectrum(self) -> List[ExperimentConfig]:
        """
        Experiment 3.2: Workload Spectrum
        Goal: Find sweet spot for read/write ratio
        """
        # Note: Requires custom workload generator modification
        # For now, use existing workload types
        experiments = []
        for workload in ['read_heavy', 'mixed', 'write_heavy']:
            exp = ExperimentConfig(
                name=f"exp3.2_spectrum_{workload}",
                compression_level=0.0,  # WT-HALI-Speed
                buffer_size=0.01,
                dataset_type='clustered',
                dataset_size=500_000,
                workload_type=workload
            )
            experiments.append(exp)
        return experiments

    def get_all_experiments(self, phase: str = "all") -> List[ExperimentConfig]:
        """Get experiments for specified phase"""
        if phase == "2.1" or phase == "all":
            return self.experiment_2_1_compression_sweep()
        elif phase == "2.2":
            return self.experiment_2_2_buffer_sweep()
        elif phase == "2.3":
            return self.experiment_2_3_scaling()
        elif phase == "3.1":
            return self.experiment_3_1_expert_count()
        elif phase == "3.2":
            return self.experiment_3_2_workload_spectrum()
        elif phase == "all":
            exps = []
            exps.extend(self.experiment_2_1_compression_sweep())
            exps.extend(self.experiment_2_2_buffer_sweep())
            exps.extend(self.experiment_2_3_scaling())
            exps.extend(self.experiment_3_1_expert_count())
            exps.extend(self.experiment_3_2_workload_spectrum())
            return exps
        else:
            raise ValueError(f"Unknown phase: {phase}")

    def run_experiment(self, config: ExperimentConfig, dry_run: bool = False) -> Dict:
        """Run a single experiment"""
        print(f"\n{'='*60}")
        print(f"Running: {config.name}")
        print(f"  Compression: {config.compression_level}")
        print(f"  Buffer: {config.buffer_size*100:.1f}%")
        print(f"  Dataset: {config.dataset_type} ({config.dataset_size:,} keys)")
        print(f"  Workload: {config.workload_type}")
        print(f"{'='*60}")

        if dry_run:
            print("[DRY RUN] Would execute:", config.to_args())
            return {"status": "dry_run", "config": asdict(config)}

        # Build command
        cmd = [str(self.simulator_path)] + config.to_args()

        # Run experiment
        start_time = time.time()
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=600  # 10 minute timeout
            )
            elapsed = time.time() - start_time

            if result.returncode != 0:
                print(f"[ERROR] Experiment failed:")
                print(result.stderr)
                return {
                    "status": "failed",
                    "error": result.stderr,
                    "config": asdict(config)
                }

            # Parse output (assuming JSON output from simulator)
            output_data = self._parse_output(result.stdout)

            return {
                "status": "success",
                "config": asdict(config),
                "results": output_data,
                "elapsed_seconds": elapsed
            }

        except subprocess.TimeoutExpired:
            print(f"[TIMEOUT] Experiment exceeded 10 minutes")
            return {
                "status": "timeout",
                "config": asdict(config)
            }
        except Exception as e:
            print(f"[EXCEPTION] {str(e)}")
            return {
                "status": "exception",
                "error": str(e),
                "config": asdict(config)
            }

    def _parse_output(self, stdout: str) -> Dict:
        """Parse simulator output"""
        # Simple parser - look for key metrics in output
        # Assuming format like:
        # Mean Lookup: 54.7 ns
        # Insert Throughput: 14700000 ops/sec
        # Memory: 17.25 bytes/key

        metrics = {}
        for line in stdout.split('\n'):
            if 'Mean Lookup:' in line:
                metrics['lookup_ns'] = float(line.split(':')[1].strip().split()[0])
            elif 'Insert Throughput:' in line:
                val = line.split(':')[1].strip().split()[0]
                metrics['insert_ops_sec'] = float(val)
            elif 'Space per Key:' in line:
                metrics['bytes_per_key'] = float(line.split(':')[1].strip().split()[0])
            elif 'Build Time:' in line:
                metrics['build_ms'] = float(line.split(':')[1].strip().split()[0])

        return metrics

    def save_results(self, results: List[Dict], filename: str = "experiment_results.json"):
        """Save results to file"""
        output_file = self.results_dir / filename
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"\n[SAVED] Results written to {output_file}")

        # Also save as CSV for easier analysis
        csv_file = output_file.with_suffix('.csv')
        self._save_csv(results, csv_file)
        print(f"[SAVED] CSV written to {csv_file}")

    def _save_csv(self, results: List[Dict], filename: Path):
        """Save results as CSV"""
        if not results:
            return

        # Flatten nested dicts for CSV
        rows = []
        for r in results:
            if r['status'] != 'success':
                continue

            row = {}
            row.update(r['config'])
            row.update(r['results'])
            row['elapsed_seconds'] = r.get('elapsed_seconds', 0)
            rows.append(row)

        if not rows:
            return

        with open(filename, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)

def main():
    import argparse

    parser = argparse.ArgumentParser(description='Run WT-HALI experiments')
    parser.add_argument('--phase', default='2.1',
                       help='Experiment phase: 2.1, 2.2, 2.3, 3.1, 3.2, or all')
    parser.add_argument('--dry-run', action='store_true',
                       help='Print experiments without running')
    parser.add_argument('--simulator', default='./build/simulator',
                       help='Path to simulator executable')
    parser.add_argument('--limit', type=int, default=None,
                       help='Limit number of experiments to run')

    args = parser.parse_args()

    # Create experiment suite
    suite = ExperimentSuite(simulator_path=args.simulator)

    # Get experiments for phase
    experiments = suite.get_all_experiments(phase=args.phase)

    if args.limit:
        experiments = experiments[:args.limit]

    print(f"\n{'='*60}")
    print(f"WT-HALI EXPERIMENT RUNNER")
    print(f"{'='*60}")
    print(f"Phase: {args.phase}")
    print(f"Total Experiments: {len(experiments)}")
    print(f"Dry Run: {args.dry_run}")
    print(f"{'='*60}\n")

    if args.dry_run:
        for exp in experiments:
            print(f"{exp.name}: {exp.to_args()}")
        return

    # Confirm before running
    response = input(f"Run {len(experiments)} experiments? (yes/no): ")
    if response.lower() not in ['yes', 'y']:
        print("Aborted.")
        return

    # Run experiments
    results = []
    for i, exp in enumerate(experiments, 1):
        print(f"\n[{i}/{len(experiments)}]")
        result = suite.run_experiment(exp, dry_run=args.dry_run)
        results.append(result)

        # Save intermediate results
        if i % 5 == 0:
            suite.save_results(results, f"experiment_results_phase{args.phase}_checkpoint.json")

    # Save final results
    suite.save_results(results, f"experiment_results_phase{args.phase}_final.json")

    # Print summary
    print(f"\n{'='*60}")
    print(f"EXPERIMENT SUMMARY")
    print(f"{'='*60}")
    successes = sum(1 for r in results if r['status'] == 'success')
    failures = sum(1 for r in results if r['status'] == 'failed')
    timeouts = sum(1 for r in results if r['status'] == 'timeout')
    print(f"Successes: {successes}/{len(results)}")
    print(f"Failures: {failures}")
    print(f"Timeouts: {timeouts}")
    print(f"{'='*60}\n")

if __name__ == '__main__':
    main()
