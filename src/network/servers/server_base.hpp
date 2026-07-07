#pragma once

#include "network/sockets/sockets.hpp"
#include <string>
#include <string_view>
#include <netinet/in.h>
#include <unistd.h>

class ServerBase {
protected:
    constexpr static int buffer_size = 4096; //2^12
    char buffer[buffer_size] = {0};
    
    ListeningSocket* const socket;
    int backlog;


    
    virtual std::string process_client_request(int client, const sockaddr_in& socket_address, socklen_t address_size, const std::string_view request) = 0;
    virtual void respond(int socket_fd, std::string_view response);


public:
    ServerBase(int domain, int service, int protocol, int port, u_long interface, int backlog);
    
    virtual void launch() = 0;

    ListeningSocket* const get_socket();
};