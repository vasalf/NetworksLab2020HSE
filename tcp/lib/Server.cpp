#include <Server.h>

#include <Message.h>
#include <Socket.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <condition_variable>
#include <cstring>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

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

        while (true) {
            sockaddr_in clientSocketAddr;
            socklen_t clientLength = sizeof(clientSocketAddr);
            int clientFD = accept(
                AcceptinngSocketFD_,
                reinterpret_cast<sockaddr*>(&clientSocketAddr),
                &clientLength
            );
            if (clientFD < 0) {
                perror("accept");
                throw EServerNetworkError("Accept failed");
            }
            Clients_.emplace_back(
                std::async(
                    std::launch::async,
                    &TImpl::InteractWithClient,
                    this,
                    clientFD
                )
            );
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
    std::vector<std::future<void>> Clients_;

    void InteractWithClient(int clientFD_) {
        TFileDescriptorSocket client(clientFD_);
        bool running = true;
        std::thread outThread([this, &client, &running]() {
            TSocketWrapper outWrapper(&client);
            {
                // Dump all messages before
                std::lock_guard<std::mutex> guard(MessagesLock_);
                for (const TMessage& message : Messages_) {
                    message.Serialize(outWrapper);
                }
            }
            std::size_t position = Messages_.size();
            while (running) {
                std::unique_lock<std::mutex> guard(MessagesLock_);
                NewMessages_.wait(
                    guard,
                    [this, &running, &position]() {
                        return !running || position < Messages_.size();
                    }
                );
                for (; position < Messages_.size(); position++) {
                    Messages_[position].Serialize(outWrapper);
                }
            }
        });
        std::thread inThread([this, &client, &running]() {
            TSocketWrapper inWrapper(&client);
            auto message = ReadMessage(inWrapper);
            while (message.has_value()) {
                std::time_t now;
                std::time(&now);
                message.value().UpdateTimestamp(now);
                {
                    std::lock_guard<std::mutex> guard(MessagesLock_);
                    Messages_.emplace_back(message.value());
                }
                NewMessages_.notify_all();
                message = ReadMessage(inWrapper);
            }
            running = false;
            NewMessages_.notify_all();
        });
        inThread.join();
        outThread.join();
    }

    std::mutex MessagesLock_;
    std::condition_variable NewMessages_;
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
