#include <Packet.h>

#include <algorithm>
#include <cctype>
#include <string_view>
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

enum class EOpcode {
    RRQ = 1,
    WRQ = 2,
    DATA = 3,
    ACK = 4,
    ERROR = 5
};

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

TDataPacket::TDataPacket(std::uint16_t blockID,
                         const std::string& data)
    : BlockID_(blockID)
    , Data_(data)
{}

std::string TDataPacket::ToBytes() const {
    std::string ret;
    ret.reserve(4 + Data_.size());

    /* Opcode */
    AppendIntegral(ret, static_cast<std::uint16_t>(EOpcode::DATA));

    /* Block id */
    AppendIntegral<std::uint16_t>(ret, BlockID_);

    /* Data */
    ret += Data_;

    return ret;
}

void TDataPacket::Accept(IPacketVisitor* visitor) const {
    visitor->VisitDataPacket(*this);
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
    AppendIntegral(ret, static_cast<std::uint16_t>(EOpcode::ACK));

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
        return "Illegal TFTP operation";
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
    AppendIntegral(ret, static_cast<std::uint16_t>(EOpcode::ERROR));

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

TParsePacketError::TParsePacketError(TErrorPacket::EType type, const std::string& message)
    : std::runtime_error(message)
    , Type_(type)
    , Message_(message)
{}

TErrorPacket::EType TParsePacketError::Type() const {
    return Type_;
}

const std::string& TParsePacketError::Message() const {
    return Message_;
}

void TExpectingPacketVisitorBase::VisitRequestPacket(const TRequestPacket&) {
    ErrorAnswer_.reset(
        new TErrorPacket(TErrorPacket::EType::ILLEGAL_OPCODE)
    );
}

void TExpectingPacketVisitorBase::VisitDataPacket(const TDataPacket&) {
    ErrorAnswer_.reset(
        new TErrorPacket(TErrorPacket::EType::ILLEGAL_OPCODE)
    );
}

void TExpectingPacketVisitorBase::VisitAcknowledgePacket(const TAcknowledgePacket&) {
    ErrorAnswer_.reset(
        new TErrorPacket(TErrorPacket::EType::ILLEGAL_OPCODE)
    );
}

void TExpectingPacketVisitorBase::VisitErrorPacket(const TErrorPacket& error) {
    ErrorRecvd_.reset(
        new TErrorPacket(error)
    );
}

bool TExpectingPacketVisitorBase::ReceivedError() const {
    return (bool)ErrorRecvd_;
}

TErrorPacket& TExpectingPacketVisitorBase::GetReceivedError() {
    return *ErrorRecvd_;
}

bool TExpectingPacketVisitorBase::AnswerError() const {
    return (bool)ErrorAnswer_;
}

TErrorPacket& TExpectingPacketVisitorBase::GetAnswerError() {
    return *ErrorAnswer_;
}

void TExpectingPacketVisitorBase::SetErrorAnswer(TErrorPacket* packet) {
    ErrorAnswer_.reset(packet);
}

namespace {

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
T ReadIntegral(std::string_view& packet) {
    T ret = 0;
    T n = sizeof(T);
    for (int i = 0; i < n; i++) {
        if (packet.empty()) {
            throw TParsePacketError(TErrorPacket::EType::UNDEFINED, "Packet is too short");
        }
        ret |= static_cast<T>(packet[0]) << 8 * (n - i - 1);
        packet.remove_prefix(1);
    }
    return ret;
}

std::string_view ReadUntilZero(std::string_view& packet) {
    std::size_t pos = std::min(packet.size(), packet.find('\0'));
    std::string_view ret(packet.begin(), pos);
    packet.remove_prefix(std::min(packet.size(), pos + 1));
    return ret;
}

template<class T>
std::string to_lower(T container) {
    std::string ret;
    ret.reserve(container.size());
    std::transform(container.begin(), container.end(),
                   std::back_inserter(ret),
                   [](char c) { return std::tolower(c); });
    return ret;
}

std::unique_ptr<IPacket> ParseRequest(const std::string& packet) {
    std::string_view s(packet.c_str(), packet.length());

    auto type = ReadIntegral<std::uint16_t>(s);
    auto filename = ReadUntilZero(s);
    auto modeStr = ReadUntilZero(s);

    ETransferMode mode;
    if (to_lower(modeStr) == "netascii") {
        mode = ETransferMode::NETASCII;
    } else if (to_lower(modeStr) == "octet") {
        mode = ETransferMode::OCTET;
    } else {
        throw TParsePacketError(TErrorPacket::EType::UNDEFINED, "Illegal mode");
    }

    return std::make_unique<TRequestPacket>(
        static_cast<TRequestPacket::EType>(type),
        FromNetASCII(std::string(filename)),
        mode
    );
}

std::unique_ptr<IPacket> ParseData(std::string_view packet) {
    auto blockID = ReadIntegral<std::uint16_t>(packet);
    return std::make_unique<TDataPacket>(blockID, std::string(packet));
}

std::unique_ptr<IPacket> ParseAcknowledgement(std::string_view packet) {
    return std::make_unique<TAcknowledgePacket>(ReadIntegral<std::uint16_t>(packet));
}

std::unique_ptr<IPacket> ParseError(std::string_view packet) {
    auto errCode = ReadIntegral<std::uint16_t>(packet);
    auto message = ReadUntilZero(packet);
    return std::make_unique<TErrorPacket>(
        static_cast<TErrorPacket::EType>(errCode),
        FromNetASCII(std::string(message))
    );
}

}

std::unique_ptr<IPacket> ParsePacket(const std::string& packet) {
    std::string_view s(packet.c_str(), packet.length());

    auto opcode = ReadIntegral<std::uint16_t>(s);

    switch (static_cast<EOpcode>(opcode)) {
    case EOpcode::RRQ:
    case EOpcode::WRQ:
        return ParseRequest(packet);
    case EOpcode::DATA:
        return ParseData(s);
    case EOpcode::ACK:
        return ParseAcknowledgement(s);
    case EOpcode::ERROR:
        return ParseError(s);
    }

    throw TParsePacketError(TErrorPacket::EType::ILLEGAL_OPCODE, "");
}

}
