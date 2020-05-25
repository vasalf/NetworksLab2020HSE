#include <doctest/doctest.h>

#include <Socket.h>
#include <util/MockSocket.h>

#include <algorithm>
#include <memory>

namespace {

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
    std::unique_ptr<NChat::IMockSocket> socket;

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
        socket.reset(new NChat::TGoodSocket(text));
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
        socket.reset(new NChat::TBadSocket(text));
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
        socket.reset(new NChat::TFlakySocket(text));
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
    std::unique_ptr<NChat::IMockSocket> socket;

    SUBCASE("Good socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        socket.reset(new NChat::TGoodSocket(text));
    }
    SUBCASE("Bad socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        socket.reset(new NChat::TBadSocket(text));
    }
    SUBCASE("Flaky socket") {
        SUBCASE("Text") {
            text = shortText;
        }
        SUBCASE("Long text") {
            text = LongText();
        }
        socket.reset(new NChat::TFlakySocket(text));
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
    std::unique_ptr<NChat::IMockSocket> socket;

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
        socket.reset(new NChat::TGoodSocket(text));
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
        socket.reset(new NChat::TBadSocket(text));
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
        socket.reset(new NChat::TFlakySocket(text));
    }

    NChat::TSocketWrapper wrapper(socket.get());
    wrapper.Write(text);

    CHECK(text == socket->GetAcceptedData());
}
