#include <Client.h>

#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <optional>
#include <random>

#include <sys/poll.h>

namespace NTFTP {

TClientError::TClientError(std::string message)
    : std::runtime_error(message)
{}

namespace {

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

class TWritePacketVisitor : public TExpectingPacketVisitorBase {
public:
    virtual void VisitAcknowledgePacket(const TAcknowledgePacket& answer) override {
        BlockID_ = answer.BlockID();
    }

    std::uint16_t BlockID() const {
        return BlockID_;
    }

private:
    std::uint16_t BlockID_;
};

}

class TClient::TImpl {
public:
    TImpl(const std::string& host, std::uint16_t port)
        : Host_(host)
        , Port_(port)
        , Logger_(nullptr)
    {}

    void SetTimeout(int milliseconds) {
        Timeout_ = milliseconds;
    }

    void SetLogger(std::shared_ptr<ITransportLogger> logger) {
        Logger_ = std::move(logger);
    }

    void Read(const std::string& filename,
              std::ostream& to) {
        TTransport transport;
        if (Logger_) {
            transport.SetLogger(Logger_);
        }
        transport.Open();

        std::uint16_t serverPort = 0;
        bool end = false;

        {
            // Read request
            TRequestPacket request(
                TRequestPacket::EType::READ,
                filename,
                ETransferMode::OCTET
            );
            transport.Send(Host_, Port_, &request);
        }
        std::size_t blockID = 1;

        while (!end) {
            auto packet = ReceivePacket(transport);
            if (std::holds_alternative<TParsePacketError>(packet.Packet)) {
                TErrorPacket error(
                    TErrorPacket::EType::ILLEGAL_OPCODE,
                    std::get<TParsePacketError>(packet.Packet).what()
                );
                transport.Send(Host_, packet.TransferID, &error);
                throw TClientError("Illegal answer from server");
            }
            auto serverAnswerPort = packet.TransferID;
            if (serverPort == 0) {
                serverPort = serverAnswerPort;
            } else if (serverPort != serverAnswerPort) {
                // Duplicate connection silently dropped as described in RFC 1350.
                TErrorPacket error(TErrorPacket::EType::UNKNOWN_TRANSFER_ID);
                transport.Send(Host_, serverAnswerPort, &error);
                continue;
            }

            TReadPacketVisitor visitor;
            std::get<std::unique_ptr<IPacket>>(packet.Packet)->Accept(&visitor);
            if (visitor.ReceivedError()) {
                throw TClientError("Server: " + visitor.GetReceivedError().Message());
            }
            if (visitor.AnswerError()) {
                transport.Send(Host_, serverPort, &visitor.GetAnswerError());
                throw TClientError("Client: " + visitor.GetAnswerError().Message());
            }
            if (visitor.DataReceived().BlockID() != blockID) {
                // Duplicate
                continue;
            }
            end = visitor.DataReceived().Data().length() < 512;
            to << visitor.DataReceived().Data();

            TAcknowledgePacket answer(blockID);
            transport.Send(Host_, serverPort, &answer);

            ++blockID;
        }
    }

    void Write(const std::string& filename, std::istream& data) {
        TTransport transport;
        if (Logger_) {
            transport.SetLogger(Logger_);
        }
        transport.Open();

        std::uint16_t transferID;

        {
            // Write request
            TRequestPacket request(
                TRequestPacket::EType::WRITE,
                filename,
                ETransferMode::OCTET
            );
            transport.Send(Host_, Port_, &request);

            auto ack = ReceivePacket(transport);
            if (std::holds_alternative<TParsePacketError>(ack.Packet)) {
                TErrorPacket error(
                    TErrorPacket::EType::ILLEGAL_OPCODE,
                    std::get<TParsePacketError>(ack.Packet).what()
                );
                transport.Send(Host_, ack.TransferID, &error);
                throw TClientError("Illegal answer from server");
            }
            transferID = ack.TransferID;

            TWritePacketVisitor visitor;
            std::get<std::unique_ptr<IPacket>>(ack.Packet)->Accept(&visitor);
            if (visitor.ReceivedError()) {
                throw TClientError("Server: " + visitor.GetReceivedError().Message());
            }
            if (visitor.AnswerError()) {
                transport.Send(Host_, transferID, &visitor.GetAnswerError());
                throw TClientError("Client: " + visitor.GetAnswerError().Message());
            }
            if (visitor.BlockID() != 0) {
                TErrorPacket error(
                    TErrorPacket::EType::ILLEGAL_OPCODE,
                    "Unexpected BlockID"
                );
                transport.Send(Host_, transferID, &error);
                throw TClientError("Unexpected BlockID in server answer");
            }
        }

        bool end = false;
        std::uint16_t blockID = 1;
        while (!end) {
            char block[512];
            data.read(block, sizeof(block));
            if (data.eof()) {
                end = true;
            }
            TDataPacket packet(
                blockID,
                std::string(block, data.gcount())
            );

            transport.Send(Host_, transferID, &packet);

            bool receivedAck = false;
            while (!receivedAck) {
                auto ack = ReceivePacket(transport);
                if (std::holds_alternative<TParsePacketError>(ack.Packet)) {
                    TErrorPacket error(
                        TErrorPacket::EType::ILLEGAL_OPCODE,
                        std::get<TParsePacketError>(ack.Packet).what()
                    );
                    transport.Send(Host_, transferID, &error);
                    throw TClientError("Illegal answer from server");
                }

                TWritePacketVisitor visitor;
                std::get<std::unique_ptr<IPacket>>(ack.Packet)->Accept(&visitor);
                if (visitor.ReceivedError()) {
                    throw TClientError("Server: " + visitor.GetReceivedError().Message());
                }
                if (visitor.AnswerError()) {
                    transport.Send(Host_, transferID, &visitor.GetAnswerError());
                    throw TClientError("Client: " + visitor.GetAnswerError().Message());
                }
                receivedAck = true;
                if (visitor.BlockID() != blockID) {
                    receivedAck = false;
                }
            }

            blockID++;
        }
    }

private:
    int Timeout_ = 2000; // Default timeout
    std::string Host_;
    std::uint16_t Port_;
    std::shared_ptr<ITransportLogger> Logger_;

    TTransport::TReceiveResult ReceivePacket(TTransport& transport) {
        pollfd listen {
            .fd = transport.PollFD(),
            .events = POLLIN
        };

        int error = poll(&listen, 1, Timeout_);
        if (error < 0) {
            throw TClientError("Unable to poll");
        }
        if (error == 0) {
            throw TClientError("Timeout");
        }

        auto ret = transport.Receive();
        if (!ret.has_value()) {
            throw TClientError("Timeout");
        }
        return std::move(ret.value());
    }
};

TClient::TClient(const std::string& host, std::uint16_t port)
    : Impl_(new TImpl(host, port))
{}

TClient::~TClient() = default;

void TClient::SetTimeout(int milliseconds) {
    Impl_->SetTimeout(milliseconds);
}

void TClient::SetLogger(std::shared_ptr<ITransportLogger> logger) {
    Impl_->SetLogger(std::move(logger));
}

void TClient::Read(const std::string& filename, std::ostream& to) {
    Impl_->Read(filename, to);
}

void TClient::Write(const std::string& filename, std::istream& data) {
    Impl_->Write(filename, data);
}

}
