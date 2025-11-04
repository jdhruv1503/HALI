#pragma once

#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstdint>
#include <string>
#include <iostream>

namespace hali {

/**
 * @brief Utility to load SOSD benchmark datasets
 * SOSD datasets are binary files containing sorted uint64_t arrays
 */
class SOSDLoader {
public:
    /**
     * @brief Load SOSD dataset from binary file
     * @param filepath Path to SOSD binary file
     * @param max_keys Maximum number of keys to load (0 = load all)
     * @return Vector of sorted 64-bit keys
     */
    static std::vector<uint64_t> load(const std::string& filepath, size_t max_keys = 0) {
        std::ifstream file(filepath, std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Cannot open SOSD file: " + filepath);
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Calculate number of keys
        size_t total_keys = file_size / sizeof(uint64_t);

        if (total_keys == 0) {
            throw std::runtime_error("SOSD file is empty: " + filepath);
        }

        // Determine how many keys to actually load
        size_t keys_to_load = (max_keys > 0 && max_keys < total_keys) ? max_keys : total_keys;

        std::cout << "Loading SOSD dataset: " << filepath << "\n";
        std::cout << "  Total keys in file: " << total_keys << "\n";
        std::cout << "  Loading: " << keys_to_load << " keys\n";

        // Read keys
        std::vector<uint64_t> keys(keys_to_load);
        file.read(reinterpret_cast<char*>(keys.data()), keys_to_load * sizeof(uint64_t));

        if (!file) {
            throw std::runtime_error("Error reading SOSD file: " + filepath);
        }

        file.close();

        // Verify keys are sorted (SOSD guarantee)
        for (size_t i = 1; i < keys.size(); ++i) {
            if (keys[i] < keys[i-1]) {
                throw std::runtime_error("SOSD file is not properly sorted: " + filepath);
            }
        }

        std::cout << "  ✓ Successfully loaded and verified\n";

        return keys;
    }

    /**
     * @brief Get dataset name from filepath
     */
    static std::string get_dataset_name(const std::string& filepath) {
        size_t last_slash = filepath.find_last_of("/\\");
        size_t last_dot = filepath.find_last_of(".");

        if (last_slash == std::string::npos) {
            last_slash = 0;
        } else {
            last_slash += 1;
        }

        if (last_dot == std::string::npos || last_dot < last_slash) {
            return filepath.substr(last_slash);
        }

        return filepath.substr(last_slash, last_dot - last_slash);
    }
};

} // namespace hali
