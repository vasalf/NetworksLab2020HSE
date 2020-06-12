#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>

#include <Packet.h>
#include <Transport.h>

namespace NTFTP {

class TClientError : public std::runtime_error {
public:
    TClientError(std::string message);
};

class TClient {
public:
    TClient(const std::string& host, std::uint16_t port = 69);
    ~TClient();

    void SetTimeout(int milliseconds);

    void SetLogger(std::shared_ptr<ITransportLogger> logger);

    void Read(const std::string& filename, std::ostream& to);

    void Write(const std::string& filename, std::istream& data);


private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}
