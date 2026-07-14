#pragma once
#include "data_structures/thread_safe/thread_safe_queue.hpp"
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <deque>


using boost_http_request =
    boost::beast::http::request<boost::beast::http::string_body>;

using boost_http_response =
    boost::beast::http::response<boost::beast::http::string_body>;

class http_connection;
struct request_task {
    std::shared_ptr<http_connection> connection;
    boost_http_request request;
    std::string client_ip;
};

class http_connection : public std::enable_shared_from_this<http_connection> {
private:

    boost::asio::ip::tcp::socket socket;

    boost::beast::flat_buffer buffer;

    boost_http_request request;


    thread_safe_queue<request_task>& request_queue;


    std::deque<std::shared_ptr<boost_http_response>> responses;

    bool writing = false;


public:

    http_connection(
        boost::asio::ip::tcp::socket socket, 
        thread_safe_queue<request_task>& queue 
    ) : socket(std::move(socket)), request_queue(queue) {}


    void start() {
        read();
    }

    std::string get_client_ip() const {
        return socket.remote_endpoint()
            .address()
            .to_string();
    }

    void send_response(boost_http_response response) {
        auto self = shared_from_this();

        boost::asio::post(
            socket.get_executor(),
            [this, self, response = std::move(response)]() mutable {
                responses.push_back(
                    std::make_shared<boost_http_response>(
                        std::move(response)
                    )
                );

                if (!writing) {
                    write();
                }
            }
        );
    }



private:

    void read() {
        auto self = shared_from_this();

        boost::beast::http::async_read(
            socket,
            buffer,
            request,

            [this,self](boost::beast::error_code ec, std::size_t bytes) {
                boost::ignore_unused(bytes);
                if(ec) {
                    close();
                    return;
                }

                request_queue.push_back(
                    request_task{
                        self,
                        std::move(request),
                        get_client_ip()
                    }
                );

                request.clear();
                read();
            }
        );
    }



    void write() {
        if(responses.empty()) {
            writing = false;
            return;
        }


        writing = true;

        auto self = shared_from_this();

        auto response = responses.front();

        response->prepare_payload();

        boost::beast::http::async_write(
            socket,
            *response,
            [this,self,response](boost::beast::error_code ec, std::size_t bytes) {
                boost::ignore_unused(bytes);
                if(ec) {
                    close();
                    return;
                }

                responses.pop_front();

                if(response->keep_alive()) {
                    write();
                    return;
                }
                close();
            }
        );
    }



    void close() {
        boost::beast::error_code ec;

        socket.shutdown(
            boost::asio::ip::tcp::socket::shutdown_both,
            ec
        );

        socket.close(ec);
    }
};


