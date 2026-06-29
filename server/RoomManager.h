#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <functional>
#include <unordered_set>

#include "User.h"
#include "Protocol.h"
#include "UserManager.h"

struct PlaybackState {
    uint16_t track_id;
    uint32_t position_ms;
    std::chrono::steady_clock::time_point started_at;
};

struct Room {
    uint16_t id;
    std::string name;
    uint16_t max_users = 1000;
    std::unordered_set<uint32_t> user_ids;
    PlaybackState playback;

    bool is_full() const {
        return user_ids.size() >= max_users;
    }

    uint32_t get_current_position_ms() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - playback.started_at).count();
        return static_cast<uint32_t>(elapsed);
    }
};

class RoomManager {
public:
    RoomManager(UserManager& um);

    uint16_t create_room(const std::string& name);
    StatusCode join_room(User& user, uint16_t room_id);
    StatusCode leave_room(User& user, uint16_t room_id);

    std::vector<RoomListEntry> list_rooms() const;
    std::vector<UserInfo> get_users_in_room(uint16_t room_id) const;

    using BroadcastFn = std::function<void(uint32_t, std::vector<uint8_t>)>;
    void set_broadcast(BroadcastFn fn);
    void broadcast_to_room(uint16_t room_id, const std::vector<uint8_t>& packet, const User& excluded_user);
    uint32_t get_current_position(uint16_t room_id) const;
    std::vector<udp::endpoint> get_udp_endpoints(uint16_t room_id) const;

    void registerUdpEndpoint(uint32_t client_id, const udp::endpoint& endpoint);

private:
    mutable std::shared_mutex mutex;
    std::unordered_map<uint16_t, Room> rooms;
    uint16_t next_room_id_ = 1;
    BroadcastFn broadcast_fn;
    UserManager& user_manager;
};