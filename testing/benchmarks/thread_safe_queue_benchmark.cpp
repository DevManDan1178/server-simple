#include "data_structures/thread_safe/thread_safe_queue.hpp"
#include "benchmark_common.hpp"

#include <atomic>
#include <barrier>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>


namespace {
    using value_type = uint64_t;
    using clock_type = std::chrono::steady_clock;

    constexpr int DEFAULT_PRODUCERS = 4;
    constexpr int DEFAULT_CONSUMERS = 4;
    constexpr uint64_t DEFAULT_OPERATIONS_COUNT = 100'000'000;

    constexpr size_t max_items = 1'000'000;
    constexpr size_t max_bytes = 1'000'000'000;
    

    /*
    * Baseline implementation.
    *
    * This intentionally uses the simplest equivalent design:
    *
    *     std::deque
    *     std::mutex
    *     std::condition_variable
    *
    * It provides the same basic producer/consumer semantics as
    * thread_safe_queue::push_back() + wait_and_pop().
    */
    class mutex_deque {  
        private:
            std::deque<value_type> queue_;
            std::mutex mutex_;
            std::condition_variable condition_;
            bool stopped_ = false;

        public:
            void push_back(value_type value)
            {
                {
                    std::lock_guard<std::mutex> lock(mutex_);

                    if (stopped_) {
                        return;
                    }

                    queue_.push_back(value);
                }

                condition_.notify_one();
            }

            std::optional<value_type> wait_and_pop()
            {
                std::unique_lock<std::mutex> lock(mutex_);

                condition_.wait(lock, [this] {
                    return stopped_ || !queue_.empty();
                });

                if (queue_.empty()) {
                    return std::nullopt;
                }

                value_type value = queue_.front();
                queue_.pop_front();

                return value;
            }

            void stop()
            {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stopped_ = true;
                }

                condition_.notify_all();
            }
    };

    /*
    * Runs the same producer/consumer workload against either:
    *     thread_safe_queue<value_type> or mutex_deque (uses value_type)
    *
    * The benchmark measures only the time spent processing the workload.
    * Thread creation and startup are excluded.
    */
    template <typename Queue>
    benchmark_result run_benchmark(Queue& queue, int producer_count, int consumer_count, uint64_t total_operations) {
        std::atomic<uint64_t> consumed{0};
        std::atomic<uint64_t> checksum{0};

        /*
        * Each producer receives a unique range of values.
        *
        * This lets us verify after the benchmark that every item was
        * consumed exactly once.
        */
        const uint64_t expected_checksum = total_operations * (total_operations + 1) / 2;

        /*
        * +1 for the main thread.
        *
        * Every producer and consumer waits here before the benchmark
        * begins, ensuring they are all ready before the timer starts.
        */
        std::barrier start_barrier(producer_count + consumer_count + 1);

        std::vector<std::thread> producers;
        std::vector<std::thread> consumers;

        producers.reserve(producer_count);
        consumers.reserve(consumer_count);

        /*
        * Distribute the work evenly between producers.
        */
        const uint64_t operations_per_producer = total_operations / static_cast<uint64_t>(producer_count);

        const uint64_t remainder = total_operations % static_cast<uint64_t>(producer_count);

        uint64_t next_value = 1;

        for (int producer_id = 0; producer_id < producer_count; ++producer_id) {
            const uint64_t operations = operations_per_producer + (static_cast<uint64_t>(producer_id) < remainder ? 1 : 0);

            const uint64_t first_value = next_value;
            next_value += operations;

            producers.emplace_back([&, first_value, operations] {
                start_barrier.arrive_and_wait();

                for (uint64_t i = 0; i < operations; ++i) {
                    queue.push_back(first_value + i);
                }
            });
        }

        /*
        * Consumers continuously remove items until the expected number
        * of operations has been consumed.
        */
        for (int consumer_id = 0; consumer_id < consumer_count; ++consumer_id) {
            consumers.emplace_back([&] {
                start_barrier.arrive_and_wait();

                while (true) {
                    const auto value = queue.wait_and_pop();

                    /*
                    * A stopped queue with no remaining items wakes
                    * consumers and returns nullopt.
                    */
                    if (!value.has_value()) {
                        return;
                    }

                    checksum.fetch_add(*value, std::memory_order_relaxed);

                    const uint64_t count =
                        consumed.fetch_add(1, std::memory_order_relaxed) + 1;

                    /*
                    * The final item has been consumed.
                    */
                    if (count == total_operations) {
                        return;
                    }
                }
            });
        }

        /*
        * Wait until all worker threads are ready.
        *
        * No thread creation time is included in the benchmark.
        */
        start_barrier.arrive_and_wait();

        const auto start = clock_type::now();

        /*
        * Wait for every producer to finish.
        */
        for (auto& producer : producers) {
            producer.join();
        }

        /*
        * Wait until every item has been consumed.
        *
        * Producers are already finished at this point, so the number of
        * expected operations cannot increase.
        */
        while (consumed.load(std::memory_order_acquire) < total_operations) {
            std::this_thread::yield();
        }

        const auto end = clock_type::now();

        /*
        * Wake any consumers that are still waiting.
        */
        queue.stop();

        for (auto& consumer : consumers) {
            consumer.join();
        }

        const double elapsed_seconds =  std::chrono::duration<double>(end - start).count();
        const uint64_t final_consumed = consumed.load(std::memory_order_acquire);
        const uint64_t final_checksum = checksum.load(std::memory_order_acquire);

        /*
        * Correctness checks.
        *
        * A benchmark should still verify that it actually performed the
        * workload correctly.
        */
        if (final_consumed != total_operations) {
            std::cerr
                << "ERROR: expected "
                << total_operations
                << " consumed operations, but got "
                << final_consumed
                << '\n';

            std::exit(EXIT_FAILURE);
        }

        if (final_checksum != expected_checksum) {
            std::cerr
                << "ERROR: checksum mismatch\n"
                << "Expected: "
                << expected_checksum
                << '\n'
                << "Actual:   "
                << final_checksum
                << '\n';

            std::exit(EXIT_FAILURE);
        }

        benchmark_result result;
        result.completed = final_consumed;
        result.failed = 0;
        result.elapsed_seconds = elapsed_seconds;

        return result;
    }

    void print_comparison(
        const benchmark_result& baseline,
        const benchmark_result& no_limits,
        const benchmark_result& max_bytes_only,
        const benchmark_result& max_size_only,
        const benchmark_result& max_size_and_bytes
    ) {
        const double baseline_throughput = baseline.throughput();

        std::cout << std::fixed << std::setprecision(2);

        std::cout << "\nResults\n" << "=======\n";

        std::cout
            << std::left
            << std::setw(32)
            << "Implementation"
            << std::right
            << std::setw(18)
            << "Operations/sec"
            << std::setw(14)
            << "Relative"
            << '\n';

        std::cout << std::string(64, '-') << '\n';

        auto print_result = [&](const char* name, const benchmark_result& result) {
            const double throughput = result.throughput();
            const double relative =baseline_throughput > 0.0
                ? throughput / baseline_throughput
                : 0.0;

            std::cout
                << std::left
                << std::setw(32)
                << name
                << std::right
                << std::setw(18)
                << throughput
                << std::setw(13)
                << relative
                << "x\n";
        };

        print_result("std::deque + mutex", baseline);
        print_result("thread_safe_queue<false,false>", no_limits);
        print_result("thread_safe_queue<false,true>", max_bytes_only);
        print_result("thread_safe_queue<true,false>", max_size_only);
        print_result("thread_safe_queue<true,true>", max_size_and_bytes);
    }

    void print_usage(const char* program) {
        std::cout
            << "Usage: "
            << program
            << " [producers] [consumers] [operations]\n\n"
            << "Defaults:\n"
            << "  producers:  "
            << DEFAULT_PRODUCERS
            << '\n'
            << "  consumers:  "
            << DEFAULT_CONSUMERS
            << '\n'
            << "  operations: "
            << DEFAULT_OPERATIONS_COUNT
            << "\n\n"
            << "Example:\n"
            << "  "
            << program
            << " 4 4 10000000\n";
    }
} // namespace end


