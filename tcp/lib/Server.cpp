#include <Server.h>

#include <Message.h>
#include <Socket.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <memory>

namespace NChat {

EServerNetworkError::EServerNetworkError(std::string what)
    : std::runtime_error(what)
{}

class TServer::TImpl {
public:
    TImpl(std::uint16_t port)
        : Port_(port)
    {
        std::memset(&AcceptingSocketAddr_, 0, sizeof(sockaddr));
    }

    ~TImpl() {
        if (AcceptinngSocketFD_ > 0) {
            close(AcceptinngSocketFD_);
        }
    }

    void Run() {
        InitAcceptingSocket();

        /* Listen for incoming connections. */
        if (listen(AcceptinngSocketFD_, MAX_CONNECTIONS_QUEUED) < 0) {
            throw EServerNetworkError("Unable to listen socket");
        }

        pollFDs_.emplace_back(
            pollfd {
                .fd = AcceptinngSocketFD_,
                .events = POLLIN
            }
        );

        while (true) {
            int pollCount = poll(pollFDs_.data(), pollFDs_.size(), -1);

            if (pollCount < 0) {
                throw EServerNetworkError("Unable to poll");
            }

            for (int i = 0; i < (int)pollFDs_.size(); i++) {
                if (!(pollFDs_[i].revents & POLLIN)) {
                    continue;
                }
                if (pollFDs_[i].fd == AcceptinngSocketFD_) {
                    AcceptClient();
                } else {
                    AcceptMessage(i);
                }
            }
        }
    }

private:
    static constexpr int MAX_CONNECTIONS_QUEUED = 16;

    std::uint16_t Port_;

    void InitAcceptingSocket() {
        /* Open the socket */
        AcceptinngSocketFD_ = socket(AF_INET, SOCK_STREAM, 0);
        if (AcceptinngSocketFD_ <= 0) {
            throw EServerNetworkError("Unable to open socket");
        }

        /* Set socket options */
        int opt = 1;
        int error = setsockopt(
            AcceptinngSocketFD_,
            SOL_SOCKET,
            SO_REUSEADDR | SO_REUSEPORT,
            &opt,
            sizeof(int)
        );
        if (error < 0) {
            throw EServerNetworkError("Unable to set socket options");
        }

        /* Bind */
        AcceptingSocketAddr_.sin_family = AF_INET;
        AcceptingSocketAddr_.sin_addr.s_addr = INADDR_ANY;
        AcceptingSocketAddr_.sin_port = htons(Port_);
        error = bind(
            AcceptinngSocketFD_,
            reinterpret_cast<sockaddr*>(&AcceptingSocketAddr_),
            sizeof(AcceptingSocketAddr_)
        );
        if (error < 0) {
            throw EServerNetworkError("Unable to bind socket");
        }
    }

    int AcceptinngSocketFD_ = 0;
    sockaddr_in AcceptingSocketAddr_;

    void AcceptClient() {
        sockaddr_storage clientAddr;
        socklen_t clientAddrLength = sizeof(clientAddr);

        int clientFD = accept(
            AcceptinngSocketFD_,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &clientAddrLength
        );

        if (clientFD < 0) {
            throw EServerNetworkError("Unable to accept new client");
        }

        pollFDs_.emplace_back(
            pollfd {
                .fd = clientFD,
                .events = POLLIN
            }
        );

        clientSockets_.emplace_back(new TFileDescriptorSocket(clientFD));
        clientWrappers_.emplace_back(clientSockets_.back().get());

        for (const TMessage& message : Messages_) {
            message.Serialize(clientWrappers_.back());
        }
    }

    void AcceptMessage(int i) {
        int sockId = i - 1;
        std::optional<TMessage> maybeMessage = ReadMessage(clientWrappers_[sockId]);

        if (!maybeMessage.has_value()) {
            // Bye!

            std::swap(clientWrappers_[sockId], clientWrappers_.back());
            clientWrappers_.pop_back();

            std::swap(clientSockets_[sockId], clientSockets_.back());
            clientSockets_.pop_back();

            std::swap(pollFDs_[i], pollFDs_.back());
            pollFDs_.pop_back();

            return;
        }

        TMessage message = maybeMessage.value();

        std::time_t time;
        std::time(&time);
        message.UpdateTimestamp(time);

        for (TSocketWrapper& client : clientWrappers_) {
            message.Serialize(client);
        }

        Messages_.emplace_back(std::move(message));
    }

    std::vector<pollfd> pollFDs_;
    std::vector<std::unique_ptr<ISocket>> clientSockets_;
    std::vector<TSocketWrapper> clientWrappers_;
    std::vector<TMessage> Messages_;
};

TServer::TServer(std::uint16_t port)
    : impl_(new TImpl(port))
{}

TServer::~TServer() = default;

void TServer::Run() {
    impl_->Run();
}

}
