#include <boost/endian/conversion.hpp>
#include "PacketBuilder.h"
#include "Protocol.h"

std::vector<uint8_t> PacketBuilder::connected(uint32_t client_id) {
    ConnectedBody body;
    body.client_id = boost::endian::native_to_big(client_id);
    return build(PacketType::Connected, body);
}

std::vector<uint8_t> PacketBuilder::pong() {
    return build(PacketType::Pong);
}

std::vector<uint8_t> PacketBuilder::error(StatusCode code) {
    ErrorBody body;
    body.error_code = static_cast<uint8_t>(code);
    return build(PacketType::Error, body);
}

std::vector<uint8_t> PacketBuilder::room_created(uint16_t room_id) {
    RoomCreatedBody body;
    body.room_id = boost::endian::native_to_big(room_id);
    return build(PacketType::RoomCreated, body);
}

std::vector<uint8_t> PacketBuilder::room_left() {
    return build(PacketType::RoomLeft);
}

std::vector<uint8_t> PacketBuilder::user_joined(uint32_t client_id, const char* username) {
    UserJoinedBody body;
    body.client_id = boost::endian::native_to_big(client_id);
    std::strncpy(body.username, username, USERNAME_MAX_LEN - 1);
    body.username[USERNAME_MAX_LEN - 1] = '\0';
    return build(PacketType::UserJoined, body);
}

std::vector<uint8_t> PacketBuilder::user_left(uint32_t client_id) {
    UserLeftBody body;
    body.client_id = boost::endian::native_to_big(client_id);
    return build(PacketType::UserLeft, body);
}

std::vector<uint8_t> PacketBuilder::room_list(const std::vector<RoomListEntry>& rooms) {
    RoomListHeader list_header;
    list_header.room_count = static_cast<uint8_t>(rooms.size());

    uint16_t payload_size = sizeof(RoomListHeader) + rooms.size() * sizeof(RoomListEntry);

    PacketHeader header;
    header.type = static_cast<uint8_t>(PacketType::RoomList);
    header.payload_size = boost::endian::native_to_big(payload_size);

    std::vector<uint8_t> packet(sizeof(PacketHeader) + payload_size);
    std::memcpy(packet.data(), &header, sizeof(PacketHeader));
    std::memcpy(packet.data() + sizeof(PacketHeader), &list_header, sizeof(RoomListHeader));

    size_t offset = sizeof(PacketHeader) + sizeof(RoomListHeader);
    for (const RoomListEntry& entry : rooms) {
        RoomListEntry e = entry;
        e.room_id = boost::endian::native_to_big(e.room_id);
        std::memcpy(packet.data() + offset, &e, sizeof(RoomListEntry));
        offset += sizeof(RoomListEntry);
    }

    return packet;
}

std::vector<uint8_t> PacketBuilder::room_joined(const RoomJoinedHeader& rj_header,
    const std::vector<UserInfo>& users) {
    size_t payload_size = sizeof(RoomJoinedHeader) + users.size() * sizeof(UserInfo);

    PacketHeader header;
    header.type = static_cast<uint8_t>(PacketType::RoomJoined);
    header.payload_size = boost::endian::native_to_big(static_cast<uint16_t>(payload_size));

    std::vector<uint8_t> packet(sizeof(PacketHeader) + payload_size);
    std::memcpy(packet.data(), &header, sizeof(PacketHeader));
    std::memcpy(packet.data() + sizeof(PacketHeader), &rj_header, sizeof(RoomJoinedHeader));

    size_t offset = sizeof(PacketHeader) + sizeof(RoomJoinedHeader);
    for (const UserInfo& user : users) {
        UserInfo u = user;
        u.client_id = boost::endian::native_to_big(u.client_id);
        std::memcpy(packet.data() + offset, &u, sizeof(UserInfo));
        offset += sizeof(UserInfo);
    }

    return packet;
}

std::vector<uint8_t> PacketBuilder::build(PacketType type) {
    PacketHeader header;
    header.type = static_cast<uint8_t>(type);
    header.payload_size = 0;
    std::vector<uint8_t> packet(sizeof(header));
    std::memcpy(packet.data(), &header, sizeof(header));
    return packet;
}