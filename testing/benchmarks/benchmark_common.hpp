#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <span>
#include <string>
#include <vector>

struct latency_stats {
    std::vector<uint64_t> samples;

    void reserve(std::size_t count) {
        samples.reserve(count);
    }

    void add(uint64_t nanoseconds) {
        samples.push_back(nanoseconds);
    }

    void merge(latency_stats&& other) {
        samples.insert(samples.end(), std::make_move_iterator(other.samples.begin()), std::make_move_iterator(other.samples.end()));
    }

    void sort() {
        std::sort(samples.begin(), samples.end());
    }

    [[nodiscard]] uint64_t percentile(double p) const {
        if (samples.empty()) {
            return 0;
        }

        const double index = p * static_cast<double>(samples.size() - 1);
        return samples[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] 
    uint64_t minimum() const {
        return samples.empty() ? 0 : samples.front();
    }

    [[nodiscard]] 
    uint64_t maximum() const {
        return samples.empty() ? 0 : samples.back();
    }

    [[nodiscard]] 
    double average() const {
        if (samples.empty()) {
            return 0.0;
        }

        const uint64_t total = std::accumulate(samples.begin(), samples.end(), uint64_t{0});
        return static_cast<double>(total) / static_cast<double>(samples.size());
    }
};

struct benchmark_result {
    uint64_t completed = 0;
    uint64_t failed = 0;
    double elapsed_seconds = 0.0;

    latency_stats latency;

    [[nodiscard]] 
    double throughput() const {
        if (elapsed_seconds <= 0.0) {
            return 0.0;
        }

        return static_cast<double>(completed) / elapsed_seconds;
    }
};

inline double ns_to_us(uint64_t ns) {
    return static_cast<double>(ns) / 1'000.0;
}

inline void print_latency(const latency_stats& stats) {
    if (stats.samples.empty()) {
        std::cout << "Latency:       no samples\n";
        return;
    }

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "Latency\n"
        << "-------\n"
        << "min:           " << ns_to_us(stats.minimum()) << " us\n"
        << "avg:           " << ns_to_us(static_cast<uint64_t>(stats.average())) << " us\n"
        << "50%+:           " << ns_to_us(stats.percentile(0.50)) << " us\n"
        << "95%+:           " << ns_to_us(stats.percentile(0.95)) << " us\n"
        << "99%+:           " << ns_to_us(stats.percentile(0.99)) << " us\n"
        << "99.9%+:         " << ns_to_us(stats.percentile(0.999)) << " us\n"
        << "max:           " << ns_to_us(stats.maximum()) << " us\n";
}

inline void print_result(const benchmark_result& result) {
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "\nResults\n"
        << "=======\n"
        << "Completed:      " << result.completed << '\n'
        << "Failed:         " << result.failed << '\n'
        << "Elapsed:        " << result.elapsed_seconds << " s\n"
        << "Throughput:     " << result.throughput() << " req/s\n\n";

    print_latency(result.latency);
}