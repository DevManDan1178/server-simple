# server-simple

**Lightweight C++20 server infrastructure for asynchronous HTTP and WebSocket applications.**

`server-simple` is a **header-only C++20 library** built on [Boost.Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html) and [Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/).

It provides the infrastructure around application logic: asynchronous networking, connection management, bounded work queues, backpressure, worker threads, rate limiting, persistent data structures, and real-time update loops.

The goal is to provide useful server infrastructure without forcing an application into a large framework.

The library is intended to be composed with application-specific code rather than replace it.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/build-CMake-064F8C.svg)](https://cmake.org/)
[![Boost.Asio](https://img.shields.io/badge/networking-Boost.Asio-00599C.svg)](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
[![Boost.Beast](https://img.shields.io/badge/HTTP%2FWebSocket-Boost.Beast-00599C.svg)](https://www.boost.org/doc/libs/release/libs/beast/)

---

## What is server-simple?

`server-simple` sits between an application's business logic and the network.

For HTTP applications, the library handles:

* TCP connection acceptance
* Asynchronous HTTP reads and writes
* Request queueing
* Worker-thread execution
* Per-IP rate limiting
* Bounded resource usage
* Backpressure when the request queue is full
* Response ordering for pipelined/concurrent requests
* Connection lifetime management

For persistent applications, it provides:

* WebSocket sessions
* Incoming packet queues
* Bounded packet buffering
* Backpressure for overloaded sessions
* Broadcasting
* Connection callbacks
* Fixed-timestep update loops

It also contains reusable thread-safe containers and JSON-backed data structures that can be used independently of the networking layer.

---

## Features

### Networking

* **Asynchronous HTTP** with Boost.Asio and Boost.Beast
* **WebSocket sessions** for persistent connections
* **Connection lifecycle management**
* **Ordered HTTP responses** even when requests are processed concurrently
* **Per-IP token-bucket rate limiting**
* **Configurable worker-thread pools**

### Concurrency

* **Bounded producer/consumer queues**
* Element-count limits
* Byte-capacity limits
* Blocking and non-blocking queue operations
* Move-aware insertion
* Queue shutdown with waiting-thread wake-up
* Thread-safe unordered maps
* Locked-value utility

### Backpressure

Resource limits are part of the design rather than an afterthought.

HTTP requests can be rejected when the request queue reaches its configured element or byte limit.

WebSocket sessions can be closed when incoming packets exceed the configured queue capacity.

This makes overload behaviour explicit instead of allowing work to accumulate indefinitely.

### Persistent data structures

* Ranked `leaderboard<T>`
* Chronological `score_stream<T>`
* Chronological `nameboard`
* JSON persistence
* Filesystem path validation

### Real-time server infrastructure

* Persistent WebSocket sessions
* Fixed-update loops
* Configurable update interval
* Incoming packet queues
* Broadcasting to active sessions
* Client connection/disconnection hooks

### Development

* CMake integration
* Header-only library
* GoogleTest-based tests
* HTTP benchmark
* WebSocket/socket benchmark
* Thread-safe queue benchmark
* Compiler warnings enabled by default

---

# Architecture

The project is intentionally split into small components.

```text
server-simple
│
├── Network
│   ├── Server lifecycle
│   ├── HTTP connections
│   ├── HTTP parsing
│   ├── WebSocket sessions
│   └── Rate limiting
│
├── Concurrency
│   ├── Bounded queues
│   ├── Thread-safe maps
│   └── Locked values
│
├── Persistent data
│   ├── Leaderboards
│   ├── Score streams
│   └── Nameboards
│
└── Storage
    ├── JSON persistence helpers
    └── Filesystem validation
```

The source tree currently looks like:

```text
src/
├── data_structures/
│   ├── global_boards/
│   │   ├── entry.hpp
│   │   ├── leaderboard.hpp
│   │   ├── nameboard.hpp
│   │   └── score_stream.hpp
│   │
│   └── thread_safe/
│       ├── locked_value.hpp
│       ├── thread_safe_queue.hpp
│       └── thread_safe_unordered_map.hpp
│
├── network/
│   ├── communication/
│   │   ├── http_connection.hpp
│   │   ├── http_parser.hpp
│   │   └── websocket_connection.hpp
│   │
│   ├── security/
│   │   └── rate_limiter.hpp
│   │
│   └── servers/
│       ├── server_base.hpp
│       ├── request_server_base.hpp
│       └── persistent_server_base.hpp
│
├── storage/
│   └── file_helper.hpp
│
└── logger.hpp
```

---

# Server hierarchy

The networking API is built around three server base classes.

```text
                 server_base
                     │
          ┌──────────┴──────────┐
          │                     │
request_server_base     persistent_server_base
          │                     │
     HTTP requests         WebSocket sessions
          │                     │
     worker pool           fixed update loop
          │                     │
     bounded queue           packet queue
```

## `server_base`

`server_base` provides the common server lifecycle:

* Boost.Asio `io_context`
* TCP acceptor
* asynchronous connection acceptance
* server launch/stop handling
* context thread management

It is the lowest-level server abstraction and is intended to be extended by higher-level server types.

---

## `request_server_base`

`request_server_base` is the HTTP-oriented server abstraction.

It adds:

* A bounded request queue
* Configurable worker threads
* Per-IP rate limiting
* HTTP request processing
* Overload responses
* Ordered response delivery

Applications derive from this class and implement:

```cpp
process_client_request()
```

The application therefore controls request behaviour without having to manage sockets, asynchronous reads, worker threads, or response ordering itself.

---

## `persistent_server_base`

`persistent_server_base` is intended for applications with long-lived WebSocket connections.

It provides:

* Persistent WebSocket sessions
* Active-session management
* Incoming packet queue
* Broadcasting
* Connection callbacks
* Disconnect callbacks
* Fixed-update loop
* Configurable packet and byte limits

The application can override:

```cpp
update()
```

and process incoming packets as part of its own application loop.

This makes the abstraction useful for applications such as:

* Multiplayer servers
* Collaborative applications
* Simulations
* Real-time state synchronization
* Persistent-session services

---

# HTTP request processing

HTTP networking and application processing are deliberately separated.

A request follows this path:

```text
                    TCP connection
                          │
                          ▼
                  Boost.Beast HTTP
                          │
                          ▼
                 ┌─────────────────┐
                 │  Request Queue  │
                 │                 │
                 │ bounded by:     │
                 │ • request count │
                 │ • request bytes │
                 └────────┬────────┘
                          │
                 ┌────────┴────────┐
                 │                 │
                 ▼                 ▼
              Worker            Worker
                 │                 │
                 └────────┬────────┘
                          │
                          ▼
             process_client_request()
                          │
                          ▼
                  response + ID
                          │
                          ▼
                  ordered write
                          │
                          ▼
                    TCP client
```

Multiple requests can be processed concurrently by the worker pool.

Each request receives a sequence number when it is read. Responses are stored until the next expected sequence number is available, ensuring that responses are written to the client in request order.

This allows application processing to be concurrent without sacrificing HTTP response ordering.

---

# Backpressure

A central design principle of `server-simple` is that queues should have explicit limits.

The built-in queue can enforce both:

1. **Maximum number of elements**
2. **Maximum estimated byte size**

For example:

```cpp
thread_safe_queue<request_task> queue(
    64 * 1024,       // maximum elements
    64 * 1024 * 1024 // maximum estimated bytes
);
```

When a producer cannot insert an item because the queue is full, `try_push_back()` returns `false`.

For HTTP requests, this results in an overload response rather than allowing the queue to grow without bound.

For WebSocket incoming packets, exceeding the configured queue capacity causes the session to be closed.

```text
                    Incoming work
                         │
                         ▼
                 ┌──────────────┐
                 │ Bounded Queue│
                 └──────┬───────┘
                        │
              ┌─────────┴─────────┐
              │                   │
       capacity available   capacity exceeded
              │                   │
              ▼                   ▼
          process              reject
```

This behaviour is particularly useful for servers that need predictable memory usage under load.

---

# WebSockets

WebSocket connections are represented by `websocket_session`.

A session provides:

* Asynchronous WebSocket handshake
* Asynchronous reads
* Asynchronous writes
* Incoming packet delivery
* Per-session outgoing write queue
* Backpressure on incoming packets

Incoming messages are converted into:

```cpp
struct incoming_packet {
    std::shared_ptr<websocket_session> session;
    std::string payload;
};
```

and placed into the server's bounded incoming-packet queue.

A persistent server can then process packets independently of the networking callbacks.

```text
             WebSocket clients
               │     │     │
               ▼     ▼     ▼
          ┌────────────────────┐
          │ WebSocket sessions │
          └──────────┬─────────┘
                     │
                     │ incoming_packet
                     ▼
              ┌──────────────┐
              │ Bounded queue│
              └──────┬───────┘
                     │
                     ▼
                update()
                     │
                     ▼
              application state
                     │
                     ▼
                 broadcast()
                     │
              ┌──────┼──────┐
              ▼      ▼      ▼
           Client  Client  Client
```

---

# Fixed-update servers

`persistent_server_base` can run application updates at a configured fixed interval.

For example:

```cpp
class game_server : public persistent_server_base {
public:
    game_server(unsigned short port)
        : persistent_server_base(port, 1.0f / 60.0f) {}

protected:
    void update() override {
        // Process packets.
        // Update application state.
        // Broadcast state changes.
    }
};
```

A `1.0f / 60.0f` update interval corresponds to approximately 60 updates per second.

The update loop measures the time spent in `update()` and only sleeps for the remaining portion of the configured interval.

---

# Rate limiting

`rate_limiter` implements a thread-safe token-bucket rate limiter.

Each identifier maintains its own bucket.

The limiter supports:

* Maximum token/burst capacity
* Token refill rate
* Variable request cost
* Inactivity cleanup
* Thread-safe access

The HTTP server uses the client's IP address as the identifier.

The default request-server configuration is:

| Setting                      |          Default |
| ---------------------------- | ---------------: |
| Worker threads               |              `4` |
| Maximum pending requests     |         `65,536` |
| Maximum queued request bytes |         `64 MiB` |
| Maximum IP tokens            |             `50` |
| IP token refill rate         | `0.5` tokens/sec |
| Rate-limit cleanup           |   `120` sec idle |

These values are configurable through the `request_server_base` constructor.

For example:

```cpp
request_server_base(
    port,
    worker_count,
    max_pending_requests,
    max_request_bytes,
    max_ip_rate_tokens,
    ip_token_refill_rate
);
```

The rate limiter is intended as lightweight application-level protection, not as a replacement for a firewall, reverse proxy, DDoS protection, or production traffic-management layer.

---

# Thread-safe queues

`thread_safe_queue<T>` is a bounded producer/consumer queue designed for concurrent server workloads.

It supports:

* Maximum element count
* Maximum byte count
* Customizable size accounting
* `try_push_back()`
* `try_push_front()`
* `push_back()`
* `push_front()`
* `pop_front()`
* `pop_back()`
* `wait_and_pop()`
* `clear()`
* `replace()`
* `to_vector()`
* Queue shutdown

Example:

```cpp
thread_safe_queue<std::string> queue(
    1000,
    1024 * 1024
);

if (queue.try_push_back("hello")) {
    // Accepted.
}

if (auto value = queue.wait_and_pop()) {
    std::cout << *value << '\n';
}
```

The default byte accounting uses:

```cpp
sizeof(T)
```

but applications can specialize `queue_size_traits<T>` when the meaningful size of an object is different.

For example, network request and packet queues account for their payload sizes rather than just the size of the containing C++ object.

Queue shutdown wakes waiting consumers:

```cpp
queue.stop();
```

After shutdown, `wait_and_pop()` returns an empty `std::optional` once no queued item remains to be consumed.

---

# Persistent data structures

The library also contains several small, application-independent data structures designed for persistent server state.

## Leaderboard

`leaderboard<T>` stores named scores in ranked order.

It provides:

* Unique player/name identifiers
* Score submission and updates
* Configurable maximum size
* Ranking queries
* Top/bottom queries
* Range queries
* JSON persistence

Example:

```cpp
leaderboard<int> scores(
    "scores.json",
    100
);

scores.submit_score("player1", 1250);
scores.submit_score("player2", 980);
scores.submit_score("player3", 1430);
```

The implementation keeps ownership of entries separately from the ranking structure, allowing player lookup and ranking to use different data structures.

---

## Score stream

`score_stream<T>` is an append-oriented, bounded history of scores.

Unlike a leaderboard, names do **not** need to be unique.

New entries are placed at the front of the stream.

This makes it useful for:

* Recent scores
* Match results
* Event histories
* Activity feeds
* Chronological server events

Example:

```cpp
score_stream<int> recent_scores(
    "recent_scores.json",
    300
);

recent_scores.submit_score("player1", 1200);
recent_scores.submit_score("player1", 1500);
recent_scores.submit_score("player2", 900);
```

Queries are available from either the newest or oldest side of the bounded history.

---

## Nameboard

`nameboard` stores unique names in chronological order.

It supports:

* Adding names
* Existence checks
* Ranking queries
* First/last entry queries
* Configurable maximum size
* JSON persistence

This is useful when an application needs a chronological list of unique names rather than a score-based ranking.

---

# Persistence

The persistent data structures use JSON files through [nlohmann/json](https://github.com/nlohmann/json).

Storage helpers provide filesystem-backed paths while rejecting filenames containing path traversal components.

For example:

```cpp
file_helper::get_file_path(
    storage_directory,
    "leaderboards",
    "global"
);
```

The helper ensures the target directory exists and adds the `.json` extension when necessary.

Persistence is intentionally simple: the data structures load their state from JSON when constructed and expose save functionality where applicable.

They are intended for lightweight application state rather than as a replacement for a database.

---

# Quick start

## Requirements

* C++20-compatible compiler
* CMake 3.16 or newer
* Boost
* Git

The project uses:

* Boost.Asio
* Boost.Beast
* nlohmann/json
* GoogleTest for tests

CMake requires Boost and adds the bundled `nlohmann/json` dependency from the repository's `external` directory.

---

## Clone

Clone the repository recursively so that its submodules are available:

```bash
git clone --recurse-submodules https://github.com/DevManDan1178/server-simple.git
cd server-simple
```

If you already cloned the repository without its submodules:

```bash
git submodule update --init --recursive
```

---

## Build

Configure and build the project:

```bash
cmake -S . -B build
cmake --build build
```

The core library is header-only, so there is no separate server library binary to compile.

The CMake project exposes the source directory as an interface include directory and requires C++20.

---

# Using the library

The simplest HTTP application derives from `request_server_base` and implements `process_client_request()`.

```cpp
#include "network/servers/request_server_base.hpp"

class my_server : public request_server_base {
public:
    explicit my_server(unsigned short port)
        : request_server_base(port) {}

protected:
    boost_http_response process_client_request(
        [[maybe_unused]] const std::string& client_ip,
        const boost_http_request& request
    ) override {
        boost_http_response response;

        response.version(request.version());
        response.result(boost::beast::http::status::ok);
        response.set(
            boost::beast::http::field::content_type,
            "text/plain"
        );

        response.body() = "Hello from server-simple!";
        response.prepare_payload();

        return response;
    }
};
```

A complete application can then launch the server:

```cpp
int main() {
    my_server server(8080);

    server.launch();
    server.join_context_thread();
}
```

The resulting server can be queried with:

```bash
curl http://localhost:8080
```

and will return:

```text
Hello from server-simple!
```

The application implements the request logic while `server-simple` handles:

* TCP acceptance
* Asynchronous HTTP I/O
* Request queueing
* Worker threads
* Rate limiting
* Response ordering
* Asynchronous writes
* Connection lifecycle

---

# Example servers

The repository includes example/test server implementations under:

```text
testing/servers/
├── echo_server.hpp
└── broadcast_server.hpp
```

The HTTP example derives from `request_server_base` and converts the request body to uppercase.

For example:

```cpp
#include "testing/servers/echo_server.hpp"
```

The WebSocket example derives from `persistent_server_base`, processes incoming packets during its update loop, and broadcasts received payloads to connected clients.

These examples are useful starting points for implementing application-specific servers.

---

# Testing

The repository contains GoogleTest-based tests covering the core data structures and rate limiter.

Enable tests with:

```bash
cmake -S . -B build \
    -DSERVER_BUILD_TESTS=ON
```

Build:

```bash
cmake --build build
```

Run:

```bash
ctest --test-dir build --output-on-failure
```

The current test suite includes coverage for:

* Thread-safe queues
* Thread-safe maps
* Rate limiting
* Data structures

GoogleTest is fetched by CMake when the test option is enabled.

---

# Benchmarks

Benchmark executables are available for:

* HTTP
* WebSocket/socket operations
* Thread-safe queues

Enable them with:

```bash
cmake -S . -B build \
    -DSERVER_BUILD_BENCHMARKS=ON
```

Build:

```bash
cmake --build build
```

The benchmark targets are:

```text
http_benchmark
sock_benchmark
tsq_benchmark
```

The HTTP benchmark accepts:

```text
http_benchmark [connections] [duration] [warmup]
```

For example:

```bash
./build/testing/benchmarks/http_benchmark 8 10 2
```

Benchmark results are highly dependent on:

* CPU
* Operating system
* Compiler
* Boost version
* Network configuration
* Build configuration
* Number of connections
* Request size
* Workload

For that reason, the project does not advertise performance numbers without reproducible benchmark conditions.

---

# CMake options

The project currently provides these configuration options:

| Option                    | Default | Description                          |
| ------------------------- | ------- | ------------------------------------ |
| `SERVER_BUILD_TESTS`      | `OFF`   | Build GoogleTest tests               |
| `SERVER_BUILD_BENCHMARKS` | `OFF`   | Build benchmark executables          |
| `DEBUG_MODE`              | `OFF`   | Enable debug-mode compile definition |

Examples:

### Normal build

```bash
cmake -S . -B build
cmake --build build
```

### Build with tests

```bash
cmake -S . -B build \
    -DSERVER_BUILD_TESTS=ON

cmake --build build
```

### Build with benchmarks

```bash
cmake -S . -B build \
    -DSERVER_BUILD_BENCHMARKS=ON

cmake --build build
```

### Build everything

```bash
cmake -S . -B build \
    -DSERVER_BUILD_TESTS=ON \
    -DSERVER_BUILD_BENCHMARKS=ON

cmake --build build
```

---

# Design principles

## 1. Bound resources explicitly

A server should have a defined answer to:

> What happens when the application cannot keep up?

`server-simple` uses bounded queues and explicit overload behaviour instead of relying on unlimited memory growth.

---

## 2. Keep networking asynchronous

Network I/O is handled using Boost.Asio and Boost.Beast.

Application processing can therefore happen independently on worker threads or in a fixed-update loop.

---

## 3. Separate transport from application logic

The server should not need to know what the application is doing.

HTTP transport, WebSocket transport, queues, and connection management are separated from application-specific processing.

---

## 4. Make concurrency explicit

The library uses standard C++ concurrency primitives and Boost.Asio rather than hiding the entire concurrency model behind a large framework.

The application can see where work is queued, where worker threads execute, and where synchronization occurs.

---

## 5. Prefer small reusable components

The data structures do not depend on the server classes.

For example, `thread_safe_queue<T>` can be used independently of HTTP or WebSockets.

Likewise, the persistent boards can be used by applications that do not use the networking layer at all.

---

## 6. Fail explicitly under overload

Backpressure is useful only if overload has a defined outcome.

Depending on the component, overload results in:

* An HTTP overload response
* A rejected queue insertion
* A closed WebSocket session

The goal is to prevent a temporary traffic spike from silently becoming unbounded memory consumption.

---

# Project structure

The repository is organized into three main areas:

```text
src/
    Library implementation

testing/
    Tests
    Benchmarks
    Example servers

external/
    Third-party dependencies
```

The `src` directory contains the reusable library itself.

The `testing` directory contains development and validation code rather than core library components.

---

# API overview

The main public components are:

### Networking

```text
server_base
request_server_base
persistent_server_base

http_connection
http_parser
websocket_session

rate_limiter
```

### Thread-safe data structures

```text
thread_safe_queue<T>
thread_safe_unordered_map<K, V>
locked_value<T>
```

### Persistent data structures

```text
leaderboard<T>
score_stream<T>
nameboard
```

### Storage

```text
file_helper
```

---

# Limitations and scope

`server-simple` is intentionally a relatively small infrastructure layer.

It does **not** attempt to provide:

* TLS/HTTPS configuration
* Authentication
* Authorization
* Database-backed persistence
* Distributed state
* Service discovery
* Reverse-proxy functionality
* Production-grade DDoS mitigation
* A routing framework
* A dependency-injection framework
* A complete web application framework

Those concerns can be implemented by the application or provided by other infrastructure.

The project is most useful when you want direct control over the server architecture while avoiding the boilerplate of implementing asynchronous connection management and basic concurrency infrastructure from scratch.

---

# Current status

`server-simple` is under active development.

The API and concurrency model may change as the project evolves.

Current areas of focus include:

* Asynchronous HTTP
* WebSockets
* Connection management
* Worker-based request processing
* Backpressure
* Rate limiting
* Thread-safe data structures
* Persistent application state
* Real-time server loops
* Benchmarking and testing

The project should currently be considered an evolving library rather than a stable, long-term ABI/API commitment.

---

# Contributing

Contributions, bug reports, and suggestions are welcome.

For changes:

1. Fork the repository.
2. Create a feature branch.
3. Make the change.
4. Add or update tests where appropriate.
5. Build with warnings enabled.
6. Run the test suite.
7. Open a pull request describing the change and its motivation.

For concurrency or networking changes, please describe:

* Expected behaviour
* Failure behaviour
* Shutdown behaviour
* Backpressure behaviour
* Thread-safety assumptions
* Any relevant performance considerations

Small, focused changes are preferred over large refactors unless the refactor is clearly justified.

