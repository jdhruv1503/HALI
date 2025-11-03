# **SPECIFICATION DOCUMENT: Hierarchical Adaptive Learned Index (HALI)**

## **1\. Objective**

The primary objective of this project is to implement, rigorously evaluate, and document a novel, OS-centric learned file system index named the **Hierarchical Adaptive Learned Index (HALI)**. The final deliverable will be a comprehensive research paper, supported by a high-fidelity C++ simulator and experimental data, that convincingly demonstrates HALI's superior performance and robustness on dynamic workloads compared to state-of-the-art traditional and learned indexes.

## **2\. Background & Motivation**

Traditional index structures (e.g., B-Trees) are data-agnostic, providing general-purpose performance but failing to capitalize on patterns in real-world data.1 "Learned Indexes" propose replacing these structures with machine learning models that learn the data's distribution, offering significant speed and memory improvements.3 However, foundational research has focused on static, read-only workloads, leaving a gap for dynamic, OS-centric use cases like file system metadata indexing.5

HALI is designed to bridge this gap. It is a hybrid architecture that combines the predictive power of learned models with the guaranteed performance and efficient update handling of traditional data structures, making it robust and practical for real-world systems.2

## **3\. Part I: Experimental Platform & Environment**

### **3.1. Simulator Architecture**

You will build a single-threaded C++17 simulator. The core design must feature a **pluggable index backend**.

* Create a standardized abstract base class (or template) with a std::map-like API: insert(key, value), find(key), erase(key), and load(dataset).  
* Each index structure (baselines and HALI) must be wrapped in a class that implements this common interface.  
* The main benchmarking loop will use this interface to run workloads and measure performance, ensuring a fair, head-to-head comparison.  
* Use std::chrono::high\_resolution\_clock for nanosecond-level latency measurements of individual operations.

### **3.2. Required Libraries & Tooling**

All libraries must be header-only to ensure ease of integration and reproducibility.

