#pragma once

#include "index_interface.h"
#include <art/map.h>
#include <cstring>

namespace hali {

/**
 * @brief Adaptive Radix Tree index using justinasvd/art_map
 * Cache-efficient trie structure optimized for sorted keys
 */
template<typename KeyType, typename ValueType>
class ARTIndex : public IndexInterface<KeyType, ValueType> {
private:
    static_assert(std::is_integral<KeyType>::value,
                  "ARTIndex requires integral key type");

    art::map<KeyType, ValueType> tree_;

public:
    ARTIndex() = default;

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
        // ART has variable node sizes: Node4, Node16, Node48, Node256
        // Approximate based on tree depth and fanout

        size_t base_overhead = sizeof(tree_);
        size_t leaf_size = (sizeof(KeyType) + sizeof(ValueType)) * tree_.size();

        // Internal node overhead (typically 20-30% for ART)
        // ART is more space-efficient than B+Tree but less than hash
        size_t internal_overhead = leaf_size * 0.25;

        return base_overhead + leaf_size + internal_overhead;
    }

    std::string name() const override {
        return "ART";
    }

    void clear() override {
        tree_.clear();
    }
};

} // namespace hali
