set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

add_executable(echo_server
    echo_server.cpp
)

target_link_libraries(echo_server PRIVATE
    server
    Boost::boost
)

target_compile_features(echo_server PRIVATE
    cxx_std_20
)