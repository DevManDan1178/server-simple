if(SERVER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(unit)
    add_subdirectory(integration)
endif()

if(SERVER_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()