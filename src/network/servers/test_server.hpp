#pragma once

#include "network/servers/server_base.hpp"

class TestServer : ServerBase {
private:
    constexpr static int buffer_size = 30000;
    char buffer[buffer_size] = {0};
    int new_socket;

    void accept();
    void handle();
    void respond();

public:
    TestServer();
    void launch();

};