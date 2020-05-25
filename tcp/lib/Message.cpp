#include <Message.h>

#include <iomanip>
#include <sstream>

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

}
