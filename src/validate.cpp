#include <iostream>
#include <cassert>
#include <vector>
#include <random>

#include "index_interface.h"
#include "indexes/btree_index.h"
#include "indexes/hash_index.h"
#include "indexes/art_index.h"
#include "indexes/pgm_index.h"
#include "indexes/rmi_index.h"
#include "indexes/hali_index.h"
#include "data_generator.h"

using namespace hali;

template<typename IndexType>
bool validate_index(const std::string& name, const std::vector<uint64_t>& keys) {
    std::cout << "Validating " << name << "..." << std::flush;

    auto index = std::make_unique<IndexType>();

    // Create values
    std::vector<uint64_t> values(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = keys[i] * 2;
    }

    // Load data
    index->load(keys, values);

    // Verify size
    if (index->size() != keys.size()) {
        std::cout << " FAIL (size mismatch: expected " << keys.size()
                  << ", got " << index->size() << ")\n";
        return false;
    }

    // Verify all keys can be found with correct values
    size_t found = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        auto result = index->find(keys[i]);
        if (!result.has_value()) {
            std::cout << " FAIL (key " << keys[i] << " not found)\n";
            return false;
        }
        if (result.value() != values[i]) {
            std::cout << " FAIL (wrong value for key " << keys[i]
                      << ": expected " << values[i] << ", got " << result.value() << ")\n";
            return false;
        }
        found++;
    }

    // Verify non-existent keys return nullopt
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<uint64_t> dist;
    size_t false_positives = 0;
    for (int i = 0; i < 1000; ++i) {
        uint64_t fake_key = dist(rng);
        // Skip if this happens to be a real key
        bool is_real = false;
        for (const auto& k : keys) {
            if (k == fake_key) {
                is_real = true;
                break;
            }
        }
        if (is_real) continue;

        auto result = index->find(fake_key);
        if (result.has_value()) {
            false_positives++;
        }
    }

    if (false_positives > 0) {
        std::cout << " WARN (" << false_positives << " false positives)\n";
    }

    // Test insert
    uint64_t new_key = keys.back() + 1000;
    uint64_t new_value = new_key * 2;
    bool inserted = index->insert(new_key, new_value);
    if (!inserted) {
        std::cout << " WARN (insert failed for new key)\n";
    } else {
        auto result = index->find(new_key);
        if (!result.has_value() || result.value() != new_value) {
            std::cout << " FAIL (inserted key not found or wrong value)\n";
            return false;
        }
    }

    // Test duplicate insert should fail
    bool dup_inserted = index->insert(keys[0], 999);
    if (dup_inserted) {
        std::cout << " FAIL (duplicate insert should return false)\n";
        return false;
    }

    std::cout << " PASS (verified " << found << " keys)\n";
    return true;
}

int main() {
    std::cout << "===========================================\n";
    std::cout << "  HALI Validation Suite\n";
    std::cout << "===========================================\n\n";

    // Generate test datasets
    std::cout << "Generating test datasets...\n";
    auto clustered = DataGenerator::generate_clustered(1000, 5);
    auto sequential = DataGenerator::generate_sequential_with_gaps(10000);
    auto uniform = DataGenerator::generate_uniform(5000);

    std::cout << "Clustered: " << clustered.size() << " keys\n";
    std::cout << "Sequential: " << sequential.size() << " keys\n";
    std::cout << "Uniform: " << uniform.size() << " keys\n";
    std::cout << "\n";

    // Validate all indexes
    bool all_passed = true;

    std::cout << "Testing with Clustered data:\n";
    all_passed &= validate_index<BTreeIndex<uint64_t, uint64_t>>("BTree", clustered);
    all_passed &= validate_index<HashIndex<uint64_t, uint64_t>>("HashTable", clustered);
    all_passed &= validate_index<ARTIndex<uint64_t, uint64_t>>("ART", clustered);
    all_passed &= validate_index<PGMIndex<uint64_t, uint64_t>>("PGM-Index", clustered);
    all_passed &= validate_index<RMIIndex<uint64_t, uint64_t>>("RMI", clustered);
    all_passed &= validate_index<HALIIndex<uint64_t, uint64_t>>("HALI", clustered);
    std::cout << "\n";

    std::cout << "Testing with Sequential data:\n";
    all_passed &= validate_index<BTreeIndex<uint64_t, uint64_t>>("BTree", sequential);
    all_passed &= validate_index<HashIndex<uint64_t, uint64_t>>("HashTable", sequential);
    all_passed &= validate_index<ARTIndex<uint64_t, uint64_t>>("ART", sequential);
    all_passed &= validate_index<PGMIndex<uint64_t, uint64_t>>("PGM-Index", sequential);
    all_passed &= validate_index<RMIIndex<uint64_t, uint64_t>>("RMI", sequential);
    all_passed &= validate_index<HALIIndex<uint64_t, uint64_t>>("HALI", sequential);
    std::cout << "\n";

    std::cout << "Testing with Uniform data:\n";
    all_passed &= validate_index<BTreeIndex<uint64_t, uint64_t>>("BTree", uniform);
    all_passed &= validate_index<HashIndex<uint64_t, uint64_t>>("HashTable", uniform);
    all_passed &= validate_index<ARTIndex<uint64_t, uint64_t>>("ART", uniform);
    all_passed &= validate_index<PGMIndex<uint64_t, uint64_t>>("PGM-Index", uniform);
    all_passed &= validate_index<RMIIndex<uint64_t, uint64_t>>("RMI", uniform);
    all_passed &= validate_index<HALIIndex<uint64_t, uint64_t>>("HALI", uniform);
    std::cout << "\n";

    if (all_passed) {
        std::cout << "===========================================\n";
        std::cout << "  ✓ ALL VALIDATION TESTS PASSED\n";
        std::cout << "===========================================\n";
        return 0;
    } else {
        std::cout << "===========================================\n";
        std::cout << "  ✗ SOME TESTS FAILED\n";
        std::cout << "===========================================\n";
        return 1;
    }
}
