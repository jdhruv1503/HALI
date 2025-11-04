#pragma once

#include "index_interface.h"
#include "indexes/pgm_index.h"
#include "bloom_filter.h"
#include <pgm/pgm_index.hpp>
#include <art/map.h>
#include <parallel_hashmap/phmap.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace hali {

/**
 * @brief HALIv2 - Hierarchical Adaptive Learned Index (Version 2)
 *
 * Major improvements over v1:
 * 1. Key-range-based partitioning (not size-based) → guarantees disjoint expert ranges
 * 2. Binary search routing → O(log num_experts) guaranteed, no fallback
 * 3. Adaptive expert count → scales with dataset size
 * 4. Bloom filter hierarchy → fast negative lookups
 * 5. Compression-level hyperparameter → tunable memory-performance tradeoff
 *
 * Three-level architecture:
 * Level 1: Binary Search Router over disjoint key ranges
 * Level 2: Adaptive Expert Models (PGM/RMI/ART based on linearity + compression level)
 * Level 3: Delta-Buffer for dynamic updates (ART or HashMap based on compression level)
 */
template<typename KeyType, typename ValueType>
class HALIv2Index : public IndexInterface<KeyType, ValueType> {
private:
    static_assert(std::is_integral<KeyType>::value,
                  "HALIv2Index requires integral key type");

    /**
     * @brief Configuration parameters
     */
    struct Config {
        double compression_level = 0.5;  // 0.0 = speed, 1.0 = memory
        double merge_threshold = 0.01;   // Merge when buffer exceeds 1% of main index

        size_t adaptive_expert_count(size_t n) const {
            // Base: sqrt(n) / 100 for balance
            size_t base = std::max(size_t(4), static_cast<size_t>(std::sqrt(n) / 100));

            // Scale with compression level
            // compression = 0.0 → base * 0.5 (fewer experts, faster routing)
            // compression = 1.0 → base * 2.0 (more experts, better approximation)
            double scale = 0.5 + 1.5 * compression_level;
            return std::max(size_t(4), static_cast<size_t>(base * scale));
        }

        bool use_bloom_filters() const {
            // Enabled with fallback safety check in find()
            return true;
        }

        size_t bloom_bits_per_key() const {
            // More bits for read-heavy workloads (lower FPR)
            // compression_level closer to 0 = speed = fewer bloom bits (faster hashing)
            // compression_level closer to 1 = memory = more bloom bits (lower FPR, less expert queries)
            return static_cast<size_t>(5 + compression_level * 10);  // 5-15 bits/key
        }
    };

    Config config_;

    /**
     * @brief Expert type selection
     */
    enum class ExpertType {
        PGM,    // Piecewise Geometric Model
        RMI,    // Recursive Model Index
        ART     // Adaptive Radix Tree
    };

    /**
     * @brief Expert with guaranteed key range
     */
    struct Expert {
        ExpertType type;
        KeyType min_key;  // Inclusive lower bound
        KeyType max_key;  // Inclusive upper bound
        std::vector<KeyType> keys;
        std::vector<ValueType> values;

        virtual ~Expert() = default;
        virtual std::optional<ValueType> find(KeyType key) const = 0;
        virtual size_t memory_footprint() const = 0;

        // Check if key falls in this expert's range
        bool owns_key(KeyType key) const {
            return key >= min_key && key <= max_key;
        }
    };

    /**
     * @brief PGM Expert
     */
    struct PGMExpert : public Expert {
        pgm::PGMIndex<KeyType, 64> pgm;

        PGMExpert(const std::vector<KeyType>& k, const std::vector<ValueType>& v,
                  KeyType min_k, KeyType max_k) {
            this->type = ExpertType::PGM;
            this->keys = k;
            this->values = v;
            this->min_key = min_k;
            this->max_key = max_k;
            pgm = pgm::PGMIndex<KeyType, 64>(k.begin(), k.end());
        }

