#include <doctest/doctest.h>

#include <Packet.h>

#include <memory>

TEST_CASE("NetASCII") {
    std::string raw, netascii;

    SUBCASE("No endls") {
        raw = "Some string with no \\n, \\r or \\0 characters";
        netascii = raw;
    }

    SUBCASE("Only \\n") {
        raw = "Some string with \n characters";
        netascii = "Some string with \r\n characters";
    }

    SUBCASE("Only \\r") {
        raw = "Some string with \r characters";

        netascii = "Some string with \r";
        netascii.push_back('\0');
        netascii += " characters";
    }

    SUBCASE("Both \\n and \\r") {
        raw = "Some string with \r\n characters";

        netascii = "Some string with \r";
        netascii.push_back('\0');
        netascii += "\r\n characters";
    }

    SUBCASE("\\n at the end") {
        raw = "Some string with \n";
        netascii = "Some string with \r\n";
    }

    SUBCASE("\\r at the end") {
        raw = "Some string with \r";
        netascii = "Some string with \r";
        netascii.push_back('\0');
    }

    SUBCASE("\\n at the beginning") {
        raw = "\n character";
        netascii = "\r\n character";
    }

    SUBCASE("\\r at the beginning") {
        raw = "\r character";
        netascii = "\r";
        netascii.push_back('\0');
        netascii += " character";
    }

    SUBCASE("\\n") {
        raw = "\n";
        netascii = "\r\n";
    }

    SUBCASE("\\r") {
        raw = "\r";
        netascii = "\r";
        netascii.push_back('\0');
    }

    SUBCASE("Empty string") {
        raw = "";
        netascii = "";
    }

    CHECK(netascii == NTFTP::ToNetASCII(raw));
    CHECK(raw == NTFTP::FromNetASCII(netascii));
}

TEST_CASE("TRequestPacket::ToBytes") {
    std::unique_ptr<NTFTP::IPacket> packet;
    std::string expected;

    SUBCASE("Read") {
        packet.reset(
            new NTFTP::TRequestPacket(
                NTFTP::TRequestPacket::EType::READ,
                "file.txt",
                NTFTP::ETransferMode::OCTET
            )
        );
        expected.push_back('\0');
        expected += "\1file.txt";
        expected.push_back('\0');
        expected += "octet";
        expected.push_back('\0');
    }

    SUBCASE("Write") {
        packet.reset(
            new NTFTP::TRequestPacket(
                NTFTP::TRequestPacket::EType::WRITE,
                "file.txt",
                NTFTP::ETransferMode::NETASCII
            )
        );
        expected.push_back('\0');
        expected += "\2file.txt";
        expected.push_back('\0');
        expected += "netascii";
        expected.push_back('\0');
    }

    SUBCASE("Filename with /") {
        packet.reset(
            new NTFTP::TRequestPacket(
                NTFTP::TRequestPacket::EType::READ,
                "dir/file.txt",
                NTFTP::ETransferMode::OCTET
            )
        );
        expected.push_back('\0');
        expected += "\1dir/file.txt";
        expected.push_back('\0');
        expected += "octet";
        expected.push_back('\0');
    }

    SUBCASE("Filename with \\n") {
        packet.reset(
            new NTFTP::TRequestPacket(
                NTFTP::TRequestPacket::EType::READ,
                "dir/file_with_\n_lol.txt",
                NTFTP::ETransferMode::OCTET
            )
        );
        expected.push_back('\0');
        expected += "\1dir/file_with_\r\n_lol.txt";
        expected.push_back('\0');
        expected += "octet";
        expected.push_back('\0');
    }

    CHECK(expected == packet->ToBytes());
}

TEST_CASE("TDataPacket::ToBytes") {
    std::unique_ptr<NTFTP::IPacket> packet;
    std::string expected;

    SUBCASE("Some data") {
        packet.reset(
            new NTFTP::TDataPacket(1, "some data at the end")
        );
        expected.push_back('\0');
        expected += "\3";
        expected.push_back('\0');
        expected += "\1some data at the end";
    }

    SUBCASE("Data in netascii") {
        packet.reset(
            new NTFTP::TDataPacket(1, "some data\r\n at the end")
        );
        expected.push_back('\0');
        expected += "\3";
        expected.push_back('\0');
        expected += "\1some data\r\n at the end";
    }

    CHECK(expected == packet->ToBytes());
}

TEST_CASE("TAcknowledgePacket::ToBytes") {
    std::unique_ptr<NTFTP::IPacket> packet;
    std::string expected;

    SUBCASE("Some block id") {
        packet.reset(new NTFTP::TAcknowledgePacket(1));
        expected.push_back('\0');
        expected += "\4";
        expected.push_back('\0');
        expected += "\1";
    }

    CHECK(expected == packet->ToBytes());
}

