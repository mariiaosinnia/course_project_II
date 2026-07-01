#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "Protocol.h"

struct ClientState {
    std::atomic<uint32_t> client_id{0};
    std::atomic<bool> connected{false};
    std::atomic<uint16_t> current_room_id{0};  // 0 = не в кімнаті
    std::mutex room_list_mutex;
    std::vector<RoomListEntry> room_list_cache;
};