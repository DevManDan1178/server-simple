#pragma once
#include "network/servers/server_base.hpp"
#include <unordered_set>
#include <string_view>
#include <string>

constexpr const float DEFAULT_FIXED_DELTA_TIME = 1.0f/60.0f;

// Server that does not close when the user joins
class PersistentServerBase : public ServerBase {
protected:
    std::unordered_set<int> clients;
    const float fixed_delta_time;
    
    virtual void on_client_connected(int client) {};
    virtual void on_client_disconnected(int client) {};
    virtual std::string process_client_request(int client, const sockaddr_in& socket_address, socklen_t address_size, const std::string_view request) override;
    
    virtual void update() {};
    void broadcast(const std::string& message);

public:
    PersistentServerBase(int domain, int service, int protocol, int port, u_long interface, int backlog, float fixed_delta_time = DEFAULT_FIXED_DELTA_TIME);
    
    virtual void launch();
};