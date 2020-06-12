#include <Transport.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netdb.h>
#include <unistd.h>

#include <ctime>
#include <iostream>
#include <limits>
#include <random>

namespace NTFTP {

TTransportError::TTransportError(const std::string& message)
    : std::runtime_error(message)
{}

struct TAddress::TImpl {
    TImpl(sockaddr address, socklen_t length)
        : Address(address)
        , Length(length)
    {}

    sockaddr Address;
    socklen_t Length;
};

TAddress::TAddress(std::unique_ptr<TImpl> impl)
    : Impl(std::move(impl))
{}

TAddress::~TAddress() = default;

TAddress::TAddress(TAddress&&) = default;
TAddress& TAddress::operator=(TAddress&&) = default;

namespace {

class TVerboseLoggerPacketVisitor : public NTFTP::IPacketVisitor {
public:
    TVerboseLoggerPacketVisitor(std::ostream& out)
        : Out_(out)
    {}

    virtual ~TVerboseLoggerPacketVisitor() = default;

    virtual void VisitRequestPacket(const NTFTP::TRequestPacket& packet) override {
        switch (packet.Type()) {
        case NTFTP::TRequestPacket::EType::READ:
            Out_ << "Read";
            break;
        case NTFTP::TRequestPacket::EType::WRITE:
            Out_ << "Write";
            break;
        }
        Out_ << "Request "
             << "Filename=\"" << packet.Filename() << "\" ";

        Out_ << "Mode=";
        switch (packet.Mode()) {
        case NTFTP::ETransferMode::NETASCII:
            Out_ << "NetASCII";
            break;
        case NTFTP::ETransferMode::OCTET:
            Out_ << "Octet";
            break;
        }
        Out_ << std::endl;
    }

    virtual void VisitDataPacket(const NTFTP::TDataPacket& packet) override {
        Out_ << "Data "
             << "BlockID=" << packet.BlockID() << " "
             << "Data=[" << packet.Data().length() << " bytes]" << std::endl;
    }

    virtual void VisitAcknowledgePacket(const NTFTP::TAcknowledgePacket& packet) override {
        Out_ << "Acknowledge "
             << "BlockID=" << packet.BlockID() << std::endl;
    }

    virtual void VisitErrorPacket(const NTFTP::TErrorPacket& packet) override {
        Out_ << "Error "
             << "Type=" << static_cast<int>(packet.Type()) << " "
             << "Message=\"" << packet.Message() << "\"" << std::endl;
    }

private:
    std::ostream& Out_;
};

}

TVerboseTransportLogger::TVerboseTransportLogger(std::ostream& out)
    : Out_(out)
{}

void TVerboseTransportLogger::OnSend(IPacket* packet) {
    TVerboseLoggerPacketVisitor visitor(Out_);
    Out_ << "SEND ";
    packet->Accept(&visitor);
}

void TVerboseTransportLogger::OnReceive(IPacket* packet) {
    TVerboseLoggerPacketVisitor visitor(Out_);
    Out_ << "RECV ";
    packet->Accept(&visitor);
}

namespace {

class TDefaultTransportLogger : public ITransportLogger {
public:
    TDefaultTransportLogger() = default;
    ~TDefaultTransportLogger() = default;

    virtual void OnSend(IPacket*) override {}
    virtual void OnReceive(IPacket*) override {}
};

}

class TTransport::TImpl {
public:
    TImpl()
        : SockFD_(-1)
        , Logger_(std::make_shared<TDefaultTransportLogger>())
    {}

    ~TImpl() {
        if (SockFD_ >= 0) {
            close(SockFD_);
        }
    }

    void SetLogger(std::shared_ptr<ITransportLogger>&& logger) {
        Logger_ = std::move(logger);
    }

    void Open(std::uint16_t port) {
        InitSocket();
        if (Bind(port) < 0) {
            throw TTransportError("Unable to bind selected port");
        }
    }

