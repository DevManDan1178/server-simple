#pragma once

#include "network/sockets/sockets.hpp"

class ServerBase {
protected:
    ListeningSocket* const socket;

public:
    ServerBase(int domain, int service, int protocol, int port, u_long interface, int backlog);
    
};