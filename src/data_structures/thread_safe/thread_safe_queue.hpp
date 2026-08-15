#pragma once

#include <condition_variable>
#include <mutex>
#include <deque>
#include <stdexcept>
#include <utility>
#include <functional>
#include <optional>
#include <vector>

/**
 * @brief Thread-safe queue supporting concurrent access.
 *
 * @tparam T Type of elements stored in the queue.
 */
template<typename T>
class thread_safe_queue {
    public:
        using size_function = std::function<size_t(const T&)>;
    protected:
        mutable std::mutex mutex_queue;
        std::condition_variable waiting;
        std::deque<T> dequeue;
        bool stopped = false;

        const size_t max_size;
        const size_t max_bytes;
        const size_function get_item_size;

        size_t current_bytes = 0;
    public:
        
        /**
         * @brief Creates an empty queue.
         * @param max_size max size of the queue [0 for unlimited size]
         * @param max_bytes max bytes of the queue [0 for unlimited size]
         * @param get_item_size function used to determine the size of each item
         */
        explicit thread_safe_queue(size_t max_size = 0, size_t max_bytes = 0, size_function get_item_size = [](const T&) {return 0;}) 
        : max_size(max_size), max_bytes(max_bytes), get_item_size(get_item_size) {}
        
        /**
         * @brief Prevents copying of the queue.
         */
        thread_safe_queue(const thread_safe_queue<T>&) = delete;

        /**
         * @brief Stops the queue on destruction.
         */
        virtual ~thread_safe_queue() {
            stop();
        }

        /**
         * @brief Gets the first element.
         * @return Front element.
         * @throws std::runtime_error If the queue is empty.
         */
        T front() {
            std::scoped_lock lock(mutex_queue);

            if (dequeue.empty()) {
                throw std::runtime_error("Queue is empty");
            }

            return dequeue.front();
        }

        /**
         * @brief Gets the last element.
         * @return Back element.
         * @throws std::runtime_error If the queue is empty.
         */
        T back() {
            std::scoped_lock lock(mutex_queue);

            if (dequeue.empty()) {
                throw std::runtime_error("Queue is empty");
            }

            return dequeue.back();
        }
    
        /**
         * @brief Adds an element to the back if the capacity will not be exceeded
         * @param item Element to add.
         * @return if it was added or not
         */
        bool try_push_back(const T& item) {
            {
                const size_t bytes = get_item_size(item);
                std::scoped_lock lock(mutex_queue);

                if (stopped || 
                    (max_bytes != 0 && (bytes > max_bytes - current_bytes)) ||
                    (max_size != 0 && dequeue.size() >= max_size)) {
                    return false;
                }

                dequeue.emplace_back(item);
                current_bytes += bytes;
            }

            waiting.notify_one();
            return true;
        }

        /**
         * @brief Adds an element to the back if the capacity will not be exceeded
         * @param item Element to add.
         * @return if it was added or not
         */
        bool try_push_back(T&& item) {
            {
                const size_t bytes = get_item_size(item);
                std::scoped_lock lock(mutex_queue);

                if (stopped || 
                    (max_bytes != 0 && (bytes > max_bytes - current_bytes)) ||
                    (max_size != 0 && dequeue.size() >= max_size)) {
                    return false;
                }

                dequeue.emplace_back(std::move(item));
                current_bytes += bytes;
            }

            waiting.notify_one();
            return true;
        }

        /**
         * @brief Adds an element to the front if the capacity will not be exceeded
         * @param item Element to add.
         * @return if it was added or not
         */
        bool try_push_front(const T& item) {
            {
                const size_t bytes = get_item_size(item);
                std::scoped_lock lock(mutex_queue);

                if (stopped || 
                    (max_bytes != 0 && (bytes > max_bytes - current_bytes)) ||
                    (max_size != 0 && dequeue.size() >= max_size)) {
                    return false;
                }

                dequeue.emplace_front(item);
                current_bytes += bytes;
            }

            waiting.notify_one();
            return true;
        }

        
        /**
         * @brief Adds an element to the front if the capacity will not be exceeded
         * @param item Element to move into the queue.
         * @return if it was added or not
         */
        bool try_push_front(T&& item) {
            {
                const size_t bytes = get_item_size(item);
                std::scoped_lock lock(mutex_queue);
                
                if (stopped || 
                    (max_bytes != 0 && (bytes > max_bytes - current_bytes)) ||
                    (max_size != 0 && dequeue.size() >= max_size)) {
                    return false;
                }

                dequeue.emplace_front(std::move(item));
                current_bytes += bytes;
            }

            waiting.notify_one();
            return true;
        }

