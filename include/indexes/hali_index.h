#pragma once

#include "index_interface.h"
#include "indexes/rmi_index.h"
#include "indexes/pgm_index.h"
#include "indexes/art_index.h"
#include <pgm/pgm_index.hpp>
#include <art/map.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

namespace hali {

/**
 * @brief Hierarchical Adaptive Learned Index (HALI)
 *
 * Three-level architecture:
 * Level 1: RMI Router - routes keys to appropriate L2 expert
 * Level 2: Adaptive Expert Models (PGM/RMI/ART based on data characteristics)
 * Level 3: ART Delta-Buffer for efficient dynamic updates
 */
template<typename KeyType, typename ValueType>
class HALIIndex : public IndexInterface<KeyType, ValueType> {
private:
    static_assert(std::is_integral<KeyType>::value,
                  "HALIIndex requires integral key type");

    /**
     * @brief Expert type selection
     */
    enum class ExpertType {
        PGM,    // For nearly linear data
        RMI,    // For complex but learnable non-linear data
        ART     // Fallback for unlearnable/random data
    };

    /**
     * @brief Base class for Level 2 experts
     */
    struct Expert {
        ExpertType type;
        size_t start_idx;
        size_t end_idx;
        std::vector<KeyType> keys;
        std::vector<ValueType> values;

        virtual ~Expert() = default;
        virtual std::optional<ValueType> find(KeyType key) const = 0;
        virtual size_t memory_footprint() const = 0;
    };

    /**
     * @brief PGM Expert for linear data
     */
    struct PGMExpert : public Expert {
        pgm::PGMIndex<KeyType, 64> pgm;

        PGMExpert(const std::vector<KeyType>& k, const std::vector<ValueType>& v) {
            this->type = ExpertType::PGM;
            this->keys = k;
            this->values = v;
            pgm = pgm::PGMIndex<KeyType, 64>(k.begin(), k.end());
        }

        std::optional<ValueType> find(KeyType key) const override {
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
                   (this->keys.size() / 5000) * 20; // PGM segments
        }
    };

    /**
     * @brief RMI Expert for complex learnable data
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

        RMIExpert(const std::vector<KeyType>& k, const std::vector<ValueType>& v) {
            this->type = ExpertType::RMI;
            this->keys = k;
            this->values = v;

            std::vector<size_t> positions(k.size());
            std::iota(positions.begin(), positions.end(), 0);
            model.train(k, positions);
        }

        std::optional<ValueType> find(KeyType key) const override {
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
     * @brief ART Expert for unlearnable data (robustness guarantee)
     */
    struct ARTExpert : public Expert {
        art::map<KeyType, ValueType> tree;

        ARTExpert(const std::vector<KeyType>& k, const std::vector<ValueType>& v) {
            this->type = ExpertType::ART;
            this->keys = k;
            this->values = v;

            for (size_t i = 0; i < k.size(); ++i) {
                tree[k[i]] = v[i];
            }
        }

