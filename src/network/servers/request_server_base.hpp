#pragma once

#include "network/servers/server_base.hpp"

class RequestServerBase : ServerBase {
protected:
    virtual int accept_client();
    virtual std::string process_client_request(int socket_fd, const sockaddr_in& socket_address, socklen_t address_size, std::string_view request) override;

public:
    RequestServerBase(int domain, int service, int protocol, int port, u_long interface, int backlog);
    void launch();

};