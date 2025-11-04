#pragma once

#include "index_interface.h"
#include <pgm/pgm_index.hpp>
#include <vector>
#include <algorithm>
#include <numeric>

namespace hali {

/**
 * @brief PGM-Index wrapper
 * Piecewise Geometric Model index with provable error bounds
 * Supports only static workloads (load once, query many times)
 */
template<typename KeyType, typename ValueType>
class PGMIndex : public IndexInterface<KeyType, ValueType> {
private:
    static_assert(std::is_integral<KeyType>::value,
                  "PGMIndex requires integral key type");

    // PGM index for position prediction
    pgm::PGMIndex<KeyType, 64> pgm_; // error bound of 64

    // Sorted arrays for actual data storage
    std::vector<KeyType> keys_;
    std::vector<ValueType> values_;

    // Dynamic buffer for inserts (not natively supported by PGM)
    std::vector<std::pair<KeyType, ValueType>> insert_buffer_;

public:
    PGMIndex() = default;

    bool insert(const KeyType& key, const ValueType& value) override {
        // PGM doesn't support efficient inserts, so buffer them
        // Check if key already exists in main index
        if (!keys_.empty()) {
            auto range = pgm_.search(key);
            auto it = std::lower_bound(keys_.begin() + range.lo,
                                      keys_.begin() + range.hi,
                                      key);
            if (it != keys_.begin() + range.hi && *it == key) {
                return false; // Key already exists
            }
        }

        // Check insert buffer
        for (const auto& p : insert_buffer_) {
            if (p.first == key) {
                return false; // Key already in buffer
            }
        }

        insert_buffer_.push_back({key, value});
        return true;
    }

    std::optional<ValueType> find(const KeyType& key) const override {
        // Search main index first
        if (!keys_.empty()) {
            auto range = pgm_.search(key);
            auto it = std::lower_bound(keys_.begin() + range.lo,
                                      keys_.begin() + range.hi,
                                      key);

            if (it != keys_.begin() + range.hi && *it == key) {
                size_t idx = std::distance(keys_.begin(), it);
                return values_[idx];
            }
        }

        // Search insert buffer
        for (const auto& p : insert_buffer_) {
            if (p.first == key) {
                return p.second;
            }
        }

        return std::nullopt;
    }

    bool erase(const KeyType& key) override {
        // Erasing from a learned index is expensive - not optimized
        // Remove from buffer if exists
        for (auto it = insert_buffer_.begin(); it != insert_buffer_.end(); ++it) {
            if (it->first == key) {
                insert_buffer_.erase(it);
                return true;
            }
        }

        // Removing from main index requires rebuild - not implemented efficiently
        return false;
    }

    void load(const std::vector<KeyType>& keys,
              const std::vector<ValueType>& values) override {
        if (keys.size() != values.size()) {
            throw std::invalid_argument("Keys and values size mismatch");
        }

        // Copy and ensure sorted
        keys_ = keys;
        values_ = values;

        // Sort by keys
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

        // Build PGM index
        pgm_ = pgm::PGMIndex<KeyType, 64>(keys_.begin(), keys_.end());

        // Clear any pending inserts
        insert_buffer_.clear();
    }

    size_t size() const override {
        return keys_.size() + insert_buffer_.size();
    }

    size_t memory_footprint() const override {
        // Keys and values arrays
        size_t data_size = keys_.capacity() * sizeof(KeyType) +
                          values_.capacity() * sizeof(ValueType);

        // PGM segments (very compact - typically ~10-20 bytes per segment)
        // Number of segments = data_size / segment_size
        // With error=64, typically one segment per ~1000-10000 keys
        size_t num_segments = std::max(size_t(1), keys_.size() / 5000);
        size_t pgm_size = num_segments * 20; // approx 20 bytes per segment

        // Insert buffer
        size_t buffer_size = insert_buffer_.capacity() *
                            (sizeof(KeyType) + sizeof(ValueType));

        return data_size + pgm_size + buffer_size;
    }

    std::string name() const override {
        return "PGM-Index";
    }

    void clear() override {
        keys_.clear();
        values_.clear();
        insert_buffer_.clear();
        pgm_ = pgm::PGMIndex<KeyType, 64>();
    }
};

} // namespace hali
