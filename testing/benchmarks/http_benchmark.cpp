#include <algorithm>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

#include "../servers/echo_server.hpp"
#include "benchmark_common.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using tcp = asio::ip::tcp;

namespace {

constexpr int DEFAULT_CONNECTIONS = 8;
constexpr int DEFAULT_DURATION = 10;
constexpr int STARTUP_TIMEOUT_SECONDS = 10;

struct benchmark_config {
  std::string host = "127.0.0.1";
  std::string port = "8080";
  std::string target = "/";

  int connections = DEFAULT_CONNECTIONS;
  int duration_seconds = DEFAULT_DURATION;

  std::string body = "hello";
};

struct client_result {
  uint64_t completed = 0;
  uint64_t failed = 0;

  latency_stats latency;
};

struct startup_state {
  std::mutex mutex;
  std::condition_variable cv;

  int ready_clients = 0;

  bool failed = false;
  bool start_measurement = false;
};

std::mutex error_mutex;

std::string uppercase(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

  return value;
}

bool run_single_request(
  beast::tcp_stream& stream,
  const benchmark_config& config,
  const std::string& expected, latency_stats* latency,
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
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
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

void report_startup_failure(startup_state& startup) {
  {
    std::lock_guard lock(startup.mutex);
    startup.failed = true;
  }

  startup.cv.notify_all();
}

void run_client(
  const benchmark_config& config, startup_state& startup,
  const std::chrono::steady_clock::time_point& start_time,
  const std::chrono::steady_clock::time_point& end_time,
  client_result& result, bool collect_latency) {
  try {
    asio::io_context io;
    tcp::resolver resolver(io);
    beast::tcp_stream stream(io);

    beast::error_code ec;
    auto endpoints = resolver.resolve(config.host, config.port, ec);

    if (ec) {
      ++result.failed;

      {
        std::lock_guard lock(error_mutex);
        std::cerr << "[Client] Resolve failed: " << ec.message() << '\n';
      }

      report_startup_failure(startup);
      return;
    }

    stream.connect(endpoints, ec);

    if (ec) {
      ++result.failed;

      {
        std::lock_guard lock(error_mutex);
        std::cerr << "[Client] Connection failed: " << ec.message() << '\n';
      }

      report_startup_failure(startup);
      return;
    }

    {
      std::lock_guard lock(startup.mutex);
      ++startup.ready_clients;
    }

    startup.cv.notify_all();

    {
      std::unique_lock lock(startup.mutex);

      startup.cv.wait(lock, [&startup]() {
        return startup.start_measurement || startup.failed;
      });
    }

    bool startup_failed = false;

    {
      std::lock_guard lock(startup.mutex);
      startup_failed = startup.failed;
    }

    if (startup_failed) {
      beast::error_code shutdown_ec;
      stream.socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
      return;
    }

    while (std::chrono::steady_clock::now() < start_time) {
      std::this_thread::yield();
    }

    const std::string expected = uppercase(config.body);

    while (std::chrono::steady_clock::now() < end_time) {
      std::string error;

      const bool success = run_single_request(
        stream, config, 
        expected, 
        collect_latency ? &result.latency : nullptr,
        error
      );

      if (success) {
        ++result.completed;
      } else {
        ++result.failed;

        if (result.failed <= 5) {
          std::lock_guard lock(error_mutex);
          std::cerr << "[Client] " << error << '\n';
        }

        break;
      }
    }

    beast::error_code shutdown_ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
  } catch (const std::exception& e) {
    ++result.failed;

    {
      std::lock_guard lock(error_mutex);
      std::cerr << "[Client] Exception: " << e.what() << '\n';
    }

    report_startup_failure(startup);
  }
}

benchmark_result run_benchmark(
  const benchmark_config& config,
  bool collect_latency
) {
  startup_state startup;

  std::vector<std::thread> clients;
  std::vector<client_result> results(static_cast<std::size_t>(config.connections));

  clients.reserve(static_cast<std::size_t>(config.connections));

  const auto start_time = std::chrono::steady_clock::now();
  const auto end_time = start_time + std::chrono::seconds(config.duration_seconds);

  for (size_t i = 0; i < static_cast<size_t>(config.connections); ++i) {
    clients.emplace_back([
      &config, 
      &startup, 
      &start_time, 
      &end_time, 
      &results, 
      i,
      &collect_latency
    ]() {
      run_client(
        config, 
        startup, 
        start_time, 
        end_time,
        results[i], 
        collect_latency
      );
    });
  }

  bool startup_successful = false;

  {
    std::unique_lock lock(startup.mutex);

    startup_successful = startup.cv.wait_for(
        lock, 
        std::chrono::seconds(STARTUP_TIMEOUT_SECONDS),
        [&startup, &config]() {
          return startup.ready_clients == config.connections || startup.failed;
        });

    if (!startup_successful) {
      startup.failed = true;

      std::cerr << "[Benchmark] Timed out waiting for " << config.connections
                << " clients. Only " << startup.ready_clients
                << " connected.\n";
    }

    if (startup.failed) {
      startup_successful = false;
    } else {
      startup.start_measurement = true;
    }
  }

  startup.cv.notify_all();

  for (auto& client : clients) {
    if (client.joinable()) {
      client.join();
    }
  }

  const auto end = std::chrono::steady_clock::now();

  benchmark_result result;

  if (startup_successful) {
    result.elapsed_seconds = std::chrono::duration<double>(end - start_time).count();
  } else {
    result.elapsed_seconds = 0.0;
  }

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

  if (!startup_successful && result.failed == 0) {
    result.failed = 1;
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
            << "  " << program << " [connections] [duration]\n\n"
            << "Example:\n"
            << "  " << program << " 8 10\n";
}

}  // namespace

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

  if (config.connections <= 0 || config.duration_seconds <= 0) {
    std::cerr << "Connections and duration must be > 0.\n";
    return 1;
  }

  std::cout << "server-simple HTTP Benchmark\n"
            << "=============================\n"
            << "Host:        " << config.host << '\n'
            << "Port:        " << config.port << '\n'
            << "Connections: " << config.connections << '\n'
            << "Duration:    " << config.duration_seconds << "s\n"
            << '\n';

  echo_server server(static_cast<unsigned short>(std::stoi(config.port)));

  std::binary_semaphore started_signal(0);
  std::thread server_thread([&server, &started_signal]() {
    server.launch();
    started_signal.release();
  });

  started_signal.acquire();

  std::cout << "[Benchmark] Starting...\n";

  benchmark_result result = run_benchmark(config, true);

  print_result(result);

  std::cout << "[Benchmark] Completed.\n";

  server.try_stop();

  if (server_thread.joinable()) {
    server_thread.join();
  }

  if (result.failed > 0) {
    std::cout << "\nWARNING: benchmark encountered " << result.failed
              << " failed requests.\n";

    return 2;
  }

  return 0;
}