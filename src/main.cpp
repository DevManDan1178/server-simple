#include "testing/echo_server.hpp"
#include <iostream>
#include <algorithm>
#include "testing/test_persistent_server.hpp"
#include "testing/test_websocket_client.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>


void run_tests() {
    constexpr unsigned short TEST_PORT = 8080;
    
    std::cout << "[TEST] Starting Server...\n";
    test_persistent_server server(TEST_PORT);

    // Run server logic asynchronously on a background thread
    std::thread server_thread([&server]() {
        server.launch();
    });

    // Give Asio context time to bind and listen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // -------------------------------------------------------------
    // TEST 1: Connection & Handshake
    // -------------------------------------------------------------
    std::cout << "[TEST 1] Testing Connection & Handshake...\n";
    test_websocket_client client1;
    bool connected = client1.connect("127.0.0.1", TEST_PORT);
    assert(connected && "Client failed to connect to WebSocket server!");
    std::cout << " -> PASSED: server now has " << server.processed_packets_count << " packets \n";

    // -------------------------------------------------------------
    // TEST 2: Sending Packet and Server Processing
    // -------------------------------------------------------------
    std::cout << "[TEST 2] Sending message from client to server...\n";
    std::string payload = "{\"action\": \"player_move\", \"x\": 10, \"y\": 20}";
    client1.send(payload);

    // Wait briefly for server loop to process `update()` tick
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    assert(server.processed_packets_count == 1 && "Server failed to process incoming packet!");
    assert(server.last_received_payload == payload && "Payload mismatch!");
    std::cout << " -> PASSED\n";

    // -------------------------------------------------------------
    // TEST 3: Server Echo / Direct Reply
    // -------------------------------------------------------------
    std::cout << "[TEST 3] Receiving Echo response from server...\n";
    std::string response = client1.receive();
    assert(response == ("Echo: " + payload) && "Server echo failed!");
    std::cout << " -> PASSED\n";

    // -------------------------------------------------------------
    // TEST 4: Broadcast to Multiple Clients
    // -------------------------------------------------------------
    std::cout << "[TEST 4] Testing Broadcast to Multiple Clients...\n";
    test_websocket_client client2;
    assert(client2.connect("127.0.0.1", TEST_PORT));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string broadcast_msg = "SERVER_TICK_UPDATE";
    server.test_broadcast(broadcast_msg);

    // Both clients should receive the broadcasted message
    std::string c1_received = client1.receive();
    std::string c2_received = client2.receive();

    assert(c1_received == broadcast_msg && "Client 1 missed broadcast!");
    assert(c2_received == broadcast_msg && "Client 2 missed broadcast!");
    std::cout << " -> PASSED\n";

    // -------------------------------------------------------------
    // CLEANUP
    // -------------------------------------------------------------
    std::cout << "[CLEANUP] Disconnecting clients & stopping server...\n";
    client1.disconnect();
    client2.disconnect();

    // Stop server event loop (assumes `stop()` or flag toggle in `server_base`)
    // server.stop(); 
    server_thread.detach(); // Detach for mock exiting

    std::cout << "\n>>> ALL WEBSOCKET TESTS PASSED SUCCESSFULLY! <<<\n";
}

int main() {
    /*
    try {
        log_debug() << "Starting echo server on port 8080...\n";
        echo_server server(8080);
        server.launch(); // This blocks and runs the async loop
    
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    */
    run_tests();
    return 0;
}