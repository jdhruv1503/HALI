# WT-HALI Experiment Quick Start Guide

This guide walks you through running the complete experimental suite to find optimal WT-HALI configurations.

## Prerequisites

1. **Docker Environment Set Up**
   ```bash
   # Build Docker image
   docker build -t wt-hali-research .

   # Run container with fixed resources
   docker run --name wt-hali -it --rm \
     --cpus="16" --memory="8g" \
     -v "$(pwd):/workspace" \
     wt-hali-research bash
   ```

2. **Build Simulator**
   ```bash
   cd /workspace
   mkdir -p build && cd build
   cmake ..
   make -j$(nproc)
   ```

3. **Verify Build**
   ```bash
   ./simulator --help  # Should show help message
   ```

## Running Experiments

### Phase 1: Setup

1. **Make scripts executable**
   ```bash
   chmod +x scripts/*.py scripts/*.sh
   ```

2. **Test with dry run**
   ```bash
   cd /workspace
   python3 scripts/experiment_runner.py --phase=2.1 --dry-run --limit=5
   ```

### Phase 2: Run Experiments

**Important:** Experiments will take 6-12 hours total. Run in phases with checkpoints.

#### Phase 2.1: Compression Level Sweep (~2-3 hours)
```bash
# Run compression experiments
python3 scripts/experiment_runner.py --phase=2.1 \
  --simulator=./build/simulator

# Results saved to: results/experiments/experiment_results_phase2.1_final.json
```

**What this tests:**
- 5 compression levels (0.0, 0.25, 0.5, 0.75, 1.0)
- 3 dataset sizes (100K, 500K, 1M)
- 2 datasets (clustered, sequential)
- **Total:** 30 configurations

**Expected output:**
- Optimal compression level for each dataset size
- Performance/memory trade-offs

#### Phase 2.2: Buffer Size Sweep (~1 hour)
```bash
# Run buffer size experiments
python3 scripts/experiment_runner.py --phase=2.2 \
  --simulator=./build/simulator

# Results saved to: results/experiments/experiment_results_phase2.2_final.json
```

**What this tests:**
- 5 buffer sizes (0.5%, 1%, 2%, 5%, 10%)
- 3 workload types (read_heavy, mixed, write_heavy)
- **Total:** 15 configurations

**Expected output:**
- Optimal buffer size for each workload type

#### Phase 2.3: Scaling Analysis (~2 hours)
```bash
# Run scaling experiments
python3 scripts/experiment_runner.py --phase=2.3 \
  --simulator=./build/simulator

# Results saved to: results/experiments/experiment_results_phase2.3_final.json
```

**What this tests:**
- 5 dataset sizes (100K, 250K, 500K, 750K, 1M)
- WT-HALI-Speed vs baselines
- **Total:** 5-20 configurations (depending on baselines)

**Expected output:**
- Scaling behavior validation
- Crossover points vs RMI/PGM

### Phase 3: Analyze Results

1. **Run analysis script**
   ```bash
   python3 scripts/analyze_experiments.py --results-dir=results/experiments
   ```

2. **Check generated files**
   ```bash
   ls results/analysis/          # Plots
   ls EXPERIMENT_RESULTS.md      # Summary report
   ls include/wt_hali_config_selector.h  # Generated code
   ```

**Outputs:**
- `results/analysis/compression_heatmap.png` - Heatmap visualization
- `results/analysis/buffer_impact.png` - Buffer size charts
- `results/analysis/scaling_behavior.png` - Scaling plots
- `EXPERIMENT_RESULTS.md` - Markdown summary report
- `include/wt_hali_config_selector.h` - Auto-config selector code

### Phase 4: Validation

1. **Run validation suite**
   ```bash
   cd build
   ./validate

   # Should show 100% pass rate for optimal configs
   ```

2. **Test auto-configuration**
   ```bash
   # TODO: Add test program that uses WTHALIConfigSelector
   ```

