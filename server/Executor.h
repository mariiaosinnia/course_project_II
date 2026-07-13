#pragma once
#include <vector>
#include <cstdint>
#include <optional>
#include "Protocol.h"
#include "PacketParser.h"
#include "UploadState.h"
#include "User.h"

class RoomManager;

class Executor {
public:
    Executor(RoomManager& rm);

    std::vector<uint8_t> execute(User& user, PacketType type,
        const std::vector<uint8_t>& body);

    void cleanup();

private:
    std::vector<uint8_t> handle_ping();
    std::vector<uint8_t> handle_connect(User& user, const std::vector<uint8_t>& body);
    std::vector<uint8_t> handle_create_room(User& user, const std::vector<uint8_t>& body);
    std::vector<uint8_t> handle_join_room(User& user, const std::vector<uint8_t>& body);
    std::vector<uint8_t> handle_leave_room(User& user);
    std::vector<uint8_t> handle_list_rooms();
    std::vector<uint8_t> handle_voice_start(User& user);
    std::vector<uint8_t> handle_voice_stop(User& user);

    std::vector<uint8_t> handle_upload_begin(User& user, const std::vector<uint8_t>& body);
    std::vector<uint8_t> handle_upload_data(User& user, const std::vector<uint8_t>& body);
    std::vector<uint8_t> handle_upload_end(User& user);

    RoomManager& room_manager;
    std::optional<UploadState> upload_state_;
};
