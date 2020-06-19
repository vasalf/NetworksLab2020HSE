#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace NTFTP {

std::string ToNetASCII(const std::string&);
std::string FromNetASCII(const std::string&);

enum class ETransferMode {
    NETASCII,
    OCTET,
};

class IPacketVisitor;

class IPacket {
public:
    IPacket() = default;
    virtual ~IPacket() = default;

    virtual std::string ToBytes() const = 0;
    virtual void Accept(IPacketVisitor* visitor) const = 0;
};

class TRequestPacket : public IPacket {
public:
    enum class EType {
        READ = 1,
        WRITE = 2
    };

    // Filename given in raw format
    TRequestPacket(EType type, const std::string& filename, ETransferMode mode);
    ~TRequestPacket() = default;

    virtual std::string ToBytes() const override;
    virtual void Accept(IPacketVisitor* visitor) const override;

    EType Type() const;

    // Filename in raw format
    const std::string& Filename() const;
    ETransferMode Mode() const;

private:
    EType Type_;
    std::string Filename_;
    ETransferMode Mode_;
};

class TDataPacket : public IPacket {
public:
    // Data is given encoded
    TDataPacket(std::uint16_t blockID, const std::string& data);
    ~TDataPacket() = default;

    virtual std::string ToBytes() const override;
    virtual void Accept(IPacketVisitor* visitor) const override;

    std::uint16_t BlockID() const;

    // Encoded data
    const std::string& Data() const;

private:
    std::uint16_t BlockID_;
    std::string Data_;
};

class TAcknowledgePacket : public IPacket {
public:
    TAcknowledgePacket(std::uint16_t blockID);
    ~TAcknowledgePacket() = default;

    virtual std::string ToBytes() const override;
    virtual void Accept(IPacketVisitor* visitor) const override;

    std::uint16_t BlockID() const;

private:
    std::uint16_t BlockID_;
};

class TErrorPacket : public IPacket {
public:
    enum class EType {
        UNDEFINED = 0,
        FILE_NOT_FOUND = 1,
        ACCESS_VIOLATION = 2,
        DISK_FULL = 3,
        ILLEGAL_OPCODE = 4,
        UNKNOWN_TRANSFER_ID = 5,
        FILE_EXISTS = 6,
        NO_USER = 7, /* WTF?? */
    };

    // Message is given raw.
    // If message is empty, it is picked from RFC 1350 error codes.
    TErrorPacket(EType type, std::string message = "");
    ~TErrorPacket() = default;

    virtual std::string ToBytes() const override;
    virtual void Accept(IPacketVisitor* visitor) const override;

    EType Type() const;

    // Raw message
    const std::string& Message() const;

private:
    EType Type_;
    std::string Message_;
};

class IPacketVisitor {
public:
    IPacketVisitor() = default;
    virtual ~IPacketVisitor() = default;

    virtual void VisitRequestPacket(const TRequestPacket& packet) = 0;
    virtual void VisitDataPacket(const TDataPacket& packet) = 0;
    virtual void VisitAcknowledgePacket(const TAcknowledgePacket& packet) = 0;
    virtual void VisitErrorPacket(const TErrorPacket& packet) = 0;
};

class TExpectingPacketVisitorBase : public IPacketVisitor {
public:
    TExpectingPacketVisitorBase() = default;
    virtual ~TExpectingPacketVisitorBase() = default;

    virtual void VisitRequestPacket(const TRequestPacket&) override;
    virtual void VisitDataPacket(const TDataPacket&) override;
    virtual void VisitAcknowledgePacket(const TAcknowledgePacket&) override;
    virtual void VisitErrorPacket(const TErrorPacket& error) override;

    bool ReceivedError() const;
    TErrorPacket& GetReceivedError();

    bool AnswerError() const;
    TErrorPacket& GetAnswerError();

protected:
    void SetErrorAnswer(TErrorPacket* packet);

private:
    std::unique_ptr<TErrorPacket> ErrorRecvd_;
    std::unique_ptr<TErrorPacket> ErrorAnswer_;
};

class TParsePacketError : public std::runtime_error {
public:
    TParsePacketError(TErrorPacket::EType type, const std::string& message);

    TErrorPacket::EType Type() const;
    const std::string& Message() const;

private:
    TErrorPacket::EType Type_;
    std::string Message_;
};

std::unique_ptr<IPacket> ParsePacket(const std::string& packet);

}
