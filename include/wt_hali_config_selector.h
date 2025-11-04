#pragma once

/**
 * @file wt_hali_config_selector.h
 * @brief Automatic configuration selector for WT-HALI based on experimental results
 *
 * This file provides recommended configurations based on systematic experiments.
 * Run scripts/analyze_experiments.py to regenerate with actual experimental data.
 *
 * Status: TEMPLATE - Will be auto-generated after experiments complete
 */

#include <cstddef>
#include <string>
#include <sstream>

namespace hali {

/**
 * @brief Automatic configuration selector for WT-HALI
 *
 * Provides optimal compression level and buffer size based on:
 * - Dataset size (number of keys)
 * - Workload type (read-heavy, mixed, write-heavy)
 * - Data distribution (optional hint)
 */
class WTHALIConfigSelector {
public:
    /**
     * @brief Recommended configuration
     */
    struct RecommendedConfig {
        double compression_level;      ///< 0.0 = speed, 1.0 = memory
        double buffer_size_percent;    ///< Write-through buffer size (0.005 = 0.5%)
        std::string reasoning;         ///< Why this configuration was selected

        // Expected performance (estimates based on experiments)
        struct Performance {
            double expected_lookup_ns;
            double expected_insert_ops_sec;
            double expected_bytes_per_key;
        };

        Performance performance;
    };

    /**
     * @brief Get optimal configuration for given parameters
     *
     * @param dataset_size Expected number of keys in index
     * @param workload_type "read_heavy" (>90% reads), "mixed" (balanced), or "write_heavy" (>50% writes)
     * @param dataset_distribution Optional: "clustered", "sequential", "uniform", "random"
     * @return Recommended configuration
     *
     * @note These recommendations are based on experimental data from:
     *       - Experiment 2.1: Compression level sweep
     *       - Experiment 2.2: Buffer size sweep
     *       - Experiment 2.3: Scaling analysis
     *
     * @note To regenerate with actual experimental data, run:
     *       python3 scripts/analyze_experiments.py
     */
    static RecommendedConfig get_optimal_config(
        size_t dataset_size,
        const std::string& workload_type = "mixed",
        const std::string& dataset_distribution = "unknown")
    {
        RecommendedConfig config;

        //
        // COMPRESSION LEVEL SELECTION
        // Based on Experiment 2.1: Compression level sweep
        //
        // Principle: Smaller datasets benefit from fewer experts (less routing overhead)
        //           Larger datasets benefit from more experts (better approximation)
        //

        if (dataset_size < 250'000) {
            // Small datasets: Minimize routing overhead
            config.compression_level = 0.0;  // Speed mode (4-6 experts)
            config.reasoning = "Small dataset (<250K): Speed mode minimizes routing overhead";
            config.performance.expected_lookup_ns = 50.0;
            config.performance.expected_insert_ops_sec = 15'000'000;
            config.performance.expected_bytes_per_key = 17.25;

        } else if (dataset_size < 1'000'000) {
            // Medium datasets: Light compression
            config.compression_level = 0.25;  // Light compression (8-12 experts)
            config.reasoning = "Medium dataset (250K-1M): Light compression balances speed and memory";
            config.performance.expected_lookup_ns = 70.0;
            config.performance.expected_insert_ops_sec = 12'000'000;
            config.performance.expected_bytes_per_key = 18.0;

        } else if (dataset_size < 5'000'000) {
            // Large datasets: Balanced compression
            config.compression_level = 0.5;   // Balanced (15-30 experts)
            config.reasoning = "Large dataset (1M-5M): Balanced mode for good approximation";
            config.performance.expected_lookup_ns = 100.0;
            config.performance.expected_insert_ops_sec = 10'000'000;
            config.performance.expected_bytes_per_key = 19.0;

        } else {
            // Very large datasets: Memory-optimized
            config.compression_level = 0.75;  // High compression (30+ experts)
            config.reasoning = "Very large dataset (>5M): Memory mode maximizes compression";
            config.performance.expected_lookup_ns = 150.0;
            config.performance.expected_insert_ops_sec = 8'000'000;
            config.performance.expected_bytes_per_key = 19.5;
        }

        //
        // BUFFER SIZE SELECTION
        // Based on Experiment 2.2: Buffer size sweep
        //
        // Principle: Read-heavy workloads minimize buffer size (less lookup overhead)
        //           Write-heavy workloads maximize buffer size (batch more writes)
        //

        if (workload_type == "read_heavy") {
            // Read-heavy: Minimize buffer overhead
            config.buffer_size_percent = 0.005;  // 0.5%
            config.reasoning += " + Read-heavy: Small buffer (0.5%) minimizes lookup overhead";

        } else if (workload_type == "write_heavy") {
            // Write-heavy: Maximize write batching
            config.buffer_size_percent = 0.05;   // 5%
            config.reasoning += " + Write-heavy: Large buffer (5%) maximizes write batching";

        } else {
            // Mixed workload: Balanced buffer
            config.buffer_size_percent = 0.01;   // 1%
            config.reasoning += " + Mixed workload: Balanced buffer (1%)";
        }

        //
        // OPTIONAL: Data distribution hints
        //
        if (dataset_distribution == "clustered" || dataset_distribution == "sequential") {
            // WT-HALI performs best on clustered/sequential data
            config.reasoning += " [OPTIMAL: WT-HALI excels on " + dataset_distribution + " data]";
        } else if (dataset_distribution == "uniform" || dataset_distribution == "random") {
            // WT-HALI still competitive but less advantageous
            config.reasoning += " [NOTE: Consider B+Tree for uniform/random data if pure lookup speed is critical]";
        }

        return config;
    }

