#pragma once
#include "network/servers/server_base.hpp"
#include <unordered_set>
#include <string_view>
#include <string>
#include <algorithm>

constexpr const  float DEFAULT_FIXED_DELTA_TIME = 1.0f;///60.0f;

using boost_http_request = boost::beast::http::request<boost::beast::http::string_body>;

// Server that does not close when the user joins
class persistent_server_base : public server_base {
    protected:
        std::unordered_set<std::shared_ptr<websocket_session>> active_sessions;
        thread_safe_queue<incoming_packet> incoming_packets_queue;
        const float fixed_delta_time;

    public:
        persistent_server_base(unsigned short port, float _fixed_delta_time = DEFAULT_FIXED_DELTA_TIME)
            : server_base(port), fixed_delta_time(std::max(_fixed_delta_time, 0.0f)) {
                if (_fixed_delta_time < 0) {
                    std::cerr << "Attempt to set server delta time to negative number - defaulted to zero (no update)\n";
                }
        }

        virtual void launch() {
            server_base::launch(); 

            if (fixed_delta_time <= 0) {
                return;
            }
            // Calculate frame duration threshold in milliseconds
            const auto frame_duration = std::chrono::duration<float>(fixed_delta_time);

            while (is_running()) {
                auto frame_start = std::chrono::steady_clock::now();

                update();

                // Maintain fixed delta time sleep
                auto frame_end = std::chrono::steady_clock::now();
                auto elapsed = frame_end - frame_start;

                if (elapsed < frame_duration) {
                    std::this_thread::sleep_for(frame_duration - elapsed);
                }
            }
        }

    protected:
        
        virtual void on_client_connected(boost::asio::ip::tcp::socket&) {
            log_debug() << "[SERVER] Client connected";
        }

        void setup_client_connection(boost::asio::ip::tcp::socket& socket) {
            std::shared_ptr<websocket_session> session = std::make_shared<websocket_session>(std::move(socket), incoming_packets_queue);
            active_sessions.insert(session);
            session->start();
            on_client_connected(socket);
        }


        virtual void on_client_disconnected(boost::asio::ip::tcp::socket&) {
            log_debug() << "[SERVER] Client disconnected";
        }
        

        
        void broadcast(const std::string& message) {
            for (auto i_ptr = active_sessions.begin(); i_ptr != active_sessions.end(); ) {
                if (auto session = *i_ptr; session) {
                    session->send(message);
                    ++i_ptr;
                } else {
                    i_ptr = active_sessions.erase(i_ptr);
                }
            }
        }
        
        
        virtual void update() {

        }



        void send(std::shared_ptr<boost::asio::ip::tcp::socket>, 
            std::shared_ptr<boost::beast::flat_buffer>,
            const boost::beast::http::response<boost::beast::http::string_body>&,
            std::function<void()>) { 
        }

        virtual void start_accept_async() override {
            auto socket = std::make_shared<boost::asio::ip::tcp::socket>(asio_context);

            asio_acceptor.async_accept(
                *socket, 
                [this, socket](const boost::system::error_code& ec) {
                    if (!ec) {
                        // Directly set up connection without extra handle_client function
                        setup_client_connection(*socket);
                    }
                    
                    if (is_running()) {
                        start_accept_async();
                    }
                }
            );
        }

    private:
        void remove_session(const std::shared_ptr<websocket_session>& session) {
            active_sessions.erase(session);
        }
};