#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "data_structures/thread_safe/thread_safe_unordered_map.hpp"

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

TEST(ThreadSafeMap, ConcurrentUpdatesToSameKey) {
  thread_safe_unordered_map<int, int> map;

  constexpr int thread_count = 8;
  constexpr int increments_per_thread = 10'000;

  map.insert(0, 0);

  std::vector<std::thread> threads;

  for (int i = 0; i < thread_count; ++i) {
    threads.emplace_back([&map] {
      for (int i = 0; i < increments_per_thread; ++i) {
        auto locked = map.get_locked(0);

        if (!locked) {
          return;
        }

        ++*locked;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(map.get(0), thread_count * increments_per_thread);
}

TEST(ThreadSafeMap, ConcurrentInsertAndErase) {
  thread_safe_unordered_map<int, int> map;

  constexpr int thread_count = 8;
  constexpr int operations_per_thread = 10'000;

  std::vector<std::thread> threads;

  for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
    threads.emplace_back([&map, thread_id] {
      for (int i = 0; i < operations_per_thread; ++i) {
        const int key = thread_id * operations_per_thread + i;

        map.insert(key, i);
        map.erase(key);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_TRUE(map.empty());
}

TEST(ThreadSafeMap, ConcurrentReads) {
  thread_safe_unordered_map<int, int> map;

  constexpr int key_count = 10'000;
  constexpr int reader_count = 8;

  for (int i = 0; i < key_count; ++i) {
    map.insert(i, i * 2);
  }

  std::vector<std::thread> threads;

  for (int reader = 0; reader < reader_count; ++reader) {
    threads.emplace_back([&map] {
      for (int i = 0; i < key_count; ++i) {
        EXPECT_EQ(map.get(i), i * 2);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

TEST(ThreadSafeMap, ConcurrentReadersAndWriters) {
  thread_safe_unordered_map<int, int> map;

  constexpr int key_count = 1'000;
  constexpr int writer_count = 4;
  constexpr int reader_count = 4;
  constexpr int iterations = 10'000;

  for (int i = 0; i < key_count; ++i) {
    map.insert(i, 0);
  }

  std::vector<std::thread> threads;

  for (int i = 0; i < writer_count; ++i) {
    threads.emplace_back([&map] {
      for (int i = 0; i < iterations; ++i) {
        const int key = i % key_count;
        map.insert(key, i);
      }
    });
  }

  for (int i = 0; i < reader_count; ++i) {
    threads.emplace_back([&map] {
      for (int i = 0; i < iterations; ++i) {
        const int key = i % key_count;

        (void)map.contains(key);
        (void)map.try_get(key, i);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(map.size(), key_count);
}

TEST(ThreadSafeMap, LockedValueSerializesAccess) {
  thread_safe_unordered_map<int, int> map;

  map.insert(0, 0);

  constexpr int thread_count = 8;
  constexpr int iterations = 1'000;

  std::vector<std::thread> threads;

  for (int i = 0; i < thread_count; ++i) {
    threads.emplace_back([&map] {
      for (int i = 0; i < iterations; ++i) {
        auto locked = map.get_locked(0);

        if (!locked) {
          return;
        }

        const int current = *locked;
        std::this_thread::yield();
        *locked = current + 1;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(map.get(0), thread_count * iterations);
}

TEST(ThreadSafeMap, LockedValueReleasesLock) {
  thread_safe_unordered_map<int, int> map;

  map.insert(0, 1);

  {
    auto locked = map.get_locked(0);
    ASSERT_TRUE(locked);
    *locked = 42;
  }

  // Another operation should be able to acquire the lock.
  EXPECT_EQ(map.get(0), 42);
  map.insert(1, 100);

  EXPECT_EQ(map.get(1), 100);
}

TEST(ThreadSafeMap, MissingKeyOperations) {
  thread_safe_unordered_map<int, int> map;

  EXPECT_FALSE(map.contains(1));
  EXPECT_FALSE(map.erase(1));

  int value = 0;
  EXPECT_FALSE(map.try_get(1, value));

  EXPECT_THROW(map.get_locked(1), std::runtime_error);
}

TEST(ThreadSafeMap, ConcurrentSnapshot) {
  thread_safe_unordered_map<int, int> map;

  constexpr int iterations = 10'000;

  std::atomic<bool> done{false};

  std::thread writer([&] {
    for (int i = 0; i < iterations; ++i) {
      map.insert(i, i);
    }

    done = true;
  });

  std::thread reader([&] {
    while (!done.load()) {
      const auto snapshot = map.snapshot();

      for (const auto& [key, value] : snapshot) {
        EXPECT_EQ(value, key);
      }
    }
  });

  writer.join();
  reader.join();

  EXPECT_EQ(map.size(), iterations);
}

TEST(ThreadSafeMap, ConcurrentExtractSameKey) {
  thread_safe_unordered_map<int, int> map;

  constexpr int thread_count = 16;
  constexpr int key = 42;
  constexpr int expected_value = 12345;

  map.insert(key, expected_value);

  std::atomic<int> successful_extracts{0};
  std::atomic<int> failed_extracts{0};
  std::atomic<int> extracted_value{-1};

  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  for (int i = 0; i < thread_count; ++i) {
    threads.emplace_back(
        [&map, &successful_extracts, &failed_extracts, &extracted_value, key] {
          try {
            const int value = map.extract(key);

            extracted_value.store(value);
            ++successful_extracts;
          } catch (const std::runtime_error&) {
            ++failed_extracts;
          }
        });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(successful_extracts.load(), 1);
  EXPECT_EQ(failed_extracts.load(), thread_count - 1);
  EXPECT_EQ(extracted_value.load(), expected_value);

  EXPECT_FALSE(map.contains(key));
  EXPECT_TRUE(map.empty());
}

TEST(ThreadSafeMap, GetMissingKeyThrows) {
  thread_safe_unordered_map<int, int> map;

  EXPECT_THROW(map.get(42), std::runtime_error);
}

TEST(ThreadSafeMap, GetLockedMissingKeyThrows) {
  thread_safe_unordered_map<int, int> map;

  EXPECT_THROW(map.get_locked(42), std::runtime_error);
}

TEST(ThreadSafeMap, ExtractMissingKeyThrows) {
  thread_safe_unordered_map<int, int> map;

  EXPECT_THROW(map.extract(42), std::runtime_error);
}
