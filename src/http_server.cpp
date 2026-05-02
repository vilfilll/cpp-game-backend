#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {

SessionBase::SessionBase(tcp::socket&& socket)
    : stream_(std::move(socket)) {}

std::string SessionBase::GetClientIP() const {
    return stream_.socket()
        .remote_endpoint()
        .address()
        .to_string();
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read,
                                            GetSharedThis()));
}

void SessionBase::Read() {
    using namespace std::literals;

    request_ = {};
    stream_.expires_after(30s);

    http::async_read(stream_, buffer_, request_,
                        beast::bind_front_handler(&SessionBase::OnRead,
                                                    GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    using namespace std::literals;

    if (ec == http::error::end_of_stream) {
        return Close();
    }

    if (ec) {
        json::object err;
        err["code"] = ec.value();
        err["text"] = ec.message();
        err["where"] = "read";

        BOOST_LOG_TRIVIAL(error)
            << logging::add_value(additional_data, err)
            << "error";

        return;
    }

    HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close, beast::error_code ec,
                 [[maybe_unused]] std::size_t bytes_written) {
    using namespace std::literals;

    if (ec) {
        json::object err;
        err["code"] = ec.value();
        err["text"] = ec.message();
        err["where"] = "write";

        BOOST_LOG_TRIVIAL(error)
            << logging::add_value(additional_data, err)
            << "error";

        return;
    }

    if (close) {
        return Close();
    }

    Read();
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        std::cout << "shutdown error: " << ec.message() << std::endl;
    }
}



}  // namespace http_server
