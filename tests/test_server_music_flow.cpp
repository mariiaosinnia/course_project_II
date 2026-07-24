#include <gtest/gtest.h>

#include <boost/endian/conversion.hpp>

#include <filesystem>
#include <unordered_map>

#include "Executor.h"
#include "PacketBuilder.h"
#include "Protocol.h"
#include "RoomManager.h"
#include "UserManager.h"

namespace {

template <typename T>
std::vector<uint8_t> to_body(const T& value) {
    std::vector<uint8_t> body(sizeof(T));
    std::memcpy(body.data(), &value, sizeof(T));
    return body;
}

PacketHeader header_of(const std::vector<uint8_t>& packet) {
    PacketHeader header{};
    std::memcpy(&header, packet.data(), sizeof(header));
    header.payload_size = boost::endian::big_to_native(header.payload_size);
    return header;
}

ErrorBody error_body_of(const std::vector<uint8_t>& packet) {
    ErrorBody body{};
    std::memcpy(&body, packet.data() + sizeof(PacketHeader), sizeof(body));
    return body;
}

TrackListEntry make_track(uint16_t id, const char* filename) {
    TrackListEntry entry{};
    entry.track_id = id;
    std::strncpy(entry.filename, filename, FILENAME_MAX_LEN - 1);
    return entry;
}

class ServerMusicFlowTest : public ::testing::Test {
protected:
    UserManager user_manager;
    RoomManager room_manager{user_manager};
    Executor executor{room_manager};
    User user{};
    std::vector<TrackListEntry> tracks;
    std::vector<std::pair<uint32_t, PacketType>> broadcasts;
    uint16_t next_track_id = 10;

    void SetUp() override {
        std::filesystem::remove_all("uploads");
        user.id = 1;
        user.name = "Alice";
        user.room_id = 0;
        user.is_speaking = false;
        user_manager.add(user.id)->name = user.name;

        room_manager.set_broadcast([this](uint32_t client_id, std::vector<uint8_t> packet) {
            broadcasts.push_back({client_id, static_cast<PacketType>(header_of(packet).type)});
        });
        room_manager.set_list_tracks_fn([this] { return tracks; });
        room_manager.set_track_exists_fn([this](uint16_t id) {
            return std::any_of(tracks.begin(), tracks.end(),
                [id](const TrackListEntry& entry) { return entry.track_id == id; });
        });
        room_manager.set_on_track_added([this](const std::string& path) {
            TrackListEntry entry{};
            entry.track_id = next_track_id++;
            std::string filename = std::filesystem::path(path).filename().string();
            std::strncpy(entry.filename, filename.c_str(), FILENAME_MAX_LEN - 1);
            tracks.push_back(entry);
            return entry.track_id;
        });
    }

    void TearDown() override {
        std::filesystem::remove_all("uploads");
    }

    uint16_t create_and_join_room() {
        uint16_t room_id = room_manager.create_room("Room");
        EXPECT_EQ(room_manager.join_room(user, room_id), StatusCode::Success);
        return room_id;
    }
};

} // namespace

TEST_F(ServerMusicFlowTest, ListTracksReflectsServerLibrary) {
    tracks.push_back(make_track(3, "alpha.mp3"));
    tracks.push_back(make_track(4, "song with spaces.mp3"));

    auto packet = executor.execute(user, PacketType::ListTracks, {});
    ASSERT_EQ(header_of(packet).type, static_cast<uint8_t>(PacketType::TrackList));

    TrackListHeader list_header{};
    std::memcpy(&list_header, packet.data() + sizeof(PacketHeader), sizeof(list_header));
    ASSERT_EQ(list_header.track_count, 2);

    TrackListEntry first{};
    std::memcpy(&first,
        packet.data() + sizeof(PacketHeader) + sizeof(TrackListHeader),
        sizeof(first));
    EXPECT_EQ(boost::endian::big_to_native(first.track_id), 3);
    EXPECT_STREQ(first.filename, "alpha.mp3");
}

TEST_F(ServerMusicFlowTest, TrackSelectAddsExistingServerTrackToAuthoritativeQueue) {
    uint16_t room_id = create_and_join_room();
    tracks.push_back(make_track(7, "queued.mp3"));

    TrackSelectBody select{};
    select.track_id = boost::endian::native_to_big(static_cast<uint16_t>(7));
    auto result = executor.execute(user, PacketType::TrackSelect, to_body(select));

    ASSERT_EQ(header_of(result).type, static_cast<uint8_t>(PacketType::TrackAdded));
    auto queue = room_manager.list_queue(room_id);
    ASSERT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue[0].track_id, 7);
    EXPECT_STREQ(queue[0].filename, "queued.mp3");
}

