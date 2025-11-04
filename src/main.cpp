#include <iostream>
#include <iomanip>
#include <memory>
#include <map>
#include <fstream>
#include <sstream>

#include "index_interface.h"
#include "indexes/btree_index.h"
#include "indexes/hash_index.h"
#include "indexes/art_index.h"
#include "indexes/pgm_index.h"
#include "indexes/rmi_index.h"
#include "indexes/hali_index.h"
#include "indexes/haliv2_index.h"
#include "timing_utils.h"
#include "data_generator.h"
#include "workload_generator.h"

using namespace hali;

/**
 * @brief Benchmark results for a single index/workload combination
 */
struct BenchmarkResults {
    std::string index_name;
    std::string workload_name;
    std::string dataset_name;

    // Metrics
    double mean_lookup_ns = 0.0;
    double p95_lookup_ns = 0.0;
    double p99_lookup_ns = 0.0;
    double insert_throughput_ops = 0.0;
    size_t memory_footprint_bytes = 0;
    double build_time_ms = 0.0;
    size_t dataset_size = 0;

    void print() const {
        std::cout << "\n========================================\n";
        std::cout << "Index: " << index_name << "\n";
        std::cout << "Workload: " << workload_name << "\n";
        std::cout << "Dataset: " << dataset_name << " (" << dataset_size << " keys)\n";
        std::cout << "----------------------------------------\n";
        std::cout << "Build Time:        " << std::fixed << std::setprecision(2)
                  << build_time_ms << " ms\n";
        std::cout << "Memory Footprint:  " << (memory_footprint_bytes / 1024.0 / 1024.0)
                  << " MB\n";
        std::cout << "Space per Key:     " << (memory_footprint_bytes / (double)dataset_size)
                  << " bytes\n";
        std::cout << "Mean Lookup:       " << std::setprecision(1) << mean_lookup_ns << " ns\n";
        std::cout << "P95 Lookup:        " << p95_lookup_ns << " ns\n";
        std::cout << "P99 Lookup:        " << p99_lookup_ns << " ns\n";
        std::cout << "Insert Throughput: " << std::setprecision(0)
                  << insert_throughput_ops << " ops/sec\n";
        std::cout << "========================================\n";
    }
};

/**
 * @brief Run benchmark on a specific index with a specific workload
 */
template<typename IndexType>
BenchmarkResults run_benchmark(
    const std::string& index_name,
    const std::string& workload_type,
    const std::string& dataset_name,
    const std::vector<uint64_t>& keys,
    size_t num_operations,
    std::unique_ptr<IndexType> index)
{
    BenchmarkResults results;
    results.index_name = index_name;
    results.workload_name = WorkloadGenerator::workload_name(workload_type);
    results.dataset_name = dataset_name;
    results.dataset_size = keys.size();

    std::cout << "\n[Running] " << index_name << " on " << dataset_name
              << " with " << workload_type << " workload..." << std::flush;

    // Build index (load data)
    Timer build_timer;
    std::vector<uint64_t> values(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        values[i] = keys[i] * 2; // Simple value = key * 2
    }
    index->load(keys, values);
    results.build_time_ms = build_timer.elapsed_ms();

    // Measure memory footprint
    results.memory_footprint_bytes = index->memory_footprint();

    // Generate workload
    WorkloadGenerator wl_gen(42);
    std::vector<Operation> operations;

    if (workload_type == "read_heavy") {
        operations = wl_gen.generate_read_heavy(keys, num_operations);
    } else if (workload_type == "write_heavy") {
        operations = wl_gen.generate_write_heavy(keys, num_operations);
    } else if (workload_type == "mixed") {
        operations = wl_gen.generate_mixed(keys, num_operations);
    }

    // Execute workload and measure latencies
    LatencyStats lookup_stats;
    LatencyStats insert_stats;

    size_t num_finds = 0;
    size_t num_inserts = 0;

    for (const auto& op : operations) {
        Timer op_timer;

        if (op.type == OpType::FIND) {
            index->find(op.key);
            uint64_t latency = op_timer.elapsed_ns();
            lookup_stats.add(latency);
            num_finds++;
        } else if (op.type == OpType::INSERT) {
            index->insert(op.key, op.value);
            uint64_t latency = op_timer.elapsed_ns();
            insert_stats.add(latency);
            num_inserts++;
        }
    }

    // Calculate metrics
    if (num_finds > 0) {
        results.mean_lookup_ns = lookup_stats.mean();
        results.p95_lookup_ns = lookup_stats.p95();
        results.p99_lookup_ns = lookup_stats.p99();
    }

    if (num_inserts > 0) {
        double total_insert_time_s = insert_stats.mean() * num_inserts / 1e9;
        results.insert_throughput_ops = num_inserts / total_insert_time_s;
    }

    std::cout << " DONE" << std::endl;

    return results;
}

/**
 * @brief Export results to CSV
 */
