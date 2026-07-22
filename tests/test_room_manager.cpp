#include <gtest/gtest.h>
#include "RoomManager.h"
#include "UserManager.h"
#include "PacketBuilder.h"
#include <boost/endian/conversion.hpp>

class RoomManagerTest : public ::testing::Test {
protected:
    UserManager user_manager;
    RoomManager room_manager{user_manager};

    User* create_user(uint32_t id, const std::string& name = "user") {
        User* u = user_manager.add(id);
        u->name = name;
        return u;
    }
};

TEST_F(RoomManagerTest, CreateRoom_ReturnsUniqueIds) {
    uint16_t id1 = room_manager.create_room("Room1");
    uint16_t id2 = room_manager.create_room("Room2");
    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_NE(id1, id2);
}

TEST_F(RoomManagerTest, JoinRoom_Success) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u = create_user(1);

    StatusCode status = room_manager.join_room(*u, room_id);
    EXPECT_EQ(status, StatusCode::Success);
    EXPECT_EQ(u->room_id, room_id);
    EXPECT_EQ(room_manager.get_user_count(room_id), 1u);
}

TEST_F(RoomManagerTest, JoinRoom_NotFound) {
    User* u = create_user(1);
    StatusCode status = room_manager.join_room(*u, 999);
    EXPECT_EQ(status, StatusCode::RoomNotFound);
}

TEST_F(RoomManagerTest, JoinRoom_AlreadyInRoom) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u = create_user(1);
    room_manager.join_room(*u, room_id);

    uint16_t room2 = room_manager.create_room("Test2");
    StatusCode status = room_manager.join_room(*u, room2);
    EXPECT_EQ(status, StatusCode::AlreadyInRoom);
}

TEST_F(RoomManagerTest, LeaveRoom_Success) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u = create_user(1);
    room_manager.join_room(*u, room_id);

    StatusCode status = room_manager.leave_room(*u, room_id);
    EXPECT_EQ(status, StatusCode::Success);
    EXPECT_EQ(u->room_id, 0);
    EXPECT_EQ(room_manager.get_user_count(room_id), 0u);
}

TEST_F(RoomManagerTest, LeaveRoom_NotInRoom) {
    User* u = create_user(1);
    StatusCode status = room_manager.leave_room(*u, 1);
    EXPECT_EQ(status, StatusCode::NotInRoom);
}

TEST_F(RoomManagerTest, LeaveRoom_StopsSpeaking) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u = create_user(1);
    room_manager.join_room(*u, room_id);

    room_manager.set_broadcast([](uint32_t, std::vector<uint8_t>) {});

    room_manager.voice_start(*u);
    EXPECT_TRUE(u->is_speaking);
    EXPECT_TRUE(room_manager.has_speakers(room_id));

    room_manager.leave_room(*u, room_id);
    EXPECT_FALSE(u->is_speaking);
    EXPECT_FALSE(room_manager.has_speakers(room_id));
}

TEST_F(RoomManagerTest, ListRooms_Empty) {
    auto rooms = room_manager.list_rooms();
    EXPECT_TRUE(rooms.empty());
}

TEST_F(RoomManagerTest, ListRooms_WithRooms) {
    room_manager.create_room("Alpha");
    room_manager.create_room("Beta");

    auto rooms = room_manager.list_rooms();
    EXPECT_EQ(rooms.size(), 2u);
}

TEST_F(RoomManagerTest, ListRooms_CorrectUserCount) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u1 = create_user(1);
    User* u2 = create_user(2);
    room_manager.join_room(*u1, room_id);
    room_manager.join_room(*u2, room_id);

    auto rooms = room_manager.list_rooms();
    ASSERT_EQ(rooms.size(), 1u);
    EXPECT_EQ(rooms[0].user_count, 2);
}

TEST_F(RoomManagerTest, GetUsersInRoom) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u1 = create_user(1, "Alice");
    User* u2 = create_user(2, "Bob");
    room_manager.join_room(*u1, room_id);
    room_manager.join_room(*u2, room_id);

    auto users = room_manager.get_users_in_room(room_id);
    EXPECT_EQ(users.size(), 2u);
}

TEST_F(RoomManagerTest, GetUsersInRoom_NonExistent) {
    auto users = room_manager.get_users_in_room(999);
    EXPECT_TRUE(users.empty());
}

