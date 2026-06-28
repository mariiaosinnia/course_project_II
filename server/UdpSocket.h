#pragma once
#include <boost/asio.hpp>
#include <vector>
#include <cstdint>

using boost::asio::ip::udp;

class UdpSocket {
public:
    explicit UdpSocket(unsigned short port);

    void sendTo(const std::vector<uint8_t>& data, const udp::endpoint& endpoint);
    size_t receiveFrom(std::vector<uint8_t>& buffer, udp::endpoint& sender);

    udp::socket& getSocket();

private:
    boost::asio::io_context io_context_;
    udp::socket socket_;
};