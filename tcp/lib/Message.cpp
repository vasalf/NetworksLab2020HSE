#include <Message.h>

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace NChat {

TMessage::TMessage(const std::string& author,
                   const std::time_t& accepted,
                   const std::string& text)
    : Author_(author)
    , Accepted_(accepted)
    , Text_(text)
{}

std::string TMessage::Show() const {
    std::tm tm = *std::localtime(&Accepted_);
    std::ostringstream ss;

    ss << "<" << std::put_time(&tm, "%H:%M") << "> "
       << "[" << Author_ << "] "
       << Text_;

    return ss.str();
}

void TMessage::Serialize(TSocketWrapper& socket) const {
    socket.Write(std::to_string(Text_.size() + 1) + "\n");
    socket.Write(Author_ + "\n");
    socket.Write(std::to_string(Accepted_) + "\n");
    socket.Write(Text_ + "\n");
}

void TMessage::UpdateTimestamp(const std::time_t& timestamp) {
    Accepted_ = timestamp;
}

namespace {

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
T ToInt(const std::string& str) {
    std::istringstream ss(str);
    T ret;
    ss >> ret;
    return ret;
}

}

std::optional<TMessage> ReadMessage(TSocketWrapper& socket) {
    auto start = socket.ReadUntil('\n');
    if (start.empty()) {
        return {};
    }
    int textLength = ToInt<int>(start);
    std::string author = socket.ReadUntil('\n');
    std::time_t accepted = ToInt<std::time_t>(socket.ReadUntil('\n'));
    std::string text = socket.ReadN(textLength);
    text.pop_back();
    return TMessage{author, accepted, text};
}

}
