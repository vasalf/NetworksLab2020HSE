#include <Socket.h>

#include <algorithm>
#include <cstring>

#include <unistd.h>

namespace NChat {

TSocketError::TSocketError(int errno)
    : std::runtime_error(std::strerror(errno))
    , Errno_(errno)
{}

int TSocketError::Errno() const {
    return Errno_;
}

TFileDescriptorSocket::TFileDescriptorSocket(int fd)
    : FD_(fd)
{}

TFileDescriptorSocket::~TFileDescriptorSocket() {
    close(FD_);
}

int TFileDescriptorSocket::Read(char* data, int len) {
    return read(FD_, data, len);
}

int TFileDescriptorSocket::Write(const char* data, int len) {
    return write(FD_, data, len);
}

TSocketWrapper::TSocketWrapper(ISocket* socket)
    : Socket_(socket)
{
    Buffer_.reserve(BufSize);
}

std::string TSocketWrapper::ReadN(int n) {
    std::string ret;
    ret.reserve(n);

    int bufLen = Buffer_.size() - BStart_;

    if (bufLen > 0) {
        int toCopy = std::min(bufLen, n);
        ret.resize(toCopy);
        std::memcpy(ret.data(), Buffer_.data() + BStart_, toCopy);
        BStart_ += toCopy;
        n -= toCopy;
    }

    while (n > 0) {
        ReadBuf();
        int toCopy = std::min<int>(Buffer_.size(), n);
        int start = ret.size();
        ret.resize(ret.size() + toCopy);
        std::memcpy(ret.data() + start, Buffer_.data(), toCopy);
        n -= toCopy;
        BStart_ += toCopy;
    }

    return ret;
}

std::string TSocketWrapper::ReadUntil(char delimiter) {
    std::string ret;

    auto it = std::find(Buffer_.begin() + BStart_, Buffer_.end(), delimiter);
    while (it == Buffer_.end()) {
        ret.append(Buffer_, BStart_, Buffer_.size() - BStart_);
        ReadBuf();
        if (Buffer_.empty()) {
            it = Buffer_.begin();
            break;
        }
        it = std::find(Buffer_.begin() + BStart_, Buffer_.end(), delimiter);
    }

    ret.append(Buffer_, BStart_, it - Buffer_.begin() - BStart_);
    BStart_ = it - Buffer_.begin() + 1;
    if (Buffer_.empty()) {
        BStart_ = 0;
    }

    return ret;

}

void TSocketWrapper::Write(const std::string& data) {
    int written = 0;
    while (written < (int)data.size()) {
        written += Socket_->Write(data.data() + written, data.size() - written);
    }
}

bool TSocketWrapper::HasCachedInput() {
    return BStart_ < (int)Buffer_.size();
}

void TSocketWrapper::ReadBuf() {
    BStart_ = 0;
    Buffer_.resize(BufSize);
    int read = Socket_->Read(Buffer_.data(), BufSize - 1);
    Buffer_.resize(read);
}

}
