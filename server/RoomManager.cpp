#include "RoomManager.h"
#include "PacketBuilder.h"
#include <iostream>

RoomManager::RoomManager(UserManager& um) : user_manager(um)
{
}

uint16_t RoomManager::create_room(const std::string& name){
    std::unique_lock<std::shared_mutex> lock(mutex);

    uint16_t room_id = next_room_id_++;

    Room room;
    room.id = room_id;
    room.name = name;

    rooms.emplace(room_id, std::move(room));

    return room_id;
}

void RoomManager::set_on_first_user_joined(OnFirstUserJoined fn) {
    on_first_user_joined_ = std::move(fn);
}

void RoomManager::set_on_track_added(TrackAddedFn fn) {
    on_track_added_ = std::move(fn);
}

uint16_t RoomManager::add_track_to_room(uint16_t room_id, const std::string& track_path) {
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (rooms.find(room_id) == rooms.end() || track_path.empty()) {
            return 0;
        }
    }

    uint16_t track_id = 0;
    if (on_track_added_) {
        track_id = on_track_added_(track_path);
    }

    if (track_id == 0) {
        return 0;
    }

    bool should_start_streaming = false;
    {
        std::unique_lock<std::shared_mutex> lock(mutex);

        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) {
            return 0;
        }

        room_it->second.track_ids.push_back(track_id);

        bool first_track = (room_it->second.track_ids.size() == 1);
        bool has_users = !room_it->second.user_ids.empty();
        should_start_streaming = first_track && has_users;
    }

    if (should_start_streaming && on_first_user_joined_) {
        on_first_user_joined_(room_id);
    }

    return track_id;
}

StatusCode RoomManager::join_room(User& user, uint16_t room_id){
    bool first_user = false;
    bool has_track = false;
    bool should_start_streaming = false;
    {
        std::unique_lock lock(mutex);

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
        first_user = (room.user_ids.size() == 1);
        has_track = !room.track_ids.empty();
        should_start_streaming = first_user && has_track;
    }

    if (should_start_streaming && on_first_user_joined_) {
        on_first_user_joined_(room_id);
    }

    return StatusCode::Success;
}

StatusCode RoomManager::leave_room(User& user, uint16_t room_id){
    bool was_speaking = false;
    std::vector<uint32_t> recipients;
    {
        std::unique_lock<std::shared_mutex> lock(mutex);

        if (user.room_id == 0) {
            return StatusCode::NotInRoom;
        }

        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) {
            return StatusCode::RoomNotFound;
        }

        if (user.is_speaking) {
            room_it->second.speaking_users.erase(user.id);
            user.is_speaking = false;
            was_speaking = true;
        }

        room_it->second.user_ids.erase(user.id);
        user.room_id = 0;

        if (was_speaking) {
            recipients.assign(room_it->second.user_ids.begin(),
                              room_it->second.user_ids.end());
        }
    }

    if (was_speaking && broadcast_fn) {
        auto packet = PacketBuilder::voice_stopped(user.id);
        for (uint32_t uid : recipients) {
            broadcast_fn(uid, packet);
        }
    }

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
        User* user = user_manager.get(id);
        if (!user){ 
            continue;
        }

        UserInfo info;
        info.client_id = id;
        std::strncpy(info.username, user->name.c_str(), USERNAME_MAX_LEN - 1);
        info.username[USERNAME_MAX_LEN - 1] = '\0';
        result.push_back(info);
    }
    return result;
}

void RoomManager::set_broadcast(BroadcastFn fn){
    broadcast_fn = std::move(fn);
}

void RoomManager::broadcast_to_room(uint16_t room_id, const std::vector<uint8_t>& packet, const User& excluded_user){
    std::vector<uint32_t> recipients;
    {
        std::shared_lock lock(mutex);
        auto room_it = rooms.find(room_id);

        if (room_it == rooms.end()) {
            return;
        }

        for (const uint32_t user_id : room_it->second.user_ids) {
            if (user_id != excluded_user.id) {
                recipients.push_back(user_id);
            }
        }
    }

    if (broadcast_fn) {
        for (uint32_t uid : recipients) {
            broadcast_fn(uid, packet);
        }
    }
}

uint32_t RoomManager::get_current_position(uint16_t room_id) const {
    std::shared_lock lock(mutex);
    auto it = rooms.find(room_id);
    if (it == rooms.end()) {
        return 0;
    }
    return it->second.get_current_position_ms();
}

std::vector<udp::endpoint> RoomManager::get_udp_endpoints(uint16_t room_id) const {
    std::shared_lock lock(mutex);

    auto room_it = rooms.find(room_id);
    if (room_it == rooms.end()) return {};

    std::vector<udp::endpoint> endpoints;
    for (uint32_t user_id : room_it->second.user_ids) {
        User* user = user_manager.get(user_id);
        if (user && user->udp_endpoint.has_value()) {
            endpoints.push_back(user->udp_endpoint.value());
        }
    }
    return endpoints;
}

std::vector<udp::endpoint> RoomManager::get_udp_endpoints_except(uint16_t room_id, uint32_t excluded_client_id) const {
    std::shared_lock lock(mutex);

    auto room_it = rooms.find(room_id);
    if (room_it == rooms.end()){
        return {};
    }

    std::vector<udp::endpoint> endpoints;
    for (uint32_t user_id : room_it->second.user_ids) {
        if (user_id == excluded_client_id){ 
            continue;
        }
        User* user = user_manager.get(user_id);
        if (user && user->udp_endpoint.has_value()) {
            endpoints.push_back(user->udp_endpoint.value());
        }
    }
    return endpoints;
}

