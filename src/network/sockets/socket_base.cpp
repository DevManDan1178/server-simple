#include "network/sockets/sockets.hpp"
#include <iostream>

SocketBase::SocketBase(
    int domain, 
    int service, 
    int protocol, 
    int port,
    u_long interface
) {
    // Define adress (sockaddr_in)
    address.sin_family = domain;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(interface);

    // Define established connection
    socket_fd = socket(domain, service, protocol);
    test_connection(socket_fd);
}

void SocketBase::test_connection(int item) {
    if (item < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
}

// Getters

int SocketBase::get_socket_fd() {
    return socket_fd;
}
int SocketBase::get_connection() {
    return connection;
}
sockaddr_in SocketBase::get_address() {
    return address;
}