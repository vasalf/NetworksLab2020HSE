#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>

namespace NChat {

class EServerNetworkError : public std::runtime_error {
public:
    EServerNetworkError(std::string what);
};

class TServer {
public:
    TServer(std::uint16_t port);
    ~TServer();

    void Run();
private:
    class TImpl;
    std::unique_ptr<TImpl> impl_;
};

}
