#pragma once

#include <unordered_map>
#include <string>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <thread>
#include <atomic>

class rate_limiter {
    private:
        struct entry {
            double tokens;
            std::chrono::steady_clock::time_point last_refill;
        };

        double max_tokens;
        double refill_rate;

        std::unordered_map<std::string, entry> requests;
        std::mutex mutex;

        std::thread cleanup_thread;
        std::atomic<bool> stop{false};

        void cleanup_loop() {
            while (!stop.load()) {
                std::this_thread::sleep_for(std::chrono::minutes(1));
                cleanup();
            }
        }

    public:
        rate_limiter(double max_tokens, double refill_rate) : max_tokens(max_tokens), refill_rate(refill_rate) {
            if (max_tokens <= 0) {
                throw std::invalid_argument("max_tokens of rate_limiter must be positive");
            }

            if (refill_rate <= 0) {
                throw std::invalid_argument("refill_rate of rate_limiter must be positive");
            }

            cleanup_thread = std::thread(
                [this] { cleanup_loop(); }
            );
        }

        ~rate_limiter() {
            stop = true;

            if (cleanup_thread.joinable()) {
                cleanup_thread.join();
            }
        }

        rate_limiter(const rate_limiter&) = delete;

        rate_limiter& operator=(const rate_limiter&) = delete;

        bool consume(const std::string& identifier, double cost = 1.0) {
            if (cost <= 0 || cost > max_tokens) {
                return false;
            }
            std::lock_guard<std::mutex> lock(mutex);

            auto now = std::chrono::steady_clock::now();

            auto it = requests.find(identifier);

            if (it == requests.end()) {
                requests[identifier] = {
                    max_tokens - cost,
                    now
                };

                return true;
            }

            auto& bucket = it->second;

            double elapsed =
                std::chrono::duration<double>(
                    now - bucket.last_refill
                ).count();

            bucket.tokens = std::min(
                max_tokens,
                bucket.tokens + elapsed * refill_rate
            );

            bucket.last_refill = now;

            if (bucket.tokens < cost) {
                return false;
            }

            bucket.tokens -= cost;
            return true;
        }

        void cleanup() {
            std::lock_guard<std::mutex> lock(mutex);

            auto now = std::chrono::steady_clock::now();

            auto expiry =
                std::chrono::duration<double>(
                    max_tokens / refill_rate
                ).count() * 2;

            for (auto it = requests.begin(); it != requests.end();) {
                double idle =
                    std::chrono::duration<double>(
                        now - it->second.last_refill
                    ).count();

                if (idle > expiry) {
                    it = requests.erase(it);
                } else {
                    ++it;
                }
            }
        }
};