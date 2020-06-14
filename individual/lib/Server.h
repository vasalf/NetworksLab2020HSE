#pragma once

#include <memory>
#include <stdexcept>
#include <string>

namespace NHttpProxy {

class TServerError : public std::runtime_error {
public:
    TServerError(const std::string& message);
};

class TServer {
public:
    TServer();
    ~TServer();

    TServer(const TServer&) = delete;
    TServer& operator=(const TServer&) = delete;
    TServer(TServer&&);
    TServer& operator=(TServer&&);

    void Bind(const std::string& host, const std::string& port);
    void Run();

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}
