#pragma once

#include <Packet.h>
#include <Transport.h>

namespace NTFTP {

class TServerError : public std::runtime_error {
public:
    TServerError(const std::string& message);
};

class TServer {
public:
    TServer(std::uint16_t port = 69);
    ~TServer();

    void SetTimeout(int milliseconds);

    void SetLogger(std::shared_ptr<ITransportLogger> logger);

    void Run(std::ostream& errStream);

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}
