#pragma once

#include "network/sockets/binding_socket.hpp"

class ListeningSocket : public BindingSocket {
private:
    int backlog;
    int listening;

public:

    ListeningSocket(int domain, int service, int protocol, int port, u_long interface, int backlog);

    void start_listening();
};