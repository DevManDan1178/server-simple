cmake_minimum_required(VERSION 3.16)

project(Server
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED)

add_library(server-simple INTERFACE)

add_subdirectory(external/json)

target_link_libraries(server-simple INTERFACE
    nlohmann_json::nlohmann_json
    Boost::boost
)

target_compile_definitions(server-simple INTERFACE
    DEBUG_MODE
)

target_include_directories(server-simple INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

#[[  OLD CMAKELISTS.TXT (before making into submodule)

cmake_minimum_required(VERSION 3.16)

project(Server
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(server)

add_subdirectory(external/json)
target_link_libraries(server PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(server PRIVATE DEBUG_MODE)

target_include_directories(server PRIVATE
    src
    include
)

target_link_libraries(server PRIVATE 
    Boost::boost
)]]