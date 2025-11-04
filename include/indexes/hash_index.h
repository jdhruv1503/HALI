#pragma once

#include "index_interface.h"
#include <parallel_hashmap/phmap.h>

namespace hali {

/**
 * @brief Hash Table index using phmap::flat_hash_map
 * Fast, cache-friendly hash table with open addressing
 */
template<typename KeyType, typename ValueType>
class HashIndex : public IndexInterface<KeyType, ValueType> {
private:
    phmap::flat_hash_map<KeyType, ValueType> map_;

public:
    HashIndex() = default;

    bool insert(const KeyType& key, const ValueType& value) override {
        auto result = map_.insert({key, value});
        return result.second; // true if inserted, false if already exists
    }

    std::optional<ValueType> find(const KeyType& key) const override {
        auto it = map_.find(key);
        if (it != map_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool erase(const KeyType& key) override {
        return map_.erase(key) > 0;
    }

    void load(const std::vector<KeyType>& keys,
              const std::vector<ValueType>& values) override {
        if (keys.size() != values.size()) {
            throw std::invalid_argument("Keys and values size mismatch");
        }

        map_.clear();
        map_.reserve(keys.size());

        for (size_t i = 0; i < keys.size(); ++i) {
            map_[keys[i]] = values[i];
        }
    }

    size_t size() const override {
        return map_.size();
    }

    size_t memory_footprint() const override {
        // Hash table overhead: buckets array + entries
        size_t base_overhead = sizeof(map_);

        // Each entry: key + value + hash + metadata
        size_t entry_size = sizeof(KeyType) + sizeof(ValueType) + sizeof(size_t) + 1;
        size_t entries_size = map_.size() * entry_size;

        // Bucket array (load factor ~0.875, so capacity is ~14% larger)
        size_t capacity = map_.capacity();
        size_t buckets_size = capacity * sizeof(void*);

        return base_overhead + entries_size + buckets_size;
    }

    std::string name() const override {
        return "HashTable";
    }

    void clear() override {
        map_.clear();
    }
};

} // namespace hali