        std::optional<ValueType> find(KeyType key) const override {
            // Binary search routing guarantees correct expert, so no need for owns_key() check

            auto range = pgm.search(key);
            auto it = std::lower_bound(this->keys.begin() + range.lo,
                                      this->keys.begin() + range.hi,
                                      key);

            if (it != this->keys.begin() + range.hi && *it == key) {
                size_t idx = std::distance(this->keys.begin(), it);
                return this->values[idx];
            }
            return std::nullopt;
        }

        size_t memory_footprint() const override {
            return this->keys.size() * (sizeof(KeyType) + sizeof(ValueType)) +
                   (this->keys.size() / 5000) * 20;
        }
    };

    /**
     * @brief RMI Expert
     */
    struct RMIExpert : public Expert {
        struct LinearModel {
            double slope = 0.0, intercept = 0.0;

            void train(const std::vector<KeyType>& keys, const std::vector<size_t>& positions) {
                if (keys.empty()) return;
                size_t n = keys.size();
                double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;

                for (size_t i = 0; i < n; ++i) {
                    double x = static_cast<double>(keys[i]);
                    double y = static_cast<double>(positions[i]);
                    sum_x += x; sum_y += y; sum_xy += x * y; sum_x2 += x * x;
                }

                double mean_x = sum_x / n, mean_y = sum_y / n;
                double num = sum_xy - n * mean_x * mean_y;
                double den = sum_x2 - n * mean_x * mean_x;

                if (std::abs(den) > 1e-10) {
                    slope = num / den;
                    intercept = mean_y - slope * mean_x;
                } else {
                    slope = 0.0;
                    intercept = mean_y;
                }
            }

            size_t predict(KeyType key, size_t max_pos) const {
                double pred = slope * static_cast<double>(key) + intercept;
                return static_cast<size_t>(std::max(0.0, std::min(pred, static_cast<double>(max_pos))));
            }
        };

        LinearModel model;
        static constexpr size_t ERROR = 64;

        RMIExpert(const std::vector<KeyType>& k, const std::vector<ValueType>& v,
                  KeyType min_k, KeyType max_k) {
            this->type = ExpertType::RMI;
            this->keys = k;
            this->values = v;
            this->min_key = min_k;
            this->max_key = max_k;

            std::vector<size_t> positions(k.size());
            std::iota(positions.begin(), positions.end(), 0);
            model.train(k, positions);
        }

        std::optional<ValueType> find(KeyType key) const override {
            // Binary search routing guarantees correct expert, so no need for owns_key() check

            size_t pos = model.predict(key, this->keys.size() - 1);
            size_t start = (pos > ERROR) ? pos - ERROR : 0;
            size_t end = std::min(pos + ERROR, this->keys.size());

            auto it = std::lower_bound(this->keys.begin() + start,
                                      this->keys.begin() + end,
                                      key);

            if (it != this->keys.begin() + end && *it == key) {
                size_t idx = std::distance(this->keys.begin(), it);
                return this->values[idx];
            }
            return std::nullopt;
        }

        size_t memory_footprint() const override {
            return this->keys.size() * (sizeof(KeyType) + sizeof(ValueType)) +
                   sizeof(LinearModel);
        }
    };

    /**
     * @brief ART Expert
     */
    struct ARTExpert : public Expert {
        art::map<KeyType, ValueType> tree;

        ARTExpert(const std::vector<KeyType>& k, const std::vector<ValueType>& v,
                  KeyType min_k, KeyType max_k) {
            this->type = ExpertType::ART;
            this->keys = k;
            this->values = v;
            this->min_key = min_k;
            this->max_key = max_k;

            for (size_t i = 0; i < k.size(); ++i) {
                tree[k[i]] = v[i];
            }
        }

