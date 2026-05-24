#include "TCPServer.h"
#include "TCPConnection.h"
#include <iostream>

TCPServer::TCPServer(boost::asio::io_context& io_cntxt)
    : io_context_(io_cntxt),
    acceptor(io_cntxt, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 12345)) {
    acceptor.listen();
    std::cout << "local endpoint: "
        << acceptor.local_endpoint().port() << "\n";
    start_accept();
}

void TCPServer::start_accept() {
    std::shared_ptr<TCPConnection> connection = TCPConnection::create(io_context_);
    acceptor.async_accept(connection->get_socket(),
        [this, connection](const boost::system::error_code& ec) {
            if (!ec) {
                std::cout << "client connected\n";
                connection->start();
            }
            start_accept();
        });
}
