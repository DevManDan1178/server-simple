#include <future>
#include <string>
#include <limits>
#include "network/servers/request_server_base.hpp" 

class echo_server : public request_server_base {
public:
    // Pass port 8080 to the base constructor
    echo_server(
        unsigned short port, 
        size_t worker_count = DEFAULT_WORKER_THREAD_COUNT, 
        size_t max_pending_requests = DEFAULT_MAX_PENDING_REQUESTS,
        size_t max_request_bytes = DEFAULT_MAX_REQUEST_BYTES,
        double max_ip_rate_tokens = DEFAULT_MAX_IP_RATE_TOKENS, 
        double ip_token_refill_rate = DEFAULT_IP_TOKEN_REFILL_RATE
    ) : request_server_base(port, worker_count, max_pending_requests, max_request_bytes, max_ip_rate_tokens, ip_token_refill_rate) {}

protected:
    // Implement the request processing logic
    virtual boost::beast::http::response<boost::beast::http::string_body> process_client_request([[maybe_unused]] const std::string client_ip, const boost_http_request request) override { 
        // Example logic: Turn the request into uppercase and send it back
        std::string response = request.body();
        std::transform(response.begin(), response.end(), response.begin(), ::toupper);
        
        boost::beast::http::response<boost::beast::http::string_body> res;
        res.version(request.version());
        res.result(boost::beast::http::status::ok);
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.body() = response; 
        res.prepare_payload();
        return res;
    }
};