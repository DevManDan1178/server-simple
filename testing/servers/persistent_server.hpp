#pragma once

#include "network/servers/persistent_server_base.hpp"
#include <atomic>

class test_persistent_server : public persistent_server_base {
public:
    std::atomic<int> processed_packets_count{0};
    std::string last_received_payload;

    test_persistent_server(unsigned short port) 
        : persistent_server_base(port, 1.0f / 60.0f) {}

    // Override update to process incoming client packets
    void update() override {
        persistent_server_base::update(); // Clears queue

        while (!incoming_packets_queue.empty()) {
            incoming_packet packet = incoming_packets_queue.pop_front();
            
            last_received_payload = packet.payload;
            processed_packets_count++;

            // Optional echo response back to the client
            if (packet.session) {
                packet.session->send("Echo: " + packet.payload);
            }
        }
    }

    // Helper to expose broadcast externally for testing
    void test_broadcast(const std::string& msg) {
        broadcast(msg);
    }
};