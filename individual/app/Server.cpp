#include <Server.h>

#include <CLI/CLI11.hpp>

int main(int argc, char* argv[]) {
    CLI::App app("HTTP proxy");

    std::string host;
    app.add_option("HOST", host, "Host")->required();

    std::string port;
    app.add_option("PORT", port, "Port")->required();

    CLI11_PARSE(app, argc, argv);

    NHttpProxy::TServer server;
    server.Bind(host, port);

    server.Run();

    return 0;
}
