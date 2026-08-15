#pragma once
#include <mutex>
#include <memory>

template<typename V>
class locked_value {
    protected:
        std::unique_lock<std::mutex> lock;
        V* value;

    public:
        locked_value(std::unique_lock<std::mutex>&& lock, V* value)
            : lock(std::move(lock)), value(value) {}

        locked_value(const locked_value&) = delete;

        locked_value& operator=(const locked_value&) = delete;

        locked_value(locked_value&&) noexcept = default;

        locked_value& operator=(locked_value&&) noexcept = default;

        
        V* operator->() {
            return value;
        }

        V& operator*() {
            return *value;
        }

        const V* operator->() const {
            return value;
        }

        const V& operator*() const {
            return *value;
        }

        explicit operator bool() const {
            return value != nullptr;
        }

        
};