TEST_CASE("TErrorPacket::ToBytes") {
    std::unique_ptr<NTFTP::IPacket> packet;
    std::string expected;

    SUBCASE("Some error") {
        packet.reset(
            new NTFTP::TErrorPacket(
                NTFTP::TErrorPacket::EType::FILE_NOT_FOUND,
                "Is a directory"
            )
        );
        expected.push_back('\0');
        expected += "\5";
        expected.push_back('\0');
        expected += "\1Is a directory";
        expected.push_back('\0');
    }

    SUBCASE("Default message") {
        packet.reset(
            new NTFTP::TErrorPacket(
                NTFTP::TErrorPacket::EType::UNKNOWN_TRANSFER_ID
            )
        );
        expected.push_back('\0');
        expected += "\5";
        expected.push_back('\0');
        expected += "\5Unknown transfer ID";
        expected.push_back('\0');
    }

    SUBCASE("Message with \\n") {
        packet.reset(
            new NTFTP::TErrorPacket(
                NTFTP::TErrorPacket::EType::UNDEFINED,
                "Something went wrong.\nContact your network administator."
            )
        );
        expected.push_back('\0');
        expected += "\5";
        expected.push_back('\0');
        expected.push_back('\0');
        expected += "Something went wrong.\r\nContact your network administator.";
        expected.push_back('\0');
    }

    CHECK(expected == packet->ToBytes());
}

namespace {

class TPacketCheckerBase : public NTFTP::IPacketVisitor {
public:
    TPacketCheckerBase() = default;
    virtual ~TPacketCheckerBase() = default;

    virtual void VisitRequestPacket(const NTFTP::TRequestPacket&) override {
        CHECK(false);
    }

    virtual void VisitDataPacket(const NTFTP::TDataPacket&) override {
        CHECK(false);
    }

    virtual void VisitAcknowledgePacket(const NTFTP::TAcknowledgePacket&) override {
        CHECK(false);
    }

    virtual void VisitErrorPacket(const NTFTP::TErrorPacket&) override {
        CHECK(false);
    }
};

class TRequestPacketChecker : public TPacketCheckerBase {
public:
    TRequestPacketChecker(const NTFTP::TRequestPacket& expected)
        : Expected_(expected)
    {}

    TRequestPacketChecker() = default;

    virtual void VisitRequestPacket(const NTFTP::TRequestPacket& actual) override {
        CHECK(Expected_.Type() == actual.Type());
        CHECK(Expected_.Filename() == actual.Filename());
        CHECK(Expected_.Mode() == actual.Mode());
    }

private:
    NTFTP::TRequestPacket Expected_;
};

class TDataPacketChecker : public TPacketCheckerBase {
public:
    TDataPacketChecker(const NTFTP::TDataPacket& expected)
        : Expected_(expected)
    {}

    TDataPacketChecker() = default;

    virtual void VisitDataPacket(const NTFTP::TDataPacket& actual) override {
        CHECK(Expected_.BlockID() == actual.BlockID());
        CHECK(Expected_.Data() == actual.Data());
    }

private:
    NTFTP::TDataPacket Expected_;
};

class TAcknowledgePacketChecker : public TPacketCheckerBase {
public:
    TAcknowledgePacketChecker(const NTFTP::TAcknowledgePacket& expected)
        : Expected_(expected)
    {}

    TAcknowledgePacketChecker() = default;

    virtual void VisitAcknowledgePacket(const NTFTP::TAcknowledgePacket& actual) override {
        CHECK(Expected_.BlockID() == actual.BlockID());
    }

private:
    NTFTP::TAcknowledgePacket Expected_;
};

class TErrorPacketChecker : public TPacketCheckerBase {
public:
    TErrorPacketChecker(const NTFTP::TErrorPacket& expected)
        : Expected_(expected)
    {}

    TErrorPacketChecker() = default;

    virtual void VisitErrorPacket(const NTFTP::TErrorPacket& actual) override {
        CHECK(Expected_.Type() == actual.Type());
        CHECK(Expected_.Message() == actual.Message());
    }

private:
    NTFTP::TErrorPacket Expected_;
};

}

TEST_CASE("ParsePacket") {
    std::unique_ptr<NTFTP::IPacket> packet;
    std::unique_ptr<NTFTP::IPacketVisitor> checker;

    SUBCASE("Read request") {
        std::unique_ptr<NTFTP::TRequestPacket> rpacket(
            new NTFTP::TRequestPacket(
                NTFTP::TRequestPacket::EType::READ,
                "file.txt",
                NTFTP::ETransferMode::OCTET
            )
        );
        checker.reset(new TRequestPacketChecker(*rpacket));
        packet = std::move(rpacket);
    }

    SUBCASE("Write request") {
        std::unique_ptr<NTFTP::TRequestPacket> rpacket(
            new NTFTP::TRequestPacket(
                NTFTP::TRequestPacket::EType::WRITE,
                "file.txt",
                NTFTP::ETransferMode::NETASCII
            )
        );
        checker.reset(new TRequestPacketChecker(*rpacket));
        packet = std::move(rpacket);
    }

    SUBCASE("Data") {
        std::unique_ptr<NTFTP::TDataPacket> rpacket(
            new NTFTP::TDataPacket(1, "some data at the end")
        );
        checker.reset(new TDataPacketChecker(*rpacket));
        packet = std::move(rpacket);
    }

    SUBCASE("Acknowledge") {
        std::unique_ptr<NTFTP::TAcknowledgePacket> rpacket(
            new NTFTP::TAcknowledgePacket(1)
        );
        checker.reset(new TAcknowledgePacketChecker(*rpacket));
        packet = std::move(rpacket);
    }

    SUBCASE("Error") {
        std::unique_ptr<NTFTP::TErrorPacket> rpacket(
            new NTFTP::TErrorPacket(
                NTFTP::TErrorPacket::EType::UNKNOWN_TRANSFER_ID
            )
        );
        checker.reset(new TErrorPacketChecker(*rpacket));
        packet = std::move(rpacket);
    }

    std::string bytes = packet->ToBytes();
    NTFTP::ParsePacket(bytes)->Accept(checker.get());
}
