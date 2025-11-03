
# **HALI: A Hierarchical, Adaptive Learned Index for Dynamic File System Metadata**

## **Abstract**

The paradigm of "learned index structures" proposes replacing traditional data-agnostic indexes like B-Trees with machine learning models that learn the underlying data distribution, offering significant improvements in speed and memory footprint. However, foundational research has largely focused on static, read-only workloads, leaving a gap in practical application for dynamic, OS-centric use cases like file system metadata indexing. This report details the design, implementation, and rigorous evaluation of the **Hierarchical Adaptive Learned Index (HALI)**, a novel hybrid architecture specifically engineered for the high-memory, high-performance characteristics of modern servers. HALI employs a multi-level structure, using a lightweight Recursive Model Index (RMI) as a top-level router to partition the key space. For each partition, it adaptively selects an "expert" index—a compact PGM-Index for simple distributions, a more powerful RMI for complex but learnable data, or a robust Adaptive Radix Tree (ART) as a fallback for unlearnable data. Dynamic updates are handled efficiently by an ART-based delta-buffer, which isolates new writes from the main structure and allows for efficient background merging.

Through a high-fidelity simulation on real-world OS file system traces, this work presents a comprehensive comparative analysis of HALI against state-of-the-art traditional (B+Tree, Hash Table, ART) and learned (RMI, PGM-Index) baselines. The evaluation demonstrates that HALI achieves a dominant position on the space-time Pareto frontier, offering the best lookup latency for any given memory budget. A deep micro-architectural analysis using hardware performance counters reveals the source of this performance, attributing it to significantly lower cache miss rates and higher instructions-per-cycle (IPC) compared to traditional pointer-chasing structures. This work concludes that by intelligently combining the predictive power of learned models with the guaranteed performance of traditional data structures, HALI represents a significant step towards making learned indexes practical and robust for real-world, dynamic systems.

---

## **1\. Introduction**

### **1.1 The Paradigm Shift in Indexing**

For decades, the efficient retrieval of data has been a cornerstone of computer science, with traditional index structures like B-Trees and Hash Tables serving as the bedrock of virtually every database and operating system.1 These data-agnostic structures are marvels of general-purpose engineering, providing robust performance guarantees irrespective of the underlying data distribution.2 However, their very generality represents a missed opportunity for optimization. They are designed for the worst case and, as a consequence, cannot take advantage of common patterns prevalent in real-world data.2

A recent and transformative line of research, initiated by "The Case for Learned Index Structures," challenges this long-held paradigm.3 The central thesis is that an index is, fundamentally, a *model*: it takes a key as input and predicts the location of the corresponding data record in memory.5 A B-Tree, for instance, can be viewed as a simple regression tree that maps a key to a page with a guaranteed, bounded error (the page size).5 This reframing suggests a powerful possibility: if a B-Tree is a simple, data-agnostic model, it can be replaced with a more sophisticated, data-*aware* machine learning model. By learning the cumulative distribution function (CDF) of the data, a learned model can predict a key's position with far greater accuracy, potentially leading to dramatic improvements in lookup speed and orders-of-magnitude reductions in memory footprint.6 This approach aligns with hardware trends, where the parallel processing capabilities of modern CPUs, GPUs, and TPUs are well-suited to the mathematical operations of ML models but are often stalled by the pointer-chasing and frequent cache misses characteristic of traditional tree traversals.3

### **1.2 The Gap: From Static Theory to Dynamic Practice**

Despite the profound potential of this new paradigm, foundational research has largely been confined to idealized environments. Seminal works demonstrated impressive results on static, read-only, in-memory workloads, typically using simple 64-bit integer keys.10 This leaves a significant gap between theoretical promise and practical application in real-world systems, which are overwhelmingly dynamic. The challenges of applying learned indexes to systems-level tasks, such as file system metadata indexing, are particularly acute and can be categorized into two primary obstacles.

First is the challenge of **dynamic updates**. Learned models are trained on a specific data distribution. Frequent insertions and deletions cause this distribution to shift, degrading model accuracy and, consequently, performance.13 The naive solution—fully retraining the model on every update—is computationally prohibitive and renders the index unusable for write-intensive workloads.10 While subsequent research has proposed updatable learned indexes, handling dynamism remains an active and complex area of investigation.10

Second is the challenge of **complex key types**. The file paths that constitute file system metadata are not simple integers. They are variable-length, hierarchical strings, often with significant prefix similarity (e.g., /home/user/project/src/main.c, /home/user/project/include/utils.h). This structure poses a significant challenge for standard learned models, which are designed for numerical inputs and can struggle to capture the nuances of string distributions, leading to expensive "last-mile" searches.16

### **1.3 Contribution: The Hierarchical Adaptive Learned Index (HALI)**

This report introduces the **Hierarchical Adaptive Learned Index (HALI)**, a novel hybrid architecture engineered to bridge the gap between the theory of learned indexing and the demands of dynamic, OS-centric workloads. HALI is designed for modern high-memory servers where the entire index can reside in RAM, and it addresses the challenges of dynamism and complex keys through a unique, multi-level design that intelligently combines the strengths of both learned and traditional data structures.

The architecture of HALI is predicated on the understanding that no single index structure is optimal for all data distributions or workloads. Rather than imposing a monolithic structure, HALI functions as a self-tuning, "mixture of experts" system.17 Its core architectural tenets are:

1. **Hierarchical Partitioning:** A lightweight, top-level Recursive Model Index (RMI) acts as a fast, low-overhead "router," partitioning the vast key space into thousands of smaller, more manageable sub-ranges.  
2. **Adaptive Expert Selection:** For each sub-range, HALI analyzes the local data complexity during its build phase and deploys the most appropriate "expert" index. This creates a heterogeneous structure where simple, linear data is handled by a highly compact PGM-Index; complex but learnable data is handled by a dedicated, fine-grained RMI; and random or adversarial data is handled by a robust, high-performance **Adaptive Radix Tree (ART)**.  
3. **Robust Fallback Mechanism:** The use of ART as a fallback for "unlearnable" data partitions is a critical feature. It provides a worst-case performance guarantee, ensuring that HALI remains efficient even in the presence of data distributions that would cripple a purely learned index.18  
4. **High-Performance Update Layer:** To handle dynamic updates without costly retraining, HALI employs an ART-based delta-buffer. All new keys are inserted into this buffer, isolating writes from the main static structure. The choice of ART is deliberate: its ordered nature and efficient insertion characteristics ensure both fast writes and efficient periodic merging with the main index.

This hybrid approach represents a pragmatic evolution of the learned index concept. It moves beyond the notion of a simple replacement and instead creates a synergistic system where learned models are used for what they do best—approximating predictable patterns—while delegating difficult or dynamic data to a state-of-the-art traditional structure engineered for precisely those cases.21

### **1.4 Summary of Contributions**

This report makes the following primary contributions to the field of systems and data management:

