#include "MusicStreamer.h"
#include "Protocol.h"
#include <cstring>

std::vector<uint8_t>
MusicStreamer::buildPacket(uint32_t seq, const std::vector<uint8_t> &opus,
                           uint32_t track_position_ms) {
  std::vector<uint8_t> packet(
      9 + opus.size()); // 1 (type) + 4 (seq) + 4 (position) + opus

  packet[0] = static_cast<uint8_t>(UdpPacketType::Music);

  packet[1] = static_cast<uint8_t>((seq >> 24) & 0xFF);
  packet[2] = static_cast<uint8_t>((seq >> 16) & 0xFF);
  packet[3] = static_cast<uint8_t>((seq >> 8) & 0xFF);
  packet[4] = static_cast<uint8_t>(seq & 0xFF);

  packet[5] = static_cast<uint8_t>((track_position_ms >> 24) & 0xFF);
  packet[6] = static_cast<uint8_t>((track_position_ms >> 16) & 0xFF);
  packet[7] = static_cast<uint8_t>((track_position_ms >> 8) & 0xFF);
  packet[8] = static_cast<uint8_t>(track_position_ms & 0xFF);

  std::memcpy(packet.data() + 9, opus.data(), opus.size());

  return packet;
}

std::vector<uint8_t>
MusicStreamer::buildVoicePacket(uint32_t sender_client_id, uint32_t seq,
                                const std::vector<uint8_t> &opus) {
  std::vector<uint8_t> packet(9 + opus.size());

  packet[0] = static_cast<uint8_t>(UdpPacketType::Voice);

  packet[1] = static_cast<uint8_t>((sender_client_id >> 24) & 0xFF);
  packet[2] = static_cast<uint8_t>((sender_client_id >> 16) & 0xFF);
  packet[3] = static_cast<uint8_t>((sender_client_id >> 8) & 0xFF);
  packet[4] = static_cast<uint8_t>(sender_client_id & 0xFF);

  packet[5] = static_cast<uint8_t>((seq >> 24) & 0xFF);
  packet[6] = static_cast<uint8_t>((seq >> 16) & 0xFF);
  packet[7] = static_cast<uint8_t>((seq >> 8) & 0xFF);
  packet[8] = static_cast<uint8_t>(seq & 0xFF);

  std::memcpy(packet.data() + 9, opus.data(), opus.size());

  return packet;
}