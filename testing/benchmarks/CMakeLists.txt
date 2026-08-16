set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

add_executable( echo_benchmark
    echo_benchmark.cpp
)

target_link_libraries(echo_benchmark PRIVATE
    server
    Boost::boost
)

target_compile_features(echo_benchmark PRIVATE
    cxx_std_20
)