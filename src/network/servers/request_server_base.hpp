#pragma once

#include "network/servers/server_base.hpp"
#include "network/communication/http_connection.hpp"


class request_server_base : public server_base {

    protected:

        thread_safe_queue<request_task> request_queue;
        std::vector<std::thread> workers;


    public:

        request_server_base(unsigned short port, size_t worker_count = 4) : server_base(port) {

            for(size_t i = 0; i < worker_count; i++) {
                workers.emplace_back(
                    [this]()
                    {
                        worker_loop();
                    }
                );
            }
        }



        virtual ~request_server_base() {
            for(auto& t : workers) {
                if(t.joinable()) {
                    t.join();
                }   
            }
        }



    protected:


        void start_accept_async() override {

            auto socket = std::make_shared<boost::asio::ip::tcp::socket>(asio_context);


            asio_acceptor.async_accept(
                *socket,
                [this,socket](boost::system::error_code ec) {

                    if(!ec) {
                        std::make_shared<http_connection>(std::move(*socket), request_queue)
                        ->start();
                    }

                    start_accept_async();
                }
            );
        }



        virtual boost_http_response process_client_request(const std::string client_ip,const boost_http_request request) = 0;

    private:

        void worker_loop(){
            while(true){
                auto task = request_queue.wait_and_pop();
                auto response = process_client_request(std::move(task.client_ip), std::move(task.request));
                task.connection->send_response(std::move(response));
            }
        }

};