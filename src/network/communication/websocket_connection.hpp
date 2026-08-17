#pragma once
#include "data_structures/thread_safe/thread_safe_queue.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <vector>



struct incoming_packet {
    std::shared_ptr<class websocket_session> session;
    std::string payload;
};

template<>
struct queue_size_traits<incoming_packet> {
    static size_t get(const incoming_packet& packet) noexcept {
        return packet.payload.size();
    }
};

class websocket_session : public std::enable_shared_from_this<websocket_session> {
    private:    

        boost::beast::websocket::stream<boost::asio::ip::tcp::socket> web_socket;
        boost::beast::flat_buffer flat_buffer;
        
        thread_safe_queue<incoming_packet>& packets_queue_in;
        
        std::deque<std::string> write_queue;

    public:
    
        explicit websocket_session(boost::asio::ip::tcp::socket socket, thread_safe_queue<incoming_packet>& incoming_queue)
            : web_socket(std::move(socket)), packets_queue_in(incoming_queue) {}

        void start() {
            auto self = shared_from_this();
            web_socket.async_accept([this, self](boost::beast::error_code ec) {
                if (ec) {
                    std::cerr << "[web_socket] Accept error: " << ec.message() << "\n";
                    return;
                }
                
                read_message_async();
            });
        }

        void send(std::string message) {
            auto self = shared_from_this();
            boost::asio::post(web_socket.get_executor(), [this, self, msg = std::move(message)]() mutable {
                bool write_in_progress = !write_queue.empty();
                write_queue.push_back(std::move(msg));
                
                if (!write_in_progress) {
                    write_message_async();
                }
            });
        }

    private:

        void read_message_async() {
            auto self = shared_from_this();
            web_socket.async_read(flat_buffer, [this, self](boost::beast::error_code ec, std::size_t bytes_transferred) {
                if (ec) {
                    if (ec != boost::beast::websocket::error::closed &&
                        ec != boost::asio::error::eof &&
                        ec != boost::asio::error::operation_aborted &&
                        ec != boost::asio::error::connection_reset) {
                        std::cerr << "[web_socket] Non-disconnect read error: " << ec.message() << "\n";
                    }
                    return; 
                }

                std::string data = boost::beast::buffers_to_string(flat_buffer.data());
                flat_buffer.consume(bytes_transferred);

                push_incoming_packet(std::move(data));
                read_message_async(); // Re-arm read loop
            });
        }

        void push_incoming_packet(std::string payload) {
            incoming_packet packet{
                shared_from_this(), 
                std::move(payload)
            };
            if (!packets_queue_in.try_push_back(std::move(packet))) {
                std::cerr << "Closing session: flooding threshold reached (Backpressure).\n";
                boost::beast::error_code ec;
                web_socket.close(boost::beast::websocket::close_code::normal, ec);
                return;
            }
        }

        void write_message_async() {
            auto self = shared_from_this();
            
            web_socket.text(true); 

            web_socket.async_write(boost::asio::buffer(write_queue.front()),
                [this, self](boost::beast::error_code ec, std::size_t bytes_transferred) {
                    boost::ignore_unused(bytes_transferred);
                    if (ec) {
                        if (ec != boost::asio::error::operation_aborted &&
                            ec != boost::beast::websocket::error::closed &&
                            ec != boost::asio::error::connection_reset) {
                            std::cerr << "[web_socket] Non-disconnect write error: " << ec.message() << "\n";
                        }                
                        return;
                    }

                    write_queue.erase(write_queue.begin());

                    if (!write_queue.empty()) {
                        write_message_async();
                    }
                }
            );
        }
};