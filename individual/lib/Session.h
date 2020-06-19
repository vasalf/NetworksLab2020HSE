#pragma once

#include <Database.h>
#include <HTTP.h>

#include <array>
#include <functional>
#include <list>
#include <optional>

#include <boost/asio.hpp>

namespace NHttpProxy {

using TSessionEndCallback = std::function<void()>;

class TSession {
public:
    TSession(
        boost::asio::ip::tcp::socket,
        boost::asio::io_context& context,
        TDatabase& database
    );

    TSession(const TSession&) = delete;
    TSession& operator=(const TSession&) = delete;

    void SetEndCallback(TSessionEndCallback callback);

    // Start first asynchronous operation
    void Start();

    // Stop all asynchronous operations
    void Stop();

private:
    void ReadClient();
    void WriteForeign();
    void ReadForeign();
    void WriteClient();

    boost::asio::ip::tcp::socket ClientSocket_;
    boost::asio::ip::tcp::socket ForeignSocket_;

    std::array<char, 4096> ClientBuffer_;
    std::string Request_;
    std::array<char, 4096> ForeignBuffer_;
    std::string Response_;

    THttpRequestParser RequestParser_;
    THttpResponseParser ResponseParser_;

    boost::asio::io_context& IOContext_;
    std::optional<TSessionEndCallback> EndCallback_;

    TDatabase& Database_;
};

}
