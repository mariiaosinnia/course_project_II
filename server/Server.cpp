#include "Server.h"
#include <iostream>

Server::Server()
    : room_manager(user_manager)
    , tcp_server(io_context, room_manager, user_manager)
    , udp_server("udp_server/summer.mp3", 9001)
{
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