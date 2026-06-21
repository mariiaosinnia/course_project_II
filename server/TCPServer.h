#pragma once
#include <boost/asio.hpp>
#include <queue>
#include <unordered_map>
#include <memory>
#include "User.h"
#include "RoomManager.h"

class TCPServer
{
public:
    TCPServer(boost::asio::io_context& io_cntxt);
    ~TCPServer() = default;

private:
    const int port = 12345;
    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    uint32_t next_client_id = 1;
    std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> free_ids;
    std::shared_ptr<RoomManager> room_manager;
    std::shared_ptr<UserManager> user_manager;

    uint32_t allocate_id();
    void release_id(uint32_t id);
    void start_accept();
};