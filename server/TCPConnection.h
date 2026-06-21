#pragma once
#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include "Protocol.h"
#include "Executor.h"
#include "User.h"

class RoomManager;

class TCPConnection : public std::enable_shared_from_this<TCPConnection>
{
public:
    ~TCPConnection() = default;

    static std::shared_ptr<TCPConnection> create(boost::asio::io_context&,
        std::function<void(uint32_t)> disconnect_callback,
        std::shared_ptr<RoomManager> room_manager);

    boost::asio::ip::tcp::socket& get_socket();
    void start(uint32_t id, User* user);
    void send(std::vector<uint8_t> data);

private:
    TCPConnection(boost::asio::io_context&,
        std::function<void(uint32_t)> disconnect_callback,
        std::shared_ptr<RoomManager> room_manager);

    void read_header();
    void read_body(uint16_t size);
    void process_packet();
    void on_disconnect();

    boost::asio::ip::tcp::socket socket;
    PacketHeader header;
    std::vector<uint8_t> body;
    Executor executor;
    uint32_t client_id = 0;
    User* user_ = nullptr;
    std::function<void(uint32_t)> disconnect_callback;
    std::shared_ptr<RoomManager> room_manager_;
};