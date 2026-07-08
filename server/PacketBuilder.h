#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <boost/endian/conversion.hpp>
#include "Protocol.h"

class PacketBuilder {
public:
    static std::vector<uint8_t> connected(uint32_t client_id);
    static std::vector<uint8_t> room_created(uint16_t room_id);
    static std::vector<uint8_t> room_left();
    static std::vector<uint8_t> pong();
    static std::vector<uint8_t> user_joined(uint32_t client_id, const char* username);
    static std::vector<uint8_t> user_left(uint32_t client_id);
    static std::vector<uint8_t> error(StatusCode code);
    static std::vector<uint8_t> room_list(const std::vector<RoomListEntry>& rooms);
    static std::vector<uint8_t> room_joined(const RoomJoinedHeader& header,
        const std::vector<UserInfo>& users);
    static std::vector<uint8_t> track_added(uint16_t room_id, uint16_t track_id, const char* filename);
private:
    static std::vector<uint8_t> build(PacketType type);
    template<typename T>
    static std::vector<uint8_t> build(PacketType type, const T& body) {
        PacketHeader header;
        header.type = static_cast<uint8_t>(type);
        header.payload_size = boost::endian::native_to_big(static_cast<uint16_t>(sizeof(T)));
        std::vector<uint8_t> packet(sizeof(header) + sizeof(T));
        std::memcpy(packet.data(), &header, sizeof(header));
        std::memcpy(packet.data() + sizeof(header), &body, sizeof(T));
        return packet;
    }
};