    void Open() {
        InitSocket();

        std::random_device rd;
        std::mt19937 rnd(rd());
        std::uniform_int_distribution<std::uint16_t> dist(
            1024,
            std::numeric_limits<std::uint16_t>::max()
        );

        int error;
        do {
            error = Bind(dist(rnd));
        } while (error < 0);
    }

    void Send(std::string host, std::uint16_t port, IPacket* packet) {
        auto to = ResolveAddress(host, port);
        std::string data = packet->ToBytes();
        int error = sendto(SockFD_, data.c_str(), data.length(), 0, &to, sizeof(to));
        if (error < 0) {
            throw TTransportError("Unable to send packet");
        }
        Logger_->OnSend(packet);
    }

    void Send(const TAddress& to, IPacket* packet) {
        std::string data = packet->ToBytes();
        int error = sendto(SockFD_, data.c_str(), data.length(), 0, &to.Impl->Address, to.Impl->Length);
        if (error < 0) {
            throw TTransportError("Unable to send packet");
        }
        Logger_->OnSend(packet);
    }

    int PollFD() {
        return SockFD_;
    }

    std::optional<TTransport::TReceiveResult> Receive() {
        char buf[520];
        sockaddr from;
        socklen_t fromLength = sizeof(from);
        int error = recvfrom(SockFD_, buf, sizeof(buf), 0, &from, &fromLength);
        if (error < 0) {
            return {};
        }

        std::uint16_t transferID = ntohs(
            reinterpret_cast<sockaddr_in*>(&from)->sin_port
        );

        TAddress addr { std::make_unique<TAddress::TImpl>(from, fromLength) };

        try {
            auto packet = ParsePacket(std::string(buf, error));
            Logger_->OnReceive(packet.get());
            return TReceiveResult{std::move(addr), transferID, std::move(packet)};
        } catch(TParsePacketError& error) {
            return TReceiveResult{std::move(addr), transferID, error};
        }
    }

private:
    int SockFD_;
    std::shared_ptr<ITransportLogger> Logger_;

    void InitSocket() {
        SockFD_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (SockFD_ < 0) {
            throw TTransportError("Unable to create socket");
        }

        int opt = 1;
        int error = setsockopt(
            SockFD_,
            SOL_SOCKET,
            SO_REUSEADDR | SO_REUSEPORT,
            &opt,
            sizeof(int)
        );
        if (error < 0) {
            throw TTransportError("Unable to set socket options");
        }
    }

    int Bind(std::uint16_t port) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        return bind(
            SockFD_,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
        );
    }

    sockaddr ResolveAddress(std::string host, std::uint16_t port) {
        std::string portStr = std::to_string(port);

        addrinfo hints;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_flags = 0;

        addrinfo* addrs;
        int error = getaddrinfo(
            host.c_str(),
            portStr.c_str(),
            &hints,
            &addrs
        );

        if (error < 0 || !addrs) {
            throw TTransportError("Unable to resolve address " + host + ":" + portStr);
        }

        auto ret = *addrs->ai_addr;

        freeaddrinfo(addrs);

        return ret;
    }
};

TTransport::TTransport()
    : Impl_(new TImpl())
{}

TTransport::~TTransport() = default;

TTransport::TTransport(TTransport&&) = default;
TTransport& TTransport::operator=(TTransport&&) = default;

void TTransport::SetLogger(std::shared_ptr<ITransportLogger> logger) {
    Impl_->SetLogger(std::move(logger));
}

void TTransport::Open(std::uint16_t port) {
    Impl_->Open(port);
}

void TTransport::Open() {
    Impl_->Open();
}

void TTransport::Send(std::string host, std::uint16_t port, IPacket* packet) {
    Impl_->Send(host, port, packet);
}

void TTransport::Send(const TAddress& address, IPacket* packet) {
    Impl_->Send(address, packet);
}

int TTransport::PollFD() {
    return Impl_->PollFD();
}

std::optional<TTransport::TReceiveResult> TTransport::Receive() {
    return Impl_->Receive();
}

}
