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
 * @brief template struct using constexpr sizeof overridable with templates for custom size behaviour
 * Defaults to compile time constexpr sizeof(T)
 * @tparam type of object
 */
template<typename T>
struct queue_size_traits {
    /**
     * @brief Function to get the byte size of the object. 
     * Set as constexpr when possible for compile-time optimization.
     * @param object the object 
     * @return size of the object
     */
    static constexpr size_t get([[maybe_unused]] const T& object) noexcept {
        return sizeof(T);
    }
};

/**
 * @brief Thread-safe queue supporting concurrent access.
 *
 * @tparam T Type of elements stored in the queue.
 */
template<
    typename T, 
    bool EnableMaxSize = true, 
    bool EnableMaxBytes = true
>
class thread_safe_queue {
    protected:
        mutable std::mutex mutex_queue;
        std::condition_variable waiting;
        std::deque<T> dequeue;
        bool stopped = false;

        const size_t maximum_size;
        const size_t maximum_bytes;

        size_t current_bytes = 0;
    public:
        /**
         * @brief Creates an empty queue.
         * @param maximum_size max size of the queue [0 for unlimited size]
         * @param maximum_bytes max bytes of the queue [0 for unlimited size]
         * @param queue_size_traits<T>::get function used to determine the size of each item
         */
        explicit thread_safe_queue(size_t maximum_size = 0, size_t maximum_bytes = 0) 
        : maximum_size(maximum_size), maximum_bytes(maximum_bytes) {}
        
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
                std::scoped_lock lock(mutex_queue);
                
                if (!pre_addition_check_protocol(item)) {
                    return false;
                }

                dequeue.emplace_back(item);
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
                std::scoped_lock lock(mutex_queue);

                if (!pre_addition_check_protocol(item)) {
                    return false;
                }

                dequeue.emplace_back(std::move(item));
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
                std::scoped_lock lock(mutex_queue);

                if (!pre_addition_check_protocol(item)) {
                    return false;
                }

                dequeue.emplace_front(item);
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
                std::scoped_lock lock(mutex_queue);
                
                if (!pre_addition_check_protocol(item)) {
                    return false;
                }

                dequeue.emplace_front(std::move(item));
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
                std::scoped_lock lock(mutex_queue);

                unchecked_addition_protocol(item);

                dequeue.emplace_back(item);      
            }

            waiting.notify_one();
        }

        /**
         * @brief Adds an element to the back.
         * @param item Element to move into the queue.
         */
        void push_back(T&& item) {
            {
                std::scoped_lock lock(mutex_queue);

                unchecked_addition_protocol(item);

                dequeue.emplace_back(std::move(item));
            }

            waiting.notify_one();
        }

        /**
         * @brief Adds an element to the front.
         * @param item Element to add.
         */
        void push_front(const T& item) {
            {
                std::scoped_lock lock(mutex_queue);

                unchecked_addition_protocol(item);

                dequeue.emplace_front(item);
            }

            waiting.notify_one();
        }

        
        /**
         * @brief Adds an element to the front.
         * @param item Element to move into the queue.
         */
        void push_front(T&& item) {
            {
                std::scoped_lock lock(mutex_queue);

                unchecked_addition_protocol(item);

                dequeue.emplace_front(std::move(item));
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
        
        /**
         * @return the max size if max size is enabled, 0 if there is no max size
         */
        size_t max_size() {
            if constexpr (EnableMaxSize) {
                return maximum_size;
            }
            return 0;
        }

        /**
         * @return the max amount of bytes if max bytes is enabled, 0 if there is no max amount of bytes
         */
        size_t max_bytes() {
            if constexpr (EnableMaxBytes) {
                return maximum_bytes;
            }
            return 0;
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
            
            removal_protocol(item);
           
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

            removal_protocol(item);

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
                return std::nullopt;
            }

            T item = std::move(dequeue.front());

            removal_protocol(item);

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
         * If the contents are longer than the current size, stops after the current size has been reached (from the start)
         * @param items New contents.
         */
        void replace(const std::vector<T>& items) {
            std::scoped_lock lock(mutex_queue);

            dequeue.clear();
            current_bytes = 0;

            size_t current_size = 0;
            for (const auto& item : items) {
                dequeue.push_back(item);
                if constexpr (EnableMaxBytes) {
                    current_bytes += queue_size_traits<T>::get(item);  
                }
            
                if constexpr (EnableMaxSize) {
                    if ((maximum_size > 0) && (++current_size >= maximum_size)) {
                        break;
                    }
                }
            }
        }
    
    protected:
        /**
         * @brief returns false if the queue is stopped or if the addition will cause a size overflow. If not, adds the item size to the counter (if counting) and returns true.
         * Call before adding items to the queue to check if can be added
         * @param item_bytes the byte size of the item
         * @return if the addition is allowed
         */
        inline bool pre_addition_check_protocol(const T& item) {
            if (stopped) {
                return false;
            }

            if constexpr (EnableMaxSize) {
                if (maximum_size != 0 && (dequeue.size() >= maximum_size)) {
                    return false;
                }
            }
            
            if constexpr (EnableMaxBytes) {
                if (maximum_bytes != 0) {
                    const size_t bytes = queue_size_traits<T>::get(item);

                    if ((current_bytes > maximum_bytes) || (bytes > maximum_bytes - current_bytes)) {
                        return false;
                    }

                    current_bytes += bytes;
                }
            }
            
            return true;
        }

        /**
         * @brief adds the item size to the counter
         * Call when adding an item to the queue without checks
         */
        inline void unchecked_addition_protocol(const T& item) {
            if constexpr(EnableMaxBytes) {
                if (maximum_bytes != 0) {
                    current_bytes += queue_size_traits<T>::get(item);
                }
            }
        }

        /**
         * @brief removes the item size from the counter (if counting)
         * Call when removing item from the queue
         */
        inline void removal_protocol(const T& item) {
            if constexpr (EnableMaxBytes) {
                if (maximum_bytes != 0) {
                    current_bytes -= queue_size_traits<T>::get(item);
                }
            }
            
        }
};