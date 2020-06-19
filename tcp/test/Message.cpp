#include <doctest/doctest.h>

#include <Message.h>
#include <util/MockSocket.h>

#include <ctime>
#include <memory>


TEST_CASE("TMessage::Show") {
    std::tm tm {
        .tm_sec = 30,
        .tm_min = 0,
        .tm_hour = 0,
        .tm_mday = 29,
        .tm_mon = 2,
        .tm_year = 120 // years since 1900 ¯\_(ツ)_/¯
    };

    std::string expected;

    SUBCASE("morning message") {
        tm.tm_hour = 4;
        tm.tm_min = 20;
        expected = "<04:20> [Peter] Hello!";
    }

    SUBCASE("evening message") {
        tm.tm_hour = 16;
        tm.tm_min = 20;
        expected = "<16:20> [Peter] Hello!";
    }

    SUBCASE("noon message") {
        tm.tm_hour = 12;
        tm.tm_min = 0;
        expected = "<12:00> [Peter] Hello!";
    }

    SUBCASE("midnight message") {
        expected = "<00:00> [Peter] Hello!";
    }

    std::time_t time = std::mktime(&tm);
    NChat::TMessage message("Peter", time, "Hello!");
    CHECK(expected == message.Show());
}

TEST_CASE("TMessage::Serialize") {
    std::unique_ptr<NChat::TGoodSocket> socket(new NChat::TGoodSocket(""));
    NChat::TSocketWrapper wrapper(socket.get());
    std::string expected;

    SUBCASE("Hello!") {
        expected = R"(7
Alice
0
Hello!
)";
        NChat::TMessage {
            "Alice",
            static_cast<std::time_t>(0),
            "Hello!"
        }.Serialize(wrapper);
    }

    SUBCASE("Message with EOLs, name with spaces") {
        expected = R"(18
Robert Doe
0
Hi!
How are you?

)";
        NChat::TMessage {
            "Robert Doe",
            static_cast<std::time_t>(0),
            "Hi!\nHow are you?\n"
        }.Serialize(wrapper);
    }

    CHECK(socket->GetAcceptedData() == expected);
}

TEST_CASE("ReadMessage") {
    std::string serialized;
    NChat::TMessage expected{"", static_cast<std::time_t>(0), ""};

    SUBCASE("Hello!") {
        expected = NChat::TMessage {
            "Alice",
            static_cast<std::time_t>(0),
            "Hello!"
        };
        serialized = R"(7
Alice
0
Hello!
)";
    }

    SUBCASE("Message with EOLs, name with spaces") {
        expected = NChat::TMessage {
            "Robert Doe",
            static_cast<std::time_t>(0),
            "Hi!\nHow are you?\n"
        };
        serialized = R"(18
Robert Doe
0
Hi!
How are you?

)";
    }

    std::unique_ptr<NChat::TGoodSocket> socket(new NChat::TGoodSocket(serialized));
    NChat::TSocketWrapper wrapper(socket.get());
    auto message = NChat::ReadMessage(wrapper);

    CHECK(message.has_value());
    CHECK(expected.Show() == message.value().Show());
}

TEST_CASE("ReadMessage from closed socket") {
    std::unique_ptr<NChat::TGoodSocket> socket(new NChat::TGoodSocket(""));
    NChat::TSocketWrapper wrapper(socket.get());
    auto message = NChat::ReadMessage(wrapper);
    CHECK(!message.has_value());
}

TEST_CASE("Serialize/ReadMessage integration") {
    NChat::TMessage message{"", static_cast<std::time_t>(0), ""};

    SUBCASE("Hello!") {
        message = NChat::TMessage {
            "Alice!",
            static_cast<std::time_t>(0),
            "Hello!"
        };
    }

    SUBCASE("Message with EOLs, name with spaces") {
        message = NChat::TMessage {
            "Robert Doe",
            static_cast<std::time_t>(0),
            "Hi!\nHow are you?\n"
        };
    }

    std::unique_ptr<NChat::TGoodSocket> out(new NChat::TGoodSocket(""));
    NChat::TSocketWrapper outWrapper(out.get());
    message.Serialize(outWrapper);

    std::unique_ptr<NChat::TGoodSocket> in(new NChat::TGoodSocket(out->GetAcceptedData()));
    NChat::TSocketWrapper inWrapper(in.get());
    auto actual = NChat::ReadMessage(inWrapper);

    CHECK(actual.has_value());
    CHECK(message.Show() == actual.value().Show());
}
