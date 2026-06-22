#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Protocol.h"


class TCPClient : public std::enable_shared_from_this<TCPClient>
{
public:
    using PacketCallback = std::function<void(PacketType, const std::vector<uint8_t>&)>;
    using DisconnectCallback = std::function<void()>;

    static std::shared_ptr<TCPClient> create(boost::asio::io_context& io_context);

    void connect(const std::string& host, uint16_t port,
        std::function<void(bool success)> on_connected);

    void send(std::vector<uint8_t> data);

    void set_packet_callback(PacketCallback cb);
    void set_disconnect_callback(DisconnectCallback cb);

private:
    explicit TCPClient(boost::asio::io_context& io_context);

    void read_header();
    void read_body(uint16_t size);
    void process_packet();
    void on_disconnect();

    boost::asio::ip::tcp::socket socket;
    boost::asio::ip::tcp::resolver resolver;

    PacketHeader header{};
    std::vector<uint8_t> body;

    PacketCallback packet_callback;
    DisconnectCallback disconnect_callback;
};
