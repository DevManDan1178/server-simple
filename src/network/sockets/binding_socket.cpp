#include "network/sockets/binding_socket.hpp"

BindingSocket::BindingSocket(
    int domain, 
    int service, 
    int protocol, 
    int port, 
    u_long interface
) : SocketBase(domain, service, protocol, port, interface) {
    connection = connect_to_network(socket_fd, address);
    test_connection(connection);
}

int BindingSocket::connect_to_network(int socket_fd, sockaddr_in address) {
    return bind(socket_fd, (sockaddr*) &address, sizeof(address));
}