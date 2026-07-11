cmake_minimum_required(VERSION 3.16)

project(Server
    VERSION 1.0
    LANGUAGES CXX
)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find system Boost (Asio and Beast are included in this)
find_package(Boost REQUIRED)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Source files (Renamed the target to 'server_app' to avoid any naming issues)
add_executable(server
    src/main.cpp
)

target_compile_definitions(server PRIVATE DEBUG_MODE)
# Include project headers
target_include_directories(server PRIVATE
    src
    include
)

# Link directly to your system Boost installation
target_link_libraries(server PRIVATE 
    Boost::boost
)