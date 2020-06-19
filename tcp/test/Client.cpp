#include <doctest/doctest.h>

#include <Client.h>

#include <util/MockSocket.h>

#include <cstdlib>
#include <sstream>

namespace {

const std::string MessagesFromServer =
    "7\n"
    "Alice\n"
    "1590510550\n"
    "Hello!\n"
    "17\n"
    "Robert Doe\n"
    "1590510610\n"
    "Hi! How are you?\n"
    "7\n"
    "Alice\n"
    "1590510670\n"
    "Great!\n";

const std::string ExpectedUserOutput =
    "<19:29> [Alice] Hello!\n"
    "<19:30> [Robert Doe] Hi! How are you?\n"
    "<19:31> [Alice] Great!\n";

const std::string UserInput = "Great!\n";

const std::string ExpectedServerOutput =
    "7\n"
    "Alice\n"
    "0\n"
    "Great!\n";

char TimeZone[] = "TZ=UTC-3";

}

TEST_CASE("Client smoke test") {
    putenv(TimeZone);
    std::ostringstream out;
    NChat::TGoodSocket netSocket(MessagesFromServer);
    NChat::TGoodSocket stdinSocket(UserInput);

    NChat::TClient client(&netSocket, &stdinSocket, "Alice", out);

    CHECK(true == client.OnServerRead());
    CHECK(true == client.OnStdinRead());

    CHECK(ExpectedUserOutput == out.str());
    CHECK(ExpectedServerOutput == netSocket.GetAcceptedData());
}
