#include <gtest/gtest.h>

#include "data_structures/thread_safe/thread_safe_queue.hpp"

#include <memory>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

TEST(ThreadSafeQueue, PushAndPopPreservesOrder) {
    thread_safe_queue<int> queue;

    queue.push_back(1);
    queue.push_back(2);
    queue.push_back(3);

    EXPECT_EQ(queue.size(), 3);
    EXPECT_EQ(queue.front(), 1);
    EXPECT_EQ(queue.back(), 3);

    EXPECT_EQ(queue.pop_front(), 1);
    EXPECT_EQ(queue.pop_front(), 2);
    EXPECT_EQ(queue.pop_front(), 3);

    EXPECT_TRUE(queue.empty());
}

TEST(ThreadSafeQueue, PushFrontChangesOrder) {
    thread_safe_queue<int> queue;

    queue.push_back(2);
    queue.push_front(1);
    queue.push_back(3);

    const auto values = queue.to_vector();

    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

TEST(ThreadSafeQueue, TryPushRespectsMaximumSize) {
    thread_safe_queue<int> queue(2);

    EXPECT_TRUE(queue.try_push_back(1));
    EXPECT_TRUE(queue.try_push_back(2));
    EXPECT_FALSE(queue.try_push_back(3));

    EXPECT_EQ(queue.size(), 2);
}

TEST(ThreadSafeQueue, TryPushRespectsMaximumBytes) {
    struct payload {
        char data[16];
    };

    thread_safe_queue<payload> queue(0, sizeof(payload) * 2);

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_FALSE(queue.try_push_back(payload{}));

    EXPECT_EQ(queue.size(), 2);
}

TEST(ThreadSafeQueue, WaitAndPopBlocksUntilItemArrives) {
    thread_safe_queue<int> queue;

    auto future = std::async(std::launch::async, [&queue] {
        return queue.wait_and_pop();
    });

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);

    queue.push_back(42);

    ASSERT_TRUE(future.wait_for(std::chrono::seconds(1)) == std::future_status::ready);

    const auto result = future.get();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(ThreadSafeQueue, StopUnblocksWaitingConsumer) {
    thread_safe_queue<int> queue;

    auto future = std::async(std::launch::async, [&queue] {
        return queue.wait_and_pop();
    });

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);

    queue.stop();

    ASSERT_TRUE(future.wait_for(std::chrono::seconds(1)) == std::future_status::ready);

    const auto result = future.get();

    EXPECT_FALSE(result.has_value());
}

TEST(ThreadSafeQueue, StoppedQueueRejectsTryPush) {
    thread_safe_queue<int> queue;

    queue.stop();

    EXPECT_FALSE(queue.try_push_back(42));
    EXPECT_TRUE(queue.empty());
}

TEST(ThreadSafeQueue, ClearRemovesAllElements) {
    thread_safe_queue<int> queue;

    queue.push_back(1);
    queue.push_back(2);
    queue.push_back(3);

    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(ThreadSafeQueue, ReplaceReplacesContents) {
    thread_safe_queue<int> queue;

    queue.push_back(1);
    queue.push_back(2);

    queue.replace({10, 20, 30});

    const auto values = queue.to_vector();

    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 30);
}

