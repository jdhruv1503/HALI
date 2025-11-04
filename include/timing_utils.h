#pragma once

#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>

namespace hali {

/**
 * @brief High-resolution timer for nanosecond-precision measurements
 */
class Timer {
private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint start_time;

public:
    Timer() : start_time(Clock::now()) {}

    /**
     * @brief Reset the timer
     */
    void reset() {
        start_time = Clock::now();
    }

    /**
     * @brief Get elapsed time in nanoseconds
     * @return Elapsed nanoseconds since timer start/reset
     */
    uint64_t elapsed_ns() const {
        auto end_time = Clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time
        ).count();
    }

    /**
     * @brief Get elapsed time in microseconds
     * @return Elapsed microseconds since timer start/reset
     */
    double elapsed_us() const {
        return elapsed_ns() / 1000.0;
    }

    /**
     * @brief Get elapsed time in milliseconds
     * @return Elapsed milliseconds since timer start/reset
     */
    double elapsed_ms() const {
        return elapsed_ns() / 1000000.0;
    }

    /**
     * @brief Get elapsed time in seconds
     * @return Elapsed seconds since timer start/reset
     */
    double elapsed_s() const {
        return elapsed_ns() / 1000000000.0;
    }
};

/**
 * @brief Statistics collection for latency measurements
 */
class LatencyStats {
private:
    std::vector<uint64_t> latencies_ns;
    bool is_sorted = false;

public:
    /**
     * @brief Add a latency measurement
     * @param latency_ns Latency in nanoseconds
     */
    void add(uint64_t latency_ns) {
        latencies_ns.push_back(latency_ns);
        is_sorted = false;
    }

    /**
     * @brief Get the mean latency
     * @return Mean latency in nanoseconds
     */
    double mean() const {
        if (latencies_ns.empty()) return 0.0;
        uint64_t sum = 0;
        for (auto lat : latencies_ns) {
            sum += lat;
        }
        return static_cast<double>(sum) / latencies_ns.size();
    }

    /**
     * @brief Get the median latency
     * @return Median latency in nanoseconds
     */
    double median() {
        if (latencies_ns.empty()) return 0.0;
        ensure_sorted();
        size_t n = latencies_ns.size();
        if (n % 2 == 0) {
            return (latencies_ns[n/2 - 1] + latencies_ns[n/2]) / 2.0;
        } else {
            return latencies_ns[n/2];
        }
    }

    /**
     * @brief Get a specific percentile
     * @param p Percentile (0-100)
     * @return Latency at the given percentile in nanoseconds
     */
    double percentile(double p) {
        if (latencies_ns.empty()) return 0.0;
        ensure_sorted();
        size_t index = static_cast<size_t>(
            (p / 100.0) * (latencies_ns.size() - 1)
        );
        return latencies_ns[index];
    }

    /**
     * @brief Get P95 latency
     * @return 95th percentile latency in nanoseconds
     */
    double p95() {
        return percentile(95.0);
    }

    /**
     * @brief Get P99 latency
     * @return 99th percentile latency in nanoseconds
     */
    double p99() {
        return percentile(99.0);
    }

    /**
     * @brief Get minimum latency
     * @return Minimum latency in nanoseconds
     */
    uint64_t min() {
        if (latencies_ns.empty()) return 0;
        ensure_sorted();
        return latencies_ns.front();
    }

    /**
     * @brief Get maximum latency
     * @return Maximum latency in nanoseconds
     */
    uint64_t max() {
        if (latencies_ns.empty()) return 0;
        ensure_sorted();
        return latencies_ns.back();
    }

    /**
     * @brief Get standard deviation
     * @return Standard deviation of latencies in nanoseconds
     */
    double stddev() const {
        if (latencies_ns.size() < 2) return 0.0;
        double m = mean();
        double sum_sq_diff = 0.0;
        for (auto lat : latencies_ns) {
            double diff = lat - m;
            sum_sq_diff += diff * diff;
        }
        return std::sqrt(sum_sq_diff / latencies_ns.size());
    }

    /**
     * @brief Get number of samples
     * @return Number of latency measurements
     */
    size_t count() const {
        return latencies_ns.size();
    }

    /**
     * @brief Clear all measurements
     */
    void clear() {
        latencies_ns.clear();
        is_sorted = false;
    }

    /**
     * @brief Get all raw latencies
     * @return Vector of all latency measurements
     */
    const std::vector<uint64_t>& raw_data() const {
        return latencies_ns;
    }

private:
    void ensure_sorted() {
        if (!is_sorted) {
            std::sort(latencies_ns.begin(), latencies_ns.end());
            is_sorted = true;
        }
    }
};

} // namespace hali
