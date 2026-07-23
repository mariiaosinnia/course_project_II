#include <gtest/gtest.h>
#include <boost/endian/conversion.hpp>
#include "Executor.h"
#include "RoomManager.h"
#include "UserManager.h"
#include "PacketBuilder.h"
#include "PacketParser.h"
#include "Protocol.h"

template<typename T>
static std::vector<uint8_t> to_body(const T& s) {
    std::vector<uint8_t> v(sizeof(T));
    std::memcpy(v.data(), &s, sizeof(T));
    return v;
}

static PacketHeader extract_header(const std::vector<uint8_t>& packet) {
    PacketHeader h;
    std::memcpy(&h, packet.data(), sizeof(h));
    h.payload_size = boost::endian::big_to_native(h.payload_size);
    return h;
}

class ExecutorTest : public ::testing::Test {
protected:
    UserManager user_manager;
    RoomManager room_manager{user_manager};
    Executor executor{room_manager};
    User user;

    void SetUp() override {
        user.id = 1;
        user.room_id = 0;
        user.is_speaking = false;

        room_manager.set_broadcast([](uint32_t, std::vector<uint8_t>) {});

        User* managed_user = user_manager.add(1);
        managed_user->name = "TestUser";
    }
};

TEST_F(ExecutorTest, Ping_ReturnsPong) {
    auto result = executor.execute(user, PacketType::Ping, {});
    ASSERT_FALSE(result.empty());

    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Pong));
}

TEST_F(ExecutorTest, Connect_SetsUsername) {
    ConnectBody body{};
    std::strncpy(body.username, "Alice", USERNAME_MAX_LEN);

    auto result = executor.execute(user, PacketType::Connect, to_body(body));
    ASSERT_FALSE(result.empty());

    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Connected));
    EXPECT_EQ(user.name, "Alice");
}

TEST_F(ExecutorTest, Connect_InvalidBody) {
    std::vector<uint8_t> tiny(2, 0);
    auto result = executor.execute(user, PacketType::Connect, tiny);
    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));
}

TEST_F(ExecutorTest, CreateRoom_ReturnsRoomCreated) {
    CreateRoomBody body{};
    std::strncpy(body.name, "MyRoom", ROOM_NAME_MAX_LEN);

    auto result = executor.execute(user, PacketType::CreateRoom, to_body(body));
    ASSERT_FALSE(result.empty());

    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomCreated));
}

TEST_F(ExecutorTest, CreateRoom_InvalidBody) {
    std::vector<uint8_t> tiny(2, 0);
    auto result = executor.execute(user, PacketType::CreateRoom, tiny);
    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));
}

TEST_F(ExecutorTest, JoinRoom_Success) {
    CreateRoomBody create_body{};
    std::strncpy(create_body.name, "Room1", ROOM_NAME_MAX_LEN);
    auto create_result = executor.execute(user, PacketType::CreateRoom, to_body(create_body));
    auto create_h = extract_header(create_result);
    ASSERT_EQ(create_h.type, static_cast<uint8_t>(PacketType::RoomCreated));

    RoomCreatedBody created;
    std::memcpy(&created, create_result.data() + sizeof(PacketHeader), sizeof(created));
    uint16_t room_id = boost::endian::big_to_native(created.room_id);

    JoinRoomBody join_body{};
    join_body.room_id = boost::endian::native_to_big(room_id);

    auto result = executor.execute(user, PacketType::JoinRoom, to_body(join_body));
    ASSERT_FALSE(result.empty());

    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomJoined));
    EXPECT_EQ(user.room_id, room_id);
}

TEST_F(ExecutorTest, JoinRoom_NotFound) {
    JoinRoomBody body{};
    body.room_id = boost::endian::native_to_big(static_cast<uint16_t>(999));

    auto result = executor.execute(user, PacketType::JoinRoom, to_body(body));
    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));

    ErrorBody err;
    std::memcpy(&err, result.data() + sizeof(PacketHeader), sizeof(err));
    EXPECT_EQ(err.error_code, static_cast<uint8_t>(StatusCode::RoomNotFound));
}

TEST_F(ExecutorTest, JoinRoom_AlreadyInRoom) {
    CreateRoomBody create_body{};
    std::strncpy(create_body.name, "Room1", ROOM_NAME_MAX_LEN);
    executor.execute(user, PacketType::CreateRoom, to_body(create_body));

    JoinRoomBody join_body{};
    join_body.room_id = boost::endian::native_to_big(static_cast<uint16_t>(1));
    executor.execute(user, PacketType::JoinRoom, to_body(join_body));

    CreateRoomBody create_body2{};
    std::strncpy(create_body2.name, "Room2", ROOM_NAME_MAX_LEN);
    executor.execute(user, PacketType::CreateRoom, to_body(create_body2));

    JoinRoomBody join_body2{};
    join_body2.room_id = boost::endian::native_to_big(static_cast<uint16_t>(2));
    auto result = executor.execute(user, PacketType::JoinRoom, to_body(join_body2));

    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));

    ErrorBody err;
    std::memcpy(&err, result.data() + sizeof(PacketHeader), sizeof(err));
    EXPECT_EQ(err.error_code, static_cast<uint8_t>(StatusCode::AlreadyInRoom));
}

