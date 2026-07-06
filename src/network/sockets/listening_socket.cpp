#include "network/sockets/listening_socket.hpp"

ListeningSocket::ListeningSocket(
    int domain,
    int service,
    int protocol,
    int port,
    u_long interface,
    int backlog
) : BindingSocket(domain, service, protocol, port, interface), backlog(backlog) {
    start_listening();
    test_connection(listening);
}


void ListeningSocket::start_listening() {
    listening = listen(socket_fd, backlog);
}