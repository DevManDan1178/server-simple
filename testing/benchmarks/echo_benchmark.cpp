#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>

#include "testing/echo_server.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using tcp = asio::ip::tcp;

constexpr uint8_t DEFAULT_CONNECTIONS_COUNT = 16;
constexpr uint8_t DEFAULT_DURATION = 2;

struct benchmark_config {
    std::string host = "127.0.0.1";
    std::string port = "8080";
    std::string target = "/";

    int connections = DEFAULT_CONNECTIONS_COUNT;
    int duration_seconds = DEFAULT_DURATION;

    std::string body = "hello";
};

struct benchmark_stats {
    std::atomic<uint64_t> completed_requests{0};
    std::atomic<uint64_t> failed_requests{0};
};

std::mutex error_mutex;

std::string uppercase(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        }
    );

    return value;
}

void run_client(const benchmark_config& config, benchmark_stats& stats, std::chrono::steady_clock::time_point end_time) {
    try {
        asio::io_context io;
        tcp::resolver resolver(io);
        beast::tcp_stream stream(io);

        beast::error_code ec;

        auto endpoints = resolver.resolve(config.host, config.port, ec);

        if (ec) {
            ++stats.failed_requests;

            std::lock_guard lock(error_mutex);
            std::cerr << "[Client] Resolve failed: " << ec.message() << '\n';

            return;
        }

        stream.connect(endpoints, ec);

        if (ec) {
            ++stats.failed_requests;

            std::lock_guard lock(error_mutex);
            std::cerr << "[Client] Connection failed: " << ec.message() << " (" << ec.value() << ")\n";

            return;
        }

        while (std::chrono::steady_clock::now() < end_time) {
            http::request<http::string_body> request{
                http::verb::post,
                config.target,
                11
            };

            request.set(http::field::host, config.host);
            request.set(http::field::content_type, "text/plain");
            request.keep_alive(true);
            request.body() = config.body;
            request.prepare_payload();

            ec.clear();
            http::write(stream, request, ec);

            if (ec) {
                ++stats.failed_requests;

                std::lock_guard lock(error_mutex);
                std::cerr << "[Client] Write failed: " << ec.message() << " (" << ec.value() << ")\n";
                break;
            }

            beast::flat_buffer buffer;
            http::response<http::string_body> response;

            ec.clear();
            http::read(stream, buffer, response, ec);

            if (ec) {
                ++stats.failed_requests;

                std::lock_guard lock(error_mutex);
                std::cerr << "[Client] Read failed: " << ec.message() << " (" << ec.value() << ")\n";
                break;
            }

            if (response.result() != http::status::ok) {
                ++stats.failed_requests;

                std::lock_guard lock(error_mutex);
                std::cerr << "[Client] Unexpected HTTP status: " << response.result_int() << '\n';
                continue;
            }

            const std::string expected = uppercase(config.body);
            if (response.body() != expected) {
                ++stats.failed_requests;

                std::lock_guard lock(error_mutex);
                std::cerr << "[Client] Invalid response body.\n"
                          << "Expected: " << expected << "\n"
                          << "Received: " << response.body() << '\n';

                continue;
            }

            ++stats.completed_requests;

            if (!response.keep_alive()) {
                break;
            }
        }
        ec.clear();
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    }
    catch (const std::exception& e) {
        ++stats.failed_requests;
        std::lock_guard lock(error_mutex);
        std::cerr << "[Client] Exception: " << e.what() << '\n';
    }
}

// Usage: echo_benchmark [connections] [duration]
int main(int argc, char** argv)
{
    benchmark_config config;

    
    try {
        if (argc > 1) {
            config.connections = std::stoi(argv[1]);
        }

        if (argc > 2) {
            config.duration_seconds = std::stoi(argv[2]);
        }
    } catch (const std::exception& e) {
        std::cerr << "Invalid command-line argument: " << e.what() << '\n';
        return 1;
    }

    if (config.connections <= 0) {
        std::cerr << "Connections must be greater than 0.\n";
        return 1;
    }

    if (config.duration_seconds <= 0) {
        std::cerr << "Duration must be greater than 0.\n";
        return 1;
    }

    std::cout
        << "HTTP Echo Server Benchmark\n"
        << "==========================\n"
        << "Host:        " << config.host << '\n'
        << "Port:        " << config.port << '\n'
        << "Target:      " << config.target << '\n'
        << "Connections: " << config.connections << '\n'
        << "Duration:    " << config.duration_seconds << "s\n"
        << "Body:        " << config.body << '\n'
        << '\n';

    std::cout << "[Benchmark] Starting server...\n";

    echo_server server(8080);

    std::thread server_thread([&server]() {
        server.launch();
    });

    // TODO: Replace this with a proper server readiness signal.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "[Benchmark] Server started.\n" << '\n';

    benchmark_stats stats;

    const auto start = std::chrono::steady_clock::now();
    const auto end_time = start + std::chrono::seconds(config.duration_seconds);

    std::vector<std::thread> clients;
    clients.reserve(config.connections);

    for (int i = 0; i < config.connections; ++i) {
        clients.emplace_back([&config, &stats, end_time]() {
            run_client(config, stats, end_time);
        });
    }

    for (auto& client : clients) {
        client.join();
    }

    const auto end = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(end - start).count();

    std::cout << '\n' << "[Benchmark] Stopping server...\n";

    server.try_stop();

    if (server_thread.joinable()) {
        server_thread.join();
    }

    const uint64_t completed = stats.completed_requests.load();
    const uint64_t failed = stats.failed_requests.load();
    const double requests_per_second = static_cast<double>(completed) / elapsed;

    std::cout << '\n'
        << "Results\n"
        << "=======\n"
        << "Completed requests: " << completed << '\n'
        << "Failed requests:    " << failed << '\n'
        << "Elapsed time:       " << elapsed << "s\n"
        << "Requests/sec:       " << requests_per_second << '\n';

    return 0;
}