void export_to_csv(const std::vector<BenchmarkResults>& all_results,
                   const std::string& filename) {
    std::ofstream csv(filename);

    // Header
    csv << "Index,Workload,Dataset,DatasetSize,BuildTime_ms,Memory_MB,BytesPerKey,"
        << "MeanLookup_ns,P95Lookup_ns,P99Lookup_ns,InsertThroughput_ops\n";

    // Data rows
    for (const auto& r : all_results) {
        csv << r.index_name << ","
            << r.workload_name << ","
            << r.dataset_name << ","
            << r.dataset_size << ","
            << r.build_time_ms << ","
            << (r.memory_footprint_bytes / 1024.0 / 1024.0) << ","
            << (r.memory_footprint_bytes / (double)r.dataset_size) << ","
            << r.mean_lookup_ns << ","
            << r.p95_lookup_ns << ","
            << r.p99_lookup_ns << ","
            << r.insert_throughput_ops << "\n";
    }

    csv.close();
    std::cout << "\nResults exported to: " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "===========================================\n";
    std::cout << "  HALI: Hierarchical Adaptive Learned Index\n";
    std::cout << "  Benchmark Suite\n";
    std::cout << "===========================================\n\n";

    // Configuration
    size_t dataset_size = 1000000; // 1M keys for quick testing
    size_t num_operations = 100000; // 100K operations per workload

    if (argc > 1) {
        dataset_size = std::stoull(argv[1]);
    }
    if (argc > 2) {
        num_operations = std::stoull(argv[2]);
    }

    std::cout << "Configuration:\n";
    std::cout << "  Dataset size: " << dataset_size << " keys\n";
    std::cout << "  Operations per workload: " << num_operations << "\n\n";

    // Generate datasets
    std::cout << "Generating datasets...\n";

    std::map<std::string, std::vector<uint64_t>> datasets;
    datasets["Lognormal"] = DataGenerator::generate_lognormal(dataset_size);
    datasets["Zipfian"] = DataGenerator::generate_zipfian(dataset_size);
    datasets["Clustered"] = DataGenerator::generate_clustered(dataset_size / 10, 10);
    datasets["Sequential"] = DataGenerator::generate_sequential_with_gaps(dataset_size);
    datasets["Mixed"] = DataGenerator::generate_mixed(dataset_size);
    datasets["Uniform"] = DataGenerator::generate_uniform(dataset_size);

    std::cout << "Generated " << datasets.size() << " datasets.\n";

    // Workload types
    std::vector<std::string> workloads = {"read_heavy", "write_heavy", "mixed"};

    // Store all results
    std::vector<BenchmarkResults> all_results;

    // Run experiments
    std::cout << "\n========================================\n";
    std::cout << "Starting Benchmark Experiments\n";
    std::cout << "========================================\n";

    for (const auto& [dataset_name, keys] : datasets) {
        for (const auto& workload : workloads) {
            // BTree
            all_results.push_back(
                run_benchmark<BTreeIndex<uint64_t, uint64_t>>(
                    "BTree", workload, dataset_name, keys, num_operations,
                    std::make_unique<BTreeIndex<uint64_t, uint64_t>>())
            );

            // Hash Table
            all_results.push_back(
                run_benchmark<HashIndex<uint64_t, uint64_t>>(
                    "HashTable", workload, dataset_name, keys, num_operations,
                    std::make_unique<HashIndex<uint64_t, uint64_t>>())
            );

            // ART
            all_results.push_back(
                run_benchmark<ARTIndex<uint64_t, uint64_t>>(
                    "ART", workload, dataset_name, keys, num_operations,
                    std::make_unique<ARTIndex<uint64_t, uint64_t>>())
            );

            // PGM-Index
            all_results.push_back(
                run_benchmark<PGMIndex<uint64_t, uint64_t>>(
                    "PGM-Index", workload, dataset_name, keys, num_operations,
                    std::make_unique<PGMIndex<uint64_t, uint64_t>>())
            );

            // RMI
            all_results.push_back(
                run_benchmark<RMIIndex<uint64_t, uint64_t>>(
                    "RMI", workload, dataset_name, keys, num_operations,
                    std::make_unique<RMIIndex<uint64_t, uint64_t>>())
            );

            // HALIv1 (baseline from Phase 1)
            all_results.push_back(
                run_benchmark<HALIIndex<uint64_t, uint64_t>>(
                    "HALIv1", workload, dataset_name, keys, num_operations,
                    std::make_unique<HALIIndex<uint64_t, uint64_t>>())
            );

            // HALIv2 - Speed mode (compression_level = 0.0)
            all_results.push_back(
                run_benchmark<HALIv2Index<uint64_t, uint64_t>>(
                    "HALIv2-Speed", workload, dataset_name, keys, num_operations,
                    std::make_unique<HALIv2Index<uint64_t, uint64_t>>(0.0))
            );

            // HALIv2 - Balanced mode (compression_level = 0.5)
            all_results.push_back(
                run_benchmark<HALIv2Index<uint64_t, uint64_t>>(
                    "HALIv2-Balanced", workload, dataset_name, keys, num_operations,
                    std::make_unique<HALIv2Index<uint64_t, uint64_t>>(0.5))
            );

            // HALIv2 - Memory mode (compression_level = 1.0)
            all_results.push_back(
                run_benchmark<HALIv2Index<uint64_t, uint64_t>>(
                    "HALIv2-Memory", workload, dataset_name, keys, num_operations,
                    std::make_unique<HALIv2Index<uint64_t, uint64_t>>(1.0))
            );
        }
    }

    // Print summary
    std::cout << "\n========================================\n";
    std::cout << "Benchmark Results Summary\n";
    std::cout << "========================================\n";

    for (const auto& result : all_results) {
        result.print();
    }

    // Export to CSV
    export_to_csv(all_results, "results/benchmark_results.csv");

    std::cout << "\nBenchmark suite completed successfully!\n";

    return 0;
}
