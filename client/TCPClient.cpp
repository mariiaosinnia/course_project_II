#include <iostream>

#include <boost/endian/conversion.hpp>

#include "TCPClient.h"

std::shared_ptr<TCPClient> TCPClient::create(boost::asio::io_context& io_context)
{
    return std::shared_ptr<TCPClient>(new TCPClient(io_context));
}

TCPClient::TCPClient(boost::asio::io_context& io_context)
    : socket(io_context), resolver(io_context)
{
}

void TCPClient::set_packet_callback(PacketCallback cb)
{
    packet_callback = std::move(cb);
}

void TCPClient::set_disconnect_callback(DisconnectCallback cb)
{
    disconnect_callback = std::move(cb);
}

void TCPClient::connect(const std::string& host, uint16_t port,
    std::function<void(bool)> on_connected)
{
    auto endpoints = resolver.resolve(host, std::to_string(port));

    boost::asio::async_connect(socket, endpoints,
        [self = shared_from_this(), on_connected](
            boost::system::error_code ec, const boost::asio::ip::tcp::endpoint&) {
            if (!ec) {
                self->read_header();
                on_connected(true);
            }
            else {
                std::cerr << "connect error: " << ec.message() << "\n";
                on_connected(false);
            }
        });
}

void TCPClient::read_header()
{
    boost::asio::async_read(socket,
        boost::asio::buffer(&header, sizeof(PacketHeader)),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                uint16_t size = boost::endian::big_to_native(self->header.payload_size);
                self->read_body(size);
            }
            else {
                self->on_disconnect();
            }
        });
}
void TCPClient::read_body(uint16_t size)
{
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

void TCPClient::process_packet()
{
    PacketType type = static_cast<PacketType>(header.type);
    if (packet_callback) {
        packet_callback(type, body);
    }
}

void TCPClient::send(std::vector<uint8_t> data)
{
    auto buffer = std::make_shared<std::vector<uint8_t>>(std::move(data));

    boost::asio::post(socket.get_executor(),
        [self = shared_from_this(), buffer] {
            bool write_in_progress = !self->write_queue.empty();
            self->write_queue.push_back(buffer);
            if (!write_in_progress) {
                self->write_next();
            }
        });
}

void TCPClient::write_next()
{
    if (write_queue.empty()) {
        return;
    }

    auto buffer = write_queue.front();
    boost::asio::async_write(socket,
        boost::asio::buffer(*buffer),
        [self = shared_from_this(), buffer](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "send error: " << ec.message() << "\n";
                self->on_disconnect();
                return;
            }

            self->write_queue.pop_front();
            if (!self->write_queue.empty()) {
                self->write_next();
            }
        });
}

void TCPClient::on_disconnect()
{
    if (disconnect_callback) {
        disconnect_callback();
    }
}
