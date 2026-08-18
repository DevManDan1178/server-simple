include(FetchContent)

if(SERVER_BUILD_TESTS)
    # GoogleTest
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )

    # Prevent GoogleTest from overriding the parent project's compiler/runtime
    # settings on Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()

if(SERVER_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()