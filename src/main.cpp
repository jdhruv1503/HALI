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

/**
 * @brief Parse command-line argument (e.g., --key=value)
 */
std::string parse_arg(int argc, char* argv[], const std::string& key, const std::string& default_val) {
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.find(key + "=") == 0) {
            return arg.substr(key.length() + 1);
        }
    }
    return default_val;
}

double parse_arg_double(int argc, char* argv[], const std::string& key, double default_val) {
    std::string val = parse_arg(argc, argv, key, "");
    if (val.empty()) return default_val;
    return std::stod(val);
}

size_t parse_arg_size(int argc, char* argv[], const std::string& key, size_t default_val) {
    std::string val = parse_arg(argc, argv, key, "");
    if (val.empty()) return default_val;
    return std::stoull(val);
}

int main(int argc, char* argv[]) {
    std::cout << "===========================================\n";
    std::cout << "  WT-HALI: Write-Through HALI\n";
    std::cout << "  Experimental Benchmark Runner\n";
    std::cout << "===========================================\n\n";

    // Parse command-line arguments
    std::string index_type = parse_arg(argc, argv, "--index", "wthali");
    double compression_level = parse_arg_double(argc, argv, "--compression", 0.25);
    double buffer_size = parse_arg_double(argc, argv, "--buffer", 0.005);
    std::string dataset_type = parse_arg(argc, argv, "--dataset", "all");
    std::string workload_type = parse_arg(argc, argv, "--workload", "all");
    size_t dataset_size = parse_arg_size(argc, argv, "--size", 500000);
    size_t num_operations = parse_arg_size(argc, argv, "--operations", 100000);

    std::cout << "Configuration:\n";
    std::cout << "  Index Type: " << index_type << "\n";
    if (index_type == "wthali" || index_type == "all") {
        std::cout << "  Compression Level: " << compression_level << "\n";
        std::cout << "  Buffer Size: " << (buffer_size * 100) << "%\n";
    }
    std::cout << "  Dataset Type: " << dataset_type << "\n";
    std::cout << "  Dataset Size: " << dataset_size << " keys\n";
    std::cout << "  Workload Type: " << workload_type << "\n";
    std::cout << "  Operations: " << num_operations << "\n\n";

    // Generate specific dataset or all datasets
    std::cout << "Generating dataset...\n";

    std::map<std::string, std::vector<uint64_t>> datasets;

    if (dataset_type == "all" || dataset_type == "lognormal") {
        datasets["Lognormal"] = DataGenerator::generate_lognormal(dataset_size);
    }
    if (dataset_type == "all" || dataset_type == "zipfian") {
        datasets["Zipfian"] = DataGenerator::generate_zipfian(dataset_size);
    }
    if (dataset_type == "all" || dataset_type == "clustered") {
        datasets["Clustered"] = DataGenerator::generate_clustered(dataset_size / 10, 10);
    }
    if (dataset_type == "all" || dataset_type == "sequential") {
        datasets["Sequential"] = DataGenerator::generate_sequential_with_gaps(dataset_size);
    }
    if (dataset_type == "all" || dataset_type == "mixed") {
        datasets["Mixed"] = DataGenerator::generate_mixed(dataset_size);
    }
    if (dataset_type == "all" || dataset_type == "uniform") {
        datasets["Uniform"] = DataGenerator::generate_uniform(dataset_size);
    }

    std::cout << "Generated " << datasets.size() << " dataset(s).\n";

    // Workload types
    std::vector<std::string> workloads;
    if (workload_type == "all") {
        workloads = {"read_heavy", "write_heavy", "mixed"};
    } else {
        workloads = {workload_type};
    }

    // Store all results
    std::vector<BenchmarkResults> all_results;

    // Run experiments
    std::cout << "\n========================================\n";
    std::cout << "Starting Benchmark Experiments\n";
    std::cout << "========================================\n";

    for (const auto& [dataset_name, keys] : datasets) {
        for (const auto& workload : workloads) {

            // Run BTree index
            if (index_type == "all" || index_type == "btree") {
                all_results.push_back(
                    run_benchmark<BTreeIndex<uint64_t, uint64_t>>(
                        "BTree", workload, dataset_name, keys, num_operations,
                        std::make_unique<BTreeIndex<uint64_t, uint64_t>>())
                );
            }

            // Run Hash index
            if (index_type == "all" || index_type == "hash") {
                all_results.push_back(
                    run_benchmark<HashIndex<uint64_t, uint64_t>>(
                        "Hash", workload, dataset_name, keys, num_operations,
                        std::make_unique<HashIndex<uint64_t, uint64_t>>())
                );
            }

            // Run ART index
            if (index_type == "all" || index_type == "art") {
                all_results.push_back(
                    run_benchmark<ARTIndex<uint64_t, uint64_t>>(
                        "ART", workload, dataset_name, keys, num_operations,
                        std::make_unique<ARTIndex<uint64_t, uint64_t>>())
                );
            }

            // Run PGM index
            if (index_type == "all" || index_type == "pgm") {
                all_results.push_back(
                    run_benchmark<PGMIndex<uint64_t, uint64_t>>(
                        "PGM-Index", workload, dataset_name, keys, num_operations,
                        std::make_unique<PGMIndex<uint64_t, uint64_t>>())
                );
            }

            // Run RMI index
            if (index_type == "all" || index_type == "rmi") {
                all_results.push_back(
                    run_benchmark<RMIIndex<uint64_t, uint64_t>>(
                        "RMI", workload, dataset_name, keys, num_operations,
                        std::make_unique<RMIIndex<uint64_t, uint64_t>>())
                );
            }

            // Run WT-HALI (HALIv2) index with optimal configuration
            if (index_type == "all" || index_type == "wthali") {
                std::string config_name = "WT-HALI";
                if (index_type == "wthali") {
                    config_name += "(comp=" + std::to_string(compression_level) +
                                  ",buf=" + std::to_string(buffer_size) + ")";
                }

                all_results.push_back(
                    run_benchmark<HALIv2Index<uint64_t, uint64_t>>(
                        config_name, workload, dataset_name, keys, num_operations,
                        std::make_unique<HALIv2Index<uint64_t, uint64_t>>(
                            compression_level, buffer_size))
                );
            }
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
