#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include "hash_utils.h"

namespace hali {

/**
 * @brief Simple Bloom filter for membership testing
 *
 * Uses k=7 hash functions with double hashing technique
 * Memory: bits_per_key bits per inserted element
 * False positive rate: ~1% with 10 bits/key
 */
class BloomFilter {
private:
    std::vector<uint64_t> bits_;  // Bit array (packed in 64-bit words)
    size_t num_bits_;              // Total bits
    size_t num_hash_functions_;    // k hash functions
    size_t num_inserted_;          // Count of inserted elements

public:
    /**
     * @brief Construct Bloom filter with target false positive rate
     *
     * @param expected_elements Expected number of elements to insert
     * @param bits_per_key Bits allocated per key (10 = ~1% FPR)
     */
    BloomFilter(size_t expected_elements = 1000, size_t bits_per_key = 10)
        : num_inserted_(0) {

        num_bits_ = expected_elements * bits_per_key;

        // Optimal k = (m/n) * ln(2) ≈ bits_per_key * 0.693
        num_hash_functions_ = std::max(size_t(1),
            static_cast<size_t>(bits_per_key * 0.693));

        // Round up to multiple of 64 for bit packing
        num_bits_ = ((num_bits_ + 63) / 64) * 64;

        // Allocate bit array
        bits_.resize(num_bits_ / 64, 0);
    }

    /**
     * @brief Insert a key into the Bloom filter
     */
    template<typename KeyType>
    void insert(const KeyType& key) {
        uint64_t h1 = HashUtils::xxhash64(&key, sizeof(KeyType), 0);
        uint64_t h2 = HashUtils::xxhash64(&key, sizeof(KeyType), h1);

        for (size_t i = 0; i < num_hash_functions_; ++i) {
            // Double hashing: h(i) = h1 + i*h2
            uint64_t bit_pos = (h1 + i * h2) % num_bits_;
            size_t word_idx = bit_pos / 64;
            size_t bit_idx = bit_pos % 64;
            bits_[word_idx] |= (1ULL << bit_idx);
        }

        num_inserted_++;
    }

    /**
     * @brief Check if a key might be in the set
     * @return true if key might exist (or false positive)
     *         false if key definitely does not exist
     */
    template<typename KeyType>
    bool contains(const KeyType& key) const {
        uint64_t h1 = HashUtils::xxhash64(&key, sizeof(KeyType), 0);
        uint64_t h2 = HashUtils::xxhash64(&key, sizeof(KeyType), h1);

        for (size_t i = 0; i < num_hash_functions_; ++i) {
            uint64_t bit_pos = (h1 + i * h2) % num_bits_;
            size_t word_idx = bit_pos / 64;
            size_t bit_idx = bit_pos % 64;

            if ((bits_[word_idx] & (1ULL << bit_idx)) == 0) {
                return false;  // Definitely not in set
            }
        }

        return true;  // Might be in set (or false positive)
    }

    /**
     * @brief Clear all bits
     */
    void clear() {
        std::fill(bits_.begin(), bits_.end(), 0);
        num_inserted_ = 0;
    }

    /**
     * @brief Get memory footprint in bytes
     */
    size_t memory_footprint() const {
        return bits_.capacity() * sizeof(uint64_t);
    }

    /**
     * @brief Get theoretical false positive rate
     */
    double false_positive_rate() const {
        if (num_inserted_ == 0) return 0.0;

        // FPR = (1 - e^(-kn/m))^k
        double exponent = -static_cast<double>(num_hash_functions_ * num_inserted_) / num_bits_;
        double base = 1.0 - std::exp(exponent);
        return std::pow(base, num_hash_functions_);
    }

    size_t size() const { return num_inserted_; }
    size_t num_bits() const { return num_bits_; }
    size_t num_hash_functions() const { return num_hash_functions_; }
};

} // namespace hali
