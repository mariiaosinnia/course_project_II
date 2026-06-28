#include "UdpSocket.h"

UdpSocket::UdpSocket(unsigned short port)
    : socket_(io_context_, udp::endpoint(udp::v4(), port))
{}

void UdpSocket::sendTo(const std::vector<uint8_t>& data, const udp::endpoint& endpoint) {
    socket_.send_to(boost::asio::buffer(data), endpoint);
}

size_t UdpSocket::receiveFrom(std::vector<uint8_t>& buffer, udp::endpoint& sender) {
    return socket_.receive_from(boost::asio::buffer(buffer), sender);
}

udp::socket& UdpSocket::getSocket() {
    return socket_;
}