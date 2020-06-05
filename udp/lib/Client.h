#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>

#include <Packet.h>

namespace NTFTP {

class TClientError : public std::runtime_error {
public:
    TClientError(std::string message);
};

class IClientLogger {
public:
    IClientLogger() = default;
    virtual ~IClientLogger() = default;

    virtual void OnSend(IPacket* packet) = 0;
    virtual void OnReceive(IPacket* packet) = 0;
};

class TClient {
public:
    TClient(const std::string& host);
    ~TClient();

    void SetTimeout(int milliseconds);

    void SetLogger(std::unique_ptr<IClientLogger>&& logger);

    void Read(const std::string& filename,
              std::ostream& to);

    void Write(const std::string& filename,
               std::istream& data,
               ETransferMode mode = ETransferMode::OCTET);

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}
