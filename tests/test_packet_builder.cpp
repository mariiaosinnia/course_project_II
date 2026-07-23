#include <gtest/gtest.h>
#include <boost/endian/conversion.hpp>
#include "PacketBuilder.h"
#include "Protocol.h"

static PacketHeader extract_header(const std::vector<uint8_t>& packet) {
    PacketHeader h;
    std::memcpy(&h, packet.data(), sizeof(h));
    h.payload_size = boost::endian::big_to_native(h.payload_size);
    return h;
}

TEST(PacketBuilder, Pong) {
    auto packet = PacketBuilder::pong();
    ASSERT_EQ(packet.size(), sizeof(PacketHeader));

    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Pong));
    EXPECT_EQ(h.payload_size, 0);
}

TEST(PacketBuilder, Connected) {
    auto packet = PacketBuilder::connected(42);
    ASSERT_EQ(packet.size(), sizeof(PacketHeader) + sizeof(ConnectedBody));

    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Connected));
    EXPECT_EQ(h.payload_size, sizeof(ConnectedBody));

    ConnectedBody body;
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    EXPECT_EQ(boost::endian::big_to_native(body.client_id), 42u);
}

TEST(PacketBuilder, Error) {
    auto packet = PacketBuilder::error(StatusCode::RoomNotFound);
    ASSERT_EQ(packet.size(), sizeof(PacketHeader) + sizeof(ErrorBody));

    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));

    ErrorBody body;
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    EXPECT_EQ(body.error_code, static_cast<uint8_t>(StatusCode::RoomNotFound));
}

TEST(PacketBuilder, RoomCreated) {
    auto packet = PacketBuilder::room_created(5);
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomCreated));

    RoomCreatedBody body;
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    EXPECT_EQ(boost::endian::big_to_native(body.room_id), 5);
}

TEST(PacketBuilder, RoomLeft) {
    auto packet = PacketBuilder::room_left();
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomLeft));
    EXPECT_EQ(h.payload_size, 0);
}

TEST(PacketBuilder, UserJoined) {
    auto packet = PacketBuilder::user_joined(10, "Bob");
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::UserJoined));

    UserJoinedBody body;
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    EXPECT_EQ(boost::endian::big_to_native(body.client_id), 10u);
    EXPECT_STREQ(body.username, "Bob");
}

TEST(PacketBuilder, UserLeft) {
    auto packet = PacketBuilder::user_left(7);
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::UserLeft));

    UserLeftBody body;
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    EXPECT_EQ(boost::endian::big_to_native(body.client_id), 7u);
}

TEST(PacketBuilder, VoiceStarted) {
    auto packet = PacketBuilder::voice_started(99);
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::VoiceStarted));

    VoiceStartedBody body;
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    EXPECT_EQ(boost::endian::big_to_native(body.client_id), 99u);
}

TEST(PacketBuilder, VoiceStopped) {
    auto packet = PacketBuilder::voice_stopped(99);
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::VoiceStopped));

    VoiceStoppedBody body;
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    EXPECT_EQ(boost::endian::big_to_native(body.client_id), 99u);
}

TEST(PacketBuilder, RoomList_Empty) {
    std::vector<RoomListEntry> rooms;
    auto packet = PacketBuilder::room_list(rooms);
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomList));

    RoomListHeader lh;
    std::memcpy(&lh, packet.data() + sizeof(PacketHeader), sizeof(lh));
    EXPECT_EQ(lh.room_count, 0);
}

TEST(PacketBuilder, RoomList_WithEntries) {
    RoomListEntry entry{};
    entry.room_id = 1;
    std::strncpy(entry.name, "Room1", ROOM_NAME_MAX_LEN);
    entry.user_count = 3;

    std::vector<RoomListEntry> rooms = {entry};
    auto packet = PacketBuilder::room_list(rooms);
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomList));

    RoomListHeader lh;
    std::memcpy(&lh, packet.data() + sizeof(PacketHeader), sizeof(lh));
    EXPECT_EQ(lh.room_count, 1);

    RoomListEntry parsed;
    std::memcpy(&parsed, packet.data() + sizeof(PacketHeader) + sizeof(RoomListHeader),
                sizeof(parsed));
    EXPECT_EQ(boost::endian::big_to_native(parsed.room_id), 1);
    EXPECT_STREQ(parsed.name, "Room1");
    EXPECT_EQ(parsed.user_count, 3);
}

TEST(PacketBuilder, TrackAdded) {
    auto packet = PacketBuilder::track_added(2, 5, "test.mp3");
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::TrackAdded));

    TrackAddedBody body;
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    EXPECT_EQ(boost::endian::big_to_native(body.room_id), 2);
    EXPECT_EQ(boost::endian::big_to_native(body.track_id), 5);
    EXPECT_STREQ(body.filename, "test.mp3");
}

TEST(PacketBuilder, TrackList_Empty) {
    std::vector<TrackListEntry> tracks;
    auto packet = PacketBuilder::track_list(tracks);
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::TrackList));

    TrackListHeader lh;
    std::memcpy(&lh, packet.data() + sizeof(PacketHeader), sizeof(lh));
    EXPECT_EQ(lh.track_count, 0);
}

TEST(PacketBuilder, RoomJoined) {
    RoomJoinedHeader rj{};
    rj.room_id = boost::endian::native_to_big(static_cast<uint16_t>(1));
    rj.udp_port = boost::endian::native_to_big(static_cast<uint16_t>(25046));
    rj.track_id = boost::endian::native_to_big(static_cast<uint16_t>(0));
    rj.track_position_ms = boost::endian::native_to_big(static_cast<uint32_t>(0));
    rj.user_count = 1;

    UserInfo user{};
    user.client_id = boost::endian::native_to_big(static_cast<uint32_t>(10));
    std::strncpy(user.username, "Alice", USERNAME_MAX_LEN);

    auto packet = PacketBuilder::room_joined(rj, {user});
    auto h = extract_header(packet);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomJoined));
    EXPECT_EQ(h.payload_size, sizeof(RoomJoinedHeader) + sizeof(UserInfo));
}
