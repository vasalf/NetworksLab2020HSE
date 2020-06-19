#include <Server.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include <sys/poll.h>

namespace NTFTP {

TServerError::TServerError(const std::string& message)
    : std::runtime_error(message)
{}

namespace {

using TTimePoint = std::chrono::steady_clock::time_point;
using TDuration = std::chrono::steady_clock::duration;

class IClient {
public:
    IClient(TAddress&& address, TTransport&& transport, std::ostream& error)
        : Address(std::move(address))
        , Transport(std::move(transport))
        , Error(error)
    {}

    virtual ~IClient() = default;

    bool HandlePacket(TTransport::TReceiveResult&& packet) {
        if (std::holds_alternative<TParsePacketError>(packet.Packet)) {
            std::string message = std::get<TParsePacketError>(packet.Packet).what();
            TErrorPacket error {
                TErrorPacket::EType::ILLEGAL_OPCODE,
                message
            };
            Transport.Send(packet.From, &error);
            Error << "Parse error: " << message << std::endl;
            return false;
        }
        return Handle(std::move(std::get<std::unique_ptr<IPacket>>(packet.Packet)));
    }

    TTransport& GetTransport() {
        return Transport;
    }

    TDuration SinceLast(const TTimePoint now) {
        return now - Last;
    }

    /** false if the transmition is over */
    virtual bool Handle(std::unique_ptr<IPacket>&& packet) = 0;

protected:
    TAddress Address;
    TTransport Transport;
    std::ostream& Error;
    TTimePoint Last;
};

class TReadingClientBase : public IClient {
    class TVisitor : public TExpectingPacketVisitorBase {
    public:
        void VisitRequestPacket(const TRequestPacket& request) override {
            BlockID_ = 0;
            if (!std::filesystem::exists(request.Filename())) {
                SetErrorAnswer(
                    new TErrorPacket(TErrorPacket::EType::FILE_NOT_FOUND)
                );
            }
        }

        void VisitAcknowledgePacket(const TAcknowledgePacket& packet) override {
            BlockID_ = packet.BlockID();
        }

        std::uint16_t BlockID() const {
            return BlockID_;
        }

    private:
        std::uint16_t BlockID_;
    };

public:
    TReadingClientBase(TAddress&& address,
                       TTransport&& transport,
                       std::ostream& error)
        : IClient(std::move(address), std::move(transport), error)
        , BlockID_(0)
    {}

    virtual ~TReadingClientBase() = default;

    bool Handle(std::unique_ptr<IPacket>&& packet) override {
        Last = std::chrono::steady_clock::now();

        TVisitor visitor;
        packet->Accept(&visitor);

        if (visitor.ReceivedError()) {
            Error << "Client: " << visitor.GetReceivedError().Message() << std::endl;
            return false;
        }

        if (visitor.AnswerError()) {
            Error << "Server: " << visitor.GetAnswerError().Message() << std::endl;
            Transport.Send(Address, &visitor.GetAnswerError());
            return false;
        }

        if (BlockID_ != visitor.BlockID()) {
            // Duplicate
            return true;
        }

        BlockID_++;

        if (Eof()) {
            // End of transmission
            return false;
        }

        TDataPacket next(BlockID_, NextBlock());
        Transport.Send(Address, &next);

        return true;
    }

protected:
    virtual bool Eof() = 0;
    virtual std::string NextBlock() = 0;

private:
    std::uint16_t BlockID_;
};

class TOctetReadingClient : public TReadingClientBase {
public:
    TOctetReadingClient(TAddress&& address,
                        TTransport&& transport,
                        std::ostream& error,
                        std::string filename)
        : TReadingClientBase(std::move(address), std::move(transport), error)
        , From_(filename, std::ios::binary)
    {}

    virtual ~TOctetReadingClient() = default;

protected:
    virtual bool Eof() override {
        return From_.eof();
    }

    virtual std::string NextBlock() override {
        char buf[512];
        From_.read(buf, 512);
        return std::string(buf, From_.gcount());
    }

private:
    std::ifstream From_;
};

// TODO: Rewrite on DFA NetASCII conversion
class TNetAsciiReadingClient : public TReadingClientBase {
public:
    TNetAsciiReadingClient(TAddress&& address,
                           TTransport&& transport,
                           std::ostream& error,
                           std::string filename)
        : TReadingClientBase(std::move(address), std::move(transport), error)
    {
        std::ifstream from(filename, std::ios::binary);
        Contents_ = ToNetASCII(
            std::string(
                std::istreambuf_iterator<char>(from),
                std::istreambuf_iterator<char>()
            )
        );
    }

protected:
    virtual bool Eof() override {
        return Begin_ == Contents_.size();
    }

