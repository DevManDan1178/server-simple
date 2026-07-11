#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <string>
#include <memory>

namespace net = boost::asio;
namespace websocket = boost::beast::websocket;
using tcp = net::ip::tcp;

class test_websocket_client {
private:
    net::io_context io_ctx_;
    websocket::stream<tcp::socket> ws_;
    boost::beast::flat_buffer buffer_;

public:
    test_websocket_client() : ws_(io_ctx_) {}

    // Connect and complete the WebSocket Handshake
    bool connect(const std::string& host, unsigned short port) {
        try {
            tcp::resolver resolver(io_ctx_);
            auto const results = resolver.resolve(host, std::to_string(port));

            net::connect(ws_.next_layer(), results.begin(), results.end());
            
            // Perform WebSocket handshake
            ws_.handshake(host + ":" + std::to_string(port), "/");
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    // Send a payload string to the server
    void send(const std::string& message) {
        ws_.write(net::buffer(message));
    }

    // Receive a response payload from the server with a timeout threshold
    std::string receive() {
        buffer_.clear();
        ws_.read(buffer_);
        return boost::beast::buffers_to_string(buffer_.data());
    }

    // Gracefully close the connection
    void disconnect() {
        boost::beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
    }
};