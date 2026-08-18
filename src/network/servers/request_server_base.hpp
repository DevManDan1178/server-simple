#pragma once
#include "network/communication/http_parser.hpp"
#include "network/security/rate_limiter.hpp"
#include "network/servers/server_base.hpp"
#include "network/communication/http_connection.hpp"

constexpr const size_t DEFAULT_WORKER_THREAD_COUNT = 4;
constexpr const size_t DEFAULT_MAX_PENDING_REQUESTS = 64 * 1024;
constexpr const size_t DEFAULT_MAX_REQUEST_BYTES = 64 * 1024 * 1024;

constexpr const double DEFAULT_MAX_IP_RATE_TOKENS = 50;
constexpr const double DEFAULT_IP_TOKEN_REFILL_RATE = 0.5;

class request_server_base : public server_base {
    protected:
        thread_safe_queue<request_task> request_queue;
        std::vector<std::thread> workers;
        rate_limiter ip_rate_limiter;

    public:

        request_server_base(
            unsigned short port, 
            size_t worker_count = DEFAULT_WORKER_THREAD_COUNT, 
            size_t max_pending_requests = DEFAULT_MAX_PENDING_REQUESTS,
            size_t max_request_bytes = DEFAULT_MAX_REQUEST_BYTES,
            double max_ip_rate_tokens = DEFAULT_MAX_IP_RATE_TOKENS, 
            double ip_token_refill_rate = DEFAULT_IP_TOKEN_REFILL_RATE
        )   : 
            server_base(port), 
            request_queue(max_pending_requests, max_request_bytes), 
            ip_rate_limiter(max_ip_rate_tokens, ip_token_refill_rate) {

            for(size_t i = 0; i < worker_count; i++) {
                workers.emplace_back([this](){
                    worker_loop();
                });
            }
        }

        virtual void launch() {
            server_base::launch();
        }

        virtual ~request_server_base() {}


    protected:
        virtual void stop() {
            server_base::stop(); 
            request_queue.stop();

            for(auto& t : workers) {    
                if (t.joinable()) {
                    t.join();
                }   
            }
        }

        void start_accept_async() override {

            auto socket = std::make_shared<boost::asio::ip::tcp::socket>(asio_context);
            
            asio_acceptor.async_accept(
                *socket,
                [this,socket](boost::system::error_code ec) {
                    if(ec) {
                        start_accept_async();
                        return;
                    }

                    std::string client_ip = socket->remote_endpoint().address().to_string();
                    if (!ip_rate_limiter.consume(client_ip)) {
                        boost::system::error_code ignored_ec;
                        socket->close(ignored_ec);
                        start_accept_async();
                        return;
                    }

                    std::make_shared<http_connection>(std::move(*socket), request_queue)->start();
                    start_accept_async();
                }
            );
        }



        virtual boost_http_response process_client_request(const std::string& client_ip, const boost_http_request& request) = 0;

    private:
        void worker_loop(){
            while(auto task = request_queue.wait_and_pop()){
            
                auto response = process_client_request(
                    std::move((*task).client_ip),
                    std::move((*task).request)
                );
                (*task).connection->send_response(std::move(response), (*task).sequence_id);
            }
        }

};