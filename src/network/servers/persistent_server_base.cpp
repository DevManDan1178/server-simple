#include "network/servers/persistent_server_base.hpp"

#include "network/servers/request_server_base.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/poll.h>

PersistentServerBase::PersistentServerBase(
    int domain, 
    int service, 
    int protocol, 
    int port, 
    u_long interface, 
    int backlog,
    float fixed_delta_time
) : ServerBase(domain, service, protocol, port, interface, backlog), fixed_delta_time(fixed_delta_time) {}


void PersistentServerBase::broadcast(const std::string& message){
    for (int client : clients) {
        send(client, message.c_str(), message.size(), 0);
    }
}

std::string PersistentServerBase::process_client_request(int client, const sockaddr_in& socket_address, socklen_t address_size, const std::string_view request) {
    return "HTTP/1.1 200 OK\r\n\r\nPersistent server recieved:\n" + std::string(request);
}

void PersistentServerBase::launch() {
    std::vector<pollfd> file_descriptors;
    int server_socket = socket->get_socket_fd();

    pollfd server_poll;
    server_poll.fd = server_socket;
    server_poll.events = POLLIN;
    server_poll.revents = 0;

    file_descriptors.push_back(server_poll);

    while (true) {
        int ready = poll(file_descriptors.data(), file_descriptors.size(), -1);

        if (ready < 0) { 
            perror("Poll error");
        }


        for (size_t i = 0; i < file_descriptors.size(); i++) {
            pollfd& file_descriptor = file_descriptors[i];
            
            if (file_descriptor.revents == 0) {
                continue;
            }
            
            // On server: check for new client
            if (file_descriptor.fd == server_socket) { 
                int client = ::accept(server_socket, nullptr, nullptr);

                pollfd client_poll;
                client_poll.fd = client;
                client_poll.events = POLLIN;
                client_poll.revents = 0;

                file_descriptors.push_back(client_poll);
                on_client_connected(client);
            } 
            // On clients: check for new data
            else if (file_descriptor.revents & POLLIN) {
                // Existing client sent data
                char buffer[1024];
                int bytesRead = read(file_descriptor.fd, buffer, sizeof(buffer));

                if (bytesRead <= 0) {
                    // Client disconnected
                    close(file_descriptor.fd);
                    file_descriptors.erase(file_descriptors.begin() + i);
                    
                    on_client_disconnected(file_descriptor.fd);
                    i--; // because vector shifted
                }
                else {
                    std::string_view request(buffer, bytesRead);
                    sockaddr_in address;
                    socklen_t address_size = sizeof(address);
                    std::string response = process_client_request(file_descriptor.fd, address, address_size, request);
                    respond(file_descriptor.fd, response);
                }
            }
        }
    }
}
