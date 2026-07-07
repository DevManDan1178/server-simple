cmake_minimum_required(VERSION 3.16)

project(Server
    VERSION 1.0
    LANGUAGES CXX
)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Source files
add_executable(server
    src/main.cpp
    ## NETWORK
        # Sockets
    src/network/sockets/socket_base.cpp
    src/network/sockets/binding_socket.cpp
    src/network/sockets/connecting_socket.cpp
    src/network/sockets/listening_socket.cpp
        # Servers
    src/network/servers/server_base.cpp
    src/network/servers/request_server_base.cpp
    src/network/servers/persistent_server_base.cpp
)

# Include headers
target_include_directories(server PRIVATE
    src
)

# Naming conventions for networking: usually snake_case