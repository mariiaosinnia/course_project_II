#pragma once
#include <boost/asio.hpp>
#include <thread>
#include "UserManager.h"
#include "RoomManager.h"
#include "TCPServer.h"

class Server {
public:
    Server();
    void run();

private:
    boost::asio::io_context io_context;
    UserManager user_manager;
    RoomManager room_manager;
    TCPServer tcp_server;
    // UDPServer udp_server;
};