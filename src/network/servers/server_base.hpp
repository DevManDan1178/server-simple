#pragma once

#include "logger.hpp"
#include "data_structures/thread_safe_queue.hpp"
#include "network/communication/websocket_connection.hpp"
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
        std::thread context_thread;

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
            context_thread = std::thread([this]() {
                asio_context.run(); 
            });
        }

        void join_context_thread() {
            if (context_thread.joinable()) {
                context_thread.join();
            }
        }

        virtual void stop() {
            asio_context.stop();
            join_context_thread();
            log_debug() << "[SERVER] stopped \n"; 
        }

        bool is_running() {
            return !asio_context.stopped();
        }

     protected:
        virtual void start_accept_async() = 0;
     
};