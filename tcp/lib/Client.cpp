#include <Client.h>

#include <Message.h>

#include <iostream>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace NChat {

class TClient::TImpl {
public:
    TImpl(ISocket* socket,
          const std::string& author,
          std::istream& in,
          std::ostream& out,
          bool joinThreads)
        : Socket_(socket)
        , Author_(author)
        , In_(in)
        , Out_(out)
        , JoinThreads_(joinThreads)
    {}

    ~TImpl() = default;

    void Run() {
        std::thread userInput(
            [this]() {
                TSocketWrapper outWrapper(Socket_);
                std::string s;
                std::getline(In_, s);
                while (!In_.eof()) {
                    TMessage send {
                        Author_,
                        static_cast<std::time_t>(0),
                        s
                    };
                    send.Serialize(outWrapper);
                    std::getline(In_, s);
                }
                std::lock_guard<std::mutex> g(Lock_);
                End_.notify_all();
            }
        );
        std::thread userOutput(
            [this]() {
                TSocketWrapper inWrapper(Socket_);
                auto message = ReadMessage(inWrapper);
                while (message.has_value()) {
                    Out_ << message.value().Show() << std::endl;
                    message = ReadMessage(inWrapper);
                }
                std::lock_guard<std::mutex> g(Lock_);
                End_.notify_all();
            }
        );
        std::unique_lock<std::mutex> g(Lock_);
        End_.wait(g);
        g.unlock();
        if (JoinThreads_) {
            userInput.join();
            userOutput.join();
        }
    }

private:
    ISocket* Socket_;
    std::string Author_;
    std::istream& In_;
    std::ostream& Out_;
    bool JoinThreads_;

    std::mutex Lock_;
    std::condition_variable End_;
};

TClient::TClient(ISocket* socket,
                 const std::string& author,
                 std::istream& in,
                 std::ostream& out,
                 bool joinThreads)
    : Impl_(new TImpl(socket, author, in, out, joinThreads))
{}

TClient::~TClient() = default;

void TClient::Run() {
    Impl_->Run();
}

}