    virtual std::string NextBlock() override {
        std::size_t end = std::min(Begin_ + 512, Contents_.size());
        std::string ret = Contents_.substr(Begin_, end - Begin_);
        Begin_ = end;
        return ret;
    }

private:
    std::string Contents_;
    std::size_t Begin_ = 0;
};

class TWritingClientBase : public IClient {
    class TVisitor : public TExpectingPacketVisitorBase {
    public:
        void VisitRequestPacket(const TRequestPacket&) override {
            BlockID_ = 0;
        }

        void VisitDataPacket(const TDataPacket& packet) override {
            BlockID_ = packet.BlockID();
            Data_ = packet.Data();
        }

        std::uint16_t BlockID() const {
            return BlockID_;
        }

        const std::string& Data() const {
            return Data_;
        }
    private:
        std::uint16_t BlockID_;
        std::string Data_;
    };
public:

    TWritingClientBase(TAddress&& address,
                       TTransport&& transport,
                       std::ostream& error)
        : IClient(std::move(address), std::move(transport), error)
        , BlockID_(0)
    {}

    virtual ~TWritingClientBase() = default;

    bool Handle(std::unique_ptr<IPacket>&& packet) {
        Last = std::chrono::steady_clock::now();

        TVisitor visitor;
        packet->Accept(&visitor);

        if (visitor.ReceivedError()) {
            Error << "Client: " << visitor.GetReceivedError().Message() << std::endl;
            return false;
        }

        if (visitor.AnswerError()) {
            Error << "Server: " << visitor.GetAnswerError().Message() << std::endl;
            Transport.Send(Address, &visitor.GetAnswerError());
            return false;
        }

        if (BlockID_ != visitor.BlockID()) {
            // Duplicate
            return true;
        }

        Append(visitor.Data());

        TAcknowledgePacket ack(BlockID_);
        Transport.Send(Address, &ack);

        BlockID_++;

        return BlockID_ == 1 || visitor.Data().length() == 512;
    }

protected:
    virtual void Append(const std::string& data) = 0;

private:
    std::uint16_t BlockID_;
};

class TOctetWritingClient : public TWritingClientBase {
public:
    TOctetWritingClient(TAddress&& address,
                        TTransport&& transport,
                        std::ostream& error,
                        std::string filename)
        : TWritingClientBase(std::move(address), std::move(transport), error)
        , Out_(filename, std::ios::binary)
    {}

    virtual ~TOctetWritingClient() = default;

protected:
    virtual void Append(const std::string& data) override {
        Out_.write(data.c_str(), data.length());
        Out_.flush();
    }

private:
    std::ofstream Out_;
};

// TODO: Rewrite on DFA NetAscii conversion
class TNetAsciiWritingClient : public TWritingClientBase {
public:
    TNetAsciiWritingClient(TAddress&& address,
                           TTransport&& transport,
                           std::ostream& error,
                           std::string filename)
        : TWritingClientBase(std::move(address), std::move(transport), error)
        , Filename_(filename)
    {}

    virtual ~TNetAsciiWritingClient() {
        std::ofstream out(Filename_, std::ios::binary);
        std::string toWrite = FromNetASCII(Data_);
        out.write(toWrite.c_str(), toWrite.length());
    }

protected:
    virtual void Append(const std::string& data) override {
        Data_ += data;
    }

private:
    std::string Filename_;
    std::string Data_;
};

class TRequestVisitor : public TExpectingPacketVisitorBase {
public:
    virtual void VisitRequestPacket(const TRequestPacket& packet) override {
        Packet_ = packet;
    }

    TRequestPacket Packet() {
        return Packet_.value();
    }

private:
    std::optional<TRequestPacket> Packet_;
};

}

class TServer::TImpl {
public:
    TImpl(std::uint16_t port)
        : Port_(port)
        , Logger_(nullptr)
    {}

    void SetTimeout(int milliseconds) {
        Timeout_ = std::chrono::milliseconds{milliseconds};
    }

    void SetLogger(std::shared_ptr<ITransportLogger> logger) {
        Logger_ = std::move(logger);
    }