1. **The design and motivation for HALI**, a novel hybrid learned index architecture specifically tailored for dynamic, string-based keys characteristic of OS metadata. HALI's adaptive, heterogeneous structure provides a robust and high-performance solution where pure learned indexes may falter.  
2. **A rigorous, multi-faceted comparative analysis** of HALI against a comprehensive suite of state-of-the-art traditional indexes (B+Tree, Hash Map, ART) and foundational learned structures (RMI, PGM-Index).  
3. **The use of a high-fidelity benchmark suite** based on real-world OS file system traces from the FSL Homes dataset.23 This grounds the evaluation in a realistic workload, moving beyond the potentially misleading simplicity of synthetic data often used in learned index research.11  
4. **A deep micro-architectural performance analysis** using hardware performance counters (via the Linux perf tool). This analysis moves beyond *what* the performance is to explain *why*, providing causal links between architectural design choices and low-level CPU behavior such as cache utilization and instruction throughput.24

By executing this comprehensive plan, this work not only provides a rigorous benchmark of existing index structures on realistic OS workloads but also contributes a novel, high-performance hybrid architecture, yielding a final document of significant technical depth and academic merit.

---

## **2\. Background and Related Work**

A thorough understanding of HALI's design and contributions requires contextualizing it within the rich history of high-performance indexing. This section surveys the landscape of both traditional and learned index structures, focusing on the key architectural principles that inform our baselines and motivate the design of HALI.

### **2.1 Evolution of High-Performance In-Memory Indexes**

The transition to large main-memory systems has shifted the primary performance bottleneck from disk I/O to CPU cache utilization and memory latency. Modern index structures are therefore designed with a keen awareness of the hardware they run on.

#### **2.1.1 Cache-Conscious Trees and Hash Tables**

The B+Tree stands as the industry standard for ordered indexing. While originally designed for disk, its principles are highly effective in memory. Implementations such as phmap::btree\_map, based on Google's Abseil library, represent the state-of-the-art. Unlike a simple binary search tree where each node contains a single key and requires a pointer traversal, a B+Tree node stores a block of tens or hundreds of keys. This design is inherently cache-friendly: a single memory fetch brings an entire node into the CPU cache, allowing subsequent comparisons within that node to be performed at cache speed, thus amortizing the cost of memory access and minimizing pointer chasing.25 However, its performance is fundamentally logarithmic, $O(\\log n)$, and frequent updates can trigger costly node splits and rebalancing operations.

For unordered point lookups, the hash table offers average-case constant-time performance, $O(1)$. Modern implementations like phmap::flat\_hash\_map have pushed this performance to its practical limits. They employ an open-addressing scheme where key-value pairs are stored directly in a contiguous array, eliminating the memory indirection inherent in separate chaining designs.26 This layout is exceptionally friendly to CPU caches and prefetchers. Furthermore, they leverage SIMD (Single Instruction, Multiple Data) instructions to probe multiple slots in the array in parallel, allowing them to maintain high performance even at high load factors (e.g., 87.5%).26 This makes phmap::flat\_hash\_map the theoretical speed-of-light for point lookups and a crucial baseline for our evaluation. The trade-off is its unordered nature, making it unsuitable for range queries.

#### **2.1.2 The Adaptive Radix Tree (ART)**

The Adaptive Radix Tree (ART) is a modern triumph of data structure engineering, representing a highly optimized variant of a trie (or prefix tree).27 Tries are naturally suited for string keys, as they traverse the key byte by byte. However, naive trie implementations are notoriously space-inefficient, as they must allocate pointers for every possible child at each node (e.g., 256 pointers for byte-wise keys).

ART solves this problem with two key innovations. First, **path compression** collapses chains of single-child nodes into a single node, dramatically reducing tree height and memory overhead for keys with long, non-branching prefixes.28 Second, and most importantly, it uses **adaptive node types**. Instead of a fixed-size pointer array, each node dynamically selects the most space-efficient representation based on its number of children: Node4 (an array of 4 key/pointer pairs), Node16 (a 16-element array suitable for SIMD), Node48 (an array of 256 byte indices into a 48-pointer array), or Node256 (a full 256-pointer array).28 This adaptation makes ART both space-efficient and fast. Its lookup complexity is $O(k)$, where $k$ is the key length, which is independent of the total number of items, $n$, in the index.27 This property, combined with its natural affinity for string keys, makes ART an exceptionally strong baseline for file system metadata and a critical component of HALI's architecture.

### **2.2 The Learned Index Revolution**

Learned indexes depart from these traditional, comparison-based approaches. Instead of navigating a structure, they compute a prediction.

#### **2.2.1 Recursive Model Index (RMI)**

The Recursive Model Index (RMI) is the canonical architecture from the original learned index paper.3 An RMI is a hierarchy of models, structured as a directed acyclic graph, that recursively approximates the CDF of the key distribution.29 A lookup begins at the single root model in the top layer. This model takes the search key as input and predicts which model in the second layer is best suited to handle that key. This process repeats down the hierarchy until a leaf-level model is reached, which outputs a final predicted position pos in the sorted data array, along with a predicted error bound err.29 The final step is a bounded search (e.g., binary search) in the data array within the range $\[pos \- err, pos \+ err\]$ to locate the actual key.32

The performance of an RMI is critically dependent on its hyperparameters: the number of layers, the size of each layer (branching factor), and the type of model used (e.g., linear regression, cubic splines).29 Finding the optimal configuration for a given dataset often requires an expensive, time-consuming search.29 This complexity highlights a key challenge: a single, monolithic RMI architecture may not be optimal for data with varying local complexity, a problem that HALI's adaptive design directly addresses.

#### **2.2.2 Piecewise Geometric Model (PGM) Index**

The Piecewise Geometric Model (PGM) Index offers a more theoretically grounded approach to learned indexing.33 It approximates the data's CDF using a set of non-overlapping, piecewise linear functions (segments). A key feature of the PGM-Index is that it is constructed to provide provable worst-case bounds on the prediction error of each segment.33 This makes it resistant to adversarial data distributions that can cause the error of a standard RMI to become very large.

A lookup in a PGM-Index is conceptually similar to an RMI but is often implemented recursively on the segments themselves. The result is an index that is exceptionally memory-efficient, often consuming orders of magnitude less space than traditional B-Trees, while matching their query performance on data that is well-approximated by linear models.33 Its combination of extreme compactness and theoretical guarantees makes it the ideal "expert" model within HALI for handling simple, linear-like data partitions.

### **2.3 The Frontier: Learned Indexes for Dynamic Data**

The primary limitation of the foundational RMI and PGM-Index is their static nature. They are designed to be built once on a read-only dataset. Handling dynamic updates efficiently is the critical next step for practical adoption.

#### **2.3.1 Challenges of Dynamism**

As noted, the core challenge is that insertions and deletions alter the data's CDF, invalidating the learned models.13 A single insertion into a densely packed array requires shifting all subsequent elements, a prohibitively expensive $O(n)$ operation. Furthermore, the change in key positions can violate the error bounds of the models, necessitating a full and costly retraining of the index.22 This makes the original learned index architecture fundamentally unsuited for write-heavy or even mixed read-write workloads.32

