#pragma once

#include "logging.h"

#include <boost/beast/http.hpp>
#include <chrono>

namespace http = boost::beast::http;

template <typename Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler& handler)
        : handler_(handler) {}

    template <typename Request, typename Send>
    void operator()(Request&& req, Send&& send, const std::string& ip) {

        auto start = std::chrono::steady_clock::now();

        json::object req_data;
        req_data["ip"] = ip;
        req_data["URI"] = std::string(req.target());
        req_data["method"] = std::string(req.method_string());

        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(additional_data, req_data)
            << "request received";

        auto logging_send =
            [start, send = std::forward<Send>(send), ip]
            (auto&& response) mutable {

                auto end = std::chrono::steady_clock::now();
                auto ms =
                    std::chrono::duration_cast<
                        std::chrono::milliseconds>(end - start)
                        .count();

                json::object resp_data;
                resp_data["response_time"] = ms;
                resp_data["code"] = response.result_int();

                if (response.find(http::field::content_type) != response.end()) { 
                    resp_data["content_type"] = std::string(response[http::field::content_type]); 
                } else { 
                    resp_data["content_type"] = nullptr; 
                }

                BOOST_LOG_TRIVIAL(info)
                    << logging::add_value(additional_data, resp_data)
                    << "response sent";

                send(std::forward<decltype(response)>(response));
            };

        handler_(std::forward<Request>(req),
                 std::move(logging_send));
    }

private:
    Handler& handler_;
};
