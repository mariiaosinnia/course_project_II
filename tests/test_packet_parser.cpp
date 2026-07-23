#include <gtest/gtest.h>
#include <boost/endian/conversion.hpp>
#include "PacketParser.h"
#include "Protocol.h"

TEST(PacketParser, ParseConnect_Valid) {
    ConnectBody body{};
    std::strncpy(body.username, "Alice", USERNAME_MAX_LEN);

    std::vector<uint8_t> raw(sizeof(body));
    std::memcpy(raw.data(), &body, sizeof(body));

    auto result = PacketParser::parse_connect(raw);
    ASSERT_TRUE(result.has_value());
    EXPECT_STREQ(result->username, "Alice");
}

TEST(PacketParser, ParseConnect_TooShort) {
    std::vector<uint8_t> raw(2);
    auto result = PacketParser::parse_connect(raw);
    EXPECT_FALSE(result.has_value());
}

TEST(PacketParser, ParseConnect_NullTerminated) {
    ConnectBody body{};
    std::memset(body.username, 'X', USERNAME_MAX_LEN);

    std::vector<uint8_t> raw(sizeof(body));
    std::memcpy(raw.data(), &body, sizeof(body));

    auto result = PacketParser::parse_connect(raw);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username[USERNAME_MAX_LEN - 1], '\0');
}

TEST(PacketParser, ParseJoinRoom_Valid) {
    JoinRoomBody body{};
    body.room_id = boost::endian::native_to_big(static_cast<uint16_t>(42));

    std::vector<uint8_t> raw(sizeof(body));
    std::memcpy(raw.data(), &body, sizeof(body));

    auto result = PacketParser::parse_join_room(raw);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->room_id, 42);
}

TEST(PacketParser, ParseJoinRoom_TooShort) {
    std::vector<uint8_t> raw(1);
    auto result = PacketParser::parse_join_room(raw);
    EXPECT_FALSE(result.has_value());
}

TEST(PacketParser, ParseCreateRoom_Valid) {
    CreateRoomBody body{};
    std::strncpy(body.name, "TestRoom", ROOM_NAME_MAX_LEN);

    std::vector<uint8_t> raw(sizeof(body));
    std::memcpy(raw.data(), &body, sizeof(body));

    auto result = PacketParser::parse_create_room(raw);
    ASSERT_TRUE(result.has_value());
    EXPECT_STREQ(result->name, "TestRoom");
}

TEST(PacketParser, ParseCreateRoom_TooShort) {
    std::vector<uint8_t> raw(5);
    auto result = PacketParser::parse_create_room(raw);
    EXPECT_FALSE(result.has_value());
}

TEST(PacketParser, ParseTrackSelect_Valid) {
    TrackSelectBody body{};
    body.track_id = boost::endian::native_to_big(static_cast<uint16_t>(7));

    std::vector<uint8_t> raw(sizeof(body));
    std::memcpy(raw.data(), &body, sizeof(body));

    auto result = PacketParser::parse_track_select(raw);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->track_id, 7);
}

TEST(PacketParser, ParseTrackSelect_TooShort) {
    std::vector<uint8_t> raw(1);
    auto result = PacketParser::parse_track_select(raw);
    EXPECT_FALSE(result.has_value());
}

TEST(PacketParser, ParseUploadBegin_Valid) {
    UploadTrackBeginBody body{};
    body.room_id = boost::endian::native_to_big(static_cast<uint16_t>(3));
    body.file_size = boost::endian::native_to_big(static_cast<uint32_t>(1024));
    std::strncpy(body.filename, "song.mp3", FILENAME_MAX_LEN);

    std::vector<uint8_t> raw(sizeof(body));
    std::memcpy(raw.data(), &body, sizeof(body));

    auto result = PacketParser::parse_upload_begin(raw);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->room_id, 3);
    EXPECT_EQ(result->file_size, 1024u);
    EXPECT_STREQ(result->filename, "song.mp3");
}

TEST(PacketParser, ParseUploadBegin_TooShort) {
    std::vector<uint8_t> raw(10);
    auto result = PacketParser::parse_upload_begin(raw);
    EXPECT_FALSE(result.has_value());
}