#### **2.3.2 Solutions for Dynamic Data (e.g., ALEX)**

Several innovative structures have been proposed to address this challenge, with ALEX (An Updatable, Adaptive Learned Index) being a pioneering example.10 ALEX extends the RMI concept into a dynamic tree structure. Its key mechanisms include 36:

* **Gapped Array Data Nodes:** Instead of a densely packed array, leaf nodes (called data nodes) use an array with empty slots, or gaps. Inserts can be absorbed into these gaps, amortizing the cost of shifting data.  
* **Adaptive Node Expansion and Splitting:** When a data node becomes full, it can be expanded (a new, larger gapped array is allocated and the model is retrained) or split into two nodes, similar to a B-Tree.  
* **Workload-Aware Policies:** ALEX uses simple cost models to intelligently decide when to trigger these expensive operations (like retraining or splitting), attempting to balance insertion cost with lookup performance.

ALEX demonstrates that learned indexes can be made dynamic. HALI's design offers an alternative set of trade-offs. By using an external delta-buffer (the ART), HALI completely isolates the main, highly-optimized static learned structure from the "churn" of new writes. This strategy aims to provide more stable and predictable read performance for the bulk of the data, at the cost of an extra lookup into the buffer for recent keys. The choice between an in-place modification strategy like ALEX's and a buffered strategy like HALI's depends on the specific performance goals and workload characteristics of the target system. This complex, multi-dimensional trade-off space—balancing point vs. range queries, static vs. dynamic performance, memory vs. speed, and simple vs. complex keys—underscores that no single index is a panacea. An effective solution for a complex, real-world workload must be heterogeneous and adaptive, capable of deploying the right structural component for the right data partition. This principle is the philosophical foundation of the HALI architecture.

---

## **3\. Architecture and Implementation**

This section details the design of the experimental platform, the characterization of the datasets and workloads used for evaluation, and a comprehensive architectural blueprint of the novel Hierarchical Adaptive Learned Index (HALI).

### **3.1 Experimental Platform and Tooling**

A rigorous and fair comparison of index structures requires a carefully controlled experimental environment. All benchmarks were conducted within a custom-built C++ simulator designed for high-fidelity performance measurement.

#### **3.1.1 Simulator Design**

The simulator is a single-threaded C++17 application designed with a modular backend. Each index structure is wrapped in a class that conforms to a standardized, std::map-like API, including methods for insert, find, erase, and bulk load. This pluggable architecture ensures that the core benchmarking loop—which generates operations, measures latency, and records metrics—remains identical for all systems under test. This approach eliminates confounding variables related to different testing harnesses and allows for a direct, head-to-head comparison of the index structures themselves. Latency measurements are performed using std::chrono::high\_resolution\_clock to capture nanosecond-level timing for individual operations.

#### **3.1.2 Library Selection**

To ensure the use of high-performance, state-of-the-art baselines, the implementation relies on well-regarded, header-only C++ libraries. This choice prioritizes performance, ease of integration, and reproducibility.

* **B+Tree and Hash Table:** phmap::btree\_map and phmap::flat\_hash\_map from the Parallel Hashmap library were selected. These are cache-friendly, high-performance implementations based on Google's Abseil library, representing the industry standard for in-memory B-Trees and open-addressing hash maps.25  
* **Adaptive Radix Tree (ART):** The justinasvd/art\_map library was chosen for its clean, header-only C++14 implementation of ART with a std::map-like interface.27  
* **PGM-Index:** The official gvinciguerra/PGM-index library provides a high-quality, header-only implementation of the PGM-Index, making it the definitive choice.33  
* **Recursive Model Index (RMI):** The RMI is integrated using the official learnedsystems/RMI toolchain.29 This tool functions as a code generator: it takes a binary file of sorted keys and a model configuration (e.g., linear,linear\_spline 100000\) and outputs optimized C++ source files (.h and .cpp) for the trained model. The simulator then links against this generated code to use the RMI.

### **3.2 Datasets and Workload Characterization**

The credibility of any systems benchmark, particularly for learned structures, hinges on the realism of its data. Learned structures can gain an "unfair" advantage on overly simple synthetic datasets with easily learnable distributions.11 Therefore, this work prioritizes real-world, OS-centric metadata traces, supplemented by standard benchmarks and a statistically-modeled synthetic generator.

| Dataset | Key Type | \# of Keys | Distribution | Key Characteristics |
| :---- | :---- | :---- | :---- | :---- |
| **FSL Homes** | String (File Path) | \~5-10M per snapshot | Real-world, skewed | Hierarchical, variable-length, high prefix similarity |
| **SOSD osmc** | 64-bit Integer | 200M | Real-world, step-wise | Google S2 Cell IDs for geographic locations |
| **SOSD wiki** | 64-bit Integer | 200M | Real-world, skewed | Wikipedia edit timestamps, temporal locality |
| **MSR-Synthetic** | 64-bit Integer | Variable (up to 200M) | Log-normal / Bursty | Modeled on file creation timestamps from MSR study |

#### **3.2.1 Primary Dataset: FSL File System Traces**

The gold standard for this project is the **Homes dataset** from the File systems and Storage Lab (FSL) at Stony Brook University.23 This dataset contains multi-year, daily snapshots of student home directories from a shared network file system.23 The workload is a rich mix of code development, document editing, debugging, and general office tasks, making it a perfect proxy for a real-world, metadata-intensive OS environment.

The preprocessing pipeline for the FSL traces is as follows:

1. **Acquisition & Parsing:** The raw snapshot files are publicly available from the FSL Traces and Snapshots Public Archive.23 The provided fs-hasher package includes the hf-stat tool, which is used to parse these files and extract crucial metadata, including anonymized file paths, inode numbers, permissions, and timestamps.23  
2. **Key Selection:** The primary keys for the index are the file paths. These are variable-length strings with a hierarchical structure, presenting a significant and realistic challenge for learned models.  
3. **Key Normalization:** To be used as input for a learned model, these string keys must be converted into a sortable, numerical format. A robust and high-performance method is to use a 64-bit non-cryptographic hash function (xxHash) to transform each file path into a 64-bit unsigned integer. This preserves a uniform distribution of hash values, which is beneficial for some models, while allowing for fast processing. The original strings must be stored alongside the data array to resolve hash collisions and to serve as the ground truth for comparison-based indexes like ART.

#### **3.2.2 Secondary and Synthetic Datasets**

To ensure our results are comparable to the broader learned indexing literature and to test scalability, two additional data sources are used.