        std::optional<ValueType> find(KeyType key) const override {
            auto it = tree.find(key);
            if (it != tree.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        size_t memory_footprint() const override {
            return this->keys.size() * (sizeof(KeyType) + sizeof(ValueType)) * 1.25; // ART overhead
        }
    };

    // Level 1: Router
    struct LinearModel {
        double slope = 0.0, intercept = 0.0;

        void train(const std::vector<KeyType>& keys, const std::vector<size_t>& expert_ids) {
            if (keys.empty()) return;
            size_t n = keys.size();
            double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;

            for (size_t i = 0; i < n; ++i) {
                double x = static_cast<double>(keys[i]);
                double y = static_cast<double>(expert_ids[i]);
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

        size_t predict(KeyType key, size_t num_experts) const {
            double pred = slope * static_cast<double>(key) + intercept;
            return static_cast<size_t>(std::max(0.0, std::min(pred, static_cast<double>(num_experts - 1))));
        }
    };

    LinearModel router_;
    std::vector<std::unique_ptr<Expert>> experts_;

    // Level 3: Delta buffer
    art::map<KeyType, ValueType> delta_buffer_;
    double merge_threshold_ = 0.01; // Merge when buffer exceeds 1% of main index

    size_t total_size_ = 0;

public:
    HALIIndex() = default;

    bool insert(const KeyType& key, const ValueType& value) override {
        // Check if key already exists in main index
        if (!experts_.empty()) {
            size_t expert_id = router_.predict(key, experts_.size());
            auto existing = experts_[expert_id]->find(key);
            if (existing.has_value()) {
                return false; // Key already exists
            }
            // Fallback check in case router was wrong
            for (size_t i = 0; i < experts_.size(); ++i) {
                if (i == expert_id) continue;
                existing = experts_[i]->find(key);
                if (existing.has_value()) {
                    return false;
                }
            }
        }

        // All inserts go to delta buffer
        auto result = delta_buffer_.insert({key, value});

        // Check if merge needed
        if (delta_buffer_.size() > total_size_ * merge_threshold_) {
            // TODO: Implement background merge and retrain
            // For now, just let buffer grow
        }

        return result.second;
    }

    std::optional<ValueType> find(const KeyType& key) const override {
        // Check delta buffer first
        auto it = delta_buffer_.find(key);
        if (it != delta_buffer_.end()) {
            return it->second;
        }

        // Query static structure
        if (experts_.empty()) {
            return std::nullopt;
        }

        // Route to predicted expert
        size_t expert_id = router_.predict(key, experts_.size());
        auto result = experts_[expert_id]->find(key);

        // If not found, router may have predicted incorrectly
        // Fallback: search all experts (ensures correctness)
        if (!result.has_value()) {
            for (size_t i = 0; i < experts_.size(); ++i) {
                if (i == expert_id) continue; // Already checked
                result = experts_[i]->find(key);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return result;
    }

    bool erase(const KeyType& key) override {
        return delta_buffer_.erase(key) > 0;
    }

    void load(const std::vector<KeyType>& keys,
              const std::vector<ValueType>& values) override {
        if (keys.size() != values.size()) {
            throw std::invalid_argument("Keys and values size mismatch");
        }

        total_size_ = keys.size();

        // Sort data
        std::vector<std::pair<KeyType, ValueType>> sorted_data;
        for (size_t i = 0; i < keys.size(); ++i) {
            sorted_data.push_back({keys[i], values[i]});
        }
        std::sort(sorted_data.begin(), sorted_data.end());

        // Partition into experts
        size_t num_experts = std::max(size_t(10), keys.size() / 10000);
        experts_.clear();
        experts_.reserve(num_experts);

        size_t partition_size = keys.size() / num_experts;

        std::vector<KeyType> all_keys;
        std::vector<size_t> expert_ids;

        for (size_t i = 0; i < num_experts; ++i) {
            size_t start = i * partition_size;
            size_t end = (i == num_experts - 1) ? keys.size() : (i + 1) * partition_size;

            std::vector<KeyType> part_keys;
            std::vector<ValueType> part_values;

            for (size_t j = start; j < end; ++j) {
                part_keys.push_back(sorted_data[j].first);
                part_values.push_back(sorted_data[j].second);
                all_keys.push_back(sorted_data[j].first);
                expert_ids.push_back(i);
            }

            // Determine expert type based on data characteristics
            ExpertType type = select_expert_type(part_keys);

            if (type == ExpertType::PGM) {
                experts_.push_back(std::make_unique<PGMExpert>(part_keys, part_values));
            } else if (type == ExpertType::RMI) {
                experts_.push_back(std::make_unique<RMIExpert>(part_keys, part_values));
            } else {
                experts_.push_back(std::make_unique<ARTExpert>(part_keys, part_values));
            }
        }

        // Train router
        router_.train(all_keys, expert_ids);

        delta_buffer_.clear();
    }

    size_t size() const override {
        return total_size_ + delta_buffer_.size();
    }

    size_t memory_footprint() const override {
        size_t total = sizeof(router_);

        for (const auto& expert : experts_) {
            total += expert->memory_footprint();
        }

        total += delta_buffer_.size() * (sizeof(KeyType) + sizeof(ValueType)) * 1.25;

        return total;
    }

    std::string name() const override {
        return "HALI";
    }

    void clear() override {
        experts_.clear();
        delta_buffer_.clear();
        total_size_ = 0;
    }

private:
    /**
     * @brief Analyze data characteristics and select appropriate expert type
     */
    ExpertType select_expert_type(const std::vector<KeyType>& keys) const {
        if (keys.size() < 100) {
            return ExpertType::ART; // Too small for learning
        }

        // Calculate linearity score
        double linearity = measure_linearity(keys);

        if (linearity > 0.95) {
            return ExpertType::PGM; // Highly linear
        } else if (linearity > 0.80) {
            return ExpertType::RMI; // Moderately complex
        } else {
            return ExpertType::ART; // Too random for learning
        }
    }

    /**
     * @brief Measure how linear the data distribution is (0 to 1)
     */
    double measure_linearity(const std::vector<KeyType>& keys) const {
        if (keys.size() < 2) return 1.0;

        // Fit linear model and measure R²
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
        return r * r; // R² coefficient of determination
    }
};

} // namespace hali
