#include <util/MockSocket.h>

namespace NChat {

TGoodSocket::TGoodSocket(const std::string& give)
    : Give_(give)
{}

int TGoodSocket::Read(char* data, int len) {
    int toGive = std::min<int>(len, Give_.size() - Start_);
    std::memcpy(data, Give_.data() + Start_, toGive);
    *(data + toGive) = '\0';
    Start_ += toGive;
    return toGive;
}

int TGoodSocket::Write(const char *data, int len) {
    std::copy(data, data + len, std::back_inserter(Take_));
    return len;
}

const std::string& TGoodSocket::GetAcceptedData() const {
    return Take_;
}

TBadSocket::TBadSocket(const std::string& give)
    : Give_(give)
{}

int TBadSocket::Read(char* data, int len) {
    if (len == 0 || Start_ == (int)Give_.size()) {
        *data = '\0';
        return 0;
    }
    *(data++) = Give_[Start_++];
    *data = '\0';
    return 1;
}

int TBadSocket::Write(const char* data, int len) {
    if (len == 0) {
        return 0;
    }
    Take_.push_back(*data);
    return 1;
}

const std::string& TBadSocket::GetAcceptedData() const {
    return Take_;
}

TFlakySocket::TFlakySocket(const std::string& give)
    : Give_(give)
    , Rnd_(179)
{}

int TFlakySocket::Read(char* data, int len) {
    int toGive = std::min<int>(len, Give_.size() - Start_);
    if (toGive > 0)
        toGive = std::uniform_int_distribution(1, toGive)(Rnd_);
    std::memcpy(data, Give_.data() + Start_, toGive);
    *(data + toGive) = '\0';
    Start_ += toGive;
    return toGive;
}

int TFlakySocket::Write(const char* data, int len) {
    len = std::uniform_int_distribution(0, len)(Rnd_);
    std::copy(data, data + len, std::back_inserter(Take_));
    return len;
}

const std::string& TFlakySocket::GetAcceptedData() const {
    return Take_;
}

}
