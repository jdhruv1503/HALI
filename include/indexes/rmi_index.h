#pragma once

#include "index_interface.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace hali {

/**
 * @brief Simple 2-layer Recursive Model Index (RMI)
 * Layer 1: Single linear model that routes to Layer 2
 * Layer 2: Multiple linear models for final position prediction
 */
template<typename KeyType, typename ValueType>
class RMIIndex : public IndexInterface<KeyType, ValueType> {
private:
    static_assert(std::is_integral<KeyType>::value,
                  "RMIIndex requires integral key type");

    /**
     * @brief Simple linear regression model
     */
    struct LinearModel {
        double slope = 0.0;
        double intercept = 0.0;

        /**
         * @brief Train linear model on sorted data
         */
        void train(const std::vector<KeyType>& keys,
                  const std::vector<size_t>& positions) {
            if (keys.empty()) return;

            size_t n = keys.size();
            double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;

            for (size_t i = 0; i < n; ++i) {
                double x = static_cast<double>(keys[i]);
                double y = static_cast<double>(positions[i]);

                sum_x += x;
                sum_y += y;
                sum_xy += x * y;
                sum_x2 += x * x;
            }

            double mean_x = sum_x / n;
            double mean_y = sum_y / n;

            double numerator = sum_xy - n * mean_x * mean_y;
            double denominator = sum_x2 - n * mean_x * mean_x;

            if (std::abs(denominator) > 1e-10) {
                slope = numerator / denominator;
                intercept = mean_y - slope * mean_x;
            } else {
                slope = 0.0;
                intercept = mean_y;
            }
        }

        /**
         * @brief Predict position for a key
         */
        size_t predict(KeyType key, size_t max_pos) const {
            double pred = slope * static_cast<double>(key) + intercept;
            pred = std::max(0.0, std::min(pred, static_cast<double>(max_pos)));
            return static_cast<size_t>(pred);
        }
    };

    // Layer 1: Root model
    LinearModel root_model_;

    // Layer 2: Expert models
    std::vector<LinearModel> expert_models_;
    size_t num_experts_;

    // Data storage
    std::vector<KeyType> keys_;
    std::vector<ValueType> values_;

    // Dynamic buffer
    std::vector<std::pair<KeyType, ValueType>> insert_buffer_;

    // Search parameters
    static constexpr size_t ERROR_BOUND = 128;

public:
    explicit RMIIndex(size_t num_experts = 100)
        : num_experts_(num_experts) {}

    bool insert(const KeyType& key, const ValueType& value) override {
        // Check main index
        if (!keys_.empty()) {
            size_t pos = predict_position(key);
            auto it = bounded_search(key, pos);
            if (it != keys_.end() && *it == key) {
                return false;
            }
        }

        // Check buffer
        for (const auto& p : insert_buffer_) {
            if (p.first == key) {
                return false;
            }
        }

        insert_buffer_.push_back({key, value});
        return true;
    }

    std::optional<ValueType> find(const KeyType& key) const override {
        // Search main index
        if (!keys_.empty()) {
            size_t pos = predict_position(key);
            auto it = bounded_search(key, pos);

            if (it != keys_.end() && *it == key) {
                size_t idx = std::distance(keys_.begin(), it);
                return values_[idx];
            }
        }

        // Search buffer
        for (const auto& p : insert_buffer_) {
            if (p.first == key) {
                return p.second;
            }
        }

        return std::nullopt;
    }

    bool erase(const KeyType& key) override {
        // Remove from buffer
        for (auto it = insert_buffer_.begin(); it != insert_buffer_.end(); ++it) {
            if (it->first == key) {
                insert_buffer_.erase(it);
                return true;
            }
        }
        return false;
    }

    void load(const std::vector<KeyType>& keys,
              const std::vector<ValueType>& values) override {
        if (keys.size() != values.size()) {
            throw std::invalid_argument("Keys and values size mismatch");
        }

        // Sort data
        keys_ = keys;
        values_ = values;

        std::vector<size_t> indices(keys_.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
                  [this](size_t a, size_t b) { return keys_[a] < keys_[b]; });

        std::vector<KeyType> sorted_keys(keys_.size());
        std::vector<ValueType> sorted_values(values_.size());

        for (size_t i = 0; i < indices.size(); ++i) {
            sorted_keys[i] = keys_[indices[i]];
            sorted_values[i] = values_[indices[i]];
        }

        keys_ = std::move(sorted_keys);
        values_ = std::move(sorted_values);

        // Train RMI
        train_models();

        insert_buffer_.clear();
    }

    size_t size() const override {
        return keys_.size() + insert_buffer_.size();
    }

    size_t memory_footprint() const override {
        size_t data_size = keys_.capacity() * sizeof(KeyType) +
                          values_.capacity() * sizeof(ValueType);

        size_t models_size = sizeof(LinearModel) * (1 + num_experts_);

        size_t buffer_size = insert_buffer_.capacity() *
                            (sizeof(KeyType) + sizeof(ValueType));

        return data_size + models_size + buffer_size;
    }

    std::string name() const override {
        return "RMI";
    }

    void clear() override {
        keys_.clear();
        values_.clear();
        insert_buffer_.clear();
        expert_models_.clear();
    }

private:
    void train_models() {
        if (keys_.empty()) return;

        // Initialize expert models
        expert_models_.resize(num_experts_);

        // Train root model (Layer 1)
        std::vector<size_t> expert_indices(keys_.size());
        for (size_t i = 0; i < keys_.size(); ++i) {
            expert_indices[i] = (i * num_experts_) / keys_.size();
        }

        root_model_.train(keys_, expert_indices);

        // Train expert models (Layer 2)
        for (size_t expert_id = 0; expert_id < num_experts_; ++expert_id) {
            std::vector<KeyType> expert_keys;
            std::vector<size_t> expert_positions;

            for (size_t i = 0; i < keys_.size(); ++i) {
                size_t predicted_expert = root_model_.predict(keys_[i], num_experts_ - 1);
                if (predicted_expert == expert_id) {
                    expert_keys.push_back(keys_[i]);
                    expert_positions.push_back(i);
                }
            }

            if (!expert_keys.empty()) {
                expert_models_[expert_id].train(expert_keys, expert_positions);
            }
        }
    }

    size_t predict_position(KeyType key) const {
        if (keys_.empty()) return 0;

        // Layer 1: Predict expert
        size_t expert_id = root_model_.predict(key, num_experts_ - 1);

        // Layer 2: Predict position within data
        size_t pos = expert_models_[expert_id].predict(key, keys_.size() - 1);

        return pos;
    }

    typename std::vector<KeyType>::const_iterator
    bounded_search(KeyType key, size_t predicted_pos) const {
        if (keys_.empty()) return keys_.end();

        size_t start = (predicted_pos > ERROR_BOUND) ?
                       predicted_pos - ERROR_BOUND : 0;
        size_t end = std::min(predicted_pos + ERROR_BOUND, keys_.size());

        auto it = std::lower_bound(keys_.begin() + start,
                                   keys_.begin() + end,
                                   key);

        return it;
    }

    typename std::vector<KeyType>::iterator
    bounded_search(KeyType key, size_t predicted_pos) {
        if (keys_.empty()) return keys_.end();

        size_t start = (predicted_pos > ERROR_BOUND) ?
                       predicted_pos - ERROR_BOUND : 0;
        size_t end = std::min(predicted_pos + ERROR_BOUND, keys_.size());

        auto it = std::lower_bound(keys_.begin() + start,
                                   keys_.begin() + end,
                                   key);

        return it;
    }
};

} // namespace hali
