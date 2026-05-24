#include "TCPConnection.h"

std::shared_ptr<TCPConnection> TCPConnection::create(boost::asio::io_context& io_context){
	return std::shared_ptr<TCPConnection>(new TCPConnection(io_context));
}

boost::asio::ip::tcp::socket& TCPConnection::get_socket(){
	return socket;
}

TCPConnection::TCPConnection(boost::asio::io_context& io_context)
    : socket(io_context)
{
}

void TCPConnection::start() {
    read_header();
}

void TCPConnection::read_header() {
    boost::asio::async_read(socket,
        boost::asio::buffer(&header_, sizeof(PacketHeader)),
        [self = shared_from_this()](auto ec, auto bytes) {
            if (!ec) {
                self->read_body(ntohs(self->header_.payload_size));
            }
        });
}

void TCPConnection::read_body(uint16_t size) {
    body_.resize(size);
    boost::asio::async_read(socket,
        boost::asio::buffer(body_),
        [self = shared_from_this()](auto ec, auto bytes) {
            if (!ec) {
                self->process_packet();
                self->read_header();
            }
        });
}