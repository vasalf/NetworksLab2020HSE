#include <doctest/doctest.h>

#include <Socket.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <random>

namespace {

// A mocked socket that can report all of the data accepted by Write()
class IMockSocket : public NChat::ISocket {
public:
    virtual const std::string& GetAcceptedData() const = 0;
};

// A good socket that always gives every single byte he can and accepts all
// bytes it is given.
class TGoodSocket : public IMockSocket {
public:
    TGoodSocket(const std::string& give)
        : Give_(give)
    {}

    virtual int Read(char* data, int len) override {
        int toGive = std::min<int>(len, Give_.size() - Start_);
        std::memcpy(data, Give_.data() + Start_, toGive);
        *(data + toGive) = '\0';
        Start_ += toGive;
        return toGive;
    }

    virtual int Write(const char *data, int len) override {
        std::copy(data, data + len, std::back_inserter(Take_));
        return len;
    }

    virtual const std::string& GetAcceptedData() const override {
        return Take_;
    }

private:
    int Start_ = 0;
    std::string Give_;
    std::string Take_;
};

// A bad socket that gives and accepts only one byte each time.
class TBadSocket : public IMockSocket {
public:
    TBadSocket(const std::string& give)
        : Give_(give)
    {}

    virtual int Read(char* data, int len) override {
        if (len == 0 || Start_ == (int)Give_.size()) {
            *data = '\0';
            return 0;
        }
        *(data++) = Give_[Start_++];
        *data = '\0';
        return 1;
    }

    virtual int Write(const char *data, int len) override {
        if (len == 0) {
            return 0;
        }
        Take_.push_back(*data);
        return 1;
    }

    virtual const std::string& GetAcceptedData() const override {
        return Take_;
    }

private:
    int Start_ = 0;
    std::string Give_;
    std::string Take_;
};

// A flaky socket that gives and accepts random amount of bytes each time.
class TFlakySocket : public IMockSocket {
public:
    TFlakySocket(const std::string& give)
        : Give_(give)
        , Rnd_(179)
    {}

    virtual int Read(char* data, int len) override {
        int toGive = std::min<int>(len, Give_.size() - Start_);
        if (toGive > 0)
            toGive = std::uniform_int_distribution(1, toGive)(Rnd_);
        std::memcpy(data, Give_.data() + Start_, toGive);
        *(data + toGive) = '\0';
        Start_ += toGive;
        return toGive;
    }

    virtual int Write(const char *data, int len) override {
        len = std::uniform_int_distribution(0, len)(Rnd_);
        std::copy(data, data + len, std::back_inserter(Take_));
        return len;
    }

    virtual const std::string& GetAcceptedData() const override {
        return Take_;
    }

private:
    int Start_ = 0;
    std::string Give_;
    std::string Take_;
    std::mt19937 Rnd_;
};

std::string LongText() {
    const char *lipsum = "Lorem ipsum dolor sit amet. ";
    // Ну не генератор же сюда втыкать.
    std::string res;
    while (res.size() < 1000)
        res += lipsum;
    return res;
}

std::string AllBytes() {
    std::string res;
    for (int i = 0; i < 256; i++) {
        res.push_back(static_cast<unsigned char>(i));
    }
    return res;
}

std::string AllBytesTwice() {
    std::string res;
    for (int i = 0; i < 256; i++) {
        res.push_back(static_cast<unsigned char>(i));
        res.push_back(static_cast<unsigned char>(i));
    }
    return res;
}

}

TEST_CASE("TSocketWrapper::ReadN") {
    const char* shortText = "Some rather short text that totally fits into the buffer";

    std::string text;
    std::unique_ptr<IMockSocket> socket;

    SUBCASE("Good socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        SUBCASE("All bytes") {
            text = AllBytes();
        }
        SUBCASE("All bytes twice") {
            text = AllBytesTwice();
        }
        socket.reset(new TGoodSocket(text));
    }
    SUBCASE("Bad socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        SUBCASE("All bytes") {
            text = AllBytes();
        }
        SUBCASE("All bytes twice") {
            text = AllBytesTwice();
        }
        socket.reset(new TBadSocket(text));
    }
    SUBCASE("Flaky socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        SUBCASE("All bytes") {
            text = AllBytes();
        }
        SUBCASE("All bytes twice") {
            text = AllBytesTwice();
        }
        socket.reset(new TFlakySocket(text));
    }

    NChat::TSocketWrapper wrapper(socket.get());
    std::string actual;
    actual += wrapper.ReadN(20);
    actual += wrapper.ReadN(text.size() - 20);

    CHECK(text == actual);
}

TEST_CASE("TSocketWrapper::ReadUntil") {
    const char* shortText = "Some rather short text that totally fits into the buffer";

    std::string text;
    std::unique_ptr<IMockSocket> socket;

    SUBCASE("Good socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        socket.reset(new TGoodSocket(text));
    }
    SUBCASE("Bad socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        socket.reset(new TBadSocket(text));
    }
    SUBCASE("Flaky socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        socket.reset(new TFlakySocket(text));
    }

    NChat::TSocketWrapper wrapper(socket.get());
    std::string actual;
    std::string toAdd = wrapper.ReadUntil(' ');
    CHECK(std::find(toAdd.begin(), toAdd.end(), ' ') == toAdd.end());
    while (!toAdd.empty()) {
        actual += toAdd + " ";
        toAdd = wrapper.ReadUntil(' ');
        CHECK(std::find(toAdd.begin(), toAdd.end(), ' ') == toAdd.end());
    }

    if (text.back() != ' ')
        text.push_back(' ');

    CHECK(text == actual);
}

TEST_CASE("TSocketWrapper::Write") {
    const char* shortText = "Some rather short text that totally fits into the buffer";

    std::string text;
    std::unique_ptr<IMockSocket> socket;

    SUBCASE("Good socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        SUBCASE("All bytes") {
            text = AllBytes();
        }
        SUBCASE("All bytes twice") {
            text = AllBytesTwice();
        }
        socket.reset(new TGoodSocket(text));
    }
    SUBCASE("Bad socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        SUBCASE("All bytes") {
            text = AllBytes();
        }
        SUBCASE("All bytes twice") {
            text = AllBytesTwice();
        }
        socket.reset(new TBadSocket(text));
    }
    SUBCASE("Flaky socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        SUBCASE("All bytes") {
            text = AllBytes();
        }
        SUBCASE("All bytes twice") {
            text = AllBytesTwice();
        }
        socket.reset(new TFlakySocket(text));
    }

    NChat::TSocketWrapper wrapper(socket.get());
    wrapper.Write(text);

    CHECK(text == socket->GetAcceptedData());
}
