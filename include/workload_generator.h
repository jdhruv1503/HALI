#pragma once

#include <vector>
#include <random>
#include <cstdint>
#include <string>

namespace hali {

/**
 * @brief Type of workload operation
 */
enum class OpType {
    INSERT,
    FIND,
    ERASE
};

/**
 * @brief Single workload operation
 */
struct Operation {
    OpType type;
    uint64_t key;
    uint64_t value;

    Operation(OpType t, uint64_t k, uint64_t v = 0)
        : type(t), key(k), value(v) {}
};

/**
 * @brief Workload generator for benchmarking
 */
class WorkloadGenerator {
private:
    std::mt19937_64 rng;

public:
    WorkloadGenerator(uint64_t seed = 42) : rng(seed) {}

    /**
     * @brief Generate read-heavy workload (95% find, 5% insert)
     * @param keys Available keys for lookups
     * @param num_ops Number of operations to generate
     * @return Vector of operations
     */
    std::vector<Operation> generate_read_heavy(
        const std::vector<uint64_t>& keys, size_t num_ops) {

        std::vector<Operation> ops;
        ops.reserve(num_ops);

        std::uniform_real_distribution<double> op_dist(0.0, 1.0);
        std::uniform_int_distribution<size_t> key_dist(0, keys.size() - 1);
        std::uniform_int_distribution<uint64_t> new_key_dist;

        for (size_t i = 0; i < num_ops; ++i) {
            double choice = op_dist(rng);

            if (choice < 0.95) {
                // 95% find
                uint64_t key = keys[key_dist(rng)];
                ops.emplace_back(OpType::FIND, key);
            } else {
                // 5% insert
                uint64_t new_key = new_key_dist(rng);
                ops.emplace_back(OpType::INSERT, new_key, new_key);
            }
        }

        return ops;
    }

    /**
     * @brief Generate write-heavy workload (90% insert, 10% find)
     * @param keys Available keys for lookups
     * @param num_ops Number of operations to generate
     * @return Vector of operations
     */
    std::vector<Operation> generate_write_heavy(
        const std::vector<uint64_t>& keys, size_t num_ops) {

        std::vector<Operation> ops;
        ops.reserve(num_ops);

        std::uniform_real_distribution<double> op_dist(0.0, 1.0);
        std::uniform_int_distribution<size_t> key_dist(0, keys.size() - 1);

        // Generate monotonically increasing keys with temporal locality
        uint64_t current_key = keys.empty() ? 0 : keys.back() + 1;
        std::uniform_int_distribution<uint64_t> increment(1, 1000);

        for (size_t i = 0; i < num_ops; ++i) {
            double choice = op_dist(rng);

            if (choice < 0.90) {
                // 90% insert with temporal locality
                ops.emplace_back(OpType::INSERT, current_key, current_key);
                current_key += increment(rng);
            } else {
                // 10% find from existing keys
                if (!keys.empty()) {
                    uint64_t key = keys[key_dist(rng)];
                    ops.emplace_back(OpType::FIND, key);
                }
            }
        }

        return ops;
    }

    /**
     * @brief Generate mixed workload (50% find, 50% insert)
     * @param keys Available keys for lookups
     * @param num_ops Number of operations to generate
     * @return Vector of operations
     */
    std::vector<Operation> generate_mixed(
        const std::vector<uint64_t>& keys, size_t num_ops) {

        std::vector<Operation> ops;
        ops.reserve(num_ops);

        std::uniform_real_distribution<double> op_dist(0.0, 1.0);
        std::uniform_int_distribution<size_t> key_dist(0, keys.size() - 1);
        std::uniform_int_distribution<uint64_t> new_key_dist;

        for (size_t i = 0; i < num_ops; ++i) {
            double choice = op_dist(rng);

            if (choice < 0.50) {
                // 50% find
                if (!keys.empty()) {
                    uint64_t key = keys[key_dist(rng)];
                    ops.emplace_back(OpType::FIND, key);
                }
            } else {
                // 50% insert
                uint64_t new_key = new_key_dist(rng);
                ops.emplace_back(OpType::INSERT, new_key, new_key);
            }
        }

        return ops;
    }

    /**
     * @brief Get workload name as string
     */
    static std::string workload_name(const std::string& type) {
        if (type == "read_heavy") return "Read-Heavy (95R/5W)";
        if (type == "write_heavy") return "Write-Heavy (10R/90W)";
        if (type == "mixed") return "Mixed (50R/50W)";
        return "Unknown";
    }
};

} // namespace hali
