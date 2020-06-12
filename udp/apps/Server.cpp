#include <Server.h>
#include <Transport.h>

#include <CLI/CLI11.hpp>

#include <iostream>

int main(int argc, char* argv[]) {
    CLI::App app("Trivial FTP server");

    std::uint16_t port = 69;
    app.add_option("-p,--port", port, "Server port, default: 69");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Print all packets");

    CLI11_PARSE(app, argc, argv);

    NTFTP::TServer server(port);
    if (verbose) {
        server.SetLogger(std::make_shared<NTFTP::TVerboseTransportLogger>(std::cout));
    }

    server.Run(std::cerr);

    return 0;
}
