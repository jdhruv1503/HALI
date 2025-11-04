#pragma once

#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hali {

/**
 * @brief Robust data generators for realistic benchmark datasets
 */
class DataGenerator {
public:
    /**
     * @brief Generate lognormal distributed data
     * Models file sizes, network traffic patterns
     * @param n Number of keys to generate
     * @param mu Mean of underlying normal distribution
     * @param sigma Standard deviation of underlying normal
     * @param seed Random seed
     * @return Sorted vector of 64-bit keys
     */
    static std::vector<uint64_t> generate_lognormal(
        size_t n, double mu = 0.0, double sigma = 2.0, uint64_t seed = 42) {

        std::mt19937_64 rng(seed);
        std::lognormal_distribution<double> dist(mu, sigma);

        std::vector<uint64_t> keys;
        keys.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            double val = dist(rng);
            // Scale to 64-bit range and clamp
            uint64_t key = static_cast<uint64_t>(
                std::min(val * 1e9, static_cast<double>(UINT64_MAX))
            );
            keys.push_back(key);
        }

        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        return keys;
    }

    /**
     * @brief Generate Zipfian distributed data
     * Models access patterns, popularity distributions
     * @param n Number of keys to generate
     * @param alpha Skewness parameter (higher = more skewed)
     * @param seed Random seed
     * @return Sorted vector of 64-bit keys
     */
    static std::vector<uint64_t> generate_zipfian(
        size_t n, double alpha = 1.5, uint64_t seed = 42) {

        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> uniform(0.0, 1.0);

        // Precompute normalization constant
        double c = 0.0;
        for (size_t i = 1; i <= n; ++i) {
            c += 1.0 / std::pow(i, alpha);
        }
        c = 1.0 / c;

        std::vector<uint64_t> keys;
        keys.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            double z = uniform(rng);
            double sum_prob = 0.0;
            for (size_t rank = 1; rank <= n; ++rank) {
                sum_prob += c / std::pow(rank, alpha);
                if (sum_prob >= z) {
                    keys.push_back(rank * 1000000); // Scale up
                    break;
                }
            }
        }

        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        return keys;
    }

    /**
     * @brief Generate clustered data with gaps
     * Models partitioned/sharded data with locality
     * @param n Number of keys per cluster
     * @param num_clusters Number of clusters
     * @param gap_size Gap between clusters
     * @param seed Random seed
     * @return Sorted vector of 64-bit keys
     */
    static std::vector<uint64_t> generate_clustered(
        size_t n, size_t num_clusters = 10, uint64_t gap_size = 1000000000,
        uint64_t seed = 42) {

        std::mt19937_64 rng(seed);
        std::vector<uint64_t> keys;
        keys.reserve(n * num_clusters);

        for (size_t cluster = 0; cluster < num_clusters; ++cluster) {
            uint64_t cluster_base = cluster * gap_size;
            std::normal_distribution<double> dist(0.0, gap_size * 0.1);

            for (size_t i = 0; i < n; ++i) {
                double offset = std::abs(dist(rng));
                uint64_t key = cluster_base + static_cast<uint64_t>(offset);
                keys.push_back(key);
            }
        }

        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        return keys;
    }

    /**
     * @brief Generate sequential data with periodic gaps
     * Models time-series with discontinuities
     * @param n Number of keys to generate
     * @param gap_frequency Insert gap every N keys
     * @param gap_size Size of the gap
     * @param seed Random seed
     * @return Sorted vector of 64-bit keys
     */
    static std::vector<uint64_t> generate_sequential_with_gaps(
        size_t n, size_t gap_frequency = 10000, uint64_t gap_size = 1000000,
        uint64_t seed = 42) {

        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<uint64_t> jitter(0, 1000);

        std::vector<uint64_t> keys;
        keys.reserve(n);

        uint64_t current = 0;
        for (size_t i = 0; i < n; ++i) {
            if (i > 0 && i % gap_frequency == 0) {
                current += gap_size;
            }
            keys.push_back(current + jitter(rng));
            current += 1 + jitter(rng);
        }

        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        return keys;
    }

    /**
     * @brief Generate mixed workload data
     * Combination of uniform, normal, and exponential distributions
     * @param n Number of keys to generate
     * @param seed Random seed
     * @return Sorted vector of 64-bit keys
     */
    static std::vector<uint64_t> generate_mixed(
        size_t n, uint64_t seed = 42) {

        std::mt19937_64 rng(seed);
        std::vector<uint64_t> keys;
        keys.reserve(n);

        size_t third = n / 3;

        // 1/3 uniform
        std::uniform_int_distribution<uint64_t> uniform(0, UINT64_MAX / 3);
        for (size_t i = 0; i < third; ++i) {
            keys.push_back(uniform(rng));
        }

        // 1/3 normal
        std::normal_distribution<double> normal(UINT64_MAX / 2.0, UINT64_MAX / 10.0);
        for (size_t i = 0; i < third; ++i) {
            double val = std::abs(normal(rng));
            uint64_t key = static_cast<uint64_t>(
                std::min(val, static_cast<double>(UINT64_MAX))
            );
            keys.push_back(key);
        }

        // 1/3 exponential
        std::exponential_distribution<double> exponential(0.0000001);
        for (size_t i = 0; i < third; ++i) {
            double val = exponential(rng);
            uint64_t key = static_cast<uint64_t>(
                std::min(val, static_cast<double>(UINT64_MAX))
            );
            keys.push_back(key);
        }

        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        return keys;
    }

    /**
     * @brief Generate uniform random data
     * Baseline for comparison
     * @param n Number of keys to generate
     * @param seed Random seed
     * @return Sorted vector of 64-bit keys
     */
    static std::vector<uint64_t> generate_uniform(
        size_t n, uint64_t seed = 42) {

        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

        std::vector<uint64_t> keys;
        keys.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            keys.push_back(dist(rng));
        }

        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        return keys;
    }
};

} // namespace hali
