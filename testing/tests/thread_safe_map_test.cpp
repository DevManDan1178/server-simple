#include <gtest/gtest.h>

#include "data_structures/thread_safe/thread_safe_unordered_map.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

TEST(ThreadSafeMap, InsertAndGet) {
    thread_safe_unordered_map<std::string, int> map;

    map.insert("alice", 42);

    EXPECT_TRUE(map.contains("alice"));
    EXPECT_EQ(map.get("alice"), 42);
    EXPECT_EQ(map.size(), 1);
    EXPECT_FALSE(map.empty());
}

TEST(ThreadSafeMap, InsertReplacesExistingValue) {
    thread_safe_unordered_map<std::string, int> map;

    map.insert("alice", 10);
    map.insert("alice", 20);

    EXPECT_EQ(map.get("alice"), 20);
    EXPECT_EQ(map.size(), 1);
}

TEST(ThreadSafeMap, EmplaceDoesNotReplaceExistingValue) {
    thread_safe_unordered_map<std::string, int> map;

    EXPECT_TRUE(map.emplace("alice", 10));
    EXPECT_FALSE(map.emplace("alice", 20));

    EXPECT_EQ(map.get("alice"), 10);
}

TEST(ThreadSafeMap, TryGetReportsMissingKeys) {
    thread_safe_unordered_map<std::string, int> map;

    int value = 0;

    EXPECT_FALSE(map.try_get("missing", value));

    map.insert("alice", 42);

    EXPECT_TRUE(map.try_get("alice", value));
    EXPECT_EQ(value, 42);
}

TEST(ThreadSafeMap, EraseRemovesKey) {
    thread_safe_unordered_map<std::string, int> map;

    map.insert("alice", 42);

    EXPECT_TRUE(map.erase("alice"));
    EXPECT_FALSE(map.contains("alice"));
    EXPECT_FALSE(map.erase("alice"));
}

TEST(ThreadSafeMap, LockedValueCanModifyStoredObject) {
    thread_safe_unordered_map<std::string, int> map;

    map.insert("counter", 1);

    {
        auto locked = map.get_locked("counter");

        ASSERT_TRUE(static_cast<bool>(locked));

        *locked = 42;
    }

    EXPECT_EQ(map.get("counter"), 42);
}

TEST(ThreadSafeMap, ExtractReturnsAndRemovesValue) {
    thread_safe_unordered_map<std::string, int> map;

    map.insert("alice", 42);

    EXPECT_EQ(map.extract("alice"), 42);
    EXPECT_FALSE(map.contains("alice"));
}

TEST(ThreadSafeMap, SnapshotContainsCurrentState) {
    thread_safe_unordered_map<std::string, int> map;

    map.insert("alice", 1);
    map.insert("bob", 2);

    const auto snapshot = map.snapshot();

    ASSERT_EQ(snapshot.size(), 2);
    EXPECT_EQ(snapshot.at("alice"), 1);
    EXPECT_EQ(snapshot.at("bob"), 2);
}

TEST(ThreadSafeMap, ConcurrentInsertsRemainConsistent) {
    thread_safe_unordered_map<int, int> map;

    constexpr int thread_count = 4;
    constexpr int items_per_thread = 1'000;

    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
        threads.emplace_back([&map, thread_id] {
            for (int i = 0; i < items_per_thread; ++i) {
                const int key = thread_id * items_per_thread + i;
                map.insert(key, key * 2);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(map.size(), thread_count * items_per_thread);

    for (int i = 0; i < thread_count * items_per_thread; ++i) {
        EXPECT_EQ(map.get(i), i * 2);
    }
}