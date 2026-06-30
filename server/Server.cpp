#include "Server.h"
#include <iostream>

Server::Server()
    : room_manager(user_manager)
    , tcp_server(io_context, room_manager, user_manager)
    , udp_server(UDP_SERVER_PORT, room_manager)
{
    room_manager.set_on_first_user_joined([this](uint16_t room_id) {
        udp_server.startStreaming(room_id);
    });
}

void Server::run() {
    std::cout << "Server running...\n";

    std::thread tcp_thread([this] {
        io_context.run();
        });

    std::thread udp_thread([this] {
        udp_server.run();
        });

    tcp_thread.join();
    udp_thread.join();
}