        /**
         * @brief Adds an element to the back.
         * @param item Element to add.
         */
        void push_back(const T& item) {
            {
                const size_t bytes = get_item_size(item);
                std::scoped_lock lock(mutex_queue);

                dequeue.emplace_back(item);
                current_bytes += bytes;
            }

            waiting.notify_one();
        }

        /**
         * @brief Adds an element to the back.
         * @param item Element to move into the queue.
         */
        void push_back(T&& item) {
            {
                const size_t bytes = get_item_size(item);
                std::scoped_lock lock(mutex_queue);

                dequeue.emplace_back(std::move(item));
                current_bytes += bytes;
            }

            waiting.notify_one();
        }

        /**
         * @brief Adds an element to the front.
         * @param item Element to add.
         */
        void push_front(const T& item) {
            {
                const size_t bytes = get_item_size(item);
                std::scoped_lock lock(mutex_queue);

                dequeue.emplace_front(item);
                current_bytes += bytes;
            }

            waiting.notify_one();
        }

        
        /**
         * @brief Adds an element to the front.
         * @param item Element to move into the queue.
         */
        void push_front(T&& item) {
            {
                const size_t bytes = get_item_size(item);
                std::scoped_lock lock(mutex_queue);

                dequeue.emplace_front(std::move(item));
                current_bytes += bytes;
            }

            waiting.notify_one();
        }


        /**
         * @brief Checks whether the queue is empty.
         * @return True if empty.
         */
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
            current_bytes = 0;
        }

        /**
         * @brief Removes and returns the first element.
         * @return Removed element.
         * @throws std::runtime_error If the queue is empty.
         */
        T pop_front() {
            std::scoped_lock lock(mutex_queue);

            if (dequeue.empty()) {
                throw std::runtime_error("Queue is empty");
            }

            T item = std::move(dequeue.front());

            current_bytes -= get_item_size(item);
            dequeue.pop_front();

            return item;
        }

        /**
         * @brief Removes and returns the last element.
         * @return Removed element.
         * @throws std::runtime_error If the queue is empty.
         */
        T pop_back() {
            std::scoped_lock lock(mutex_queue);

            if (dequeue.empty()) {
                throw std::runtime_error("Queue is empty");
            }

            T item = std::move(dequeue.back());

            current_bytes -= get_item_size(item);
            dequeue.pop_back();

            return item;
        }

        /**
         * @brief Waits until an element is available.
         *
         * @return Next element in the queue.
         * @throws std::runtime_error If the queue is stopped.
         */
        std::optional<T> wait_and_pop() {
            std::unique_lock<std::mutex> lock(mutex_queue);

            waiting.wait(lock, [this] {
                return stopped || !dequeue.empty();
            });

            if (dequeue.empty()) {
                throw std::runtime_error("Queue stopped");
                return std::nullopt;
            }

            T item = std::move(dequeue.front());

            current_bytes -= get_item_size(item);
            dequeue.pop_front();

            return item;
        }

        /**
         * @brief Stops the queue and wakes waiting threads.
         */
        void stop() {
            {
                std::scoped_lock lock(mutex_queue);
                if (stopped) {
                    return;
                }
                stopped = true;
            }

            waiting.notify_all();
        }

        /**
         * @brief Returns a copy of all elements in queue order.
         *
         * @return Vector containing all elements.
         */
        std::vector<T> to_vector() const {
            std::scoped_lock lock(mutex_queue);

            return {
                dequeue.begin(),
                dequeue.end()
            };
        }


        /**
         * @brief Replaces the queue contents.
         *
         * @param items New contents.
         */
        void replace(const std::vector<T>& items) {
            std::scoped_lock lock(mutex_queue);

            dequeue.clear();
            current_bytes = 0;

            for (const auto& item : items) {
                dequeue.push_back(item);
                current_bytes += get_item_size(item);
            }
        }
};