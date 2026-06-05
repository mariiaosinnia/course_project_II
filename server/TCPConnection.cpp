#include <iostream>

#include "TCPConnection.h"

boost::asio::ip::tcp::socket& TCPConnection::get_socket(){
	return socket;
}

TCPConnection::TCPConnection(boost::asio::io_context& io_context,
    std::function<void(uint32_t)> callback)
    : socket(io_context),
    disconnect_callback(std::move(callback))
{
}

std::shared_ptr<TCPConnection> TCPConnection::create(
    boost::asio::io_context& io_context,
    std::function<void(uint32_t)> callback)
{
    return std::shared_ptr<TCPConnection>(
        new TCPConnection(io_context, std::move(callback)));
}

void TCPConnection::on_disconnect() {
    if (disconnect_callback) {
        disconnect_callback(client_id);
    }
}

void TCPConnection::start(uint32_t id) {
    client_id = id;
    read_header();
}

void TCPConnection::read_header() {
    boost::asio::async_read(socket,
        boost::asio::buffer(&header, sizeof(PacketHeader)),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) {
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
        [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) {
            if (!ec) {
                self->process_packet();
                self->read_header();
            }
        });
}

void TCPConnection::send(std::vector<uint8_t> data) {
    std::shared_ptr<std::vector<uint8_t>> buffer =
        std::make_shared<std::vector<uint8_t>>(std::move(data));

    boost::asio::async_write(socket,
        boost::asio::buffer(*buffer),
        [self = shared_from_this(), buffer](boost::system::error_code ec, std::size_t bytes) {
            if (ec) {
                std::cerr << "send error: " << ec.message() << "\n";
            }
        });
}

void TCPConnection::process_packet(){
    PacketType type = static_cast<PacketType>(header.type);
    std::vector<uint8_t> response = executor.execute(client_id, type, body);
    send(response);
}
