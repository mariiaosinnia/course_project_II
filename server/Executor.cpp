#include "Executor.h"
#include "RoomManager.h"
#include "PacketBuilder.h"

Executor::Executor(RoomManager& rm)
    : room_manager(rm)
{
}

std::vector<uint8_t> Executor::execute(User& user, PacketType type,
    const std::vector<uint8_t>& body) {
    switch (type) {
        case PacketType::Ping: return handle_ping();
        case PacketType::Connect: return handle_connect(user, body);
        case PacketType::CreateRoom: return handle_create_room(user, body);
        case PacketType::JoinRoom: return handle_join_room(user, body);
        case PacketType::LeaveRoom: return handle_leave_room(user);
        case PacketType::ListRooms: return handle_list_rooms();
        default: return PacketBuilder::error(StatusCode::Unknown);
    }
}

std::vector<uint8_t> Executor::handle_ping() {
    return PacketBuilder::pong();
}

std::vector<uint8_t> Executor::handle_connect(User& user, const std::vector<uint8_t>& body) {
    std::optional<ConnectBody> parsed = PacketParser::parse_connect(body);
    if (!parsed) {
        return PacketBuilder::error(StatusCode::Unknown);
    }
    user.name = std::string(parsed->username, strnlen(parsed->username, USERNAME_MAX_LEN));
    return PacketBuilder::connected(user.id);
}

std::vector<uint8_t> Executor::handle_create_room(User& user, const std::vector<uint8_t>& body) {
    std::optional<CreateRoomBody> parsed = PacketParser::parse_create_room(body);
    if (!parsed) {
        return PacketBuilder::error(StatusCode::Unknown);
    }
    std::string name(parsed->name, strnlen(parsed->name, ROOM_NAME_MAX_LEN));
    uint16_t room_id = room_manager.create_room(name);
    return PacketBuilder::room_created(room_id);
}

std::vector<uint8_t> Executor::handle_join_room(User& user, const std::vector<uint8_t>& body) {
    std::optional<JoinRoomBody> parsed = PacketParser::parse_join_room(body);
    if (!parsed) {
        return PacketBuilder::error(StatusCode::Unknown);
    }

    StatusCode status = room_manager.join_room(user, parsed->room_id);
    if (status != StatusCode::Success) {
        return PacketBuilder::error(status);
    }

    std::vector<UserInfo> users_in_room = room_manager.get_users_in_room(parsed->room_id);

    RoomJoinedHeader rj_header;
    rj_header.room_id = boost::endian::native_to_big(parsed->room_id);
    rj_header.udp_port = boost::endian::native_to_big(static_cast<uint16_t>(UDP_SERVER_PORT));
    rj_header.track_id = 0;
    rj_header.track_position_ms = boost::endian::native_to_big(room_manager.get_current_position(parsed->room_id));
    rj_header.user_count = static_cast<uint8_t>(users_in_room.size());

    std::vector<uint8_t> packet = PacketBuilder::user_joined(user.id, user.name.c_str());
    room_manager.broadcast_to_room(parsed->room_id, packet, user);

    return PacketBuilder::room_joined(rj_header, users_in_room);
}

std::vector<uint8_t> Executor::handle_leave_room(User& user) {
    if (user.room_id == 0) {
        return PacketBuilder::error(StatusCode::NotInRoom);
    }

    uint16_t room_id = user.room_id;
    StatusCode status = room_manager.leave_room(user, room_id);
    if (status != StatusCode::Success) return PacketBuilder::error(status);

    std::vector<uint8_t>
        packet = PacketBuilder::user_left(user.id);
    room_manager.broadcast_to_room(room_id, packet, user);

    return PacketBuilder::room_left();
}

std::vector<uint8_t> Executor::handle_list_rooms() {
    std::vector<RoomListEntry> rooms = room_manager.list_rooms();
    return PacketBuilder::room_list(rooms);
}