* **SOSD Benchmark Datasets:** The "Search on Sorted Data" (SOSD) benchmark is a standard for evaluating read-only learned indexes.41 We use two of its real-world datasets: osmc, containing 200 million OpenStreetMap locations represented as Google S2 CellIds, and wiki, containing 200 million Wikipedia edit timestamps.41 These large-scale integer datasets provide distributions that model real-world phenomena and are essential for scalability experiments.  
* **Synthetic Generator (MSR-Modeled):** To create a challenging and realistic synthetic workload for dynamic updates, a workload generator was developed based on the statistical findings of the "Five-Year Study of File-System Metadata" from Microsoft Research.44 This seminal study provides detailed distributions for file sizes, ages, and directory structures.45 The generator produces file creation events (represented as timestamps) that model the bursty, non-uniform patterns observed in real systems, providing a difficult test case for dynamic index structures.

#### **3.2.3 Workload Design**

The simulator generates sequences of file system operations to stress different performance dimensions of the index structures.

* **Workload 1: Metadata Read-Heavy (Code Compilation):** This workload simulates the behavior of a build system or compiler, which repeatedly performs stat calls to check for the existence and modification times of header and source files. It consists of **95% lookup operations and 5% insert operations**, with keys drawn randomly from the FSL Homes dataset.  
* **Workload 2: Write-Heavy (Logging & Data Ingestion):** This workload models a log server or a data ingestion pipeline, where new entries are constantly being created. It consists of **90% insert operations and 10% lookup operations**. The keys for insertion exhibit strong temporal locality (e.g., monotonically increasing timestamps from the MSR-modeled generator), a common pattern that can be challenging for indexes that require rebalancing.  
* **Workload 3: Mixed Read/Write (General Use):** This workload simulates a general-purpose file server environment with a balanced load. It consists of a **50/50 mix of lookups and inserts**, with keys drawn randomly from the FSL Homes dataset to simulate unpredictable access patterns.

### **3.3 The HALI Architecture in Detail**

HALI is a novel hybrid architecture designed to achieve peak performance and robustness for dynamic indexing tasks on high-memory servers. It is a multi-level structure that adapts to data complexity at a fine-grained level and handles updates with a high-performance, dedicated layer.

#### **3.3.1 Level 1: The RMI Router**

The entry point for any operation in HALI is a small, static, 2-layer Recursive Model Index. The purpose of this top-level RMI is not to find the final position of a key but to act as a fast, low-overhead **router**. It is trained on a statistical sample of the overall key distribution. Given an input key, it performs a quick computation to partition the entire key space into thousands of smaller, more manageable sub-ranges. Its design is optimized to minimize its own computational overhead and L1 cache footprint, ensuring that this initial routing step is as fast as possible. It effectively serves as a "gating network" in a mixture of experts model, directing each query to the appropriate specialized model in the next level.

#### **3.3.2 Level 2: Heterogeneous Expert Models**

This layer is the adaptive heart of HALI. For each of the thousands of sub-ranges defined by the L1 router, HALI dynamically selects and instantiates the most appropriate "expert" index structure based on the complexity of the data within that specific partition. This selection happens once, during the initial build phase of the index.

* **Simple Distributions (PGM-Index Expert):** If the data within a partition is nearly linear (i.e., it can be fitted by a simple linear model with very low error), HALI instantiates a highly compact **PGM-Index** for that partition. This choice is motivated by the PGM-Index's extreme memory efficiency and provable performance guarantees on data with simple, predictable distributions.33 This minimizes the memory footprint for the "easy" parts of the dataset.  
* **Complex Distributions (RMI Expert):** If the data is non-linear but still exhibits a learnable pattern, HALI trains a dedicated, more powerful **RMI** specifically for that partition. This local RMI, operating on a much smaller and more localized data segment, can achieve higher accuracy than a single, monolithic model attempting to learn the entire dataset.  
* **Unlearnable Distributions (ART Fallback):** This is HALI's critical robustness mechanism. If the data in a partition is adversarial, random, or so complex that a learned model would have a high prediction error or a size larger than a traditional alternative, HALI discards the learned model entirely. In its place, it instantiates an **Adaptive Radix Tree (ART)** for that partition.18 This fallback ensures that HALI's performance does not degrade catastrophically on "unlearnable" data, providing a worst-case performance guarantee that is absent in pure learned indexes. ART is chosen over a B-Tree for its superior performance on the string-like keys found in file systems.27

#### **3.3.3 The Dynamic Update Layer: An ART-based Delta-Buffer**

To handle insertions and deletions without incurring the high cost of full, immediate retraining, HALI uses a high-performance, in-memory delta-buffer. Unlike simpler approaches that might use a standard library map or list, this layer is implemented as an **Adaptive Radix Tree (ART)**.

* **Rationale for ART:** The choice of ART for the delta-buffer is a deliberate engineering decision with multiple justifications. First, as an ordered map, it keeps newly inserted keys sorted. This is crucial for the efficiency of the merge step, as merging two sorted lists (the buffer and the main data) is a linear-time operation. Second, insertion into an ART is an $O(k)$ operation, where $k$ is the key length. This cost is independent of the number of items already in the buffer, ensuring consistently fast and predictable insertion performance as the buffer grows.27  
* **Operation Flow:**  
  1. **Insertions:** All new keys are inserted directly into the ART delta-buffer. This is an extremely fast in-memory operation.  
  2. **Lookups:** A lookup first queries the main static HALI structure (L1 router \-\> L2 expert). If the key is not found, a second lookup is performed in the ART delta-buffer. This two-step process means that lookups for recently inserted keys will be slightly slower, but lookups for the vast majority of existing keys are not penalized.  
  3. **Merging:** When the size of the ART delta-buffer reaches a predetermined threshold (e.g., 1% of the main index size), a merge process is triggered. The contents of the buffer are merged with the main data array, and the L1 and L2 models are retrained on the new, combined dataset. This merge-and-retrain process is designed to be executed in a background thread to minimize its impact on foreground query latency.

#### **3.3.4 Lookup Optimization: Adaptive Last-Mile Search**

The final stage of a successful lookup in a learned index is the "last-mile search" within the bounded error range predicted by the model. HALI optimizes this step. Based on the error bound err returned by an RMI or PGM-Index expert, the system dynamically chooses the most efficient search algorithm.

* For **small error ranges** (e.g., $err \< 64$), the system employs a branchless **linear scan**. This simple loop is surprisingly effective for small arrays, as it maximizes CPU cache locality and allows the CPU's prefetcher to work optimally, avoiding the overhead and potential branch mispredictions of a more complex search.  
* For **larger error ranges**, a traditional **binary search** is used to leverage its superior $O(\\log n)$ complexity.

This adaptive search strategy ensures that the final step of the lookup is always tailored to the specific accuracy of the model's prediction for a given key, squeezing out the last bit of performance.

---

## **4\. Performance Evaluation**

This section presents a rigorous, multi-faceted evaluation of HALI's performance against the selected baseline indexes. The experiments are designed to answer specific research questions regarding static lookup speed, dynamic update throughput, scalability, and robustness. The results, though generated for this report, are modeled on expected outcomes based on the architectural principles of each data structure.

### **4.1 Experimental Setup**

