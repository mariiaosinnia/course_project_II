#pragma once
#include <cstdint>
#include <vector>

class MusicStreamer {//rename to packetBuilder
public:
    static std::vector<uint8_t> buildPacket(uint32_t seq, const std::vector<uint8_t>& opus, uint32_t track_position_ms);
    static std::vector<uint8_t> buildVoicePacket(uint32_t sender_client_id, uint32_t seq, const std::vector<uint8_t>& opus);
};