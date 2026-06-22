#include "TCPServer.h"
#include "TCPConnection.h"
#include <iostream>

TCPServer::TCPServer(boost::asio::io_context& io, RoomManager& rm, UserManager& um)
    : io_context(io)
    , acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), TCP_SERVER_PORT))
    , room_manager(rm)
    , user_manager(um)
{
    room_manager.set_broadcast([this](uint32_t client_id, std::vector<uint8_t> packet) {
        auto it = active_connections.find(client_id);
        if (it != active_connections.end()) {
            it->second->send(packet); 
        }
        });

    acceptor.listen();
    std::cout << "TCP server listening on port: " << acceptor.local_endpoint().port() << "\n";
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
            user_manager.remove(id);
            release_id(id);
            active_connections.erase(id);
        },
        room_manager);

    acceptor.async_accept(connection->get_socket(),
        [this, connection](const boost::system::error_code& ec) {
            if (!ec) {
                uint32_t id = allocate_id();
                User* user = user_manager.add(id);
                active_connections[id] = connection;
                connection->start(id, user);
            }
            start_accept();
        });
}