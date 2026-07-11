#pragma once
#include <iostream>

class log_debug {
public:
    log_debug() {
#ifdef DEBUG_MODE
        std::cout << "<Debug> ";
#endif
    }

    ~log_debug() {
#ifdef DEBUG_MODE
        std::cout << "\n"; 
#endif
    }

    // Overload the << operator to accept any type (ints, strings, etc.)
    template <typename T>
    const log_debug& operator<<(const T& val) const {
#ifdef DEBUG_MODE
        std::cout << val;
#endif
        return *this;
    }
};
