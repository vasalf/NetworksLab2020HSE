#pragma once

#include <Socket.h>

#include <memory>

namespace NChat {

// A very simple command line client.
class TClient {
public:
    TClient(ISocket* socket,           // Connection to the server
            const std::string& author, // Username
            std::istream& in,          // Stream to accept messages from
            std::ostream& out,         // Stream to write messages to
            bool joinThreads = false); // XXX: There's no simple non-blocking way to read
                                       //      from stdin. Now client must just do exit(0)
                                       //      to clear blocked thread reading from stdin.
                                       //      However, in tests it's better to wait for
                                       //      the threads to finish.
                                       // TODO: Make an async libc wrapper.

    ~TClient();

    // The interface is so simple that I feel too lazy to separate Model and Controller.
    void Run();

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}
