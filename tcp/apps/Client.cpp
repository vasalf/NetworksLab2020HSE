#include <Client.h>
#include <Socket.h>

#include <CLI/CLI11.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <iostream>

int main (int argc, char* argv[]) {
    CLI::App app("Chat client");

    std::string hostname;
    app.add_option("hostname", hostname, "Server hostname")->required();

    std::string port;
    app.add_option("port", port, "Server port")->required();

    std::string author = "anonymous";
    app.add_option("-n,--name", author, "Your name in the chat");

    CLI11_PARSE(app, argc, argv);

    // Resolve hostname
    addrinfo hints;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addrs;

    int err = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &addrs);
    if (err != 0) {
        std::cerr << hostname << ": " << gai_strerror(err) << std::endl;
        return 1;
    }

    int fileDescriptor = -1;
    for (addrinfo* addr = addrs; addr != NULL; addr = addr->ai_next) {
        fileDescriptor = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fileDescriptor <= 0) {
            std::cerr << "Error opening socket" << std::endl;
            return 1;
        }
        if (connect(fileDescriptor, addr->ai_addr, addr->ai_addrlen) == 0) {
            break;
        }
        close(fileDescriptor);
        fileDescriptor = -1;
    }

    freeaddrinfo(addrs);

    if (fileDescriptor == -1) {
        std::cerr << "Could not connect to server" << std::endl;
        return 1;
    }

    NChat::TFileDescriptorSocket serverSocket(fileDescriptor);
    NChat::TClient client(&serverSocket, author, std::cin, std::cout);
    client.Run();

    exit(0);
}
