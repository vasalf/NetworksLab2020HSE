#pragma once

#include <ctime>
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

private:
    std::string Author_;
    std::time_t Accepted_;
    std::string Text_;
};

}
