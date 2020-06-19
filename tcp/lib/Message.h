#pragma once

#include <Socket.h>

#include <ctime>
#include <optional>
#include <string>

namespace NChat {

class TMessage {
public:
    TMessage(const std::string& author,
             const std::time_t& accepted,
             const std::string& text);

    ~TMessage() = default;
    TMessage(const TMessage&) = default;
    TMessage& operator=(const TMessage&) = default;
    TMessage(TMessage&&) = default;
    TMessage& operator=(TMessage&&) = default;

    std::string Show() const;
    void Serialize(TSocketWrapper& socket) const;

    void UpdateTimestamp(const std::time_t& timestamp);

private:
    std::string Author_;
    std::time_t Accepted_;
    std::string Text_;
};

// Empty if the connection is closed.
std::optional<TMessage> ReadMessage(TSocketWrapper& socket);

}
