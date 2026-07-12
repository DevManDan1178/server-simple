#pragma once

#include <condition_variable>
#include <mutex>
#include <deque>
#include <stdexcept>
#include <utility>

template<typename T>
class thread_safe_queue {
protected:
    std::mutex mutex_queue;
    std::condition_variable waiting;
    std::deque<T> dequeue;
    bool stopped = false;

public:
    thread_safe_queue() = default;

    thread_safe_queue(const thread_safe_queue<T>&) = delete;

    virtual ~thread_safe_queue() {
        stop();
    }

    T front() {
        std::scoped_lock lock(mutex_queue);

        if (dequeue.empty()) {
            throw std::runtime_error("Queue is empty");
        }

        return dequeue.front();
    }

    T back() {
        std::scoped_lock lock(mutex_queue);

        if (dequeue.empty()) {
            throw std::runtime_error("Queue is empty");
        }

        return dequeue.back();
    }

    void push_back(const T& item) {
        {
            std::scoped_lock lock(mutex_queue);
            dequeue.emplace_back(item);
        }

        waiting.notify_one();
    }

    void push_back(T&& item) {
        {
            std::scoped_lock lock(mutex_queue);
            dequeue.emplace_back(std::move(item));
        }

        waiting.notify_one();
    }

    void push_front(const T& item) {
        {
            std::scoped_lock lock(mutex_queue);
            dequeue.emplace_front(item);
        }

        waiting.notify_one();
    }

    void push_front(T&& item) {
        {
            std::scoped_lock lock(mutex_queue);
            dequeue.emplace_front(std::move(item));
        }

        waiting.notify_one();
    }

    bool empty() {
        std::scoped_lock lock(mutex_queue);
        return dequeue.empty();
    }

    size_t size() {
        std::scoped_lock lock(mutex_queue);
        return dequeue.size();
    }

    void clear() {
        std::scoped_lock lock(mutex_queue);
        dequeue.clear();
    }

    T pop_front() {
        std::scoped_lock lock(mutex_queue);

        if (dequeue.empty()) {
            throw std::runtime_error("Queue is empty");
        }

        T item = std::move(dequeue.front());
        dequeue.pop_front();

        return item;
    }

    T pop_back() {
        std::scoped_lock lock(mutex_queue);

        if (dequeue.empty()) {
            throw std::runtime_error("Queue is empty");
        }

        T item = std::move(dequeue.back());
        dequeue.pop_back();

        return item;
    }

    /**
     * Blocks until an item is available or the queue is stopped.
     */
    T wait_and_pop() {
        std::unique_lock<std::mutex> lock(mutex_queue);

        waiting.wait(lock, [this] {
            return stopped || !dequeue.empty();
        });

        if (stopped && dequeue.empty()) {
            throw std::runtime_error("Queue stopped");
        }

        T item = std::move(dequeue.front());
        dequeue.pop_front();

        return item;
    }

    /**
     * @brief Wakes up all waiting threads and prevents further waiting.
     */
    void stop() {
        {
            std::scoped_lock lock(mutex_queue);
            stopped = true;
        }

        waiting.notify_all();
    }
};