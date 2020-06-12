#include <Client.h>
#include <Transport.h>

#include <CLI/CLI11.hpp>

#include <iostream>
#include <fstream>

void Read(NTFTP::TClient& client, const std::string& filename) {
    std::ofstream out(filename);

    try {
        client.Read(filename, out);
    } catch(NTFTP::TClientError& e) {
        std::cerr << e.what() << std::endl;
    }
}

void Write(NTFTP::TClient& client, const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);

    try {
        client.Write(filename, in);
    } catch(NTFTP::TClientError& e) {
        std::cerr << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    CLI::App app("Trivial FTP client");

    std::string hostname;
    app.add_option("hostname", hostname, "Server hostname")->required();

    std::uint16_t port = 69;
    app.add_option("-p,--port", port, "Server port, default: 69");

    int timeout = -1;
    app.add_option("-t,--timeout", timeout, "Timeout (milliseconds), default: 2000");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Print all packets");

    CLI11_PARSE(app, argc, argv);

    NTFTP::TClient client(hostname, port);
    if (timeout > 0) {
        client.SetTimeout(timeout);
    }
    if (verbose) {
        client.SetLogger(std::make_shared<NTFTP::TVerboseTransportLogger>(std::cout));
    }

    while (true) {
        std::cout << "> ";
        std::cout.flush();

        std::string command, file;
        std::cin >> command;

        if (std::cin.eof()) {
            break;
        }

        if (command == "help") {
            std::cout << "read FILENAME\tGet file from the server" << std::endl;
            std::cout << "get FILENAME\tGet file from the server" << std::endl;
            std::cout << "write FILENAME\tPut file to the server" << std::endl;
            std::cout << "put FILENAME\tPut file to the server" << std::endl;
        } else if (command == "read" || command == "get") {
            std::cin >> file;
            Read(client, file);
        } else if (command == "write" || command == "put") {
            std::cin >> file;
            Write(client, file);
        } else {
            std::cerr << "Unknown command" << std::endl;
        }
    }

    return 0;
}
