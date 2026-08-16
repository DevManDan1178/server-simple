set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

add_executable(http_server
    http_server.cpp
)

target_link_libraries(http_server PRIVATE
    server
    Boost::boost
)

target_compile_features(http_server PRIVATE
    cxx_std_20
)