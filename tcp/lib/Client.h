#pragma once

#include <Socket.h>

#include <memory>

namespace NChat {

// A very simple command line client.
class TClient {
public:
    TClient(ISocket* netSocket,        // Connection to the server
            ISocket* stdinSocket,      // Standard in
            const std::string& author, // Username
            std::ostream& out);        // Stream to write messages to
    ~TClient();

    // Time to read from server
    bool OnServerRead();

    // Time to read from input
    bool OnStdinRead();

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}