int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    int producer_count = DEFAULT_PRODUCERS;
    int consumer_count = DEFAULT_CONSUMERS;
    uint64_t total_operations = DEFAULT_OPERATIONS_COUNT;

    try {
        if (argc > 1) {
            producer_count = std::stoi(argv[1]);
        }

        if (argc > 2) {
            consumer_count = std::stoi(argv[2]);
        }

        if (argc > 3) {
            total_operations = std::stoull(argv[3]);
        }
    } catch (const std::exception& e) {
        std::cerr
            << "Invalid argument: "
            << e.what()
            << "\n\n";

        print_usage(argv[0]);
        return 1;
    }

    if (producer_count <= 0) {
        std::cerr << "Producer count must be greater than zero.\n";
        return 1;
    }

    if (consumer_count <= 0) {
        std::cerr << "Consumer count must be greater than zero.\n";
        return 1;
    }

    if (total_operations == 0) {
        std::cerr << "Operation count must be greater than zero.\n";
        return 1;
    }

    std::cout
        << "Thread-Safe Queue Benchmark\n"
        << "===========================\n"
        << "Producers:  "
        << producer_count
        << '\n'
        << "Consumers:  "
        << consumer_count
        << '\n'
        << "Operations: "
        << total_operations
        << "\n\n";

    std::cout << "Running std::deque + mutex...\n";
    mutex_deque mutex_dq;
    thread_safe_queue<value_type, false, false> no_limits_dq(max_items, max_bytes);
    thread_safe_queue<value_type, false, true> max_bytes_only_dq(max_items, max_bytes);
    thread_safe_queue<value_type, true, false> max_size_only_dq(max_items, max_bytes);
    thread_safe_queue<value_type, true, true> max_size_and_bytes_dq(max_items, max_bytes);
    const benchmark_result baseline = run_benchmark(mutex_dq, producer_count, consumer_count, total_operations);
    
    std::cout << "Running thread_safe_queue<false, false>...\n";
    
    const benchmark_result no_limits = run_benchmark(no_limits_dq, producer_count, consumer_count, total_operations);
    
    std::cout << "Running thread_safe_queue<false, true>...\n";
    
    const benchmark_result max_bytes_only = run_benchmark(max_bytes_only_dq, producer_count, consumer_count, total_operations);
    
    std::cout << "Running thread_safe_queue<true, false>...\n";
    
    const benchmark_result max_size_only = run_benchmark(max_size_only_dq, producer_count, consumer_count, total_operations);

    std::cout << "Running thread_safe_queue<true, true>...\n";

    const benchmark_result max_size_and_bytes = run_benchmark(max_size_and_bytes_dq, producer_count, consumer_count, total_operations);

    print_comparison(baseline, no_limits, max_bytes_only, max_size_only, max_size_and_bytes);

    return 0;
}