#include <Client.h>

#include <Message.h>

#include <iostream>

namespace NChat {

class TClient::TImpl {
public:
    TImpl(ISocket* netSocket,
          ISocket* stdinSocket,
          const std::string& author,
          std::ostream& out)
        : NetSocket_(netSocket)
        , StdinSocket_(stdinSocket)
        , Author_(author)
        , Out_(out)
    {}

    ~TImpl() = default;

    bool OnServerRead() {
        do {
            auto message = ReadMessage(NetSocket_);
            if (!message.has_value()) {
                return false;
            }
            Out_ << message.value().Show() << std::endl;
        } while (NetSocket_.HasCachedInput());
        return true;
    }

    bool OnStdinRead() {
        do {
            auto text = StdinSocket_.ReadUntil('\n');
            if (text.empty()) {
                return false;
            }
            TMessage message (
                Author_,
                static_cast<std::time_t>(0),
                text
            );
            message.Serialize(NetSocket_);
        } while (StdinSocket_.HasCachedInput());
        return true;
    }

private:
    TSocketWrapper NetSocket_;
    TSocketWrapper StdinSocket_;
    std::string Author_;
    std::ostream& Out_;
};

TClient::TClient(ISocket* netSocket,
                 ISocket *stdinSocket,
                 const std::string& author,
                 std::ostream& out)
    : Impl_(new TImpl(netSocket, stdinSocket, author, out))
{}

TClient::~TClient() = default;

bool TClient::OnServerRead() {
    return Impl_->OnServerRead();
}

bool TClient::OnStdinRead() {
    return Impl_->OnStdinRead();
}

}
