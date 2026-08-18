cmake_minimum_required(VERSION 3.16)

project(ServerSimple
    VERSION 1.0
    LANGUAGES CXX
)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------

option(SERVER_BUILD_TESTS "Build tests" OFF)

option(SERVER_BUILD_BENCHMARKS "Build benchmarks" OFF)

option(DEBUG_MODE "Enable debug mode" OFF)

# ------------------------------------------------------------------------------
# C++ configuration
# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

find_package(Boost REQUIRED)
add_subdirectory(external/json)

# ------------------------------------------------------------------------------
# Compiler warnings
# ------------------------------------------------------------------------------

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------------------------
# Server library
# ------------------------------------------------------------------------------

add_library(server-simple INTERFACE)

target_include_directories(server-simple INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(server-simple INTERFACE
    nlohmann_json::nlohmann_json
    Boost::boost
)

target_compile_features(server-simple INTERFACE
    cxx_std_20
)

target_compile_definitions(server-simple INTERFACE
    $<$<CONFIG:Debug>:DEBUG_MODE>
)

# ------------------------------------------------------------------------------
# Testing / Benchmarking
# ------------------------------------------------------------------------------

if(SERVER_BUILD_TESTS)
    enable_testing()
endif()

if(SERVER_BUILD_TESTS OR SERVER_BUILD_BENCHMARKS)
    add_subdirectory(testing)
endif()