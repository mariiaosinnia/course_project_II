#include "PacketBuilder.h"

namespace {
    void copy_fixed(char* dst, size_t dst_len, const std::string& src)
    {
        std::strncpy(dst, src.c_str(), dst_len - 1);
        dst[dst_len - 1] = '\0';
    }
}

std::vector<uint8_t> PacketBuilder::connect(const std::string& username)
{
    ConnectBody body{};
    copy_fixed(body.username, sizeof(body.username), username);
    return build(PacketType::Connect, body);
}

std::vector<uint8_t> PacketBuilder::disconnect()
{
    return build(PacketType::Disconnect);
}

std::vector<uint8_t> PacketBuilder::create_room(const std::string& name)
{
    CreateRoomBody body{};
    copy_fixed(body.name, sizeof(body.name), name);
    return build(PacketType::CreateRoom, body);
}

std::vector<uint8_t> PacketBuilder::join_room(uint16_t room_id)
{
    JoinRoomBody body{};
    body.room_id = boost::endian::native_to_big(room_id);
    return build(PacketType::JoinRoom, body);
}

std::vector<uint8_t> PacketBuilder::leave_room()
{
    return build(PacketType::LeaveRoom);
}

std::vector<uint8_t> PacketBuilder::list_rooms()
{
    return build(PacketType::ListRooms);
}

std::vector<uint8_t> PacketBuilder::list_tracks()
{
    return build(PacketType::ListTracks);
}

std::vector<uint8_t> PacketBuilder::track_select(uint16_t track_id)
{
    TrackSelectBody body{};
    body.track_id = boost::endian::native_to_big(track_id);
    return build(PacketType::TrackSelect, body);
}

std::vector<uint8_t> PacketBuilder::voice_start()
{
    return build(PacketType::VoiceStart);
}

std::vector<uint8_t> PacketBuilder::voice_stop()
{
    return build(PacketType::VoiceStop);
}

std::vector<uint8_t> PacketBuilder::ping()
{
    return build(PacketType::Ping);
}

std::vector<uint8_t> PacketBuilder::upload_track_begin(
    uint16_t room_id,
    uint32_t file_size,
    const std::string& filename)
{
    UploadTrackBeginBody body{};
    body.room_id = boost::endian::native_to_big(room_id);
    body.file_size = boost::endian::native_to_big(file_size);
    copy_fixed(body.filename, sizeof(body.filename), filename);
    return build(PacketType::UploadTrackBegin, body);
}

std::vector<uint8_t> PacketBuilder::upload_track_data(const std::vector<uint8_t>& chunk)
{
    PacketHeader header;
    header.type = static_cast<uint8_t>(PacketType::UploadTrackData);
    header.payload_size = boost::endian::native_to_big(static_cast<uint16_t>(chunk.size()));

    std::vector<uint8_t> packet(sizeof(header) + chunk.size());
    std::memcpy(packet.data(), &header, sizeof(header));
    if (!chunk.empty()) {
        std::memcpy(packet.data() + sizeof(header), chunk.data(), chunk.size());
    }
    return packet;
}

std::vector<uint8_t> PacketBuilder::upload_track_end()
{
    return build(PacketType::UploadTrackEnd);
}

std::vector<uint8_t> PacketBuilder::build(PacketType type)
{
    PacketHeader header;
    header.type = static_cast<uint8_t>(type);
    header.payload_size = 0;

    std::vector<uint8_t> packet(sizeof(header));
    std::memcpy(packet.data(), &header, sizeof(header));
    return packet;
}
