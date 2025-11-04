#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace hali {

/**
 * @brief Abstract base class for all index structures
 *
 * Provides a standardized std::map-like API for fair comparison
 * All index implementations must inherit from this interface
 */
template<typename KeyType, typename ValueType>
class IndexInterface {
public:
    virtual ~IndexInterface() = default;

    /**
     * @brief Insert a key-value pair into the index
     * @param key The key to insert
     * @param value The value associated with the key
     * @return true if insertion was successful, false if key already exists
     */
    virtual bool insert(const KeyType& key, const ValueType& value) = 0;

    /**
     * @brief Find a value by key
     * @param key The key to search for
     * @return Optional containing the value if found, empty otherwise
     */
    virtual std::optional<ValueType> find(const KeyType& key) const = 0;

    /**
     * @brief Erase a key-value pair from the index
     * @param key The key to erase
     * @return true if key was found and erased, false otherwise
     */
    virtual bool erase(const KeyType& key) = 0;

    /**
     * @brief Load a batch of key-value pairs into the index
     * @param keys Vector of keys to load
     * @param values Vector of values corresponding to keys
     * @note keys and values must have the same size
     */
    virtual void load(const std::vector<KeyType>& keys,
                     const std::vector<ValueType>& values) = 0;

    /**
     * @brief Get the number of elements in the index
     * @return Number of key-value pairs stored
     */
    virtual size_t size() const = 0;

    /**
     * @brief Get the memory footprint of the index in bytes
     * @return Total memory used by the index structure
     */
    virtual size_t memory_footprint() const = 0;

    /**
     * @brief Get the name of the index implementation
     * @return String identifier for this index type
     */
    virtual std::string name() const = 0;

    /**
     * @brief Check if the index is empty
     * @return true if size() == 0
     */
    virtual bool empty() const {
        return size() == 0;
    }

    /**
     * @brief Clear all entries from the index
     */
    virtual void clear() = 0;
};

} // namespace hali