TEST(ThreadSafeQueue, ConcurrentProducersAndConsumersPreserveWork) {
    thread_safe_queue<int> queue;

    constexpr int producer_count = 4;
    constexpr int consumer_count = 4;
    constexpr int items_per_producer = 5'000;

    std::atomic<int> consumed_count{0};
    std::atomic<long long> checksum{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    producers.reserve(producer_count);
    consumers.reserve(consumer_count);

    for (int producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&queue, producer] {
            for (int i = 0; i < items_per_producer; ++i) {
                queue.push_back(producer * items_per_producer + i);
            }
        });
    }

    for (int consumer = 0; consumer < consumer_count; ++consumer) {
        consumers.emplace_back([&queue, &consumed_count, &checksum] {
            while (true) {
                auto value = queue.wait_and_pop();

                if (!value.has_value()) {
                    return;
                }

                checksum += *value;
                ++consumed_count;
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    queue.stop();

    for (auto& consumer : consumers) {
        consumer.join();
    }

    constexpr int total = producer_count * items_per_producer;

    EXPECT_EQ(consumed_count.load(), total);

    long long expected_checksum = 0;

    for (int i = 0; i < total; ++i) {
        expected_checksum += i;
    }

    EXPECT_EQ(checksum.load(), expected_checksum);
}

TEST(ThreadSafeQueue, EmptyAccessThrows) {
    thread_safe_queue<int> queue;

    EXPECT_THROW(queue.front(), std::runtime_error);
    EXPECT_THROW(queue.back(), std::runtime_error);
    EXPECT_THROW(queue.pop_front(), std::runtime_error);
    EXPECT_THROW(queue.pop_back(), std::runtime_error);
}

TEST(ThreadSafeQueue, PopBackPreservesReverseOrder) {
    thread_safe_queue<int> queue;

    queue.push_back(1);
    queue.push_back(2);
    queue.push_back(3);

    EXPECT_EQ(queue.pop_back(), 3);
    EXPECT_EQ(queue.pop_back(), 2);
    EXPECT_EQ(queue.pop_back(), 1);
    EXPECT_TRUE(queue.empty());
}

TEST(ThreadSafeQueue, TryPushFrontRespectsMaximumSize) {
    thread_safe_queue<int> queue(2);

    EXPECT_TRUE(queue.try_push_front(2));
    EXPECT_TRUE(queue.try_push_front(1));
    EXPECT_FALSE(queue.try_push_front(0));

    EXPECT_EQ(queue.to_vector(), (std::vector<int>{1, 2}));
}

TEST(ThreadSafeQueue, PushAndPopSupportsMoveOnlyTypes) {
    thread_safe_queue<std::unique_ptr<int>> queue;

    queue.push_back(std::make_unique<int>(42));

    auto value = queue.pop_front();

    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 42);
}


TEST(ThreadSafeQueue, PoppingFreesMaximumByteCapacity) {
    struct payload {
        char data[16];
    };

    thread_safe_queue<payload> queue(0, sizeof(payload) * 2);

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_FALSE(queue.try_push_back(payload{}));

    queue.pop_front();

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_EQ(queue.size(), 2);
}


TEST(ThreadSafeQueue, PopBackUpdatesMaximumByteAccounting) {
    struct payload {
        char data[16];
    };

    thread_safe_queue<payload> queue(0, sizeof(payload) * 2);

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.try_push_back(payload{}));

    queue.pop_back();

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_EQ(queue.size(), 2);
}

TEST(ThreadSafeQueue, ClearResetsMaximumByteAccounting) {
    struct payload {
        char data[16];
    };

    thread_safe_queue<payload> queue(0, sizeof(payload) * 2);

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_FALSE(queue.try_push_back(payload{}));

    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.try_push_back(payload{}));
}

TEST(ThreadSafeQueue, ReplaceResetsMaximumByteAccounting) {
    struct payload {
        char data[16];
    };

    thread_safe_queue<payload> queue(0, sizeof(payload) * 3);

    queue.push_back(payload{});
    queue.push_back(payload{});

    queue.replace({payload{}});

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_FALSE(queue.try_push_back(payload{}));
}


TEST(ThreadSafeQueue, StopIsIdempotent) {
    thread_safe_queue<int> queue;

    EXPECT_NO_THROW(queue.stop());
    EXPECT_NO_THROW(queue.stop());
    EXPECT_NO_THROW(queue.stop());

    EXPECT_FALSE(queue.try_push_back(1));
}

TEST(ThreadSafeQueue, ReplaceRespectsMaximumSize) {
    thread_safe_queue<int, true, false> queue(2);

    queue.replace({1, 2, 3});

    EXPECT_LE(queue.size(), 2);
}

TEST(ThreadSafeQueue, StopAllowsExistingItemsToBeConsumed) {
    thread_safe_queue<int> queue;

    queue.push_back(1);
    queue.push_back(2);

    queue.stop();

    EXPECT_EQ(queue.wait_and_pop(), 1);
    EXPECT_EQ(queue.wait_and_pop(), 2);
    EXPECT_FALSE(queue.wait_and_pop().has_value());
}


