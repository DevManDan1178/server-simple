#include <iostream>
#include "network/sockets/sockets.hpp"
#include "network/servers/test_server.hpp"

int main() {
    /*
    std::cout << "Running" << std::endl;

    std::cout << "creating binding socket \n";
    BindingSocket binding =  BindingSocket(AF_INET, SOCK_STREAM, 0, 8080, INADDR_ANY);
    
    std::cout << "creating listening socket \n";
    ListeningSocket listening = ListeningSocket(AF_INET, SOCK_STREAM, 0, 8081, INADDR_ANY, 10);
    
    std::cout << "success \n";
    */
    TestServer test;
    return 0;
}