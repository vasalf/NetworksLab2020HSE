#pragma once

#include <stdexcept>
#include <string>
#include <queue>

namespace NChat {

class TSocketError : public std::runtime_error {
public:
    TSocketError(int errno);

    int Errno() const;

private:
    int Errno_;
};

// Mockable interface for testing
class ISocket {
public:
    ISocket() = default;
    virtual ~ISocket() = default;

    virtual int Read(char* data, int len) = 0;
    virtual int Write(const char* data, int len) = 0;
};

// A real socket created from a file descriptor.
class TFileDescriptorSocket : public ISocket {
public:
    TFileDescriptorSocket(int fd);
    virtual ~TFileDescriptorSocket();

    virtual int Read(char* data, int len) override;
    virtual int Write(const char* data, int len) override;

private:
    int FD_;
};

// A buffered wrapper for comfortable reading from a socket.
// Does not own the wrapped socket.
class TSocketWrapper {
public:
    TSocketWrapper(ISocket* socket);
    ~TSocketWrapper() = default;

    // Read exactly n bytes
    std::string ReadN(int n);

    // Read until the nearest delimiter char
    std::string ReadUntil(char delimiter);

    // Write the given string
    void Write(const std::string& data);

    // Return true if there is retrieved cached data left
    bool HasCachedInput();

private:
    static constexpr int BufSize = 256;

    ISocket* Socket_;

    void ReadBuf();

    std::string Buffer_;
    int BStart_ = 0;
};

}
