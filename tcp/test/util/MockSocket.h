#pragma once

#include <Socket.h>

#include <random>
#include <cstring>

namespace NChat {

// A mocked socket that can report all of the data accepted by Write()
class IMockSocket : public NChat::ISocket {
public:
    virtual const std::string& GetAcceptedData() const = 0;
};

// A good socket that always gives every single byte he can and accepts all
// bytes it is given.
class TGoodSocket : public IMockSocket {
public:
    TGoodSocket(const std::string& give);

    virtual int Read(char* data, int len);
    virtual int Write(const char *data, int len) override;
    virtual const std::string& GetAcceptedData() const override;

private:
    int Start_ = 0;
    std::string Give_;
    std::string Take_;
};

// A bad socket that gives and accepts only one byte each time.
class TBadSocket : public IMockSocket {
public:
    TBadSocket(const std::string& give);

    virtual int Read(char* data, int len) override;
    virtual int Write(const char *data, int len) override;
    virtual const std::string& GetAcceptedData() const override;

private:
    int Start_ = 0;
    std::string Give_;
    std::string Take_;
};

// A flaky socket that gives and accepts random amount of bytes each time.
class TFlakySocket : public IMockSocket {
public:
    TFlakySocket(const std::string& give);

    virtual int Read(char* data, int len) override;
    virtual int Write(const char *data, int len) override;
    virtual const std::string& GetAcceptedData() const override;

private:
    int Start_ = 0;
    std::string Give_;
    std::string Take_;
    std::mt19937 Rnd_;
};

}