### Phase 5: Update Documentation

1. **Review experimental results**
   ```bash
   cat EXPERIMENT_RESULTS.md
   ```

2. **Update README.md**
   - Add "Optimal Configuration Guide" section
   - Include recommended configs from EXPERIMENT_RESULTS.md
   - Update performance tables with optimal results

3. **Update presentation**
   - Replace "TODO" placeholders with actual results
   - Add experimental methodology slides
   - Update performance charts

### Phase 6: Final Commit

```bash
git add -A
git commit -m "Complete experimental analysis with optimal configurations

Experimental Results:
- Compression sweep: 30 configs tested
- Buffer sweep: 15 configs tested
- Scaling analysis: 5-20 configs tested

Optimal Configurations Identified:
- [List from EXPERIMENT_RESULTS.md]

Documentation Updated:
- README.md with configuration guide
- EXPERIMENT_RESULTS.md with full analysis
- Presentation with experimental findings
- Auto-configuration selector implemented

Validation: 100% correctness on all optimal configs
"
git push
```

## Quick Commands Reference

```bash
# Run all experiments (LONG - 6+ hours)
python3 scripts/experiment_runner.py --phase=all \
  --simulator=./build/simulator

# Run specific phase
python3 scripts/experiment_runner.py --phase=2.1 \
  --simulator=./build/simulator

# Dry run to preview experiments
python3 scripts/experiment_runner.py --phase=2.1 --dry-run

# Limit number of experiments (for testing)
python3 scripts/experiment_runner.py --phase=2.1 --limit=5

# Analyze results
python3 scripts/analyze_experiments.py

# Run validation
cd build && ./validate
```

## Troubleshooting

### Experiment fails with "timeout"
- Increase timeout in `experiment_runner.py` (line 162)
- Reduce dataset size for testing

### Simulator crashes
- Check memory limits in Docker
- Verify dataset size is reasonable
- Check simulator logs

### Analysis fails with "no results found"
- Verify experiments completed successfully
- Check results directory path
- Ensure JSON files exist

### Generated config selector doesn't compile
- Check C++ syntax in generated file
- Verify include paths
- Manually edit if needed

## Expected Timeline

| Phase | Duration | Checkpoint |
|-------|----------|------------|
| 2.1 Compression Sweep | 2-3 hours | 30 results |
| 2.2 Buffer Sweep | 1 hour | 15 results |
| 2.3 Scaling | 2 hours | 5-20 results |
| **Day 1 Total** | **5-6 hours** | **50-65 results** |
| | | |
| 3.1 Analysis | 30 min | Plots + report |
| 3.2 Validation | 30 min | 100% pass |
| 3.3 Documentation | 2 hours | Docs updated |
| **Day 2 Total** | **3 hours** | **Complete** |

## Next Steps After Experiments

1. **Review EXPERIMENT_RESULTS.md** for optimal configurations
2. **Integrate auto-configuration selector** into production code
3. **Update README.md** with findings
4. **Update presentation** with results
5. **Run final validation** suite
6. **Commit and push** all changes

## Support

If you encounter issues:
1. Check EXPERIMENTAL_PROTOCOL.md for detailed methodology
2. Review experiment_runner.py source code
3. Manually run single configurations for debugging
4. Check Docker resource allocation

## Files Created by Experiments

```
results/
├── experiments/
│   ├── experiment_results_phase2.1_final.json
│   ├── experiment_results_phase2.1_final.csv
│   ├── experiment_results_phase2.2_final.json
│   ├── experiment_results_phase2.2_final.csv
│   ├── experiment_results_phase2.3_final.json
│   └── experiment_results_phase2.3_final.csv
└── analysis/
    ├── compression_heatmap.png
    ├── buffer_impact.png
    └── scaling_behavior.png

include/
└── wt_hali_config_selector.h  # Auto-generated

EXPERIMENT_RESULTS.md  # Summary report
```

Good luck! 🚀
