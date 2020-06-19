#include <Session.h>
#include <Compress.h>

#include <iostream>
#include <string_view>

namespace NHttpProxy {

TSession::TSession(
    boost::asio::ip::tcp::socket socket,
    boost::asio::io_context& context,
    TDatabase& database
)
    : ClientSocket_(std::move(socket))
    , ForeignSocket_(context)
    , IOContext_(context)
    , Database_(database)
{}

void TSession::SetEndCallback(TSessionEndCallback callback) {
    EndCallback_ = std::move(callback);
}

void TSession::Start() {
    ReadClient();
}

void TSession::Stop() {
    ClientSocket_.close();
    ForeignSocket_.close();
    if (EndCallback_.has_value()) {
        EndCallback_.value()();
    }
}

void TSession::ReadClient() {
    ClientSocket_.async_read_some(
        boost::asio::buffer(ClientBuffer_),
        [this](boost::system::error_code ec, std::size_t size) {
            if (ec && ec != boost::asio::error::operation_aborted) {
                Stop();
            }
            if (ec) {
                return;
            }
            EParseResult status = EParseResult::Await;
            for (std::size_t i = 0; i < size && status == EParseResult::Await; i++) {
                status = RequestParser_.Consume(ClientBuffer_[i]);
            }
            if (status == EParseResult::Await) {
                ReadClient();
            } else {
                WriteForeign();
            }
        }
    );
}

namespace {

std::pair<std::string, std::string> SplitURL(const std::string& url) {
    std::size_t i = 0;
    auto ss = std::strstr(url.c_str(), "://");
    if (ss != nullptr) {
        i = ss - url.c_str() + 3;
    }
    auto j = std::string_view(url.c_str() + i, url.size() - i).find('/');
    if (j == std::string_view::npos) {
        j = url.size();
    }
    if (ss == nullptr) {
        return {"http", url.substr(i, j)};
    } else {
        return {url.substr(0, i - 3), url.substr(i, j)};
    }
}

void LogRequest(const std::string& url) {
    std::cout << "[REQ]   " << url << std::endl;
}

void LogResponse(const std::string& url, bool compressed) {
    std::cout << "[RESP]  " << url;
    if (compressed) {
        std::cout << " (gzip)";
    }
    std::cout << std::endl;
}

void LogCachedResponse(const std::string& url, bool compressed) {
    std::cout << "[CACHE] " << url;
    if (compressed) {
        std::cout << " (gzip)";
    }
    std::cout << std::endl;
}

}

void TSession::WriteForeign() {
    auto request = RequestParser_.Parsed();
    request.Headers().Remove("Accept-Encoding");

    Request_ = request.Serialize();
    std::string url = request.RequestLine().URL();
    auto [scheme, host] = SplitURL(url);
    LogRequest(url);

    auto cached = Database_.ServeCached(url);
    if (cached.has_value()) {
        bool compressed = false;
        if (CompressionSupported(RequestParser_.Parsed())) {
            Compress(cached.value());
            compressed = true;
        }
        LogCachedResponse(url, compressed);
        Response_ = cached.value().Serialize();
        WriteClient();
        return;
    }

    boost::asio::ip::tcp::resolver resolver(IOContext_);
    auto endpoints = resolver.resolve(host, scheme);
    boost::asio::connect(ForeignSocket_, endpoints);
    boost::asio::async_write(
        ForeignSocket_,
        boost::asio::buffer(Request_),
        [this](boost::system::error_code ec, std::size_t) {
            if (ec && ec != boost::asio::error::operation_aborted) {
                Stop();
            }
            if (ec) {
                return;
            }
            ReadForeign();
        }
    );
}

void TSession::ReadForeign() {
    ForeignSocket_.async_read_some(
        boost::asio::buffer(ForeignBuffer_),
        [this](boost::system::error_code ec, std::size_t size) {
            if (ec && ec != boost::asio::error::operation_aborted) {
                Stop();
            }
            if (ec) {
                return;
            }
            EParseResult status = EParseResult::Await;
            for (std::size_t i = 0; i < size && status == EParseResult::Await; i++) {
                status = ResponseParser_.Consume(ForeignBuffer_[i]);
            }
            if (status == EParseResult::Await) {
                ReadForeign();
            } else {
                ForeignSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                auto response = ResponseParser_.Parsed();
                bool compressed = false;
                if (CompressionSupported(RequestParser_.Parsed())) {
                    compressed = true;
                    Compress(response);
                }
                Response_ = response.Serialize();
                LogResponse(RequestParser_.Parsed().RequestLine().URL(), compressed);
                Database_.CacheResponse(RequestParser_.Parsed(), ResponseParser_.Parsed());
                WriteClient();
            }
        }
    );
}

void TSession::WriteClient() {
    boost::asio::async_write(
        ClientSocket_,
        boost::asio::buffer(Response_),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                ClientSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            }
            Stop();
        }
    );
}

}
