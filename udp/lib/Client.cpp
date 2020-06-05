#include <Client.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netdb.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <optional>
#include <random>

namespace NTFTP {

TClientError::TClientError(std::string message)
    : std::runtime_error(message)
{}

namespace {

class TIgnoreLogger : public IClientLogger {
public:
    TIgnoreLogger() = default;
    virtual ~TIgnoreLogger() = default;

    virtual void OnSend(IPacket*) override {}
    virtual void OnReceive(IPacket*) override {}
};

class TExpectingPacketVisitorBase : public IPacketVisitor {
public:
    TExpectingPacketVisitorBase() = default;
    virtual ~TExpectingPacketVisitorBase() = default;

    virtual void VisitRequestPacket(const TRequestPacket&) override {
        ErrorAnswer_.reset(
            new TErrorPacket(TErrorPacket::EType::ILLEGAL_OPCODE)
        );
    }

    virtual void VisitDataPacket(const TDataPacket&) override {
        ErrorAnswer_.reset(
            new TErrorPacket(TErrorPacket::EType::ILLEGAL_OPCODE)
        );
    }

    virtual void VisitAcknowledgePacket(const TAcknowledgePacket&) override {
        ErrorAnswer_.reset(
            new TErrorPacket(TErrorPacket::EType::ILLEGAL_OPCODE)
        );
    }

    virtual void VisitErrorPacket(const TErrorPacket& error) override {
        ErrorRecvd_.reset(
            new TErrorPacket(error)
        );
    }

    bool ReceivedError() const {
        return (bool)ErrorRecvd_;
    }

    TErrorPacket& GetReceivedError() {
        return *ErrorRecvd_;
    }

    bool AnswerError() const {
        return (bool)ErrorAnswer_;
    }

    TErrorPacket& GetAnswerError() {
        return *ErrorAnswer_;
    }

private:
    std::unique_ptr<TErrorPacket> ErrorRecvd_;
    std::unique_ptr<TErrorPacket> ErrorAnswer_;
};

class TReadPacketVisitor : public TExpectingPacketVisitorBase {
public:
    TReadPacketVisitor() = default;
    virtual ~TReadPacketVisitor() = default;

    virtual void VisitDataPacket(const TDataPacket& answer) override {
        DataReceived_.reset(
            new TDataPacket(answer)
        );
    }

    TDataPacket& DataReceived() {
        return *DataReceived_;
    }

private:
    std::unique_ptr<TDataPacket> DataReceived_;
};

}

class TClient::TImpl {
    // Automatically close socket on exception.
    struct SockFDHolder {
        int FD = -1;

        SockFDHolder() {}

        SockFDHolder(int fd)
            : FD(fd)
        {}

        SockFDHolder(const SockFDHolder&) = delete;
        SockFDHolder& operator=(const SockFDHolder&) = delete;

        SockFDHolder(SockFDHolder&& from) {
            std::swap(FD, from.FD);
        }

        SockFDHolder& operator=(SockFDHolder&& from) {
            std::swap(FD, from.FD);
            return *this;
        }

        ~SockFDHolder() {
            if (FD >= 0) {
                close(FD);
            }
        }
    };

public:
    TImpl(const std::string& host, std::uint16_t port)
        : Host_(host)
        , Port_(port)
        , Rnd_(time(nullptr))
        , Logger_(new TIgnoreLogger())
    {}

    void SetTimeout(int milliseconds) {
        Timeout_ = milliseconds;
    }

    void SetLogger(std::unique_ptr<IClientLogger>&& logger) {
        Logger_ = std::move(logger);
    }

    void Read(const std::string& filename,
              std::ostream& to) {
        SockFDHolder socket { OpenSocket() };
        auto serverReq = GetServerAddress();
        std::uint16_t serverPort = 0;
        bool end = false;

        {
            // Read request
            TRequestPacket request(
                TRequestPacket::EType::READ,
                filename,
                ETransferMode::OCTET
            );
            SendPacket(socket.FD, serverReq, &request);
        }
        std::size_t blockID = 1;

        while (!end) {
            auto inPacket = ReceivePacket(socket.FD);
            if (inPacket.ParseError.has_value()) {
                TErrorPacket error(
                    TErrorPacket::EType::ILLEGAL_OPCODE,
                    inPacket.ParseError.value()
                );
                SendPacket(socket.FD, inPacket.From, &error);
                throw TClientError("Illegal answer from server");
            }
            if (!inPacket.ReceivedPacket) {
                throw TClientError("Timeout");
            }
            auto serverAnswerPort = ntohs(
                reinterpret_cast<sockaddr_in*>(&inPacket.From)->sin_port
            );
            if (serverPort == 0) {
                serverPort = serverAnswerPort;
            } else if (serverPort != serverAnswerPort) {
                // Duplicate connection silently dropped as described in RFC 1350.
                TErrorPacket error(TErrorPacket::EType::UNKNOWN_TRANSFER_ID);
                SendPacket(socket.FD, inPacket.From, &error);
                continue;
            }

            TReadPacketVisitor visitor;
            inPacket.ReceivedPacket->Accept(&visitor);
            if (visitor.ReceivedError()) {
                throw TClientError("Server: " + visitor.GetReceivedError().Message());
            }
            if (visitor.AnswerError()) {
                SendPacket(socket.FD, inPacket.From, &visitor.GetAnswerError());
                throw TClientError("Client: " + visitor.GetAnswerError().Message());
            }
            if (visitor.DataReceived().BlockID() != blockID) {
                // Duplicate
                continue;
            }
            end = visitor.DataReceived().Data().length() < 512;
            to << visitor.DataReceived().Data();

            TAcknowledgePacket answer(blockID);
            SendPacket(socket.FD, inPacket.From, &answer);

            ++blockID;
        }
    }

