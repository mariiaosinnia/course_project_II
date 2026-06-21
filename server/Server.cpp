#include "Server.h"
#include <iostream>

Server::Server()
    : room_manager(user_manager)
    , tcp_server(io_context, room_manager, user_manager)
{
}

void Server::run() {
    std::cout << "Server running...\n";
    io_context.run();
}