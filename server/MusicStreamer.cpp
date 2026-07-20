#include "MusicStreamer.h"
#include <cstring>

std::vector<uint8_t> MusicStreamer::buildPacket(uint32_t seq, const std::vector<uint8_t>& opus, uint32_t track_position_ms) {
    std::vector<uint8_t> packet(4 + opus.size());

    packet[0] = static_cast<uint8_t>((seq >> 24) & 0xFF);
    packet[1] = static_cast<uint8_t>((seq >> 16) & 0xFF);
    packet[2] = static_cast<uint8_t>((seq >> 8)  & 0xFF);
    packet[3] = static_cast<uint8_t>( seq        & 0xFF);

    packet[4] = static_cast<uint8_t>((track_position_ms >> 24) & 0xFF);
    packet[5] = static_cast<uint8_t>((track_position_ms >> 16) & 0xFF);
    packet[6] = static_cast<uint8_t>((track_position_ms >> 8)  & 0xFF);
    packet[7] = static_cast<uint8_t>( track_position_ms        & 0xFF);

    std::memcpy(packet.data() + 4, opus.data(), opus.size());

    return packet;
}