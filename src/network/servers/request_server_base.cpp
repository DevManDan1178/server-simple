#include "network/servers/request_server_base.hpp"
#include <iostream>
#include <cstring>


RequestServerBase::RequestServerBase(
    int domain, 
    int service, 
    int protocol, 
    int port, 
    u_long interface, 
    int backlog
) : ServerBase(domain, service, protocol, port, interface, backlog) {}


std::string RequestServerBase::process_client_request(int socket_fd, const sockaddr_in& socket_address, socklen_t address_size, std::string_view request) {
    std::cout << buffer << std::endl;
    return "HTTP/1.1 200 OK\r\n\r\nHello from request server";
}


int RequestServerBase::accept_client() {
    sockaddr_in address;
    socklen_t address_size = sizeof(address);
    int detected_socket_fd = ::accept(socket->get_socket_fd(), (sockaddr*) &address, (socklen_t *) &address_size);

    if (detected_socket_fd < 0) {
        perror("Could not accept socket");
        exit(EXIT_FAILURE);
    }

    ssize_t bytesRead = read(detected_socket_fd, buffer, buffer_size);
    
    if (bytesRead > 0) {
        std::string_view request(buffer, bytesRead);
        std::string response = process_client_request(detected_socket_fd, address, address_size, request);
        respond(detected_socket_fd, response);
        
        std::cout << "Processed request: \n" << request << "\n Into response: \n" << response << std::endl;
        return detected_socket_fd;
    } else if (bytesRead == 0) {
        std::cout << "Client disconnected \n";
        return detected_socket_fd;
    }
    
    perror("No bytes read from the request");
    return -1;
}

void RequestServerBase::launch() {
    while (true) {
        std::cout << "Waiting" << std::endl;
        accept_client();
        std::cout << "Finished" << std::endl;
    }
}

