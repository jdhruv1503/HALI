#pragma once

#include "index_interface.h"
#include "alex_map.h"
#include <cstring>

namespace hali {

/**
 * @brief ALEX (Adaptive Learned Index) from Microsoft Research
 * Learned index structure that adapts to data distribution and workload
 * Paper: "ALEX: An Updatable Adaptive Learned Index" (SIGMOD 2020)
 */
template<typename KeyType, typename ValueType>
class ALEXIndex : public IndexInterface<KeyType, ValueType> {
private:
    alex::AlexMap<KeyType, ValueType> alex_;

public:
    ALEXIndex() = default;

    bool insert(const KeyType& key, const ValueType& value) override {
        auto result = alex_.insert(key, value);
        return result.second; // true if inserted, false if already exists
    }

    std::optional<ValueType> find(const KeyType& key) const override {
        auto it = alex_.find(key);
        if (it != alex_.cend()) {
            return it.payload();
        }
        return std::nullopt;
    }

    bool erase(const KeyType& key) override {
        return alex_.erase(key) > 0;
    }

    void load(const std::vector<KeyType>& keys,
              const std::vector<ValueType>& values) override {
        if (keys.size() != values.size()) {
            throw std::invalid_argument("Keys and values size mismatch");
        }

        alex_.clear();

        // ALEX supports bulk loading for better performance
        // Create sorted array of key-value pairs
        std::vector<std::pair<KeyType, ValueType>> pairs;
        pairs.reserve(keys.size());

        for (size_t i = 0; i < keys.size(); ++i) {
            pairs.emplace_back(keys[i], values[i]);
        }

        // Sort by key (ALEX requires sorted data for bulk load)
        std::sort(pairs.begin(), pairs.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        // Bulk load into ALEX
        alex_.bulk_load(pairs.data(), pairs.size());
    }

    size_t size() const override {
        return alex_.size();
    }

    size_t memory_footprint() const override {
        // Get ALEX statistics which include memory usage
        const auto& stats = alex_.get_stats();

        // ALEX stats include:
        // - num_keys: number of keys
        // - num_model_nodes: number of internal nodes with linear models
        // - num_data_nodes: number of leaf nodes
        // Each model node: ~64 bytes (model parameters + metadata)
        // Each data node: variable size based on capacity

        size_t model_node_size = 64;
        size_t data_node_overhead = 32; // metadata per data node
        size_t pair_size = sizeof(KeyType) + sizeof(ValueType);

        size_t total =
            stats.num_model_nodes * model_node_size +
            stats.num_data_nodes * data_node_overhead +
            stats.num_keys * pair_size;

        return total;
    }

    std::string name() const override {
        return "ALEX";
    }

    void clear() override {
        alex_.clear();
    }
};

} // namespace hali
