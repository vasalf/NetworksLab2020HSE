#include <doctest/doctest.h>

#include <Message.h>

#include <ctime>


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
