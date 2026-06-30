#include <cstring>

#include <boost/endian/conversion.hpp>

#include "PacketParser.h"

std::optional<ConnectedBody> PacketParser::parse_connected(const std::vector<uint8_t>& body)
{
    if (body.size() < sizeof(ConnectedBody)) {
        return std::nullopt;
    }

    ConnectedBody result;
    std::memcpy(&result, body.data(), sizeof(result));
    result.client_id = boost::endian::big_to_native(result.client_id);

    return result;
}

std::optional<RoomCreatedBody> PacketParser::parse_room_created(const std::vector<uint8_t>& body)
{
    if (body.size() < sizeof(RoomCreatedBody)) {
        return std::nullopt;
    }

    RoomCreatedBody result;
    std::memcpy(&result, body.data(), sizeof(result));
    result.room_id = boost::endian::big_to_native(result.room_id);

    return result;
}

std::optional<UserJoinedBody> PacketParser::parse_user_joined(const std::vector<uint8_t>& body)
{
    if (body.size() < sizeof(UserJoinedBody)) {
        return std::nullopt;
    }

    UserJoinedBody result;
    std::memcpy(&result, body.data(), sizeof(result));
    result.client_id = boost::endian::big_to_native(result.client_id);
    result.username[sizeof(result.username) - 1] = '\0';

    return result;
}

std::optional<UserLeftBody> PacketParser::parse_user_left(const std::vector<uint8_t>& body)
{
    if (body.size() < sizeof(UserLeftBody)) {
        return std::nullopt;
    }

    UserLeftBody result;
    std::memcpy(&result, body.data(), sizeof(result));
    result.client_id = boost::endian::big_to_native(result.client_id);

    return result;
}

std::optional<ErrorBody> PacketParser::parse_error(const std::vector<uint8_t>& body)
{
    if (body.size() < sizeof(ErrorBody)) {
        return std::nullopt;
    }

    ErrorBody result;
    std::memcpy(&result, body.data(), sizeof(result));

    return result;
}

std::optional<PacketParser::RoomJoinedResult> PacketParser::parse_room_joined(
    const std::vector<uint8_t>& body)
{
    if (body.size() < sizeof(RoomJoinedHeader)) {
        return std::nullopt;
    }

    RoomJoinedResult result;
    std::memcpy(&result.header, body.data(), sizeof(RoomJoinedHeader));
    result.header.room_id = boost::endian::big_to_native(result.header.room_id);
    result.header.udp_port = boost::endian::big_to_native(result.header.udp_port);
    result.header.track_id = boost::endian::big_to_native(result.header.track_id);
    result.header.track_position_ms = boost::endian::big_to_native(result.header.track_position_ms);

    const size_t expected_size =
        sizeof(RoomJoinedHeader) + result.header.user_count * sizeof(UserInfo);
    if (body.size() < expected_size) {
        return std::nullopt;
    }

    const uint8_t* cursor = body.data() + sizeof(RoomJoinedHeader);
    for (uint8_t i = 0; i < result.header.user_count; ++i) {
        UserInfo user;
        std::memcpy(&user, cursor, sizeof(UserInfo));
        user.client_id = boost::endian::big_to_native(user.client_id);
        user.username[sizeof(user.username) - 1] = '\0';
        result.users.push_back(user);
        cursor += sizeof(UserInfo);
    }

    return result;
}

std::optional<PacketParser::RoomListResult> PacketParser::parse_room_list(
    const std::vector<uint8_t>& body)
{
    if (body.size() < sizeof(RoomListHeader)) {
        return std::nullopt;
    }

    RoomListHeader header;
    std::memcpy(&header, body.data(), sizeof(RoomListHeader));

    const size_t expected_size =
        sizeof(RoomListHeader) + header.room_count * sizeof(RoomListEntry);
    if (body.size() < expected_size) {
        return std::nullopt;
    }

    RoomListResult result;
    const uint8_t* cursor = body.data() + sizeof(RoomListHeader);
    for (uint8_t i = 0; i < header.room_count; ++i) {
        RoomListEntry entry;
        std::memcpy(&entry, cursor, sizeof(RoomListEntry));
        entry.room_id = boost::endian::big_to_native(entry.room_id);
        entry.name[sizeof(entry.name) - 1] = '\0';
        result.rooms.push_back(entry);
        cursor += sizeof(RoomListEntry);
    }

    return result;
}
