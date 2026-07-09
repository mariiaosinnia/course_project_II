#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "Protocol.h"


class PacketParser
{
public:
    static std::optional<ConnectedBody> parse_connected(const std::vector<uint8_t>& body);
    static std::optional<RoomCreatedBody> parse_room_created(const std::vector<uint8_t>& body);
    static std::optional<UserJoinedBody> parse_user_joined(const std::vector<uint8_t>& body);
    static std::optional<UserLeftBody> parse_user_left(const std::vector<uint8_t>& body);
    static std::optional<TrackAddedBody> parse_track_added(const std::vector<uint8_t>& body);
    static std::optional<ErrorBody> parse_error(const std::vector<uint8_t>& body);

    struct RoomJoinedResult {
        RoomJoinedHeader header;
        std::vector<UserInfo> users;
    };
    static std::optional<RoomJoinedResult> parse_room_joined(const std::vector<uint8_t>& body);

    struct RoomListResult {
        std::vector<RoomListEntry> rooms;
    };
    static std::optional<RoomListResult> parse_room_list(const std::vector<uint8_t>& body);
};
