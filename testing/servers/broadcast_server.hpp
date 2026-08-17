#pragma once

#include "network/servers/persistent_server_base.hpp"
#include <atomic>

class broadcast_server : public persistent_server_base {
    public:
        std::atomic<int> processed_packets_count{0};
        std::string last_received_payload;

        broadcast_server(unsigned short port)
            : persistent_server_base(port, 1.0f / 60.0f) {}

        

        void update() override {
            while (!incoming_packets_queue.empty()) {
                incoming_packet packet = incoming_packets_queue.pop_front();

                last_received_payload = packet.payload;
                processed_packets_count++;
                
                broadcast(packet.payload);
            }
        }

        void test_broadcast(const std::string& msg) {
            broadcast(msg);
        }

    protected:
        virtual void on_client_disconnected(boost::asio::ip::tcp::socket&) {
            return;
        }

        virtual void on_client_connected(boost::asio::ip::tcp::socket&) {
            return;
        }
};