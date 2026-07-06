#pragma once
#include "network/sockets/socket_base.hpp"

class ConnectingSocket : SocketBase {
public:
    
    ConnectingSocket(int domain, int service, int protocol, int port, u_long interface);

    int connect_to_network(int socket_fd, sockaddr_in address);

};