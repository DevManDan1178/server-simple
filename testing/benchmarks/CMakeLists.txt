set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

# HTTP BENCHMARK
add_executable(http_benchmark
    http_benchmark.cpp
)

target_link_libraries(http_benchmark PRIVATE
    server
    Boost::boost
)

target_compile_features(http_benchmark PRIVATE
    cxx_std_20
)


# THREAD SAFE QUEUE BENCHMARK
add_executable(tsq_benchmark
    thread_safe_queue_benchmark.cpp
)

target_link_libraries(tsq_benchmark PRIVATE
    server
    Boost::boost
)

target_compile_features(tsq_benchmark PRIVATE
    cxx_std_20
)