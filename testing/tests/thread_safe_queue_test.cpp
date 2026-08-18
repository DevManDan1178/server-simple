#include <gtest/gtest.h>

#include "data_structures/thread_safe/thread_safe_queue.hpp"

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