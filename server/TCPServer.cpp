#include <iostream>
#include "TCPServer.h"
#include "TCPConnection.h"

TCPServer::TCPServer(boost::asio::io_context& io_cntxt)
    : io_context(io_cntxt),
    acceptor(io_cntxt, boost::asio::ip::tcp::endpoint(
        boost::asio::ip::tcp::v4(), port)),
    user_manager(std::make_shared<UserManager>()),
    room_manager(std::make_shared<RoomManager>(user_manager))
{
    acceptor.listen();
    std::cout << "TCP server is listening on port: " << acceptor.local_endpoint().port() << "\n";
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
            user_manager->remove(id);
            release_id(id);
        },
        room_manager);

    acceptor.async_accept(connection->get_socket(),
        [this, connection](const boost::system::error_code& ec) {
            if (!ec) {
                uint32_t id = allocate_id();
                User* user = user_manager->add(id);
                connection->start(id, user);
            }
            start_accept();
        });
}