| Component | Library/Tool | Repository/Link |
| :---- | :---- | :---- |
| **B+Tree** | phmap::btree\_map | [greg7mdp/parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) |
| **Hash Table** | phmap::flat\_hash\_map | [greg7mdp/parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) |
| **Adaptive Radix Tree** | art\_map | [justinasvd/art\_map](https://github.com/justinasvd/art_map) |
| **PGM-Index** | pgm::PGMIndex | [gvinciguerra/PGM-index](https://github.com/gvinciguerra/PGM-index) |
| **Recursive Model Index** | RMI Codegen Tool | ([https://github.com/learnedsystems/RMI](https://github.com/learnedsystems/RMI)) |

### **3.3. Development & Benchmarking Environment**

To ensure reproducible and isolated performance measurements, all compilation and benchmarking must be conducted within a Docker container running on Windows Subsystem for Linux (WSL 2).

* **Host OS:** Windows  
* **Virtualization:** Windows Subsystem for Linux (WSL 2\)  
* **Containerization:** Docker Desktop (WSL 2 engine)  
* **Container OS:** The Docker container must be based on an **Ubuntu 22.04 LTS** image.  
* **Compiler:** GCC 11.4.0+ (or Clang equivalent) installed inside the container.  
* **Build Flags:** Compile all code with the \-O3 optimization level.  
* **Resource Allocation:** To guarantee consistent results across runs, the Docker container must be launched with pre-allocated CPU cores and a fixed amount of RAM. Use the following flags:  
  Bash  
  docker run \--cpus="4" \--memory="16g"... your\_image\_name

  *(Adjust the core count and memory as appropriate for the host machine, but keep them constant for all experiments.)*

## **4\. Part II: Datasets & Workloads**

### **4.1. Dataset Acquisition & Preprocessing**

#### **4.1.1. Primary Dataset: FSL Homes**

* **Description:** Multi-year snapshots of student home directories, representing a real-world OS metadata workload.10 This dataset is ideal for testing performance on complex, hierarchical string keys.  
* **Acquisition:** Download from the FSL Traces and Snapshots Public Archive.11 You will also need the fs-hasher package from the same source, which contains the hf-stat tool required to parse the snapshot files.11  
* **Preprocessing:**  
  1. Use the hf-stat tool to parse the raw snapshot files and extract anonymized file paths.11  
  2. The primary keys for the index will be these file paths (variable-length strings).  
  3. **Key Normalization:** For use in learned models, convert each file path string into a 64-bit unsigned integer using a high-quality, non-cryptographic hash function (e.g., xxHash, MurmurHash3). Store the original strings separately to resolve hash collisions.

#### **4.1.2. Secondary Dataset: MSR-Modeled Synthetic Generator**

* **Description:** A synthetic workload generator based on a foundational OS research paper, used to test dynamic update performance and scalability with realistic, bursty data patterns.  
* **Implementation:** Create a generator that produces file creation timestamps (as 64-bit integers). The statistical properties of these timestamps (e.g., burstiness, non-uniformity) must be modeled on the findings of the "Five-Year Study of File-System Metadata" from Microsoft Research.6

### **4.2. Workload Definitions**

Implement a workload generator in the simulator to produce the following sequences of operations:

1. **Workload 1: Read-Heavy (Code Compilation)**  
   * **Mix:** 95% find operations, 5% insert operations.  
   * **Keys:** Drawn randomly from the FSL Homes dataset.  
2. **Workload 2: Write-Heavy (Logging)**  
   * **Mix:** 90% insert operations, 10% find operations.  
   * **Keys:** Generated with strong temporal locality (monotonically increasing timestamps) from the MSR-Synthetic generator.  
3. **Workload 3: Mixed Read/Write (General Use)**  
   * **Mix:** 50% find operations, 50% insert operations.  
   * **Keys:** Drawn randomly from the FSL Homes dataset.

## **5\. Part III: Algorithm Implementation**

### **5.1. Baseline Indexes**

Implement wrappers for the following baseline structures using the specified libraries.

| Index Type | C++ Implementation | Justification |
| :---- | :---- | :---- |
| **B+Tree** | phmap::btree\_map | State-of-the-art, cache-friendly in-memory B-Tree.\[15\] |
| **Hash Table** | phmap::flat\_hash\_map | Fastest possible point-lookup performance.\[16\] |
| **Adaptive Radix Tree (ART)** | justinasvd/art\_map | Modern, cache-efficient trie optimized for string-like keys.\[17, 18\] |
| **PGM-Index** | gvinciguerra/PGM-index | State-of-the-art learned index with provable guarantees.\[19\] |
| **Recursive Model Index (RMI)** | learnedsystems/RMI | Foundational learned index architecture.\[20, 21, 22\] |

### **5.2. Novel Contribution: HALI Implementation**

Implement the HALI architecture as a new index type within the simulator.

1. **Level 1: Top-Level RMI Router**  
   * Train a small, static 2-layer RMI on a sample of the full dataset.  
   * Its purpose is to take a key and output an integer index corresponding to a specific sub-range (i.e., which L2 expert to query).  
2. **Level 2: Specialized Expert Models**  
   * During the initial build phase, partition the entire dataset according to the L1 router.  
   * For each partition, implement the following selection logic:  
     * **PGM-Index Expert:** If data in the partition is nearly linear (can be fit with a PGM-Index below a certain error/size threshold), instantiate a PGM-Index.  
     * **RMI Expert:** If the data is non-linear but still learnable, train a dedicated, fine-grained RMI for that partition.  
     * **ART Fallback (Robustness):** If the data is unlearnable (a learned model would have high error or be larger than a traditional alternative), discard the model and instantiate an ART for that partition.3 This is HALI's performance guarantee.  
3. **Dynamic Update Layer: The ART Delta-Buffer**  
   * Instantiate a single, global art\_map to serve as the delta-buffer.  
   * **Insertions:** All new keys are inserted directly into this ART. This must be a fast $O(k)$ operation.17  
   * **Lookups:** A find operation must first query the static L1/L2 structure. If the key is not found, it must then perform a second lookup in the ART delta-buffer.  
   * **Merging:** When the delta-buffer's size exceeds a configurable threshold (e.g., 1% of the main index size), trigger a merge. The contents of the ART are merged with the main data array, and the L1/L2 models are retrained in the background on the new, complete dataset.  
4. **Optimization: Adaptive Last-Mile Search**  
   * For lookups resolved by an RMI or PGM-Index expert, the final bounded search must be optimized.  
   * Based on the returned error bound err:  
     * If err is small (e.g., \< 64), use a branchless **linear scan**.  
     * If err is large, use a traditional **binary search**.

## **6\. Part IV: Evaluation & Synthesis**

### **6.1. Metrics to Collect**

Instrument the simulator to collect the following metrics for each index on each workload.

* **Point Lookup Performance:** Mean Latency (ns/op), P95 & P99 Tail Latency (ns/op).  
* **Update Performance:** Insertion Throughput (ops/sec).  
* **Memory Footprint:** Total Index Size (MB), Space Efficiency (bytes/key).  
* **Build Cost:** Wall-clock time to construct the index from the full dataset, including model training.

### **6.2. Experimental Design**

Execute the following experiments and record the results.

| Experiment | Question | Datasets | Workloads | Metrics of Interest |
| :---- | :---- | :---- | :---- | :---- |
| **E1: Static Lookup** | On read-only OS data, how do indexes compare in speed and size? | FSL Homes | Read-Heavy | Mean/Tail Latency, Memory Footprint |
| **E2: Dynamic Throughput** | How do indexes handle high-volume, write-intensive workloads? | MSR-Synthetic | Write-Heavy | Insertion Throughput, P99 Lookup Latency |
| **E3: Mixed Workload** | Under a balanced load, what is the overall performance trade-off? | FSL Homes | Mixed R/W | Throughput, P95/P99 Latency |
| **E4: Scalability** | How does performance change as key count increases from 10M to 200M? | MSR-Synthetic | Read-Heavy | Mean Latency, Build Time, Memory Footprint |
| **E5: Robustness** | How do learned indexes handle unlearnable, random data? | Uniform Random | Read-Heavy | Mean Latency, Memory Footprint |

### **6.3. Micro-architectural Analysis**

To explain *why* one index outperforms another, use the Linux perf tool inside the Docker container to measure hardware-level behavior.27 For the core lookup loop of each index, run targeted micro-benchmarks and measure:

* **Cache Misses:** cache-references, cache-misses (for L1, L2, LLC).  
* **Branch Prediction:** branch-instructions, branch-misses.  
* **Instructions Per Cycle (IPC):** cycles, instructions.

**Example perf stat command (to be run inside the container):**

Bash

perf stat \-e cycles,instructions,cache-misses,branch-misses./simulator \--index=BTree \--workload=E1

### **6.4. Synthesis and Deliverables**

The final output will be a research paper that presents the findings in a clear, convincing format. The paper must include:

1. **Pareto Frontier Plots:** For each static dataset, plot the space-time trade-off (Memory Footprint vs. Mean Lookup Latency) for every index.29 This will visually demonstrate which indexes dominate others.  
2. **Latency CDF Plots:** For dynamic workloads, generate Cumulative Distribution Function (CDF) plots of lookup latencies to provide a detailed view of the entire latency distribution, making it easy to compare tail performance.31 A steeper curve to the left indicates better, more predictable performance.  
3. **Performance Tables:** Present the results from the experimental design and the micro-architectural analysis in clear, well-formatted tables.  
4. **Written Analysis:** The core of the paper is the analysis. You must:  
   * Describe the HALI architecture in detail.  
   * Present the results from all experiments.  
   * **Crucially, explain the results.** Connect the observed performance (latency, throughput) to the architectural design of each index and the low-level hardware behavior measured with perf. For example, explain that HALI's lower latency is caused by fewer cache misses and a higher IPC.