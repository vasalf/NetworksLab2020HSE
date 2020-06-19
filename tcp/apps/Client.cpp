#include <Client.h>
#include <Socket.h>

#include <CLI/CLI11.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
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
    hints.ai_flags = 0;

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
    NChat::TFileDescriptorSocket stdinSocket(STDIN_FILENO);

    NChat::TClient client(&serverSocket, &stdinSocket, author, std::cout);

    std::vector<pollfd> pollFDs {
        pollfd {
            .fd = fileDescriptor,
            .events = POLLIN
        },
        pollfd {
            .fd = STDIN_FILENO,
            .events = POLLIN
        }
    };

    while (true) {
        int err = poll(pollFDs.data(), pollFDs.size(), -1);
        if (err < 0) {
            std::cerr << "Poll error" << std::endl;
            return 1;
        }
        if (pollFDs[0].revents & POLLIN) {
            if (!client.OnServerRead()) {
                return 0;
            }
        }
        if (pollFDs[1].revents & POLLIN) {
            if (!client.OnStdinRead()) {
                return 0;
            }
        }
    }
}
