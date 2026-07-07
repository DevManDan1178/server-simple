#include "network/servers/server_base.hpp"


ServerBase::ServerBase(
    int domain, 
    int service, 
    int protocol, 
    int port, 
    u_long interface, 
    int backlog
) : socket(new ListeningSocket(domain, service, protocol, port, interface, backlog)) {}


ListeningSocket* const ServerBase::get_socket() {
    return socket;
}


void ServerBase::respond(int socket_fd, std::string_view response) {
    write(socket_fd, response.data(), response.size());
    close(socket_fd);
}