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