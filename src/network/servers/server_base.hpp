#pragma once

#include "network/sockets/sockets.hpp"

class ServerBase {
protected:
    ListeningSocket* const socket;

    virtual void accept() = 0;
    virtual void handle() = 0;
    virtual void respond() = 0;

public:
    ServerBase(int domain, int service, int protocol, int port, u_long interface, int backlog);
    
    virtual void launch() = 0;

    ListeningSocket* const get_socket();
};