    void Run(std::ostream& errStream) {
        TTransport requests;
        if (Logger_) {
            requests.SetLogger(Logger_);
        }
        requests.Open(Port_);

        std::vector<std::unique_ptr<IClient>> clients;

        while (true) {
            std::vector<pollfd> toPoll = {
                pollfd {
                    .fd = requests.PollFD(),
                    .events = POLLIN
                }
            };
            for (auto& client : clients) {
                toPoll.emplace_back(
                    pollfd {
                        .fd = client->GetTransport().PollFD(),
                        .events = POLLIN
                    }
                );
            }

            int error = poll(toPoll.data(), toPoll.size(), -1);
            if (error < 0) {
                throw TServerError("Unable to poll");
            }

            TTimePoint pollTime = std::chrono::steady_clock::now();

            std::vector<bool> stillAlive(clients.size(), true);

            for (std::size_t i = 1; i < clients.size() + 1; i++) {
                if (toPoll[i].revents & POLLIN) {
                    auto& transport = clients[i - 1]->GetTransport();
                    auto packet = transport.Receive();
                    if (!packet.has_value()) {
                        throw TServerError("Poll behaves funny");
                    }
                    if (!clients[i - 1]->HandlePacket(std::move(packet.value()))) {
                        stillAlive[i - 1] = false;
                    }
                } else if (clients[i - 1]->SinceLast(pollTime) > Timeout_) {
                    errStream << "Server: timeout" << std::endl;
                    stillAlive[i - 1] = false;
                }
            }

            std::vector<std::unique_ptr<IClient>> newClients;
            for (std::size_t i = 0; i < clients.size(); i++) {
                if (stillAlive[i]) {
                    newClients.emplace_back(std::move(clients[i]));
                }
            }
            clients = std::move(newClients);

            if (toPoll[0].revents & POLLIN) {
                auto client = AcceptClient(errStream, requests);
                if (client.has_value()) {
                    clients.emplace_back(std::move(client.value()));
                }
            }
        }
    }

private:
    std::uint16_t Port_;
    std::chrono::milliseconds Timeout_{2000};
    std::shared_ptr<ITransportLogger> Logger_;

    std::optional<std::unique_ptr<IClient>> AcceptClient(
        std::ostream& error,
        TTransport& transport
    ) {
        auto mbPacket = transport.Receive();
        if (!mbPacket.has_value())
            throw TServerError("Poll behaves funny");
        auto packet = std::move(mbPacket.value());

        if (std::holds_alternative<TParsePacketError>(packet.Packet)) {
            auto message = std::get<TParsePacketError>(packet.Packet).what();
            error << "Server: " << message << std::endl;
            TErrorPacket err {
                TErrorPacket::EType::ILLEGAL_OPCODE,
                message
            };
            transport.Send(packet.From, &err);
            return {};
        }

        auto requestPacket = std::move(std::get<std::unique_ptr<IPacket>>(packet.Packet));
        TRequestVisitor visitor;
        requestPacket->Accept(&visitor);

        if (visitor.ReceivedError()) {
            error << "Client: " << visitor.GetReceivedError().Message() << std::endl;
            return {};
        }

        if (visitor.AnswerError()) {
            error << "Server: " << visitor.GetAnswerError().Message() << std::endl;
            transport.Send(packet.From, &visitor.GetAnswerError());
            return {};
        }

        TTransport answer;
        if (Logger_) {
            answer.SetLogger(Logger_);
        }
        answer.Open();

        std::unique_ptr<IClient> ret;

        switch (visitor.Packet().Type()) {
        case TRequestPacket::EType::READ:
            switch (visitor.Packet().Mode()) {
            case ETransferMode::OCTET:
                ret.reset(
                    new TOctetReadingClient(
                        std::move(packet.From),
                        std::move(answer),
                        error,
                        visitor.Packet().Filename()
                    )
                );
                break;
            case ETransferMode::NETASCII:
                ret.reset(
                    new TNetAsciiReadingClient(
                        std::move(packet.From),
                        std::move(answer),
                        error,
                        visitor.Packet().Filename()
                    )
                );
            }
            break;

        case TRequestPacket::EType::WRITE:
            switch (visitor.Packet().Mode()) {
            case ETransferMode::OCTET:
                ret.reset(
                    new TOctetWritingClient(
                        std::move(packet.From),
                        std::move(answer),
                        error,
                        visitor.Packet().Filename()
                    )
                );
                break;
            case ETransferMode::NETASCII:
                ret.reset(
                    new TNetAsciiWritingClient(
                        std::move(packet.From),
                        std::move(answer),
                        error,
                        visitor.Packet().Filename()
                    )
                );
            }
        }

        std::unique_ptr<IPacket> copy(
            new TRequestPacket(visitor.Packet())
        );
        if (!ret->Handle(std::move(copy))) {
            return {};
        }

        return ret;
    }
};

TServer::TServer(std::uint16_t port)
    : Impl_(new TImpl(port))
{}

TServer::~TServer() = default;

void TServer::SetTimeout(int milliseconds) {
    Impl_->SetTimeout(milliseconds);
}

void TServer::SetLogger(std::shared_ptr<ITransportLogger> logger) {
    Impl_->SetLogger(std::move(logger));
}

void TServer::Run(std::ostream& errStream) {
    Impl_->Run(errStream);
}

}
