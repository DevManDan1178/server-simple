if(SERVER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(servers)
    target_include_directories(server INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/testing
    )
endif()

if(SERVER_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

