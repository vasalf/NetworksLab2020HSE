#include <Server.h>

#include <CLI/CLI11.hpp>

#include <cstdint>

int main(int argc, char* argv[]) {
    CLI::App app {"Chat server"};

    std::uint16_t port;
    app.add_option("port", port, "Port")->required();

    CLI11_PARSE(app, argc, argv);

    NChat::TServer server(port);

    try {
        server.Run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
