#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>

#include "../servers/broadcast_server.hpp"
#include "benchmark_common.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

using tcp = asio::ip::tcp;

namespace {

constexpr int DEFAULT_CONNECTIONS = 8;
constexpr int DEFAULT_DURATION = 10;
constexpr int DEFAULT_WARMUP = 2;

struct benchmark_config {
    std::string host = "127.0.0.1";
    std::string port = "8080";
    int connections = DEFAULT_CONNECTIONS;
    int duration_seconds = DEFAULT_DURATION;
    int warmup_seconds = DEFAULT_WARMUP;
    std::string body = "hello";
};

struct client_result {
    uint64_t sent = 0;
    uint64_t received = 0;
    uint64_t failed = 0;
    latency_stats latency;
};

std::mutex error_mutex;
std::atomic<uint64_t> global_message_id{0};

std::string make_message() {
    const uint64_t message_id = global_message_id.fetch_add(1, std::memory_order_relaxed);
    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    return std::to_string(message_id) + ":" + std::to_string(timestamp) + ":" + "hello";
}

bool parse_message(const std::string& message, uint64_t& message_id, uint64_t& timestamp) {
    const auto first_separator = message.find(':');

    if (first_separator == std::string::npos) {
        return false;
    }

    const auto second_separator = message.find(':', first_separator + 1);

    if (second_separator == std::string::npos) {
        return false;
    }

    try {
        message_id = std::stoull(message.substr(0, first_separator));
        timestamp = std::stoull(message.substr(first_separator + 1, second_separator - first_separator - 1));
        return true;
    } catch (...) {
        return false;
    }
}

void run_client(const benchmark_config& config, std::chrono::steady_clock::time_point start_time, std::chrono::steady_clock::time_point end_time, client_result& result, bool sender, bool collect_latency) {
    try {
        asio::io_context io;
        tcp::resolver resolver(io);
        websocket::stream<tcp::socket> stream(io);

        beast::error_code ec;
        auto endpoints = resolver.resolve(config.host, config.port, ec);

        if (ec) {
            ++result.failed;
            std::lock_guard lock(error_mutex);
            std::cerr << "[Client] Resolve failed: " << ec.message() << '\n';
            return;
        }

        asio::connect(stream.next_layer(), endpoints, ec);

        if (ec) {
            ++result.failed;
            std::lock_guard lock(error_mutex);
            std::cerr << "[Client] Connection failed: " << ec.message() << '\n';
            return;
        }

        stream.handshake(config.host, "/", ec);

        if (ec) {
            ++result.failed;
            std::lock_guard lock(error_mutex);
            std::cerr << "[Client] WebSocket handshake failed: " << ec.message() << '\n';
            return;
        }

        stream.text(true);

        /*
         * Wait until all clients have had a chance to connect before
         * starting the measurement.
         */
        while (std::chrono::steady_clock::now() < start_time) {
            std::this_thread::yield();
        }

        std::atomic<bool> receiving{true};

        /*
         * Every client continuously receives broadcasts.
         *
         * The sender also receives its own broadcast because this is
         * a global broadcast.
         */
        std::thread receiver([&]() {
            beast::flat_buffer buffer;

            while (receiving) {
                buffer.clear();

                beast::error_code read_ec;
                stream.read(buffer, read_ec);

                if (read_ec) {
                    if (receiving &&
                        read_ec != websocket::error::closed &&
                        read_ec != asio::error::operation_aborted &&
                        read_ec != asio::error::eof &&
                        read_ec != asio::error::connection_reset) {

                        ++result.failed;

                        std::lock_guard lock(error_mutex);

                        if (result.failed <= 5) {
                            std::cerr << "[Client] Read failed: " << read_ec.message() << '\n';
                        }
                    }

                    break;
                }

                const std::string message = beast::buffers_to_string(buffer.data());
                uint64_t message_id = 0;
                uint64_t timestamp = 0;

                if (!parse_message(message, message_id, timestamp)) {
                    ++result.failed;

                    if (result.failed <= 5) {
                        std::lock_guard lock(error_mutex);
                        std::cerr << "[Client] Invalid broadcast message\n";
                    }

                    continue;
                }

                (void)message_id;
                ++result.received;

                if (collect_latency) {
                    const auto received_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count();

                    if (received_time >= static_cast<int64_t>(timestamp)) {
                        result.latency.add(static_cast<uint64_t>(
                            received_time - static_cast<int64_t>(timestamp)
                        ));
                    }
                }
            }
        });

        /*
         * Client 0 is the only sender in this benchmark.
         *
         * This gives us a simple global-chat workload:
         *
         *     1 incoming message
         *             |
         *             v
         *     N connected clients
         */
        while (sender && std::chrono::steady_clock::now() < end_time) {
            const std::string message = make_message();
            beast::error_code write_ec;
            stream.write(asio::buffer(message), write_ec);

            if (write_ec) {
                ++result.failed;

                if (result.failed <= 5) {
                    std::lock_guard lock(error_mutex);
                    std::cerr << "[Client] Write failed: " << write_ec.message() << '\n';
                }

                break;
            }

            ++result.sent;
        }

        /*
         * Non-senders remain connected for the full duration so they
         * continue receiving broadcasts.
         */
        if (!sender) {
            while (std::chrono::steady_clock::now() < end_time) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        receiving = false;

       receiving = false;

        beast::error_code cancel_ec;
        stream.next_layer().cancel(cancel_ec);

        if (receiver.joinable()) {
            receiver.join();
        }

        beast::error_code shutdown_ec;
        stream.next_layer().shutdown(tcp::socket::shutdown_both, shutdown_ec);

        beast::error_code close_ec;
        stream.next_layer().close(close_ec);
    } catch (const std::exception& e) {
        ++result.failed;
        std::lock_guard lock(error_mutex);
        std::cerr << "[Client] Exception: " << e.what() << '\n';
    }
}

benchmark_result run_benchmark(const benchmark_config& config, bool collect_latency) {
    broadcast_server server(static_cast<unsigned short>(std::stoi(config.port)));

    std::thread server_thread([&server]() {
        server.launch();
    });

    /*
     * Give the server time to begin accepting connections.
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<std::thread> clients;
    std::vector<client_result> results(static_cast<std::size_t>(config.connections));

    clients.reserve(static_cast<std::size_t>(config.connections));

    /*
     * Give all clients a small connection window.
     *
     * The measurement starts after this point.
     */
    const auto start = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    const auto end_time = start + std::chrono::seconds(config.duration_seconds);

    for (int i = 0; i < config.connections; ++i) {
        clients.emplace_back([&config, start, end_time, &results, i, collect_latency]() {
            const bool sender = i == 0;

            run_client(config, start, end_time, results[static_cast<std::size_t>(i)], sender, collect_latency);
        });
    }

    for (auto& client : clients) {
        client.join();
    }

    const auto end = std::chrono::steady_clock::now();

    server.try_stop();

    if (server_thread.joinable()) {
        server_thread.join();
    }

    benchmark_result result;
    result.elapsed_seconds = std::chrono::duration<double>(end - start).count();

    for (auto& client : results) {
        /*
         * For this benchmark, "completed" means successful broadcast
         * deliveries rather than messages sent.
         */
        result.completed += client.received;
        result.failed += client.failed;

        if (collect_latency) {
            result.latency.merge(std::move(client.latency));
        }
    }

    if (collect_latency) {
        result.latency.sort();
    }

    return result;
}

bool parse_int(const char* value, int& output) {
    try {
        output = std::stoi(value);
        return true;
    } catch (...) {
        return false;
    }
}

void print_usage(const char* program) {
    std::cout << "Usage:\n"
        << "  " << program << " [connections] [duration] [warmup]\n\n"
        << "Example:\n"
        << "  " << program << " 8 10 2\n";
}

} // namespace

int main(int argc, char** argv) {
    benchmark_config config;

    if (argc > 1) {
        if (!parse_int(argv[1], config.connections)) {
            std::cerr << "Invalid connection count.\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (argc > 2) {
        if (!parse_int(argv[2], config.duration_seconds)) {
            std::cerr << "Invalid duration.\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (argc > 3) {
        if (!parse_int(argv[3], config.warmup_seconds)) {
            std::cerr << "Invalid warmup duration.\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (config.connections <= 0 || config.duration_seconds <= 0 || config.warmup_seconds < 0) {
        std::cerr << "Connections and duration must be > 0.\n";
        return 1;
    }

    std::cout << "server-simple Persistent Server Benchmark\n"
        << "===========================================\n"
        << "Host:        " << config.host << '\n'
        << "Port:        " << config.port << '\n'
        << "Connections: " << config.connections << '\n'
        << "Warmup:      " << config.warmup_seconds << "s\n"
        << "Duration:    " << config.duration_seconds << "s\n"
        << '\n';

    /*
     * Warmup
     *
     * This run is deliberately ignored.
     */
    if (config.warmup_seconds > 0) {
        std::cout << "[Benchmark] Warming up...\n";

        benchmark_config warmup_config = config;
        warmup_config.duration_seconds = config.warmup_seconds;

        run_benchmark(warmup_config, false);

        std::cout << "[Benchmark] Warmup complete.\n\n";
    }

    std::cout << "[Benchmark] Running measurement...\n";

    benchmark_result result = run_benchmark(config, true);
    print_result(result);

    /*
     * A single message is broadcast to every connected client.
     *
     * Therefore:
     *
     *     messages sent = result.completed / connections
     *
     * approximately, assuming every broadcast was delivered.
     */
    const double messages_per_second = result.elapsed_seconds > 0.0
        ? static_cast<double>(result.completed) / static_cast<double>(config.connections) / result.elapsed_seconds
        : 0.0;

    std::cout << "\nBroadcast\n"
        << "---------\n"
        << "Connections:     " << config.connections << '\n'
        << "Deliveries:      " << result.completed << '\n'
        << "Deliveries/sec:  " << result.throughput() << '\n'
        << "Messages/sec:    " << messages_per_second << '\n';

    if (result.failed > 0) {
        std::cout << "\nWARNING: benchmark encountered " << result.failed << " failed operations.\n";
        return 2;
    }

    return 0;
}