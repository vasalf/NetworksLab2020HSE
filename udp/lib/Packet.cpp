#include <Packet.h>

#include <type_traits>

namespace NTFTP {

std::string ToNetASCII(const std::string& data) {
    std::string ret;
    ret.reserve(data.size());
    for (char c : data) {
        if (c == '\n') {
            ret.push_back('\r');
        }
        ret.push_back(c);
        if (c == '\r') {
            ret.push_back('\0');
        }
    }
    return ret;
}

std::string FromNetASCII(const std::string& data) {
    std::string ret;
    ret.reserve(data.size());
    for (int i = 0; i < (int)data.size(); i++) {
        if (i == (int)data.size() - 1) {
            ret.push_back(data[i]);
            continue;
        }
        if (data[i] == '\r' && data[i + 1] == '\n') {
            i++;
        }
        ret.push_back(data[i]);
        if (data[i] == '\r' && data[i + 1] == '\0') {
            i++;
        }
    }
    return ret;
}

namespace {

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
void AppendIntegral(std::string& to, T value) {
    std::size_t n = sizeof(T);
    for (T i = 0; i < n; i++) {
        T byte = (value >> (8 * (n - i - 1))) & 0xf;
        to.push_back(static_cast<std::uint8_t>(byte));
    }
}

std::string TransferMode(ETransferMode mode) {
    switch(mode) {
    case ETransferMode::NETASCII:
        return "netascii";
    case ETransferMode::OCTET:
        return "octet";
    }
    return "";
}

}

TRequestPacket::TRequestPacket(TRequestPacket::EType type,
                               const std::string& filename,
                               ETransferMode mode)
    : Type_(type)
    , Filename_(filename)
    , Mode_(mode)
{}

std::string TRequestPacket::ToBytes() const {
    std::string ret;
    ret.reserve(Filename_.size() + 12);

    /* Opcode */
    AppendIntegral(ret, static_cast<std::uint16_t>(Type_));

    /* Filename */
    ret += ToNetASCII(Filename_);
    ret.push_back('\0');

    /* Mode */
    ret += TransferMode(Mode_);
    ret.push_back('\0');

    return ret;
}

void TRequestPacket::Accept(IPacketVisitor* visitor) const {
    visitor->VisitRequestPacket(*this);
}

TRequestPacket::EType TRequestPacket::Type() const {
    return Type_;
}

const std::string& TRequestPacket::Filename() const {
    return Filename_;
}

ETransferMode TRequestPacket::Mode() const {
    return Mode_;
}

TDataPacket::TDataPacket(ETransferMode mode,
                         std::uint16_t blockID,
                         const std::string& data)
    : Mode_(mode)
    , BlockID_(blockID)
    , Data_(data)
{}

std::string TDataPacket::ToBytes() const {
    std::string ret;
    ret.reserve(4 + Data_.size());

    /* Opcode */
    AppendIntegral<std::uint16_t>(ret, 3);

    /* Block id */
    AppendIntegral<std::uint16_t>(ret, BlockID_);

    /* Data */
    ret += Data_;

    return ret;
}

void TDataPacket::Accept(IPacketVisitor* visitor) const {
    visitor->VisitDataPacket(*this);
}

ETransferMode TDataPacket::Mode() const {
    return Mode_;
}

std::uint16_t TDataPacket::BlockID() const {
    return BlockID_;
}

const std::string& TDataPacket::Data() const {
    return Data_;
}

TAcknowledgePacket::TAcknowledgePacket(std::uint16_t blockID)
    : BlockID_(blockID)
{}

std::string TAcknowledgePacket::ToBytes() const {
    std::string ret;
    ret.reserve(4);

    /* Opcode */
    AppendIntegral<std::uint16_t>(ret, 4);

    /* Block id */
    AppendIntegral<std::uint16_t>(ret, BlockID_);

    return ret;
}

void TAcknowledgePacket::Accept(IPacketVisitor* visitor) const {
    visitor->VisitAcknowledgePacket(*this);
}

std::uint16_t TAcknowledgePacket::BlockID() const {
    return BlockID_;
}

namespace {

std::string DefaultErrorMessage(TErrorPacket::EType errorType) {
    switch(errorType) {
    case TErrorPacket::EType::FILE_NOT_FOUND:
        return "File not found";
    case TErrorPacket::EType::ACCESS_VIOLATION:
        return "Access violation";
    case TErrorPacket::EType::DISK_FULL:
        return "Disk full or allocation exceeded";
    case TErrorPacket::EType::ILLEGAL_OPCODE:
        return "Illegal TFTP opetaion";
    case TErrorPacket::EType::UNKNOWN_TRANSFER_ID:
        return "Unknown transfer ID";
    case TErrorPacket::EType::FILE_EXISTS:
        return "File already exists";
    default:
        return "";
    }
}

}

TErrorPacket::TErrorPacket(TErrorPacket::EType type, std::string message)
    : Type_(type)
    , Message_(message)
{
    if (Message_.empty()) {
        Message_ = DefaultErrorMessage(Type_);
    }
}

std::string TErrorPacket::ToBytes() const {
    std::string ret;
    ret.reserve(4 + Message_.size());

    /* Opcode */
    AppendIntegral<std::uint16_t>(ret, 5);

    /* Error code */
    AppendIntegral(ret, static_cast<std::uint16_t>(Type_));

    /* Message */
    ret += ToNetASCII(Message_);
    ret.push_back('\0');

    return ret;
}

void TErrorPacket::Accept(IPacketVisitor* visitor) const {
    visitor->VisitErrorPacket(*this);
}

TErrorPacket::EType TErrorPacket::Type() const {
    return Type_;
}

const std::string& TErrorPacket::Message() const {
    return Message_;
}

}
