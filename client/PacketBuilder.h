#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <boost/endian/conversion.hpp>

#include "Protocol.h"

class PacketBuilder
{
public:
    static std::vector<uint8_t> connect(const std::string& username);
    static std::vector<uint8_t> disconnect();
    static std::vector<uint8_t> create_room(const std::string& name);
    static std::vector<uint8_t> join_room(uint16_t room_id);
    static std::vector<uint8_t> leave_room();
    static std::vector<uint8_t> list_rooms();
    static std::vector<uint8_t> list_tracks();
    static std::vector<uint8_t> track_select(uint16_t track_id);
    static std::vector<uint8_t> voice_start();
    static std::vector<uint8_t> voice_stop();
    static std::vector<uint8_t> ping();
    static std::vector<uint8_t> upload_track_begin(
        uint16_t room_id,
        uint32_t file_size,
        const std::string& filename);
    static std::vector<uint8_t> upload_track_data(const std::vector<uint8_t>& chunk);
    static std::vector<uint8_t> upload_track_end();

private:
    static std::vector<uint8_t> build(PacketType type);

    template<typename T>
    static std::vector<uint8_t> build(PacketType type, const T& body)
    {
        PacketHeader header;
        header.type = static_cast<uint8_t>(type);
        header.payload_size = boost::endian::native_to_big(static_cast<uint16_t>(sizeof(T)));

        std::vector<uint8_t> packet(sizeof(header) + sizeof(T));
        std::memcpy(packet.data(), &header, sizeof(header));
        std::memcpy(packet.data() + sizeof(header), &body, sizeof(T));
        return packet;
    }
};
