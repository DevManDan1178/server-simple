#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>

#include "benchmark_common.hpp"

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

namespace {

constexpr int DEFAULT_CONNECTIONS = 8;
constexpr int DEFAULT_DURATION = 10;
constexpr int DEFAULT_WARMUP = 2;

struct benchmark_config {
    std::string host = "127.0.0.1";
    std::string port = "8080";
    std::string target = "/";

    int connections = DEFAULT_CONNECTIONS;
    int duration_seconds = DEFAULT_DURATION;
    int warmup_seconds = DEFAULT_WARMUP;

    std::string body = "hello";
};

struct client_result {
    uint64_t completed = 0;
    uint64_t failed = 0;
    latency_stats latency;
};

std::mutex error_mutex;

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    return value;
}

bool run_single_request(
    beast::tcp_stream& stream,
    const benchmark_config& config,
    const std::string& expected,
    latency_stats* latency,
    std::string& error
) {
    http::request<http::string_body> request{http::verb::post, config.target, 11};

    request.set(http::field::host, config.host);
    request.set(http::field::content_type, "text/plain");
    request.keep_alive(true);
    request.body() = config.body;
    request.prepare_payload();

    beast::error_code ec;
    const auto start = std::chrono::steady_clock::now();

    http::write(stream, request, ec);

    if (ec) {
        error = "write failed: " + ec.message();
        return false;
    }

    beast::flat_buffer buffer;
    http::response<http::string_body> response;

    http::read(stream, buffer, response, ec);

    const auto end = std::chrono::steady_clock::now();

    if (ec) {
        error = "read failed: " + ec.message();
        return false;
    }

    if (latency != nullptr) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        latency->add(static_cast<uint64_t>(elapsed.count()));
    }

    if (response.result() != http::status::ok) {
        error = "unexpected HTTP status: " + std::to_string(response.result_int());
        return false;
    }

    if (response.body() != expected) {
        error = "invalid response body";
        return false;
    }

    if (!response.keep_alive()) {
        error = "server closed keep-alive connection";
        return false;
    }

    return true;
}

void run_client(
    const benchmark_config& config,
    std::chrono::steady_clock::time_point end_time,
    client_result& result,
    bool collect_latency
) {
    try {
        asio::io_context io;
        tcp::resolver resolver(io);
        beast::tcp_stream stream(io);
        beast::error_code ec;

        auto endpoints = resolver.resolve(config.host, config.port, ec);

        if (ec) {
            ++result.failed;

            std::lock_guard lock(error_mutex);
            std::cerr << "[Client] Resolve failed: " << ec.message() << '\n';

            return;
        }

        stream.connect(endpoints, ec);

        if (ec) {
            ++result.failed;

            std::lock_guard lock(error_mutex);
            std::cerr << "[Client] Connection failed: " << ec.message() << '\n';

            return;
        }

        const std::string expected = uppercase(config.body);

        while (std::chrono::steady_clock::now() < end_time) {
            std::string error;

            if (run_single_request(stream, config, expected, collect_latency ? &result.latency : nullptr, error)) {
                ++result.completed;
            } else {
                ++result.failed;

                // Don't spam the terminal with thousands of errors.
                if (result.failed <= 5) {
                    std::lock_guard lock(error_mutex);
                    std::cerr << "[Client] " << error << '\n';
                }

                break;
            }
        }

        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    } catch (const std::exception& e) {
        ++result.failed;

        std::lock_guard lock(error_mutex);
        std::cerr << "[Client] Exception: " << e.what() << '\n';
    }
}

benchmark_result run_benchmark(const benchmark_config& config, bool collect_latency) {
    std::vector<std::thread> clients;
    std::vector<client_result> results(static_cast<std::size_t>(config.connections));

    clients.reserve(static_cast<std::size_t>(config.connections));

    const auto start = std::chrono::steady_clock::now();
    const auto end_time = start + std::chrono::seconds(config.duration_seconds);

    for (int i = 0; i < config.connections; ++i) {
        clients.emplace_back([&config, end_time, &results, i, collect_latency]() {
            run_client(config, end_time, results[static_cast<std::size_t>(i)], collect_latency);
        });
    }

    for (auto& client : clients) {
        client.join();
    }

    const auto end = std::chrono::steady_clock::now();

    benchmark_result result;
    result.elapsed_seconds = std::chrono::duration<double>(end - start).count();

    for (auto& client : results) {
        result.completed += client.completed;
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

    std::cout << "server-simple HTTP Benchmark\n"
        << "=============================\n"
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

    if (result.failed > 0) {
        std::cout << "\nWARNING: benchmark encountered " << result.failed << " failed requests.\n";
        return 2;
    }

    return 0;
}