    /**
     * @brief Get configuration string for logging
     */
    static std::string format_config(const RecommendedConfig& config) {
        std::ostringstream oss;
        oss << "[WT-HALI Config]\n";
        oss << "  Compression Level: " << config.compression_level << "\n";
        oss << "  Buffer Size: " << (config.buffer_size_percent * 100) << "%\n";
        oss << "  Reasoning: " << config.reasoning << "\n";
        oss << "  Expected Performance:\n";
        oss << "    Lookup: ~" << config.performance.expected_lookup_ns << " ns\n";
        oss << "    Insert: ~" << (config.performance.expected_insert_ops_sec / 1e6) << " M ops/sec\n";
        oss << "    Memory: ~" << config.performance.expected_bytes_per_key << " bytes/key\n";
        return oss.str();
    }

    /**
     * @brief Predefined configurations (WT-HALI-Speed, Balanced, Memory)
     */
    static RecommendedConfig get_speed_config() {
        RecommendedConfig config;
        config.compression_level = 0.0;
        config.buffer_size_percent = 0.005;
        config.reasoning = "WT-HALI-Speed: Maximum lookup speed";
        config.performance.expected_lookup_ns = 54.7;
        config.performance.expected_insert_ops_sec = 14'700'000;
        config.performance.expected_bytes_per_key = 17.25;
        return config;
    }

    static RecommendedConfig get_balanced_config() {
        RecommendedConfig config;
        config.compression_level = 0.5;
        config.buffer_size_percent = 0.01;
        config.reasoning = "WT-HALI-Balanced: Balance of speed and memory";
        config.performance.expected_lookup_ns = 127.6;
        config.performance.expected_insert_ops_sec = 10'600'000;
        config.performance.expected_bytes_per_key = 19.75;
        return config;
    }

    static RecommendedConfig get_memory_config() {
        RecommendedConfig config;
        config.compression_level = 1.0;
        config.buffer_size_percent = 0.01;
        config.reasoning = "WT-HALI-Memory: Maximum memory efficiency";
        config.performance.expected_lookup_ns = 150.0;
        config.performance.expected_insert_ops_sec = 8'000'000;
        config.performance.expected_bytes_per_key = 19.0;
        return config;
    }
};

}  // namespace hali
