#include "RoomManager.h"

uint16_t RoomManager::create_room(const std::string& name){
    std::unique_lock<std::shared_mutex> lock(mutex);

    uint16_t room_id = next_room_id_++;

    Room room;
    room.id = room_id;
    room.name = name;

    rooms.emplace(room_id, std::move(room));

    return room_id;
}

StatusCode RoomManager::join_room(User& user, uint16_t room_id){
    std::unique_lock<std::shared_mutex> lock(mutex);

    auto room_it = rooms.find(room_id);
    if (room_it == rooms.end()) {
        return StatusCode::RoomNotFound;
    }

    if (user.room_id != 0) {
        return StatusCode::AlreadyInRoom;
    }

    Room& room = room_it->second;
    if (room.is_full()) {
        return StatusCode::RoomFull;
    }

    room.user_ids.insert(user.id);
    user.room_id = room_id;

    return StatusCode::Success;
}

StatusCode RoomManager::leave_room(User& user, uint16_t room_id){
    std::unique_lock<std::shared_mutex> lock(mutex);

    if (user.room_id == 0) {
        return StatusCode::NotInRoom;
    }

    auto room_it = rooms.find(room_id);
    if (room_it == rooms.end()) {
        return StatusCode::RoomNotFound;
    }

    room_it->second.user_ids.erase(user.id);
    user.room_id = 0;

    return StatusCode::Success;
}

std::vector<RoomListEntry> RoomManager::list_rooms() const{
    std::shared_lock<std::shared_mutex> lock(mutex);
    std::vector<RoomListEntry> rooms_list;
    for (const auto& [key, value] : rooms) {
        RoomListEntry room_list_entry;
        room_list_entry.room_id = value.id;
        std::strncpy(room_list_entry.name, value.name.c_str(), ROOM_NAME_MAX_LEN - 1);
        room_list_entry.name[ROOM_NAME_MAX_LEN - 1] = '\0';
        room_list_entry.user_count = static_cast<uint8_t>(value.user_ids.size());
        rooms_list.push_back(room_list_entry);
    }

    return rooms_list;
}

std::vector<UserInfo> RoomManager::get_users_in_room(uint16_t room_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto it = rooms.find(room_id);
    if (it == rooms.end()) return {};

    std::vector<UserInfo> result;
    for (uint32_t id : it->second.user_ids) {
        UserInfo info;
        info.client_id = id;
        User* user = user_manager->get(id);
        if (user) {
            std::strncpy(info.username, user->name.c_str(), USERNAME_MAX_LEN - 1);
            info.username[USERNAME_MAX_LEN - 1] = '\0';
        }
        result.push_back(info);
    }
    return result;
}

void RoomManager::set_broadcast(BroadcastFn fn){
    broadcast_fn = std::move(fn);
}

void RoomManager::broadcast_to_room(uint16_t room_id, const std::vector<uint8_t>& packet, const User& excluded_user){
    auto room_it = rooms.find(room_id);

    if (room_it == rooms.end()) {
        return;
    }

    Room& room = room_it->second;

    for (const uint32_t user_id : room.user_ids)
    {
        if (user_id == excluded_user.id) {
            continue;
        }

        broadcast_fn(user_id, packet);
    }
}
