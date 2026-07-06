#pragma once

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>


class SocketBase {
private:
    struct sockaddr_in address;
    int socket_fd;
    int connection;

public:
    SocketBase(int domain, int service, int protocol, int port, u_long interface);    

    /**
     * @brief Virtual function for connecting to a network
     */
    virtual int connect_to_network(int socket_fd, sockaddr_in adress) = 0;

    /**
     * @brief Tests sockets and connections
     * Exits the program if the test failed (item < 0)
     */
    void test_connection(int item);

    // Getters

    int get_socket_fd();
    int get_connection();
    sockaddr_in get_adress();
};