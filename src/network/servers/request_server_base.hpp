#pragma once

#include "network/servers/server_base.hpp"

class request_server_base : public server_base {
    public:
        request_server_base(unsigned short port) : server_base(port) {}

        virtual ~request_server_base() = default;

    protected:
        virtual void start_accept_async() {
            std::shared_ptr<boost::asio::ip::tcp::socket> socket = std::make_shared<boost::asio::ip::tcp::socket>(asio_context);

            asio_acceptor.async_accept(
                *socket, 
                [this, socket](const boost::system::error_code& ec) {
                    if (ec) {
                        start_accept_async();
                        return;
                        
                    }
                    handle_client(socket);
                    start_accept_async();
                }
            );
        }

        virtual boost::beast::http::response<boost::beast::http::string_body> process_client_request(boost::asio::ip::tcp::socket& socket, boost::beast::http::request<boost::beast::http::string_body> request) = 0;
        

        virtual void send(
            std::shared_ptr<boost::asio::ip::tcp::socket> socket, 
            std::shared_ptr<boost::beast::flat_buffer> buffer,
            const boost::beast::http::response<boost::beast::http::string_body>& response,
            std::function<void()> keep_alive_func =  [](){}
        ) {
            
            // Move response to a heap-allocated shared pointer so it survives the async boundary
            auto res_ptr = std::make_shared<boost::beast::http::response<boost::beast::http::string_body>>(std::move(response));

            bool keep_alive = res_ptr->keep_alive();

            res_ptr->prepare_payload();
            boost::beast::http::async_write(*socket, *res_ptr,
                [this, socket, buffer, res_ptr, keep_alive, keep_alive_func](boost::beast::error_code ec, std::size_t bytes_transferred) {
                    boost::ignore_unused(bytes_transferred);
                    if (ec) {
                        std::cerr << "Write error: " << ec.message() << "\n";
                        return;
                    }

                    if (keep_alive) {
                       keep_alive_func();
                    } else {
                        // Shut down the socket cleanly if keep-alive is false
                        boost::beast::error_code sc;
                        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_send, sc);
                    }
                }
            );
        }

    private:

        virtual void handle_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
            // Allocate a buffer unique to this client connection
            std::shared_ptr<boost::beast::flat_buffer> buffer = std::make_shared<boost::beast::flat_buffer>();
            auto request = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();
            log_debug() << "Handling client";
            boost::beast::http::async_read(
                *socket,
                *buffer, 
                *request,
                [this, socket, buffer, request](boost::beast::error_code ec, std::size_t bytes_transferred) {
                    boost::ignore_unused(bytes_transferred);    
                    if (ec) {
                        if (ec != boost::beast::http::error::end_of_stream) {
                            std::cerr << "[SERVER] HTTP read error: " << ec.message() << "\n";
                        }
                        return;
                    }

                    auto response = process_client_request(*socket, *request);

                   auto res_ptr = std::make_shared<boost::beast::http::response<boost::beast::http::string_body>>(std::move(response));

                    send(
                        socket, 
                        buffer, 
                        *res_ptr,
                        [this, socket]() {
                            handle_client(socket); 
                        }
                    );
                }
            );
        }


};