    void Write(const std::string& filename,
               std::istream& data,
               ETransferMode mode) {
    }

private:
    int Timeout_ = 2000; // Default timeout
    std::string Host_;
    std::uint16_t Port_;

    int InitSocket() {
        int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
        if (socketFD < 0) {
            throw TClientError("Unable to create socket");
        }

        int opt = 1;
        int error = setsockopt(
            socketFD,
            SOL_SOCKET,
            SO_REUSEADDR | SO_REUSEPORT,
            &opt,
            sizeof(int)
        );
        if (error < 0) {
            throw TClientError("Unable to set socket options");
        }

        return socketFD;
    }

    std::uint16_t BindPort(int socketFD) {
        std::uniform_int_distribution<std::uint16_t> dist(
            1024,
            std::numeric_limits<std::uint16_t>::max()
        );

        sockaddr_in clientAddr;
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_addr.s_addr = INADDR_ANY;

        int error;
        std::uint16_t port;

        do {
            port = dist(Rnd_);
            clientAddr.sin_port = htons(port);

            error = bind(
                socketFD,
                reinterpret_cast<sockaddr*>(&clientAddr),
                sizeof(clientAddr)
            );
        } while (error < 0);

        return port;
    }

    int OpenSocket() {
        int socketFD = InitSocket();
        BindPort(socketFD);
        return socketFD;
    }

    sockaddr GetServerAddress() {
        std::string portStr = std::to_string(Port_);

        addrinfo hints;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_flags = 0;

        addrinfo* addrs;
        int error = getaddrinfo(
            Host_.c_str(),
            portStr.c_str(),
            &hints,
            &addrs
        );

        if (error < 0 || !addrs) {
            throw TClientError("Could not resolve host");
        }

        auto ret = *addrs->ai_addr;

        freeaddrinfo(addrs);

        return ret;
    }

    void SendPacket(int sockFD, sockaddr& to, IPacket* packet) {
        std::string data = packet->ToBytes();
        int error = sendto(sockFD, data.c_str(), data.length(), 0, &to, sizeof(to));
        if (error < 0) {
            throw TClientError("Couldn't send packet");
        }
        Logger_->OnSend(packet);
    }

    struct InPacket {
        sockaddr From;
        socklen_t FromLength = sizeof(sockaddr);
        std::unique_ptr<IPacket> ReceivedPacket = nullptr;
        std::optional<std::string> ParseError;
    };

    InPacket ReceivePacket(int sockFD) {
        InPacket ret;

        pollfd listen {
            .fd = sockFD,
            .events = POLLIN
        };
        int err = poll(&listen, 1, Timeout_);
        if (err < 0) {
            throw TClientError("Poll error");
        }
        if (err == 0) {
            return ret;
        }

        char buf[520];
        err = recvfrom(sockFD, buf, sizeof(buf), 0, &ret.From, &ret.FromLength);
        if (err < 0) {
            throw TClientError("Couldn't read from socket");
        }
        try {
            ret.ReceivedPacket = ParsePacket(std::string(buf, err));
            Logger_->OnReceive(ret.ReceivedPacket.get());
        } catch(TParsePacketError& e) {
            ret.ParseError = e.what();
        }

        return ret;
    }

    std::mt19937 Rnd_;
    std::unique_ptr<IClientLogger> Logger_;
};

TClient::TClient(const std::string& host, std::uint16_t port)
    : Impl_(new TImpl(host, port))
{}

TClient::~TClient() = default;

void TClient::SetTimeout(int milliseconds) {
    Impl_->SetTimeout(milliseconds);
}

void TClient::SetLogger(std::unique_ptr<IClientLogger>&& logger) {
    Impl_->SetLogger(std::move(logger));
}

void TClient::Read(const std::string& filename, std::ostream& to) {
    Impl_->Read(filename, to);
}

void TClient::Write(const std::string& filename, std::istream& data, ETransferMode mode) {
    Impl_->Write(filename, data, mode);
}

}
