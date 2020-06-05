#include <Client.h>

#include <CLI/CLI11.hpp>

#include <iostream>
#include <fstream>

class TVerboseLogger : public NTFTP::IClientLogger {
    class TPacketVisitor : public NTFTP::IPacketVisitor {
    public:
        TPacketVisitor() = default;
        virtual ~TPacketVisitor() = default;

        virtual void VisitRequestPacket(const NTFTP::TRequestPacket& packet) override {
            switch (packet.Type()) {
            case NTFTP::TRequestPacket::EType::READ:
                std::cout << "Read";
                break;
            case NTFTP::TRequestPacket::EType::WRITE:
                std::cout << "Write";
                break;
            }
            std::cout << "Request "
                      << "Filename=\"" << packet.Filename() << "\" ";

            std::cout << "Mode=";
            switch (packet.Mode()) {
            case NTFTP::ETransferMode::NETASCII:
                std::cout << "NetASCII";
                break;
            case NTFTP::ETransferMode::OCTET:
                std::cout << "Octet";
                break;
            }
            std::cout << std::endl;
        }

        virtual void VisitDataPacket(const NTFTP::TDataPacket& packet) override {
            std::cout << "Data "
                      << "BlockID=" << packet.BlockID() << " "
                      << "Data=[" << packet.Data().length() << " bytes]" << std::endl;
        }

        virtual void VisitAcknowledgePacket(const NTFTP::TAcknowledgePacket& packet) override {
            std::cout << "Acknowledge "
                      << "BlockID=" << packet.BlockID() << std::endl;
        }

        virtual void VisitErrorPacket(const NTFTP::TErrorPacket& packet) override {
            std::cout << "Error "
                      << "Type=" << static_cast<int>(packet.Type()) << " "
                      << "Message=\"" << packet.Message() << "\"" << std::endl;
        }
    };

public:
    TVerboseLogger() = default;
    virtual ~TVerboseLogger() = default;

    virtual void OnSend(NTFTP::IPacket* packet) {
        std::cout << "SEND ";
        packet->Accept(&Visitor_);
    }

    virtual void OnReceive(NTFTP::IPacket* packet) {
        std::cout << "RECV ";
        packet->Accept(&Visitor_);
    }

private:
    TPacketVisitor Visitor_;
};

void Read(NTFTP::TClient& client, const std::string& filename) {
    std::ofstream out(filename);

    try {
        client.Read(filename, out);
    } catch(NTFTP::TClientError& e) {
        std::cerr << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    CLI::App app("TFTP client");

    std::string hostname;
    app.add_option("hostname", hostname, "Server hostname")->required();

    int timeout = -1;
    app.add_option("-t,--timeout", timeout, "Timeout (milliseconds), default = 2000");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Print all packets");

    CLI11_PARSE(app, argc, argv);

    NTFTP::TClient client(hostname);
    if (timeout > 0) {
        client.SetTimeout(timeout);
    }
    if (verbose) {
        client.SetLogger(std::make_unique<TVerboseLogger>());
    }

    while (true) {
        std::cout << "> ";
        std::cout.flush();

        std::string command, file;
        std::cin >> command >> file;

        if (std::cin.eof()) {
            break;
        }

        if (command == "read") {
            Read(client, file);
        } else {
            std::cerr << "Unknown command" << std::endl;
        }
    }

    return 0;
}