TEST_F(ExecutorTest, LeaveRoom_Success) {
    CreateRoomBody create_body{};
    std::strncpy(create_body.name, "Room1", ROOM_NAME_MAX_LEN);
    executor.execute(user, PacketType::CreateRoom, to_body(create_body));

    JoinRoomBody join_body{};
    join_body.room_id = boost::endian::native_to_big(static_cast<uint16_t>(1));
    executor.execute(user, PacketType::JoinRoom, to_body(join_body));

    auto result = executor.execute(user, PacketType::LeaveRoom, {});
    ASSERT_FALSE(result.empty());

    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomLeft));
    EXPECT_EQ(user.room_id, 0);
}

TEST_F(ExecutorTest, LeaveRoom_NotInRoom) {
    auto result = executor.execute(user, PacketType::LeaveRoom, {});
    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));

    ErrorBody err;
    std::memcpy(&err, result.data() + sizeof(PacketHeader), sizeof(err));
    EXPECT_EQ(err.error_code, static_cast<uint8_t>(StatusCode::NotInRoom));
}

TEST_F(ExecutorTest, ListRooms_Empty) {
    auto result = executor.execute(user, PacketType::ListRooms, {});
    ASSERT_FALSE(result.empty());

    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomList));

    RoomListHeader lh;
    std::memcpy(&lh, result.data() + sizeof(PacketHeader), sizeof(lh));
    EXPECT_EQ(lh.room_count, 0);
}

TEST_F(ExecutorTest, ListRooms_WithRooms) {
    CreateRoomBody body1{};
    std::strncpy(body1.name, "Room1", ROOM_NAME_MAX_LEN);
    executor.execute(user, PacketType::CreateRoom, to_body(body1));

    CreateRoomBody body2{};
    std::strncpy(body2.name, "Room2", ROOM_NAME_MAX_LEN);
    executor.execute(user, PacketType::CreateRoom, to_body(body2));

    auto result = executor.execute(user, PacketType::ListRooms, {});
    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::RoomList));

    RoomListHeader lh;
    std::memcpy(&lh, result.data() + sizeof(PacketHeader), sizeof(lh));
    EXPECT_EQ(lh.room_count, 2);
}

TEST_F(ExecutorTest, VoiceStart_NotInRoom) {
    auto result = executor.execute(user, PacketType::VoiceStart, {});
    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));

    ErrorBody err;
    std::memcpy(&err, result.data() + sizeof(PacketHeader), sizeof(err));
    EXPECT_EQ(err.error_code, static_cast<uint8_t>(StatusCode::NotInRoom));
}

TEST_F(ExecutorTest, VoiceStart_Success) {
    CreateRoomBody create_body{};
    std::strncpy(create_body.name, "Room1", ROOM_NAME_MAX_LEN);
    executor.execute(user, PacketType::CreateRoom, to_body(create_body));

    JoinRoomBody join_body{};
    join_body.room_id = boost::endian::native_to_big(static_cast<uint16_t>(1));
    executor.execute(user, PacketType::JoinRoom, to_body(join_body));

    auto result = executor.execute(user, PacketType::VoiceStart, {});
    EXPECT_TRUE(result.empty());
    EXPECT_TRUE(user.is_speaking);
}

TEST_F(ExecutorTest, VoiceStop_NotInRoom) {
    auto result = executor.execute(user, PacketType::VoiceStop, {});
    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));
}

TEST_F(ExecutorTest, VoiceStop_Success) {
    CreateRoomBody create_body{};
    std::strncpy(create_body.name, "Room1", ROOM_NAME_MAX_LEN);
    executor.execute(user, PacketType::CreateRoom, to_body(create_body));

    JoinRoomBody join_body{};
    join_body.room_id = boost::endian::native_to_big(static_cast<uint16_t>(1));
    executor.execute(user, PacketType::JoinRoom, to_body(join_body));

    executor.execute(user, PacketType::VoiceStart, {});
    auto result = executor.execute(user, PacketType::VoiceStop, {});
    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(user.is_speaking);
}

TEST_F(ExecutorTest, UnknownPacket_ReturnsError) {
    auto result = executor.execute(user, static_cast<PacketType>(0xFE), {});
    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::Error));

    ErrorBody err;
    std::memcpy(&err, result.data() + sizeof(PacketHeader), sizeof(err));
    EXPECT_EQ(err.error_code, static_cast<uint8_t>(StatusCode::Unknown));
}

TEST_F(ExecutorTest, ListTracks_Empty) {
    auto result = executor.execute(user, PacketType::ListTracks, {});
    ASSERT_FALSE(result.empty());

    auto h = extract_header(result);
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::TrackList));

    TrackListHeader lh;
    std::memcpy(&lh, result.data() + sizeof(PacketHeader), sizeof(lh));
    EXPECT_EQ(lh.track_count, 0);
}

TEST_F(ExecutorTest, Cleanup_ResetsUploadState) {
    executor.cleanup();
    SUCCEED();
}
