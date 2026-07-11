#pragma once

#include "network/servers/request_server_base.hpp" 

class echo_server : public request_server_base {
public:
    // Pass port 8080 to the base constructor
    echo_server(unsigned short port) : request_server_base(port) {}

protected:
    // Implement the request processing logic
    virtual boost::beast::http::response<boost::beast::http::string_body> process_client_request(boost::asio::ip::tcp::socket& socket, boost::beast::http::request<boost::beast::http::string_body> request) override {
        log_debug() << "[Server] Received: " << request << "\n";
        
        // Example logic: Turn the request into uppercase and send it back
        std::string response = request.body();
        std::transform(response.begin(), response.end(), response.begin(), ::toupper);
        
        boost::beast::http::response<boost::beast::http::string_body> res;
        res.version(request.version());
        res.result(boost::beast::http::status::ok);
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.body() = "Hello World!\n"; 
        res.prepare_payload();
        return res;
    }
};
