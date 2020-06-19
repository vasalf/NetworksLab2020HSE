#pragma once

#include <Packet.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace NTFTP {

class TTransportError : public std::runtime_error {
public:
    TTransportError(const std::string& message);
};

class ITransportLogger {
public:
    ITransportLogger() = default;
    virtual ~ITransportLogger() = default;

    virtual void OnSend(IPacket* packet) = 0;
    virtual void OnReceive(IPacket* packet) = 0;
};

class TVerboseTransportLogger : public ITransportLogger {
public:
    TVerboseTransportLogger(std::ostream& out);
    virtual ~TVerboseTransportLogger() = default;

    virtual void OnSend(IPacket* packet) override;
    virtual void OnReceive(IPacket* packet) override;

private:
    std::ostream& Out_;
};

struct TAddress {
    struct TImpl;

    TAddress(std::unique_ptr<TImpl>);
    ~TAddress();

    TAddress(const TAddress&) = delete;
    TAddress& operator=(const TAddress&) = delete;
    TAddress(TAddress&&);
    TAddress& operator=(TAddress&&);

    std::unique_ptr<TImpl> Impl;
};

class TTransport {
public:
    TTransport();
    ~TTransport();

    TTransport(const TTransport&) = delete;
    TTransport& operator=(const TTransport&) = delete;
    TTransport(TTransport&&);
    TTransport& operator=(TTransport&&);

    void SetLogger(std::shared_ptr<ITransportLogger> logger);

    void Open(std::uint16_t port);

    // Choose a random free-to-use port
    void Open();

    void Send(std::string host, std::uint16_t port, IPacket* packet);
    void Send(const TAddress& to, IPacket* packet);

    // File descriptor for polling
    int PollFD();

    struct TReceiveResult {
        TAddress From;
        std::uint16_t TransferID;
        std::variant<std::unique_ptr<IPacket>, TParsePacketError> Packet;
    };

    std::optional<TReceiveResult> Receive();

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}
