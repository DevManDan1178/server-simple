#include "network/servers/test_server.hpp"
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>

int DOMAIN = AF_INET;
int SERVICE = SOCK_STREAM;
int PROTOCOL = 0;
int PORT = 8080;
u_long INTERFACE = INADDR_ANY;
int BACKLOG = 10;

TestServer::TestServer() : ServerBase(DOMAIN, SERVICE, PROTOCOL, PORT, INTERFACE, BACKLOG) {
    launch();
}

void TestServer::accept() {
    sockaddr_in address = socket->get_address();
    int addressSize = sizeof(address);
    new_socket = ::accept(socket->get_socket_fd(), (sockaddr*) &address, (socklen_t *) addressSize);
    
    read(new_socket, buffer, buffer_size);
}

void TestServer::handle() {
    std::cout << buffer << std::endl;
}

void TestServer::respond() {
    char* str = "hello from server";
    write(new_socket, str, strlen(str));
    close(new_socket);
}

void TestServer::launch() {
    while (true) {
        std::cout << "Waiting" << std::endl;
        accept();
        handle();
        respond();
        std::cout << "Finished" << std::endl;
    }
}