TEST(ThreadSafeQueue, WaitAndPopWakesAfterPushFront) {
    thread_safe_queue<int> queue;

    auto future = std::async(std::launch::async, [&queue] {
        return queue.wait_and_pop();
    });

    EXPECT_EQ(
        future.wait_for(std::chrono::milliseconds(50)),
        std::future_status::timeout
    );

    queue.push_front(42);

    ASSERT_EQ(
        future.wait_for(std::chrono::seconds(1)),
        std::future_status::ready
    );

    auto result = future.get();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}


TEST(ThreadSafeQueue, StopUnblocksAllWaitingConsumers) {
    thread_safe_queue<int> queue;

    constexpr int consumer_count = 8;

    std::vector<std::future<std::optional<int>>> futures;
    futures.reserve(consumer_count);

    for (int i = 0; i < consumer_count; ++i) {
        futures.emplace_back(
            std::async(std::launch::async, [&queue] {
                return queue.wait_and_pop();
            })
        );
    }

    for (auto& future : futures) {
        EXPECT_EQ(
            future.wait_for(std::chrono::milliseconds(50)),
            std::future_status::timeout
        );
    }

    queue.stop();

    for (auto& future : futures) {
        ASSERT_EQ(
            future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready
        );

        EXPECT_FALSE(future.get().has_value());
    }
}


TEST(ThreadSafeQueue, StoppedQueueRemainsStoppedAfterDraining) {
    thread_safe_queue<int> queue;

    queue.push_back(42);
    queue.stop();

    EXPECT_EQ(queue.wait_and_pop(), 42);
    EXPECT_FALSE(queue.wait_and_pop().has_value());

    EXPECT_FALSE(queue.try_push_back(43));
}

struct variable_size {
    std::vector<char> data;
};

template<>
struct queue_size_traits<variable_size> {
    static size_t get(const variable_size& value) noexcept {
        return value.data.size();
    }
};
TEST(ThreadSafeQueue, CustomSizeTraitsAreUsed) {
    thread_safe_queue<variable_size> queue(0, 10);

    EXPECT_TRUE(queue.try_push_back(variable_size{{std::vector<char>(6)}}));
    EXPECT_FALSE(queue.try_push_back(variable_size{{std::vector<char>(5)}}));

    queue.pop_front();

    EXPECT_TRUE(queue.try_push_back(variable_size{{std::vector<char>(10)}}));
}


TEST(ThreadSafeQueue, ByteCapacityAcceptsExactLimit) {
    struct payload {
        char data[16];
    };

    thread_safe_queue<payload> queue(0, sizeof(payload));

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_FALSE(queue.try_push_back(payload{}));
}


TEST(ThreadSafeQueue, ByteCapacityRejectsOversizedItem) {
    struct payload {
        char data[16];
    };

    thread_safe_queue<payload> queue(0, 8);

    EXPECT_FALSE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.empty());
}


TEST(ThreadSafeQueue, MaximumSizeCanBeDisabled) {
    thread_safe_queue<int, false, true> queue(2);

    EXPECT_TRUE(queue.try_push_back(1));
    EXPECT_TRUE(queue.try_push_back(2));
    EXPECT_TRUE(queue.try_push_back(3));

    EXPECT_EQ(queue.size(), 3);
}

TEST(ThreadSafeQueue, MaximumBytesCanBeDisabled) {
    struct payload {
        char data[16];
    };

    thread_safe_queue<payload, true, false> queue(0, sizeof(payload));

    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.try_push_back(payload{}));
    EXPECT_TRUE(queue.try_push_back(payload{}));

    EXPECT_EQ(queue.size(), 3);
}

TEST(ThreadSafeQueue, ConcurrentTryPushRespectsMaximumSize) {
    thread_safe_queue<int> queue(1);

    constexpr int producer_count = 32;

    std::atomic<int> successful_pushes{0};
    std::vector<std::thread> producers;

    for (int i = 0; i < producer_count; ++i) {
        producers.emplace_back([&queue, &successful_pushes, i] {
            if (queue.try_push_back(i)) {
                ++successful_pushes;
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    EXPECT_EQ(successful_pushes.load(), 1);
    EXPECT_EQ(queue.size(), 1);
}
