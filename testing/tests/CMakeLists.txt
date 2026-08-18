add_executable(server_tests
    thread_safe_queue_test.cpp
    thread_safe_map_test.cpp
    rate_limiter_test.cpp
    data_structures_test.cpp
)

target_link_libraries(server_tests PRIVATE
    server
    GTest::gtest_main
)

target_compile_features(server_tests PRIVATE
    cxx_std_20
)

include(GoogleTest)

gtest_discover_tests(server_tests)