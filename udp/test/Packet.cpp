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
            new NTFTP::TDataPacket(
                NTFTP::ETransferMode::OCTET,
                1,
                "some data at the end"
            )
        );
        expected.push_back('\0');
        expected += "\3";
        expected.push_back('\0');
        expected += "\1some data at the end";
    }

    SUBCASE("Data in netascii") {
        packet.reset(
            new NTFTP::TDataPacket(
                NTFTP::ETransferMode::NETASCII,
                1,
                "some data\r\n at the end"
            )
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
