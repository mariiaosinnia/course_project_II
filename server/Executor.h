#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include "Protocol.h"
#include "PacketParser.h"
#include "User.h"

class RoomManager;

class Executor
{
public:
    Executor(std::shared_ptr<RoomManager> rm);
    std::vector<uint8_t> execute(User& user, PacketType type,
        const std::vector<uint8_t>& body);
private:
    std::vector<uint8_t> handle_ping();
    std::vector<uint8_t> handle_connect(User& user, const std::vector<uint8_t>& body);
    std::vector<uint8_t> handle_create_room(User& user, const std::vector<uint8_t>& body);
    std::vector<uint8_t> handle_join_room(User& user, const std::vector<uint8_t>& body);
    std::vector<uint8_t> handle_leave_room(User& user);
    std::vector<uint8_t> handle_list_rooms();

    std::shared_ptr<RoomManager> room_manager;
};