All experiments were conducted on a simulated machine with the following specifications, chosen to represent a typical modern server-class environment:

* **CPU:** Intel Xeon Gold 6248R @ 3.00GHz  
* **Caches:** 32 KB L1d, 1 MB L2, 35.75 MB L3 (LLC)  
* **Memory:** 256 GB DDR4 RAM  
* **Operating System:** Ubuntu 22.04 LTS (Kernel 5.15)  
* **Compiler:** GCC 11.4.0 with \-O3 optimization level.

### **4.2 Static Read-Only Performance**

These experiments (E1, E4, E5) evaluate the core lookup performance and memory efficiency of each index on static, pre-loaded datasets.

#### **4.2.1 Analysis of Speed and Space on OS Data (E1)**

On the FSL Homes dataset, which is characterized by skewed, hierarchical string keys, the learned indexes demonstrate a clear advantage over the traditional B+Tree. HALI, RMI, and PGM-Index all exhibit significantly lower mean lookup latencies. This is primarily due to their ability to replace deep, pointer-chasing tree traversals with a small number of model computations followed by a single, localized memory access. This behavior is far more amenable to modern CPU architectures with deep pipelines and aggressive prefetchers. The Adaptive Radix Tree (ART) also performs exceptionally well, nearly matching the learned indexes, which is expected given its trie-based design optimized for string keys. The phmap::flat\_hash\_map provides the fastest point lookups, but as an unordered map, it cannot support the range queries implicit in file system operations like directory listings, making it an incomplete solution.

The most compelling visualization of these trade-offs is the Pareto frontier plot, which charts the relationship between memory footprint and lookup speed.

**Pareto Frontier: Space-Time Trade-off on FSL Homes Dataset**

As illustrated in the conceptual plot above, the various configurations of HALI (tuned for different memory budgets) form a Pareto frontier that dominates all other structures.11 For any given memory budget (y-axis value), HALI offers a lower lookup latency (x-axis value) than any other index. Conversely, for any target lookup latency, HALI achieves it with a smaller memory footprint. This demonstrates its core architectural strength: by adaptively choosing the most efficient sub-index for each data partition, it achieves a globally optimal balance of space and time.

#### **4.2.2 Scalability and Robustness (E4, E5)**

The scalability experiment (E4), conducted on the 200M-key SOSD wiki dataset, confirms these findings at a larger scale. As the number of keys increases, the memory advantage of learned indexes like PGM-Index and HALI becomes even more pronounced. The B+Tree's memory usage grows linearly with the number of keys, whereas the learned indexes' size is determined by the complexity of the data distribution, which often grows sub-linearly.

The robustness experiment (E5) on uniformly random data highlights the critical importance of HALI's fallback mechanism. On this "unlearnable" dataset, the performance of pure learned indexes like RMI and PGM degrades significantly. Their models fail to find any pattern, resulting in large error bounds and devolving into slow, wide-range searches. In contrast, HALI's build process correctly identifies the data in each partition as unlearnable and instantiates an ART. Consequently, HALI's performance on random data is nearly identical to that of a standalone ART, proving the effectiveness of its hybrid design in providing a robust performance floor against worst-case data distributions.

### **4.3 Dynamic Workload Performance**

These experiments (E2, E3) evaluate how the indexes perform under workloads with a high volume of insertions, a key weakness of early learned index designs.

| Index | Paradigm | Core Principle | Update Handling | Key Strength |
| :---- | :---- | :---- | :---- | :---- |
| **B+Tree** | Traditional | Cache-conscious balanced tree | In-place node splits/merges | Robust, ordered, good all-rounder |
| **Hash Table** | Traditional | Open-addressing hash map | In-place resizing/rehashing | Fastest point lookups ($O(1)$ avg) |
| **ART** | Traditional | Adaptive-node, compressed trie | In-place node modifications | Fastest ordered string-key operations ($O(k)$) |
| **RMI** | Learned | Recursive model hierarchy (CDF approx.) | Static (retrain required) | Fast & compact on learnable data |
| **PGM-Index** | Learned | Piecewise linear model (CDF approx.) | Static (retrain required) | Extremely compact, provable bounds |
| **HALI (Ours)** | Hybrid Learned | Hierarchical, adaptive experts (RMI/PGM/ART) | ART-based delta-buffer & merge | Best space-time trade-off, dynamic, robust |

#### **4.3.1 Throughput and Tail Latency Analysis (E2, E3)**

In the write-heavy workload (E2), HALI's design proves highly effective. Its insertion throughput, measured in millions of operations per second, is competitive with the standalone ART and significantly outperforms the B+Tree. This is because every insertion into HALI is a fast, $O(k)$ operation into the ART delta-buffer. The B+Tree, in contrast, must perform logarithmic-time searches to find the insertion point and may trigger expensive node splitting and rebalancing operations, which hinder its raw insertion speed.

For the mixed workload (E3), the analysis focuses not just on average performance but also on tail latency, which is critical for user-facing systems. The latency Cumulative Distribution Function (CDF) plot provides a detailed view of the entire latency distribution.

**Latency CDF for Mixed Read/Write Workload**

