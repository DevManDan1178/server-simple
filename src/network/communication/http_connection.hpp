#pragma once
#include "data_structures/thread_safe/thread_safe_queue.hpp"
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include "network/communication/http_parser.hpp"
#include <memory>
#include <map>


using boost_http_request =
    boost::beast::http::request<boost::beast::http::string_body>;

using boost_http_response =
    boost::beast::http::response<boost::beast::http::string_body>;

using sequence_key = uint64_t;

class http_connection;
struct request_task {
    std::shared_ptr<http_connection> connection;
    boost_http_request request;
    std::string client_ip;
    sequence_key sequence_id;
};

    class http_connection : public std::enable_shared_from_this<http_connection> {
    private:
        boost::asio::ip::tcp::socket socket;
        boost::beast::flat_buffer buffer;
        boost::asio::strand<boost::asio::any_io_executor> strand;
        boost_http_request request;
        thread_safe_queue<request_task>& request_queue;
        std::map<sequence_key, std::shared_ptr<boost_http_response>> responses;
        

        bool writing = false;
        sequence_key next_sequence_id = 0;
        sequence_key next_response_sequence_id{next_sequence_id};

    public:

        http_connection(
            boost::asio::ip::tcp::socket socket_, 
            thread_safe_queue<request_task>& queue 
        ) : socket(std::move(socket_)), strand(socket.get_executor()), request_queue(queue) {}


        void start() {
            read();
        }

        std::string get_client_ip() const {
            return socket.remote_endpoint()
                .address()
                .to_string();
        }

        void send_response(boost_http_response response, sequence_key sequence_id) {
            auto self = shared_from_this();

            boost::asio::post(
                strand,
                [this, self, sequence_id, response = std::move(response)]() mutable {
                    responses.emplace(
                        sequence_id,
                        std::make_shared<boost_http_response>(std::move(response))
                    );

                    if (!writing) {
                        write();
                    }
                }
            );
        }



    private:
        void reject_overloaded(sequence_key sequence_id, unsigned int request_version) {
            auto self = shared_from_this();

            boost::asio::post(
                strand,
                [this, sequence_id, request_version, self]() {
                    boost_http_response response;
                    response.version(request_version);
                    http_parser::set_response_server_overloaded(response);
                    
                    response.prepare_payload();
                    response.keep_alive(false);
                    
                    responses.emplace(
                        sequence_id,
                        std::make_shared<boost_http_response>(std::move(response))
                    );

                    if (!writing) {
                        write();
                    }
                }
            );
        }

        void read() {
            auto self = shared_from_this();

            boost::beast::http::async_read(
                socket,
                buffer,
                request,
                boost::asio::bind_executor(
                    strand,
                    [this,self](boost::beast::error_code ec, std::size_t bytes) {
                        boost::ignore_unused(bytes);
                        if(ec) {
                            close();
                            return;
                        }

                        sequence_key sequence_id = next_sequence_id++;
                        request_task task {
                            self,
                            std::move(request),
                            get_client_ip(),
                            sequence_id,    
                        };
                        
                        if (!request_queue.try_push_back(std::move(task))) {
                            auto request_version = request.version();
                            reject_overloaded(sequence_id, request_version);
                            return;
                        }

                        request.clear();
                        read();
                    }
                )
            );
        }



        void write() {
            auto it = responses.find(next_response_sequence_id);

            if (it == responses.end()) {
                writing = false;
                return;
            }

            writing = true;

            auto self = shared_from_this();

            auto response = it->second;

            response->prepare_payload();

            boost::beast::http::async_write(
                socket,
                *response,
                boost::asio::bind_executor(
                    strand,
                    [this,self,response](boost::beast::error_code ec, std::size_t bytes) {
                        boost::ignore_unused(bytes);
                        if(ec) {
                            close();
                            return;
                        }

                        responses.erase(next_response_sequence_id++);

                        if(response->keep_alive()) {
                            write();
                            return;
                        }
                        close();
                    }
                )
            );
        }



        void close() {
            boost::beast::error_code ec;
            socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket.close(ec);
        }
};


