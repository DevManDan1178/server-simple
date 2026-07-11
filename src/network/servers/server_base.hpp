#pragma once

#include "logger.hpp"
#include "data_structures/thread_safe_queue.hpp"

#include <thread>
#include <string_view>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <string>
#include <utility>


class server_base {
    protected:
        boost::asio::io_context asio_context;
    private:
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard;
    protected:    
        boost::asio::ip::tcp::acceptor asio_acceptor;
        

    public:
        server_base(unsigned short port) : 
            asio_context(),
            work_guard(
                boost::asio::make_work_guard(asio_context)
            ),
            asio_acceptor(
                asio_context, 
                boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::tcp::v4(),
                    port
                )
            ) {
                
        }

        virtual ~server_base() {
            stop();
        };

        virtual void launch() {
            log_debug() << "[SERVER] launching \n";

            start_accept_async();
            asio_context.run(); 
        }

        virtual void stop() {
            asio_context.stop();

            log_debug() << "[SERVER] stopped \n"; 
        }

    protected:
        virtual void start_accept_async() = 0;
        
        virtual void handle_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket) = 0;
};