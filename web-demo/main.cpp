#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <unordered_set>
#include <mutex>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class WSSession;

class Broadcaster : public std::enable_shared_from_this<Broadcaster> {
public:
    Broadcaster(asio::io_context& ioc) : timer_(ioc) {
        file_.open("audio.mp3", std::ios::binary);
        if (!file_) {
            std::cerr << "Broadcaster: Cannot open audio.mp3\n";
        }
    }

    void add_client(std::shared_ptr<WSSession> session);
    void remove_client(std::shared_ptr<WSSession> session);

private:
    void broadcast_next_chunk();

    std::unordered_set<std::shared_ptr<WSSession>> sessions_;
    std::mutex mutex_;
    std::ifstream file_;
    asio::steady_timer timer_;
};

class WSSession : public std::enable_shared_from_this<WSSession> {
public:
    WSSession(tcp::socket socket, std::shared_ptr<Broadcaster> broadcaster)
        : ws_(std::move(socket)), broadcaster_(broadcaster) {
    }

    void start() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                std::cout << "Client joined the room!\n";

                self->ws_.binary(true);

                self->broadcaster_->add_client(self);
                self->read_loop();
            }
            });
    }

    void send_chunk(std::shared_ptr<std::vector<char>> data) {
        ws_.async_write(asio::buffer(*data),
            [self = shared_from_this(), data](beast::error_code ec, std::size_t bytes) {
                if (ec) {
                    self->close();
                }
            });
    }

    void close() {
        broadcaster_->remove_client(shared_from_this());
    }

private:
    void read_loop() {
        ws_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec) {
                self->close();
            }
            else {
                self->read_loop();
            }
            });
    }

    websocket::stream<tcp::socket> ws_;
    std::shared_ptr<Broadcaster> broadcaster_;
    beast::flat_buffer buffer_;
};


void Broadcaster::add_client(std::shared_ptr<WSSession> session) {
    bool was_empty = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        was_empty = sessions_.empty();
        sessions_.insert(session);
    } 

    if (was_empty) {
        std::cout << "First client connected. Starting stream...\n";
        broadcast_next_chunk();
    }
}

void Broadcaster::remove_client(std::shared_ptr<WSSession> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session);
    std::cout << "Client left. Listeners: " << sessions_.size() << "\n";
}

void Broadcaster::broadcast_next_chunk() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessions_.empty()) {
        std::cout << "Room empty, pausing stream.\n";
        return; 
    }

    char buf[32768];
    file_.read(buf, sizeof(buf));
    std::streamsize bytes_read = file_.gcount();

    if (bytes_read > 0) {
        auto data = std::make_shared<std::vector<char>>(buf, buf + bytes_read);

        for (auto& session : sessions_) {
            session->send_chunk(data);
        }

        timer_.expires_after(std::chrono::milliseconds(800));
        timer_.async_wait([self = shared_from_this()](beast::error_code ec) {
            if (!ec) self->broadcast_next_chunk();
            });
    }
    else {
        std::cout << "Track ended. Looping...\n";
        file_.clear();
        file_.seekg(0, std::ios::beg);

        timer_.expires_after(std::chrono::milliseconds(0));
        timer_.async_wait([self = shared_from_this()](beast::error_code ec) {
            if (!ec) self->broadcast_next_chunk();
            });
    }
}

class WSServer {
public:
    WSServer(asio::io_context& io_context, uint16_t port)
        : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
        broadcaster_(std::make_shared<Broadcaster>(io_context)) {
        accept_next();
        std::cout << "Live Radio Server on port " << port << "\n";
    }

private:
    void accept_next() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<WSSession>(std::move(socket), broadcaster_)->start();
                }
                accept_next();
            });
    }

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::shared_ptr<Broadcaster> broadcaster_;
};

int main() {
    try {
        asio::io_context io_context;
        WSServer server(io_context, 8080);
        io_context.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
    }
    return 0;
}