void RoomManager::registerUdpEndpoint(uint32_t client_id, const udp::endpoint& endpoint) {
    std::unique_lock<std::shared_mutex> lock(mutex);

    User* user = user_manager.get(client_id);
    if (user == nullptr) {
        std::cerr << "registerUdpEndpoint: user " << client_id << " not found\n";
        return;
    }

    user->udp_endpoint = endpoint;
}

std::vector<uint16_t> RoomManager::get_active_room_ids() const {
    std::shared_lock lock(mutex);
    std::vector<uint16_t> ids;
    for (const auto& [id, room] : rooms) {
        ids.push_back(id);
    }
    return ids;
}

void RoomManager::start_playback(uint16_t room_id, std::chrono::steady_clock::time_point start_time) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto it = rooms.find(room_id);
    if (it != rooms.end()) {
        it->second.track_started_at = start_time;
        it->second.is_playing = true;
    }
}

uint16_t RoomManager::get_current_track_id(uint16_t room_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto it = rooms.find(room_id);
    if (it != rooms.end()) {
        return it->second.current_track_id();
    }
    return 0;
}

uint16_t RoomManager::advance_track(uint16_t room_id, std::chrono::steady_clock::time_point start_time) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto it = rooms.find(room_id);
    if (it != rooms.end()) {
        uint16_t next_id = it->second.next_track();
        it->second.track_started_at = start_time;
        return next_id;
    }
    return 0;
}

size_t RoomManager::get_user_count(uint16_t room_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex);

    auto it = rooms.find(room_id);
    if (it != rooms.end()) {
        return it->second.user_ids.size();
    }
    return 0;
}

void RoomManager::voice_start(User& user) {
    if (user.room_id == 0){
        return;
    }

    uint16_t room_id = user.room_id;
    std::vector<uint32_t> recipients;
    {
        std::unique_lock lock(mutex);
        auto it = rooms.find(room_id);
        if (it == rooms.end()){
            return;
        }

        user.is_speaking = true;
        it->second.speaking_users.insert(user.id);
        recipients.assign(it->second.user_ids.begin(),
                          it->second.user_ids.end());
    }

    if (broadcast_fn) {
        auto packet = PacketBuilder::voice_started(user.id);
        for (uint32_t uid : recipients) {
            broadcast_fn(uid, packet);
        }
    }

    std::cout << "Voice started: user=" << user.id << " room=" << room_id << "\n";
}

void RoomManager::voice_stop(User& user) {
    if (user.room_id == 0){
         return;
    }

    uint16_t room_id = user.room_id;
    std::vector<uint32_t> recipients;
    {
        std::unique_lock lock(mutex);
        auto it = rooms.find(room_id);
        if (it == rooms.end()){
            return;
        }

        user.is_speaking = false;
        it->second.speaking_users.erase(user.id);
        recipients.assign(it->second.user_ids.begin(),
                          it->second.user_ids.end());
    }

    if (broadcast_fn) {
        auto packet = PacketBuilder::voice_stopped(user.id);
        for (uint32_t uid : recipients) {
            broadcast_fn(uid, packet);
        }
    }

    std::cout << "Voice stopped: user=" << user.id << " room=" << room_id << "\n";
}

bool RoomManager::has_speakers(uint16_t room_id) const {
    std::shared_lock lock(mutex);
    auto it = rooms.find(room_id);
    if (it == rooms.end()){
        return false;
    }
    return !it->second.speaking_users.empty();
}

std::optional<uint32_t> RoomManager::find_client_by_endpoint(const udp::endpoint& ep) const {
    std::shared_lock lock(mutex);
    for (const auto& [room_id, room] : rooms) {
        for (uint32_t uid : room.user_ids) {
            User* user = user_manager.get(uid);
            if (user && user->udp_endpoint.has_value() && user->udp_endpoint.value() == ep) {
                return uid;
            }
        }
    }
    return std::nullopt;
}

uint16_t RoomManager::get_room_id_for_user(uint32_t client_id) const {
    std::shared_lock lock(mutex);
    User* user = user_manager.get(client_id);
    if (user) return user->room_id;
    return 0;
}

void RoomManager::set_list_tracks_fn(ListTracksFn fn) {
    list_tracks_fn_ = std::move(fn);
}

std::vector<TrackListEntry> RoomManager::list_tracks() const {
    if (list_tracks_fn_) {
        return list_tracks_fn_();
    }
    return {};
}

void RoomManager::set_track_exists_fn(TrackExistsFn fn) {
    track_exists_fn_ = std::move(fn);
}

StatusCode RoomManager::select_track_for_room(uint16_t room_id, uint16_t track_id) {
    if (!track_exists_fn_ || !track_exists_fn_(track_id)) {
        return StatusCode::TrackNotFound;
    }

    bool should_start_streaming = false;
    {
        std::unique_lock<std::shared_mutex> lock(mutex);

        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) {
            return StatusCode::RoomNotFound;
        }

        Room& room = room_it->second;

        for (uint16_t existing_id : room.track_ids) {
            if (existing_id == track_id) {
                return StatusCode::Success;
            }
        }

        room.track_ids.push_back(track_id);

        bool first_track = (room.track_ids.size() == 1);
        bool has_users = !room.user_ids.empty();
        should_start_streaming = first_track && has_users;
    }

    if (should_start_streaming && on_first_user_joined_) {
        on_first_user_joined_(room_id);
    }

    return StatusCode::Success;
}
