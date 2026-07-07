#include "TCPConnection.h"
#include "RoomManager.h"
#include <iostream>

TCPConnection::TCPConnection(
    boost::asio::io_context& io,
    std::function<void(uint32_t)> callback,
    RoomManager& room_manager)
    : socket(io), disconnect_callback(std::move(callback)), executor(room_manager)
{
}

std::shared_ptr<TCPConnection> TCPConnection::create(
    boost::asio::io_context& io,
    std::function<void(uint32_t)> callback,
    RoomManager& room_manager)
{
    return std::shared_ptr<TCPConnection>(
        new TCPConnection(io, std::move(callback), room_manager));
}

boost::asio::ip::tcp::socket& TCPConnection::get_socket() {
    return socket;
}

void TCPConnection::start(uint32_t id, User* user) {
    client_id = id;
    user_ = user;
    read_header();
}

void TCPConnection::on_disconnect() {
    executor.cleanup();
    if (disconnect_callback) {
        disconnect_callback(client_id);
    }
}

void TCPConnection::read_header() {
    boost::asio::async_read(socket,
        boost::asio::buffer(&header, sizeof(PacketHeader)),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                self->read_body(ntohs(self->header.payload_size));
            }
            else {
                self->on_disconnect();
            }
        });
}

void TCPConnection::read_body(uint16_t size) {
    body.resize(size);
    boost::asio::async_read(socket,
        boost::asio::buffer(body),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                self->process_packet();
                self->read_header();
            }
            else {
                self->on_disconnect();
            }
        });
}

void TCPConnection::process_packet() {
    PacketType type = static_cast<PacketType>(header.type);
    auto response = executor.execute(*user_, type, body);
    if (!response.empty()) {
        send(response);
    }
}

void TCPConnection::send(std::vector<uint8_t> data) {
    auto buffer = std::make_shared<std::vector<uint8_t>>(std::move(data));
    boost::asio::async_write(socket,
        boost::asio::buffer(*buffer),
        [self = shared_from_this(), buffer](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "send error: " << ec.message() << "\n";
            }
        });
}