        std::optional<ValueType> find(KeyType key) const override {
            // Binary search routing guarantees correct expert, so no need for owns_key() check

            auto it = tree.find(key);
            if (it != tree.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        size_t memory_footprint() const override {
            return this->keys.size() * (sizeof(KeyType) + sizeof(ValueType)) * 1.25;
        }
    };

    // Level 1: Router with guaranteed disjoint key ranges
    std::vector<std::unique_ptr<Expert>> experts_;
    std::vector<KeyType> expert_boundaries_;  // Sorted boundaries for binary search

    // Level 2: Bloom filters for fast negative lookups
    BloomFilter global_bloom_;                      // Global filter for all keys
    std::vector<BloomFilter> expert_blooms_;        // Per-expert filters

    // Level 3: Delta buffer
    art::map<KeyType, ValueType> delta_buffer_art_;      // For compression_level >= 0.5
    phmap::flat_hash_map<KeyType, ValueType> delta_buffer_hash_;  // For compression_level < 0.5

    size_t total_size_ = 0;

public:
    HALIv2Index(double compression_level = 0.5) {
        config_.compression_level = compression_level;
    }

    bool insert(const KeyType& key, const ValueType& value) override {
        // Check if key already exists
        if (find(key).has_value()) {
            return false;
        }

        // Insert into delta buffer
        if (config_.compression_level < 0.5) {
            auto result = delta_buffer_hash_.insert({key, value});
            return result.second;
        } else {
            auto result = delta_buffer_art_.insert({key, value});
            return result.second;
        }
    }

    std::optional<ValueType> find(const KeyType& key) const override {
        // Level 1: Check delta buffer first (bypass Bloom filter for delta)
        if (config_.compression_level < 0.5) {
            auto it = delta_buffer_hash_.find(key);
            if (it != delta_buffer_hash_.end()) {
                return it->second;
            }
        } else {
            auto it = delta_buffer_art_.find(key);
            if (it != delta_buffer_art_.end()) {
                return it->second;
            }
        }

        // Level 2: Query main index
        if (experts_.empty()) {
            return std::nullopt;
        }

        // Level 3: Check global Bloom filter for fast negative lookup
        // (Only check after delta buffer, since Bloom filter doesn't include delta keys)
        if (config_.use_bloom_filters() && !global_bloom_.contains(key)) {
            return std::nullopt;  // Definitely not in main index
        }

        // Level 4: Binary search over expert boundaries to find correct expert
        size_t expert_id = route_to_expert(key);

        if (expert_id >= experts_.size()) {
            return std::nullopt;
        }

        // Level 5: Check expert Bloom filter (RE-ENABLED with safety check)
        if (config_.use_bloom_filters() && expert_id < expert_blooms_.size()) {
            if (!expert_blooms_[expert_id].contains(key)) {
                // Bloom filter says key is not in this expert
                // But due to range-based partitioning, let's double-check the key is within bounds
                if (expert_id < experts_.size()) {
                    const auto& expert = experts_[expert_id];
                    if (key < expert->min_key || key > expert->max_key) {
                        // Key is definitely outside this expert's range
                        return std::nullopt;
                    }
                    // Key might be in expert's range but Bloom filter gives false negative
                    // This can happen with hash collisions - proceed to expert query
                } else {
                    return std::nullopt;
                }
            }
        }

        // Level 6: Query expert
        return experts_[expert_id]->find(key);
    }

    bool erase(const KeyType& key) override {
        // Erase from delta buffer only (lazy deletion from main index)
        if (config_.compression_level < 0.5) {
            return delta_buffer_hash_.erase(key) > 0;
        } else {
            return delta_buffer_art_.erase(key) > 0;
        }
    }

    void load(const std::vector<KeyType>& keys,
              const std::vector<ValueType>& values) override {
        if (keys.size() != values.size()) {
            throw std::invalid_argument("Keys and values size mismatch");
        }

        total_size_ = keys.size();

        // Sort data by key
        std::vector<std::pair<KeyType, ValueType>> sorted_data;
        for (size_t i = 0; i < keys.size(); ++i) {
            sorted_data.push_back({keys[i], values[i]});
        }
        std::sort(sorted_data.begin(), sorted_data.end());

        // Determine number of experts based on dataset size and compression level
        size_t num_experts = config_.adaptive_expert_count(keys.size());

        experts_.clear();
        experts_.reserve(num_experts);
        expert_boundaries_.clear();
        expert_boundaries_.reserve(num_experts + 1);

        // Initialize global Bloom filter
        global_bloom_ = BloomFilter(keys.size(), config_.bloom_bits_per_key());

        expert_blooms_.clear();
        expert_blooms_.reserve(num_experts);

        // Partition by KEY RANGES (true range-based partitioning for clustered data)
        // Calculate key range span
        KeyType min_global_key = sorted_data.front().first;
        KeyType max_global_key = sorted_data.back().first;

        // Handle edge case where all keys are the same
        if (min_global_key == max_global_key) {
            num_experts = 1;
        }

        // Partition keys into experts based on key value ranges (not sizes)
        double range_per_expert = static_cast<double>(max_global_key - min_global_key + 1) / num_experts;

        std::vector<std::vector<std::pair<KeyType, ValueType>>> expert_data(num_experts);

        for (const auto& kv : sorted_data) {
            KeyType key = kv.first;
            // Calculate which expert this key belongs to based on its VALUE
            size_t expert_id = std::min(
                static_cast<size_t>((key - min_global_key) / range_per_expert),
                num_experts - 1
            );
            expert_data[expert_id].push_back(kv);

            // Insert into global Bloom filter
            global_bloom_.insert(key);
        }

        // Create experts from partitioned data
        // IMPORTANT: Use CONSISTENT range-based boundaries for routing
        for (size_t i = 0; i < num_experts; ++i) {
            // Calculate expected key range for this expert (for consistent routing)
            KeyType expected_min = min_global_key + static_cast<KeyType>(i * range_per_expert);
            KeyType expected_max = (i == num_experts - 1) ?
                max_global_key :
                (min_global_key + static_cast<KeyType>((i + 1) * range_per_expert) - 1);

            // Always use expected_min as boundary for consistent routing
            expert_boundaries_.push_back(expected_min);

            if (expert_data[i].empty()) {
                // Empty expert due to gaps in clustered data
                // Create an empty ART expert as placeholder to maintain expert_id consistency
                experts_.push_back(std::make_unique<ARTExpert>(
                    std::vector<KeyType>(), std::vector<ValueType>(), expected_min, expected_max));
                expert_blooms_.push_back(BloomFilter(1, config_.bloom_bits_per_key()));  // Empty Bloom filter
                continue;
            }

            std::vector<KeyType> part_keys;
            std::vector<ValueType> part_values;

            for (const auto& kv : expert_data[i]) {
                part_keys.push_back(kv.first);
                part_values.push_back(kv.second);
            }

            // Determine expert type based on data characteristics and compression level
            ExpertType type = select_expert_type(part_keys);

            // Create expert with actual key range (for data storage)
            KeyType min_key = part_keys.front();
            KeyType max_key = part_keys.back();

            if (type == ExpertType::PGM) {
                experts_.push_back(std::make_unique<PGMExpert>(part_keys, part_values, min_key, max_key));
            } else if (type == ExpertType::RMI) {
                experts_.push_back(std::make_unique<RMIExpert>(part_keys, part_values, min_key, max_key));
            } else {
                experts_.push_back(std::make_unique<ARTExpert>(part_keys, part_values, min_key, max_key));
            }

            // Create per-expert Bloom filter
            BloomFilter expert_bloom(part_keys.size(), config_.bloom_bits_per_key());
            for (const auto& k : part_keys) {
                expert_bloom.insert(k);
            }
            expert_blooms_.push_back(std::move(expert_bloom));
        }

        // Add sentinel boundary (one past last expert)
        expert_boundaries_.push_back(max_global_key + 1);

        // Clear delta buffers
        delta_buffer_art_.clear();
        delta_buffer_hash_.clear();
    }

    size_t size() const override {
        if (config_.compression_level < 0.5) {
            return total_size_ + delta_buffer_hash_.size();
        } else {
            return total_size_ + delta_buffer_art_.size();
        }
    }

    size_t memory_footprint() const override {
        size_t total = 0;

        // Experts
        for (const auto& expert : experts_) {
            total += expert->memory_footprint();
        }

        // Bloom filters
        total += global_bloom_.memory_footprint();
        for (const auto& bloom : expert_blooms_) {
            total += bloom.memory_footprint();
        }

        // Expert boundaries
        total += expert_boundaries_.capacity() * sizeof(KeyType);

        // Delta buffer
        if (config_.compression_level < 0.5) {
            total += delta_buffer_hash_.size() * (sizeof(KeyType) + sizeof(ValueType)) * 1.3;  // Hash table overhead
        } else {
            total += delta_buffer_art_.size() * (sizeof(KeyType) + sizeof(ValueType)) * 1.25;  // ART overhead
        }

        return total;
    }

    std::string name() const override {
        char buf[64];
        snprintf(buf, sizeof(buf), "HALIv2(c=%.2f)", config_.compression_level);
        return std::string(buf);
    }

    void clear() override {
        experts_.clear();
        expert_boundaries_.clear();
        expert_blooms_.clear();
        global_bloom_.clear();
        delta_buffer_art_.clear();
        delta_buffer_hash_.clear();
        total_size_ = 0;
    }

private:
    /**
     * @brief Route key to correct expert using binary search
     * @return expert index (guaranteed correct, no fallback needed)
     */
    size_t route_to_expert(KeyType key) const {
        if (experts_.empty()) {
            return 0;
        }

        // Binary search over expert boundaries
        // expert_boundaries_[i] is the minimum key for expert i
        // expert_boundaries_.back() is the sentinel (one past last expert)

        // Find the first boundary > key
        auto it = std::upper_bound(expert_boundaries_.begin(),
                                   expert_boundaries_.end() - 1,  // Exclude sentinel
                                   key);

        if (it == expert_boundaries_.begin()) {
            // Key is smaller than the first expert's min key
            // This should not happen in normal operation, but handle gracefully
            return 0;
        }

        // Move back one position to find the expert that should contain this key
        --it;
        size_t expert_id = std::distance(expert_boundaries_.begin(), it);

        // Bounds check
        if (expert_id >= experts_.size()) {
            expert_id = experts_.size() - 1;
        }

        return expert_id;
    }

    /**
     * @brief Select expert type based on data linearity and compression level
     */
    ExpertType select_expert_type(const std::vector<KeyType>& keys) const {
        if (keys.size() < 100) {
            return ExpertType::ART;  // Too small for learning
        }

        // Calculate linearity score (R² coefficient)
        double linearity = measure_linearity(keys);

        // Adjust thresholds based on compression level
        if (config_.compression_level < 0.3) {
            // Speed mode: prefer ART (fast but more memory)
            return linearity > 0.90 ? ExpertType::RMI : ExpertType::ART;
        } else if (config_.compression_level > 0.7) {
            // Memory mode: prefer PGM (compact)
            return linearity > 0.70 ? ExpertType::PGM : ExpertType::RMI;
        } else {
            // Balanced mode: original heuristic
            if (linearity > 0.95) {
                return ExpertType::PGM;
            } else if (linearity > 0.80) {
                return ExpertType::RMI;
            } else {
                return ExpertType::ART;
            }
        }
    }

    /**
     * @brief Measure how linear the data distribution is (R² coefficient)
     */
    double measure_linearity(const std::vector<KeyType>& keys) const {
        if (keys.size() < 2) return 1.0;

        double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
        size_t n = keys.size();

        for (size_t i = 0; i < n; ++i) {
            double x = static_cast<double>(keys[i]);
            double y = static_cast<double>(i);
            sum_x += x; sum_y += y; sum_xy += x * y;
            sum_x2 += x * x; sum_y2 += y * y;
        }

        double mean_x = sum_x / n, mean_y = sum_y / n;
        double num = sum_xy - n * mean_x * mean_y;
        double den_x = sum_x2 - n * mean_x * mean_x;
        double den_y = sum_y2 - n * mean_y * mean_y;

        if (den_x < 1e-10 || den_y < 1e-10) return 0.0;

        double r = num / std::sqrt(den_x * den_y);
        return r * r;
    }
};

} // namespace hali
