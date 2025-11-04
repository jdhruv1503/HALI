#pragma once

#include "index_interface.h"
#include <parallel_hashmap/btree.h>
#include <cstring>

namespace hali {

/**
 * @brief B+Tree index using phmap::btree_map
 * Cache-friendly B+Tree implementation from parallel-hashmap
 */
template<typename KeyType, typename ValueType>
class BTreeIndex : public IndexInterface<KeyType, ValueType> {
private:
    phmap::btree_map<KeyType, ValueType> tree_;

public:
    BTreeIndex() = default;

    bool insert(const KeyType& key, const ValueType& value) override {
        auto result = tree_.insert({key, value});
        return result.second; // true if inserted, false if already exists
    }

    std::optional<ValueType> find(const KeyType& key) const override {
        auto it = tree_.find(key);
        if (it != tree_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool erase(const KeyType& key) override {
        return tree_.erase(key) > 0;
    }

    void load(const std::vector<KeyType>& keys,
              const std::vector<ValueType>& values) override {
        if (keys.size() != values.size()) {
            throw std::invalid_argument("Keys and values size mismatch");
        }

        tree_.clear();
        for (size_t i = 0; i < keys.size(); ++i) {
            tree_[keys[i]] = values[i];
        }
    }

    size_t size() const override {
        return tree_.size();
    }

    size_t memory_footprint() const override {
        // Approximate: each node is ~256 bytes (typical B+Tree node)
        // Plus size of key-value pairs
        size_t base_overhead = sizeof(tree_);
        size_t pair_size = sizeof(KeyType) + sizeof(ValueType);
        size_t data_size = tree_.size() * pair_size;

        // B+Tree internal node overhead (approximately 15-20%)
        size_t tree_overhead = data_size * 0.2;

        return base_overhead + data_size + tree_overhead;
    }

    std::string name() const override {
        return "BTree";
    }

    void clear() override {
        tree_.clear();
    }
};

} // namespace hali
