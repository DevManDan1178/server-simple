#pragma once

#include <mutex>
#include <deque>

template<typename T>
class thread_safe_queue {
    protected:
        std::mutex mutex_queue;
        std::deque<T> dequeue;


        std::mutex mutex_waiting;
        std::condition_variable waiting;


    public:
        thread_safe_queue() = default;
        thread_safe_queue(const thread_safe_queue<T>&) = delete;
        virtual ~thread_safe_queue() {
            clear();
        }

        const T& front() {
            std::scoped_lock lock (mutex_queue);
            return dequeue.front();
        }

        const T& back() {
            std::scoped_lock lock(mutex_queue);
            return dequeue.back();
        }

        void push_back(const T& item) {
            std::scoped_lock lock(mutex_queue);
            dequeue.emplace_back(item);
            stop_wait();
        }

        void push_back(T&& item) {
            std::scoped_lock lock(mutex_queue);
            dequeue.emplace_back(item);
            stop_wait();
        }


        void push_front(const T& item) {
            std::scoped_lock lock (mutex_queue);
            return dequeue.emplace_front(item);
            stop_wait();
        }

        
        void push_front(T&& item) {
            std::scoped_lock lock (mutex_queue);
            return dequeue.emplace_front(std::move(item));
            stop_wait();
        }

        bool empty() {
            std::scoped_lock lock (mutex_queue);
            return dequeue.empty();
        }

        size_t size() {
            std::scoped_lock lock (mutex_queue);
            return dequeue.size();
        }

        void clear() {
            std::scoped_lock lock (mutex_queue);
            dequeue.clear();  
        }


        T pop_front() {
            std::scoped_lock lock (mutex_queue);
            T t = std::move(dequeue.front());
            dequeue.pop_front();
            return t;
        }
        
        T pop_back() {
             std::scoped_lock lock (mutex_queue);
            T t = std::move(dequeue.back());
            dequeue.pop_back();
            return t;
        }

        /**
         * @brief 
         */
        void wait() {
            while(empty()) {
                std::unique_lock<std::mutex> unique_lock(mutex_waiting);
                waiting.wait(unique_lock);
            }
        }
    private:
        void stop_wait() {
            std::unique_lock<std::mutex> unique_lock(mutex_waiting);
            waiting.notify_one();
        }
};