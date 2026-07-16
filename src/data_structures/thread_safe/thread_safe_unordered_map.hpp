#pragma once

#include "data_structures/thread_safe/locked_value.hpp"
#include <mutex>
#include <unordered_map>
#include <stdexcept>
#include <utility>

template<typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class thread_safe_unordered_map {
    protected:
        std::mutex mutex_map;
        std::unordered_map<K, V, Hash, KeyEqual> map;

    public:
        thread_safe_unordered_map() = default;

        thread_safe_unordered_map(const thread_safe_unordered_map<K, V, Hash, KeyEqual>&) = delete;

        virtual ~thread_safe_unordered_map() = default;

        /**
         * @brief Inserts or replaces a key/value pair.
         */
        void insert(const K& key, const V& value) {
            std::scoped_lock lock(mutex_map);
            map[key] = value;
        }

        /**
         * @brief Inserts or replaces a key/value pair using move semantics.
         */
        void insert(K&& key, V&& value) {
            std::scoped_lock lock(mutex_map);
            map[std::move(key)] = std::move(value);
        }

        /**
         * @brief Inserts only if the key does not already exist.
         *
         * @return true if inserted, false if key already exists.
         */
        bool emplace(const K& key, const V& value) {
            std::scoped_lock lock(mutex_map);

            auto result = map.emplace(key, value);
            return result.second;
        }

        bool emplace(K&& key, V&& value) {
            std::scoped_lock lock(mutex_map);

            auto result = map.emplace(std::move(key), std::move(value));
            return result.second;
        }

        /**
         * @brief Returns a copy of the value associated with a key.
         */
        V get(const K& key) {
            std::scoped_lock lock(mutex_map);

            auto iterator = map.find(key);

            if (iterator == map.end()) {
                throw std::runtime_error("Key not found - thread_safe_map");
            }

            return iterator->second;
        }

        locked_value<V> get_locked(const K& key) {
            std::unique_lock<std::mutex> lock(mutex_map);

            auto iterator = map.find(key);

            if (iterator == map.end()) {
                throw std::runtime_error("Key not found - thread_safe_map");
            }

            return locked_value<V>(
                std::move(lock),
                &iterator->second
            );
        }

        /**
         * @brief Attempts to retrieve a value.
         *
         * @return true if key exists, false otherwise.
         */
        bool try_get(const K& key, V& value) {
            std::scoped_lock lock(mutex_map);

            auto iterator = map.find(key);

            if (iterator == map.end()) {
                return false;
            }

            value = iterator->second;
            return true;
        }

        /**
         * @brief Removes a key.
         *
         * @return true if removed, false if key did not exist.
         */
        bool erase(const K& key) {
            std::scoped_lock lock(mutex_map);

            return map.erase(key) > 0;
        }

        /**
         * @brief Checks if a key exists.
         */
        bool contains(const K& key) {
            std::scoped_lock lock(mutex_map);

            return map.find(key) != map.end();
        }

        /**
         * @brief Returns the number of elements.
         */
        size_t size() {
            std::scoped_lock lock(mutex_map);

            return map.size();
        }

        /**
         * @brief Checks if the map is empty.
         */
        bool empty() {
            std::scoped_lock lock(mutex_map);

            return map.empty();
        }

        /**
         * @brief Removes all elements.
         */
        void clear() {
            std::scoped_lock lock(mutex_map);

            map.clear();
        }

        /**
         * @brief Returns a copy of the underlying map.
         *
         * Useful for iteration without holding the mutex.
         */
        std::unordered_map<K, V, Hash, KeyEqual> snapshot() {
            std::scoped_lock lock(mutex_map);

            return map;
        }

        /**
         * @brief Returns a value and removes the key.
         */
        V extract(const K& key) {
            std::scoped_lock lock(mutex_map);

            auto iterator = map.find(key);

            if (iterator == map.end()) {
                throw std::runtime_error("Key not found");
            }

            V value = std::move(iterator->second);
            map.erase(iterator);

            return value;
        }

        template<typename Function>
        void for_each(Function&& function) {
            std::scoped_lock lock(mutex_map);

            for (auto& [key, value] : map) {
                function(key, value);
            }
        }

        template<typename... Args>
        bool try_emplace(const K& key, Args&&... args) {
            std::scoped_lock lock(mutex_map);

            return map.try_emplace(
                key,
                std::forward<Args>(args)...
            ).second;
        }

        template<typename... Args>
        std::pair<locked_value<V>, bool> try_emplace_locked(const K& key, Args&&... args) {
            std::unique_lock<std::mutex> lock(mutex_map);

            auto [iterator, inserted] = map.try_emplace(
                key,
                std::forward<Args>(args)...
            );

            return {
                locked_value<V>(
                    std::move(lock),
                    &iterator->second
                ),
                inserted
            };
        }
};