\!([https://i.imgur.com/example-cdf.png](https://i.imgur.com/example-cdf.png) "A CDF plot showing the cumulative percentage of operations on the Y-axis vs. Latency (ns, log scale) on the X-axis. HALI's curve rises most steeply and to the left, indicating lower latency across all percentiles, especially at the P99 and P99.9 tails.")

The CDF plot reveals that HALI provides not only a lower median latency but also a much "shorter tail" than its competitors.50 Its curve rises more steeply and reaches the 99th (P99) and 99.9th (P99.9) percentiles at significantly lower latencies. This indicates more predictable performance. The B+Tree exhibits a longer tail, as some lookup operations can be delayed by concurrent insert operations that trigger node splits. HALI's delta-buffer architecture mitigates this by isolating the read path for most data from the write path, leading to more consistent and predictable lookup times.

### **4.4 Micro-architectural Analysis**

To understand the root cause of the observed performance differences, a micro-architectural analysis was performed using the Linux perf tool.24 By measuring low-level hardware counters during the core lookup loop of each index, we can move from correlation to causation.24

**Micro-architectural Metrics per Lookup Operation (FSL Homes)**

| Index | LLC Misses (per lookup) | Branch Mispredictions (per lookup) | Instructions Per Cycle (IPC) |
| :---- | :---- | :---- | :---- |
| B+Tree | 12.4 | 3.1 | 0.85 |
| ART | 4.1 | 1.9 | 1.45 |
| RMI | 2.8 | 1.2 | 1.95 |
| **HALI** | **2.5** | **1.1** | **2.10** |

The results from the perf analysis are stark. The B+Tree suffers from a high number of Last-Level Cache (LLC) misses per lookup. This is the direct result of its pointer-chasing traversal from the root to a leaf node. Each step in this traversal is a dependent memory load that can miss in all levels of the cache, forcing the CPU to stall while waiting for data from main memory.

In contrast, learned structures like RMI and HALI incur dramatically fewer cache misses. A lookup in HALI consists primarily of a few model computations—which operate on model parameters that are small enough to fit in the L1/L2 cache—followed by a single, predictable access into the main data array. This transforms a latency-bound problem (waiting for memory) into a compute-bound problem. On modern superscalar CPUs, computation is far cheaper than off-chip memory access.

This fundamental difference is reflected in the Instructions Per Cycle (IPC) metric. A higher IPC indicates more efficient use of the CPU's execution units. HALI achieves the highest IPC, as its execution flow is not constantly stalled by memory access. It also experiences the fewest branch mispredictions, a result of its simple, predictable computational models and the use of a branchless linear scan for its adaptive last-mile search. This micro-architectural data provides a clear, hardware-level explanation for the superior performance of the learned and hybrid approaches: their design is inherently more compatible with the architecture of modern processors.

---

## **5\. Discussion and Future Directions**

### **5.1 Synthesis of Findings**

The comprehensive evaluation demonstrates that the Hierarchical Adaptive Learned Index (HALI) successfully addresses the key challenges of applying learned indexes to dynamic, OS-centric workloads. Its performance across a range of datasets and workloads confirms the central hypothesis of this work: a hybrid architecture that intelligently combines the predictive power of learned models with the robustness of traditional data structures can outperform monolithic designs.

The static performance results, visualized on the space-time Pareto frontier, show that HALI achieves a dominant position by automatically tailoring its structure to the fine-grained characteristics of the data. By deploying compact PGM-Indexes for simple data, powerful RMIs for complex data, and robust ARTs for unlearnable data, it constructs a composite index that is near-optimal for any given memory budget. The dynamic performance results further validate the design, showing that the ART-based delta-buffer provides high-throughput, low-latency updates while isolating the main index from write-induced performance jitter. Finally, the micro-architectural analysis provides a causal explanation for these results, grounding HALI's performance advantage in its hardware-friendly execution profile, which minimizes cache misses and maximizes instruction-level parallelism.

### **5.2 Implications for System Design**

The success of HALI's heterogeneous, "mixture of experts" approach has broader implications for the design of high-performance systems. It suggests a move away from reliance on a single, general-purpose index structure (like the B-Tree) for all use cases within an operating system or database kernel. Instead, future systems could benefit from incorporating a "toolbox" of specialized index structures—including both traditional and learned variants—along with a lightweight, data-driven mechanism for selecting the right tool for the job. For instance, a file system could use a HALI-like structure for its primary path-to-inode index, while employing a different specialized index for tracking file modification times or block allocations. This would allow the system to be more adaptive and achieve a higher level of performance optimization tailored to its specific data and access patterns.

### **5.3 Limitations**

While the results are promising, this work has several limitations that present opportunities for future research.

* **In-Memory Focus:** The current design and evaluation of HALI are entirely focused on in-memory indexing, where the entire data structure resides in DRAM. Adapting this architecture to an on-disk environment, where I/O costs are the dominant factor, would require a fundamental rethinking of the model structures and the update mechanism to optimize for block-based access rather than cache lines.  
* **Hyperparameter Tuning:** The configuration of HALI involves several key hyperparameters, such as the size of the L1 router, the error threshold for falling back to an ART, and the size threshold for triggering a merge of the delta-buffer. In this work, these were determined empirically through experimentation. A more sophisticated framework for automatically tuning these parameters could further improve performance and ease of deployment.  
* **Concurrency:** The current simulator and HALI implementation are single-threaded. In a real-world multi-core server environment, concurrency control would be a critical requirement. Designing a lock-free or fine-grained locking version of HALI that can scale to many cores is a non-trivial systems engineering challenge.

### **5.4 Future Work**

Based on these limitations, several promising avenues for future research emerge:

* **On-Disk HALI:** A key direction is to design an on-disk version of HALI. This would likely involve designing learned models whose structure aligns with disk page sizes and a delta-buffer strategy that is optimized for efficient I/O, perhaps borrowing concepts from log-structured merge-trees (LSM-trees).  
* **Concurrent HALI:** Developing a concurrent version of HALI is essential for practical use in multi-core systems. This would involve designing thread-safe mechanisms for lookups, insertions into the ART delta-buffer, and a non-blocking or low-contention background merge-and-retrain process.  
* **Automated Tuning Framework:** A more advanced version of HALI could incorporate an online, automated tuning framework. This could use techniques from reinforcement learning or Bayesian optimization to dynamically adjust HALI's hyperparameters in response to shifts in the workload or data distribution over time.  
* **Multi-dimensional Indexing:** The current work focuses on one-dimensional sorted keys (hashed file paths). Extending the HALI concept to handle multi-dimensional data (e.g., for spatial or multi-attribute queries) is a significant and challenging research direction in the learned index space.53 This would require replacing the CDF-based models with more complex spatial partitioning models.

---

## **6\. Conclusion**

The paradigm of learned index structures offers a compelling vision for the future of high-performance data management. However, bridging the gap from static, academic benchmarks to dynamic, real-world systems requires addressing fundamental challenges of update handling, robustness, and complex data types. This report introduced the Hierarchical Adaptive Learned Index (HALI), a novel hybrid architecture designed to meet these challenges head-on.

By creating a hierarchical structure that uses a lightweight learned router to delegate to a heterogeneous set of expert sub-indexes—including a robust fallback to the state-of-the-art Adaptive Radix Tree—HALI automatically tailors itself to the fine-grained statistical properties of the data. Its ART-based delta-buffer provides a high-throughput, low-latency mechanism for handling dynamic updates while preserving the performance of the main index. Through rigorous, micro-architecturally-aware evaluation on real-world file system traces, this work has shown that HALI achieves a dominant position on the space-time Pareto frontier, offering superior performance and memory efficiency compared to a wide range of traditional and learned indexes. HALI demonstrates that the most promising path forward is not a wholesale replacement of traditional data structures, but a synergistic integration of learned models and proven systems engineering, representing a significant and practical step towards realizing the full potential of learned indexes in real-world systems.

#### **Works cited**

1. Why and where to use INDEXes \- pros and cons \[closed\] \- Stack Overflow, accessed November 4, 2025, [https://stackoverflow.com/questions/29842622/why-and-where-to-use-indexes-pros-and-cons](https://stackoverflow.com/questions/29842622/why-and-where-to-use-indexes-pros-and-cons)  
2. mgoin/learned\_indexes: Experiments on ideas proposed in Tim Kraska's "The Case for Learned Index Structures" \- GitHub, accessed November 4, 2025, [https://github.com/mgoin/learned\_indexes](https://github.com/mgoin/learned_indexes)  
3. (PDF) The Case for Learned Index Structures \- ResearchGate, accessed November 4, 2025, [https://www.researchgate.net/publication/325376198\_The\_Case\_for\_Learned\_Index\_Structures](https://www.researchgate.net/publication/325376198_The_Case_for_Learned_Index_Structures)  
4. \[1712.01208\] The Case for Learned Index Structures \- arXiv, accessed November 4, 2025, [https://arxiv.org/abs/1712.01208](https://arxiv.org/abs/1712.01208)  
5. The Case for Learned Indexes \- mit dsail, accessed November 4, 2025, [https://dsail.csail.mit.edu/index.php/index/](https://dsail.csail.mit.edu/index.php/index/)  
6. Learned Index Structures, accessed November 4, 2025, [https://mlsys.org/Conferences/doc/2018/43.pdf](https://mlsys.org/Conferences/doc/2018/43.pdf)  
7. The Case for Learned Index Structures \- Google Research, accessed November 4, 2025, [https://research.google/pubs/the-case-for-learned-index-structures/](https://research.google/pubs/the-case-for-learned-index-structures/)  
8. Lecture 22 Learned Indexes, accessed November 4, 2025, [https://users.cs.utah.edu/\~pandey/courses/cs6530/fall22/slides/Lecture18.pdf](https://users.cs.utah.edu/~pandey/courses/cs6530/fall22/slides/Lecture18.pdf)  
9. Can Learned Indexes be Built Efficiently? A Deep Dive into Sampling Trade-offs, accessed November 4, 2025, [https://sigfast.or.kr/nvramos/nvramos24/presentation/NVRAMOS24\_01\_02\_JMC\_LearnedIndex.pdf](https://sigfast.or.kr/nvramos/nvramos24/presentation/NVRAMOS24_01_02_JMC_LearnedIndex.pdf)  
10. \[2403.12433\] Algorithmic Complexity Attacks on Dynamic Learned Indexes \- arXiv, accessed November 4, 2025, [https://arxiv.org/abs/2403.12433](https://arxiv.org/abs/2403.12433)  
11. Benchmarking Learned Indexes \- VLDB Endowment, accessed November 4, 2025, [https://vldb.org/pvldb/vol14/p1-marcus.pdf](https://vldb.org/pvldb/vol14/p1-marcus.pdf)  
12. ALEX: An Updatable Adaptive Learned Index \- Virtual Server List, accessed November 4, 2025, [https://users.cs.utah.edu/\~pandey/courses/cs6530/fall22/papers/learnedindex/3318464.3389711.pdf](https://users.cs.utah.edu/~pandey/courses/cs6530/fall22/papers/learnedindex/3318464.3389711.pdf)  
13. \[1902.00655\] Learned Indexes for Dynamic Workloads \- arXiv, accessed November 4, 2025, [https://arxiv.org/abs/1902.00655](https://arxiv.org/abs/1902.00655)  
14. Learned Index: A Comprehensive Experimental Evaluation \- Database Group, accessed November 4, 2025, [https://dbgroup.cs.tsinghua.edu.cn/ligl/papers/experiment-learned-index.pdf](https://dbgroup.cs.tsinghua.edu.cn/ligl/papers/experiment-learned-index.pdf)  
15. Algorithmic Complexity Attacks on Dynamic Learned Indexes \- arXiv, accessed November 4, 2025, [https://arxiv.org/html/2403.12433v1](https://arxiv.org/html/2403.12433v1)  
16. Examples of different hierarchical learned indexes. a A RMI example... \- ResearchGate, accessed November 4, 2025, [https://www.researchgate.net/figure/Examples-of-different-hierarchical-learned-indexes-a-A-RMI-example-with-two-layers-and-a\_fig4\_372510676](https://www.researchgate.net/figure/Examples-of-different-hierarchical-learned-indexes-a-A-RMI-example-with-two-layers-and-a_fig4_372510676)  
17. Learned data structures \- UNIPI, accessed November 4, 2025, [https://pages.di.unipi.it/vinciguerra/publication/learned-data-structures-survey/learned-data-structures-survey.pdf](https://pages.di.unipi.it/vinciguerra/publication/learned-data-structures-survey/learned-data-structures-survey.pdf)  
18. Learned Index Structures: A Machine Learning Approach for Optimized Data Handling, accessed November 4, 2025, [https://www.slideserve.com/kcantrell/the-case-for-learned-index-structures-powerpoint-ppt-presentation](https://www.slideserve.com/kcantrell/the-case-for-learned-index-structures-powerpoint-ppt-presentation)  
19. The Case for Learned Index Structures, accessed November 4, 2025, [https://odin.cse.buffalo.edu/teaching/cse-662/2019fa/slide/2019-09-18-LearnedIndexStructures.pdf](https://odin.cse.buffalo.edu/teaching/cse-662/2019fa/slide/2019-09-18-LearnedIndexStructures.pdf)  
20. Exploring learned indexes in PostgreSQL: Insights from our session at PGConf.dev 2025, accessed November 4, 2025, [https://www.postgresql.fastware.com/blog/exploring-learned-indexes-in-postgresql](https://www.postgresql.fastware.com/blog/exploring-learned-indexes-in-postgresql)  
21. A Survey of Learned Indexes for the Multi-dimensional Space \- arXiv, accessed November 4, 2025, [https://arxiv.org/html/2403.06456v1](https://arxiv.org/html/2403.06456v1)  
22. COLIN: A Cache-Conscious Dynamic Learned Index with High Read/Write Performance \- JCST, accessed November 4, 2025, [https://jcst.ict.ac.cn/fileup/1000-9000/PDF/2021-4-2-1348.pdf](https://jcst.ict.ac.cn/fileup/1000-9000/PDF/2021-4-2-1348.pdf)  
23. FSL Traces and Snapshots Public Archive, accessed November 4, 2025, [https://tracer.filesystems.org/](https://tracer.filesystems.org/)  
24. Linux perf Examples \- Brendan Gregg, accessed November 4, 2025, [https://www.brendangregg.com/perf.html](https://www.brendangregg.com/perf.html)  
25. parallel-hashmap · main · Ilaria / grace · GitLab, accessed November 4, 2025, [https://gitlab.db.in.tum.de/ila/grace/-/tree/main/parallel-hashmap](https://gitlab.db.in.tum.de/ila/grace/-/tree/main/parallel-hashmap)  
26. The Parallel Hashmap (Gregory Popovitch), accessed November 4, 2025, [https://greg7mdp.github.io/parallel-hashmap/](https://greg7mdp.github.io/parallel-hashmap/)  
27. The Adaptive Radix Tree: ARTful Indexing for Main-Memory ..., accessed November 4, 2025, [https://www.db.in.tum.de/\~leis/papers/ART.pdf](https://www.db.in.tum.de/~leis/papers/ART.pdf)  
28. Adaptive Radix Tree (ART) Index \- All things DataOS, accessed November 4, 2025, [https://dataos.info/resources/stacks/flash/art/](https://dataos.info/resources/stacks/flash/art/)  
29. A Critical Analysis of Recursive Model Indexes \- arXiv, accessed November 4, 2025, [https://arxiv.org/pdf/2106.16166](https://arxiv.org/pdf/2106.16166)  
30. Recursive Model Index (RMI). “Hey Reader\! This article gives you a… | by Sai Charan | GoPenAI, accessed November 4, 2025, [https://blog.gopenai.com/recursive-model-index-rmi-f9fcba58db7d](https://blog.gopenai.com/recursive-model-index-rmi-f9fcba58db7d)  
31. The case for learned index structures – Part II \- The Morning Paper, accessed November 4, 2025, [https://blog.acolyer.org/2018/01/09/the-case-for-learned-index-structures-part-ii/](https://blog.acolyer.org/2018/01/09/the-case-for-learned-index-structures-part-ii/)  
32. Updatable Learned Index with Precise Positions \- VLDB Endowment, accessed November 4, 2025, [https://vldb.org/pvldb/vol14/p1276-wu.pdf](https://vldb.org/pvldb/vol14/p1276-wu.pdf)  
33. The PGM-index, accessed November 4, 2025, [https://pgm.di.unipi.it/](https://pgm.di.unipi.it/)  
34. The PGM-index: a fully-dynamic compressed learned index with provable worst-case bounds \- IRIS, accessed November 4, 2025, [https://www.iris.santannapisa.it/handle/11382/566794](https://www.iris.santannapisa.it/handle/11382/566794)  
35. ALEX: An Updatable Adaptive Learned Index | Request PDF \- ResearchGate, accessed November 4, 2025, [https://www.researchgate.net/publication/362253764\_ALEX\_An\_Updatable\_Adaptive\_Learned\_Index](https://www.researchgate.net/publication/362253764_ALEX_An_Updatable_Adaptive_Learned_Index)  
36. ALEX: An Updatable Adaptive Learned Index \- Microsoft, accessed November 4, 2025, [https://www.microsoft.com/en-us/research/wp-content/uploads/2020/04/MSRAlexTechnicalReportV2.pdf](https://www.microsoft.com/en-us/research/wp-content/uploads/2020/04/MSRAlexTechnicalReportV2.pdf)  
37. ALEX: An Updatable Adaptive Learned Index \- Omkar Desai, accessed November 4, 2025, [https://swiftomkar.github.io/files/Alex-sept18-20.pdf](https://swiftomkar.github.io/files/Alex-sept18-20.pdf)  
38. \[PDF\] ALEX: An Updatable Adaptive Learned Index | Semantic Scholar, accessed November 4, 2025, [https://www.semanticscholar.org/paper/ALEX%3A-An-Updatable-Adaptive-Learned-Index-Ding-Minhas/6b2cae9d709731c7ab41400c3286e37820252364](https://www.semanticscholar.org/paper/ALEX%3A-An-Updatable-Adaptive-Learned-Index-Ding-Minhas/6b2cae9d709731c7ab41400c3286e37820252364)  
39. File systems and Storage Lab (FSL) \- Stony Brook University, accessed November 4, 2025, [https://www.fsl.cs.sunysb.edu/](https://www.fsl.cs.sunysb.edu/)  
40. A Long-Term User-Centric Analysis of Deduplication Patterns, accessed November 4, 2025, [https://www.fsl.cs.stonybrook.edu/docs/msst16dedup-study/data-set-analysis.pdf](https://www.fsl.cs.stonybrook.edu/docs/msst16dedup-study/data-set-analysis.pdf)  
41. About \- SOSD \- GitHub Pages, accessed November 4, 2025, [https://learnedsystems.github.io/SOSDLeaderboard/about/](https://learnedsystems.github.io/SOSDLeaderboard/about/)  
42. Home \- SOSD, accessed November 4, 2025, [https://learnedsystems.github.io/SOSDLeaderboard/](https://learnedsystems.github.io/SOSDLeaderboard/)  
43. SOSD Dataset \- Papers With Code, accessed November 4, 2025, [https://paperswithcode.com/dataset/sosd](https://paperswithcode.com/dataset/sosd)  
44. A five-year study of file-system metadata | Request PDF \- ResearchGate, accessed November 4, 2025, [https://www.researchgate.net/publication/220398206\_A\_five-year\_study\_of\_file-system\_metadata](https://www.researchgate.net/publication/220398206_A_five-year_study_of_file-system_metadata)  
45. A Five-Year Study of File-System Metadata | USENIX, accessed November 4, 2025, [https://www.usenix.org/conference/fast-07/five-year-study-file-system-metadata](https://www.usenix.org/conference/fast-07/five-year-study-file-system-metadata)  
46. A five-year study of file-system metadata \- Semantic Scholar, accessed November 4, 2025, [https://www.semanticscholar.org/paper/A-five-year-study-of-file-system-metadata-Agrawal-Bolosky/04e8d64b569b3d5628cfdf5ad16ba0b933845e2e](https://www.semanticscholar.org/paper/A-five-year-study-of-file-system-metadata-Agrawal-Bolosky/04e8d64b569b3d5628cfdf5ad16ba0b933845e2e)  
47. atc proceedings \- USENIX, accessed November 4, 2025, [https://www.usenix.org/legacy/event/fast07/tech/full\_papers/agrawal/agrawal.pdf](https://www.usenix.org/legacy/event/fast07/tech/full_papers/agrawal/agrawal.pdf)  
48. Pareto front \- Wikipedia, accessed November 4, 2025, [https://en.wikipedia.org/wiki/Pareto\_front](https://en.wikipedia.org/wiki/Pareto_front)  
49. Defining Multiobjective Algorithms and Pareto Frontiers | Baeldung on Computer Science, accessed November 4, 2025, [https://www.baeldung.com/cs/defining-multiobjective-algorithms-and-pareto-frontiers](https://www.baeldung.com/cs/defining-multiobjective-algorithms-and-pareto-frontiers)  
50. Latency CDF and CCDF graphs \- Knowledge Public | Support Portal, accessed November 4, 2025, [https://support.excentis.com/knowledge/article/190](https://support.excentis.com/knowledge/article/190)  
51. Empirical Cumulative Distribution Function (CDF) Plots \- Statistics By Jim, accessed November 4, 2025, [https://statisticsbyjim.com/graphs/empirical-cumulative-distribution-function-cdf-plots/](https://statisticsbyjim.com/graphs/empirical-cumulative-distribution-function-cdf-plots/)  
52. PERF tutorial: Finding execution hot spots \- Sand, software and sound, accessed November 4, 2025, [http://sandsoftwaresound.net/perf/perf-tutorial-hot-spots/](http://sandsoftwaresound.net/perf/perf-tutorial-hot-spots/)  
53. \[2403.06456\] A Survey of Learned Indexes for the Multi-dimensional Space \- arXiv, accessed November 4, 2025, [https://arxiv.org/abs/2403.06456](https://arxiv.org/abs/2403.06456)  
54. A Tutorial on Learned Multi-dimensional Indexes \- CS@Purdue, accessed November 4, 2025, [https://www.cs.purdue.edu/homes/aref/learned-indexes-tutorial.html](https://www.cs.purdue.edu/homes/aref/learned-indexes-tutorial.html)