TEST_F(ServerMusicFlowTest, TrackSelectRejectsUnknownTrackWithoutChangingQueue) {
    uint16_t room_id = create_and_join_room();

    TrackSelectBody select{};
    select.track_id = boost::endian::native_to_big(static_cast<uint16_t>(99));
    auto result = executor.execute(user, PacketType::TrackSelect, to_body(select));

    ASSERT_EQ(header_of(result).type, static_cast<uint8_t>(PacketType::Error));
    EXPECT_EQ(error_body_of(result).error_code, static_cast<uint8_t>(StatusCode::TrackNotFound));
    EXPECT_TRUE(room_manager.list_queue(room_id).empty());
}

TEST_F(ServerMusicFlowTest, ListQueueRequiresJoinedRoom) {
    auto result = executor.execute(user, PacketType::ListQueue, {});
    ASSERT_EQ(header_of(result).type, static_cast<uint8_t>(PacketType::Error));
    EXPECT_EQ(error_body_of(result).error_code, static_cast<uint8_t>(StatusCode::NotInRoom));
}

TEST_F(ServerMusicFlowTest, QueueListMarksCurrentlyPlayingTrack) {
    uint16_t room_id = create_and_join_room();
    tracks.push_back(make_track(1, "one.mp3"));
    tracks.push_back(make_track(2, "two.mp3"));
    EXPECT_EQ(room_manager.select_track_for_room(room_id, 1), StatusCode::Success);
    EXPECT_EQ(room_manager.select_track_for_room(room_id, 2), StatusCode::Success);
    room_manager.start_playback(room_id, std::chrono::steady_clock::now());

    auto packet = executor.execute(user, PacketType::ListQueue, {});
    ASSERT_EQ(header_of(packet).type, static_cast<uint8_t>(PacketType::QueueList));

    QueueListHeader list_header{};
    std::memcpy(&list_header, packet.data() + sizeof(PacketHeader), sizeof(list_header));
    ASSERT_EQ(list_header.track_count, 2);

    QueueListEntry first{};
    std::memcpy(&first,
        packet.data() + sizeof(PacketHeader) + sizeof(QueueListHeader),
        sizeof(first));
    EXPECT_EQ(boost::endian::big_to_native(first.track_id), 1);
    EXPECT_EQ(first.is_current, 1);
    EXPECT_STREQ(first.filename, "one.mp3");
}

TEST_F(ServerMusicFlowTest, UploadPacketsRegisterTrackAndReturnTrackAdded) {
    uint16_t room_id = create_and_join_room();

    UploadTrackBeginBody begin{};
    begin.room_id = boost::endian::native_to_big(room_id);
    begin.file_size = boost::endian::native_to_big(static_cast<uint32_t>(6));
    std::strncpy(begin.filename, "demo.mp3", FILENAME_MAX_LEN - 1);

    EXPECT_TRUE(executor.execute(user, PacketType::UploadTrackBegin, to_body(begin)).empty());
    EXPECT_TRUE(executor.execute(user, PacketType::UploadTrackData, {'a', 'b', 'c'}).empty());
    EXPECT_TRUE(executor.execute(user, PacketType::UploadTrackData, {'d', 'e', 'f'}).empty());
    auto result = executor.execute(user, PacketType::UploadTrackEnd, {});

    ASSERT_EQ(header_of(result).type, static_cast<uint8_t>(PacketType::TrackAdded));
    TrackAddedBody added{};
    std::memcpy(&added, result.data() + sizeof(PacketHeader), sizeof(added));
    EXPECT_EQ(boost::endian::big_to_native(added.room_id), room_id);
    EXPECT_STREQ(added.filename, "demo.mp3");
    EXPECT_EQ(tracks.size(), 1u);
}

TEST_F(ServerMusicFlowTest, UploadSizeMismatchFailsAndDoesNotRegisterTrack) {
    uint16_t room_id = create_and_join_room();

    UploadTrackBeginBody begin{};
    begin.room_id = boost::endian::native_to_big(room_id);
    begin.file_size = boost::endian::native_to_big(static_cast<uint32_t>(6));
    std::strncpy(begin.filename, "short.mp3", FILENAME_MAX_LEN - 1);

    EXPECT_TRUE(executor.execute(user, PacketType::UploadTrackBegin, to_body(begin)).empty());
    EXPECT_TRUE(executor.execute(user, PacketType::UploadTrackData, {'a', 'b', 'c'}).empty());
    auto result = executor.execute(user, PacketType::UploadTrackEnd, {});

    ASSERT_EQ(header_of(result).type, static_cast<uint8_t>(PacketType::Error));
    EXPECT_EQ(error_body_of(result).error_code, static_cast<uint8_t>(StatusCode::UploadFailed));
    EXPECT_TRUE(tracks.empty());
}
