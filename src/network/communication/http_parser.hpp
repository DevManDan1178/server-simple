#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace http_parser {
    json parse_query(const std::string& target) {
        json params = json::object();

        auto pos = target.find('?');
        if (pos == std::string::npos) {
            return params;
        }

        std::string query = target.substr(pos + 1);

        size_t start = 0;
        while (start < query.size()) {
            size_t amp = query.find('&', start);
            if (amp == std::string::npos) {
                amp = query.size();
            }

            std::string pair = query.substr(start, amp - start);

            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                std::string key = pair.substr(0, eq);
                std::string value = pair.substr(eq + 1);

                // Parsing value in order (float -> int -> default to string)
                try {
                    size_t pos;
                    long long i = std::stoll(value, &pos);

                    // Entire string was an integer
                    if (pos == value.size()) {
                        params[key] = i;
                    } else {
                        throw std::invalid_argument("not int");
                    }
                }
                catch (...) {
                    try {
                        size_t pos;
                        double f = std::stod(value, &pos);

                        // Entire string was a float
                        if (pos == value.size()) {
                            params[key] = f;
                        } else {
                            params[key] = value;
                        }
                    }
                    catch (...) {
                        params[key] = value;
                    }
                }
            }

            start = amp + 1;
        }

        return params;
    }

    using boost_http_response =
        boost::beast::http::response<boost::beast::http::string_body>;
    using boost_http_status = boost::beast::http::status;

    /**
     * @brief sets response result to bad request and the response text as text plain
     */
    void set_response_bad_request(boost_http_response& response, const std::string& response_text) {
        response.result(boost::beast::http::status::bad_request);
        response.set(boost::beast::http::field::content_type, "text/plain");
        response.body() = response_text;
    }

    /**
     * @brief sets response to 404 Not Found 
     */
    void set_response_not_found(boost_http_response& response) {
        response.set(boost::beast::http::field::content_type, "text/plain");
        response.result(boost::beast::http::status::not_found);
        response.body() = "404 Not Found";
    }

    /**
     * @brief sets resposne to content type json and dumps json into result
     */
    void set_response_json(boost_http_response& response, json j) {
        response.set(boost::beast::http::field::content_type, "application/json");
        response.body() = j.dump();
    }
}
