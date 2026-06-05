#include <iostream>

#include "TCPServer.h"
#include "TCPConnection.h"

TCPServer::TCPServer(boost::asio::io_context& io_cntxt)
    : io_context(io_cntxt),
    acceptor(io_cntxt, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
    acceptor.listen();
    std::cout << "local endpoint: "
        << acceptor.local_endpoint().port() << "\n";
    start_accept();
}

uint32_t TCPServer::allocate_id() {
    if (!free_ids.empty()) {
        uint32_t id = free_ids.top();
        free_ids.pop();
        return id;
    }
    return next_client_id++;
}

void TCPServer::release_id(uint32_t id) {
    free_ids.push(id);
}

void TCPServer::start_accept() {
    auto connection = TCPConnection::create(io_context,
        [this](uint32_t id) {
            release_id(id);
        });

    acceptor.async_accept(connection->get_socket(),
        [this, connection](const boost::system::error_code& ec) {
            if (!ec) {
                connection->start(allocate_id());
            }
            start_accept();
        });
}
