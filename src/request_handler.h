#pragma once
#include <filesystem>
#include "application.h"
#include "http_server.h"
#include "api_handler.h"

#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <optional>
#include <unordered_map>

namespace http_handler {
namespace net = boost::asio;
namespace beast = boost::beast;
namespace fs = std::filesystem;
namespace http = beast::http;

class RequestHandler {
public:
    using ApiStrand = net::strand<net::io_context::executor_type>;

    explicit RequestHandler(app::Application& app,
                            std::filesystem::path static_root,
                            std::optional<ApiStrand> api_strand = std::nullopt,
                            bool tick_endpoint_enabled = true)
        : api_handler_(app, tick_endpoint_enabled)
        , static_root_(std::filesystem::weakly_canonical(static_root))
        , api_strand_(std::move(api_strand)) {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const std::string target = std::string(req.target());

        if (target.starts_with("/api/")) {
            if (api_strand_) {
                net::post(*api_strand_, [this, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                    HandleApi(std::move(req), std::move(send));
                });
                return;
            }
            return HandleApi(std::move(req), std::forward<Send>(send));
        }

        return HandleStatic(std::move(req), std::forward<Send>(send));
    }

private:
    ApiHandler api_handler_;
    std::filesystem::path static_root_;
    std::optional<ApiStrand> api_strand_;

    template <typename Body, typename Allocator, typename Send>
    void HandleApi(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        api_handler_.HandleApiRequest(std::move(req), std::forward<Send>(send));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleStatic(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        namespace fs = std::filesystem;

        std::string decoded = UrlDecode(std::string(req.target()));

        if (decoded.empty() || decoded == "/") {
            decoded = "/index.html";
        }

        fs::path path = static_root_ / decoded.substr(1);

        if (fs::is_directory(path)) {
            path /= "index.html";
        }

        path = fs::weakly_canonical(path);

        if (!IsSubPath(path, static_root_)) {
            http::response<http::string_body> res{http::status::bad_request, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.body() = "Bad request";
            res.prepare_payload();
            return send(std::move(res));
        }

        if (!fs::exists(path)) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.body() = "File not found";
            res.prepare_payload();
            return send(std::move(res));
        }

        http::file_body::value_type file;
        beast::error_code ec;

        file.open(path.c_str(), beast::file_mode::read, ec);
        if (ec) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.body() = "File not found";
            res.prepare_payload();
            return send(std::move(res));
        }

        http::response<http::file_body> res{
            http::status::ok,
            req.version()
        };

        res.set(http::field::content_type, GetMimeType(path.extension().string()));
        res.body() = std::move(file);
        res.prepare_payload();

        send(std::move(res));
    }


    std::string UrlDecode(std::string_view str) {
        std::string result;
        result.reserve(str.size());

        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%' && i + 2 < str.size()) {
                auto hex_to_int = [](char c) -> int {
                    if ('0' <= c && c <= '9') return c - '0';
                    if ('A' <= c && c <= 'F') return c - 'A' + 10;
                    if ('a' <= c && c <= 'f') return c - 'a' + 10;
                    return 0;
                };

                int value = hex_to_int(str[i + 1]) * 16 + hex_to_int(str[i + 2]);
                result.push_back(static_cast<char>(value));
                i += 2;
            }
            else if (str[i] == '+') {
                result.push_back(' ');
            }
            else {
                result.push_back(str[i]);
            }
        }

        return result;
    }

    bool IsSubPath(std::filesystem::path path, std::filesystem::path base) {
        path = std::filesystem::weakly_canonical(path);
        base = std::filesystem::weakly_canonical(base);

        auto pit = path.begin();
        auto bit = base.begin();

        for (; bit != base.end(); ++bit, ++pit) {
            if (pit == path.end() || *pit != *bit) {
                return false;
            }
        }
        return true;
    }

    std::string GetMimeType(std::string_view ext) {
        static const std::unordered_map<std::string, std::string> types = {
            {".html", "text/html"},
            {".htm", "text/html"},
            {".css", "text/css"},
            {".txt", "text/plain"},
            {".js", "text/javascript"},
            {".json", "application/json"},
            {".xml", "application/xml"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".bmp", "image/bmp"},
            {".ico", "image/vnd.microsoft.icon"},
            {".tiff", "image/tiff"},
            {".svg", "image/svg+xml"},
            {".mp3", "audio/mpeg"}
        };

        std::string key(ext);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (types.contains(key))
            return types.at(key);

        return "application/octet-stream";
    }

};

}  // namespace http_handler