TEST_F(RoomManagerTest, VoiceStart_SetsSpeaking) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u = create_user(1);
    room_manager.join_room(*u, room_id);

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> broadcasts;
    room_manager.set_broadcast([&](uint32_t id, std::vector<uint8_t> pkt) {
        broadcasts.push_back({id, pkt});
    });

    room_manager.voice_start(*u);
    EXPECT_TRUE(u->is_speaking);
    EXPECT_TRUE(room_manager.has_speakers(room_id));

    ASSERT_FALSE(broadcasts.empty());
    auto& pkt = broadcasts[0].second;
    PacketHeader h;
    std::memcpy(&h, pkt.data(), sizeof(h));
    EXPECT_EQ(h.type, static_cast<uint8_t>(PacketType::VoiceStarted));
}

TEST_F(RoomManagerTest, VoiceStop_ClearsSpeaking) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u = create_user(1);
    room_manager.join_room(*u, room_id);

    room_manager.set_broadcast([](uint32_t, std::vector<uint8_t>) {});

    room_manager.voice_start(*u);
    room_manager.voice_stop(*u);
    EXPECT_FALSE(u->is_speaking);
    EXPECT_FALSE(room_manager.has_speakers(room_id));
}

TEST_F(RoomManagerTest, VoiceStart_NotInRoom_NoEffect) {
    User* u = create_user(1);
    room_manager.voice_start(*u);
    EXPECT_FALSE(u->is_speaking);
}

TEST_F(RoomManagerTest, VoiceStart_CallsCallback) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u = create_user(1);
    room_manager.join_room(*u, room_id);
    room_manager.set_broadcast([](uint32_t, std::vector<uint8_t>) {});

    uint32_t callback_client_id = 0;
    uint16_t callback_room_id = 0;
    room_manager.set_on_voice_start([&](uint32_t cid, uint16_t rid) {
        callback_client_id = cid;
        callback_room_id = rid;
    });

    room_manager.voice_start(*u);
    EXPECT_EQ(callback_client_id, u->id);
    EXPECT_EQ(callback_room_id, room_id);
}

TEST_F(RoomManagerTest, VoiceStop_CallsCallback) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u = create_user(1);
    room_manager.join_room(*u, room_id);
    room_manager.set_broadcast([](uint32_t, std::vector<uint8_t>) {});

    uint32_t callback_client_id = 0;
    uint16_t callback_room_id = 0;
    room_manager.set_on_voice_stop([&](uint32_t cid, uint16_t rid) {
        callback_client_id = cid;
        callback_room_id = rid;
    });

    room_manager.voice_start(*u);
    room_manager.voice_stop(*u);
    EXPECT_EQ(callback_client_id, u->id);
    EXPECT_EQ(callback_room_id, room_id);
}

TEST_F(RoomManagerTest, Broadcast_ExcludesUser) {
    uint16_t room_id = room_manager.create_room("Test");
    User* u1 = create_user(1, "Alice");
    User* u2 = create_user(2, "Bob");
    User* u3 = create_user(3, "Charlie");
    room_manager.join_room(*u1, room_id);
    room_manager.join_room(*u2, room_id);
    room_manager.join_room(*u3, room_id);

    std::vector<uint32_t> received_by;
    room_manager.set_broadcast([&](uint32_t id, std::vector<uint8_t>) {
        received_by.push_back(id);
    });

    auto packet = PacketBuilder::user_joined(1, "Alice");
    room_manager.broadcast_to_room(room_id, packet, *u1);

    EXPECT_EQ(received_by.size(), 2u);
    EXPECT_TRUE(std::find(received_by.begin(), received_by.end(), 1) == received_by.end());
    EXPECT_TRUE(std::find(received_by.begin(), received_by.end(), 2) != received_by.end());
    EXPECT_TRUE(std::find(received_by.begin(), received_by.end(), 3) != received_by.end());
}

TEST_F(RoomManagerTest, HasSpeakers_Empty) {
    uint16_t room_id = room_manager.create_room("Test");
    EXPECT_FALSE(room_manager.has_speakers(room_id));
}

TEST_F(RoomManagerTest, HasSpeakers_NonExistentRoom) {
    EXPECT_FALSE(room_manager.has_speakers(999));
}
