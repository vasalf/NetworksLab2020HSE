#include <Server.h>
#include <Session.h>
#include <Database.h>

#include <iterator>

#include <boost/asio.hpp>

#include <signal.h>

namespace NHttpProxy {

TServerError::TServerError(const std::string& message)
    : std::runtime_error(message)
{}

class TServer::TImpl {
public:
    TImpl()
        : IOContext_(1)
        , Signals_(IOContext_)
        , Acceptor_(IOContext_)
    {
        Signals_.add(SIGINT);
        Signals_.add(SIGTERM);
        Signals_.add(SIGQUIT);
    }

    void Bind(const std::string& host, const std::string& port) {
        boost::asio::ip::tcp::resolver resolver(IOContext_);
        boost::asio::ip::tcp::resolver::query query(host, port);
        auto endpoints = resolver.resolve(query);

        if (endpoints.empty()) {
            throw TServerError("Couldn't resolve " + host + ":" + port);
        }

        boost::asio::ip::tcp::endpoint endpoint = *endpoints.begin();
        Acceptor_.open(endpoint.protocol());
        Acceptor_.set_option(
            boost::asio::ip::tcp::acceptor::reuse_address(true)
        );
        Acceptor_.bind(endpoint);
    }

    void Run() {
        Signals_.async_wait(
            [this](...) {
                Acceptor_.close();
                for (TSession& session : Sessions_) {
                    session.Stop();
                }
                Sessions_.clear();
            }
        );

        Acceptor_.listen();
        AsyncAccept();

        IOContext_.run();
    }

private:
    void AsyncAccept() {
        Acceptor_.async_accept(
            [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                if (!Acceptor_.is_open()) {
                    return;
                }
                if (!ec) {
                    Serve(std::move(socket));
                }
                AsyncAccept();
            }
        );
    }

    void Serve(boost::asio::ip::tcp::socket socket) {
        Sessions_.emplace_back(std::move(socket), IOContext_, Database_);
        Sessions_.back().SetEndCallback(
            [this, it = std::prev(Sessions_.end())]() {
                Sessions_.erase(it);
            }
        );
        Sessions_.back().Start();
    }

    boost::asio::io_context IOContext_;
    boost::asio::signal_set Signals_;
    boost::asio::ip::tcp::acceptor Acceptor_;
    std::list<TSession> Sessions_;
    TDatabase Database_;
};

TServer::TServer()
    : Impl_(new TImpl())
{}

TServer::~TServer() = default;

TServer::TServer(TServer&&) = default;

TServer& TServer::operator=(TServer&&) = default;

void TServer::Bind(const std::string& host, const std::string& port) {
    Impl_->Bind(host, port);
}

void TServer::Run() {
    Impl_->Run();
}

}
