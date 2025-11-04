#pragma once

#include <cstdint>
#include <string>

namespace hali {

/**
 * @brief Fast non-cryptographic hash functions
 */
class HashUtils {
public:
    /**
     * @brief xxHash64 implementation
     * Fast, high-quality 64-bit hash
     * @param data Input data pointer
     * @param len Length of data
     * @param seed Hash seed
     * @return 64-bit hash value
     */
    static uint64_t xxhash64(const void* data, size_t len, uint64_t seed = 0) {
        const uint64_t PRIME64_1 = 11400714785074694791ULL;
        const uint64_t PRIME64_2 = 14029467366897019727ULL;
        const uint64_t PRIME64_3 = 1609587929392839161ULL;
        const uint64_t PRIME64_4 = 9650029242287828579ULL;
        const uint64_t PRIME64_5 = 2870177450012600261ULL;

        const uint8_t* p = static_cast<const uint8_t*>(data);
        const uint8_t* const end = p + len;
        uint64_t h64;

        if (len >= 32) {
            const uint8_t* const limit = end - 32;
            uint64_t v1 = seed + PRIME64_1 + PRIME64_2;
            uint64_t v2 = seed + PRIME64_2;
            uint64_t v3 = seed + 0;
            uint64_t v4 = seed - PRIME64_1;

            do {
                v1 += read_u64(p) * PRIME64_2; p += 8;
                v1 = rotl64(v1, 31);
                v1 *= PRIME64_1;

                v2 += read_u64(p) * PRIME64_2; p += 8;
                v2 = rotl64(v2, 31);
                v2 *= PRIME64_1;

                v3 += read_u64(p) * PRIME64_2; p += 8;
                v3 = rotl64(v3, 31);
                v3 *= PRIME64_1;

                v4 += read_u64(p) * PRIME64_2; p += 8;
                v4 = rotl64(v4, 31);
                v4 *= PRIME64_1;
            } while (p <= limit);

            h64 = rotl64(v1, 1) + rotl64(v2, 7) + rotl64(v3, 12) + rotl64(v4, 18);

            v1 *= PRIME64_2; v1 = rotl64(v1, 31); v1 *= PRIME64_1;
            h64 ^= v1; h64 = h64 * PRIME64_1 + PRIME64_4;

            v2 *= PRIME64_2; v2 = rotl64(v2, 31); v2 *= PRIME64_1;
            h64 ^= v2; h64 = h64 * PRIME64_1 + PRIME64_4;

            v3 *= PRIME64_2; v3 = rotl64(v3, 31); v3 *= PRIME64_1;
            h64 ^= v3; h64 = h64 * PRIME64_1 + PRIME64_4;

            v4 *= PRIME64_2; v4 = rotl64(v4, 31); v4 *= PRIME64_1;
            h64 ^= v4; h64 = h64 * PRIME64_1 + PRIME64_4;
        } else {
            h64 = seed + PRIME64_5;
        }

        h64 += static_cast<uint64_t>(len);

        while (p + 8 <= end) {
            uint64_t k1 = read_u64(p);
            k1 *= PRIME64_2;
            k1 = rotl64(k1, 31);
            k1 *= PRIME64_1;
            h64 ^= k1;
            h64 = rotl64(h64, 27) * PRIME64_1 + PRIME64_4;
            p += 8;
        }

        if (p + 4 <= end) {
            h64 ^= static_cast<uint64_t>(read_u32(p)) * PRIME64_1;
            h64 = rotl64(h64, 23) * PRIME64_2 + PRIME64_3;
            p += 4;
        }

        while (p < end) {
            h64 ^= (*p) * PRIME64_5;
            h64 = rotl64(h64, 11) * PRIME64_1;
            p++;
        }

        h64 ^= h64 >> 33;
        h64 *= PRIME64_2;
        h64 ^= h64 >> 29;
        h64 *= PRIME64_3;
        h64 ^= h64 >> 32;

        return h64;
    }

    /**
     * @brief Hash a string to uint64_t
     * @param str Input string
     * @param seed Hash seed
     * @return 64-bit hash value
     */
    static uint64_t hash_string(const std::string& str, uint64_t seed = 0) {
        return xxhash64(str.data(), str.size(), seed);
    }

private:
    static inline uint64_t rotl64(uint64_t x, int r) {
        return (x << r) | (x >> (64 - r));
    }

    static inline uint64_t read_u64(const uint8_t* p) {
        uint64_t val;
        std::memcpy(&val, p, sizeof(val));
        return val;
    }

    static inline uint32_t read_u32(const uint8_t* p) {
        uint32_t val;
        std::memcpy(&val, p, sizeof(val));
        return val;
    }
};

} // namespace hali
