set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

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