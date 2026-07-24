#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "RoomScreen.h"

using ftxui::Event;
using ftxui::ScreenInteractive;

namespace {

std::filesystem::path make_temp_mp3(const std::string& name, const std::string& bytes = "abcdef") {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(path, std::ios::binary);
    file << bytes;
    return path;
}

class RoomScreenTest : public ::testing::Test {
protected:
    ScreenInteractive screen = ScreenInteractive::FixedSize(120, 35);
    int leave_count = 0;
    int talk_count = 0;
    std::string uploaded_path;
    uint16_t queued_track = 0;

    RoomScreen make_screen() {
        return RoomScreen(
            screen,
            [&] { leave_count++; },
            [&] { talk_count++; },
            [&](const std::string& path) { uploaded_path = path; },
            [&](uint16_t track_id) { queued_track = track_id; },
            [] {});
    }
};

} // namespace

TEST_F(RoomScreenTest, TypingPrintableTextFocusesUploadInputAndKeepsVisibleState) {
    auto room = make_screen();
    auto component = room.component();

    for (char ch : std::string("C:/music/demo.mp3")) {
        EXPECT_TRUE(component->OnEvent(Event::Character(std::string(1, ch))));
    }

    EXPECT_EQ(room.test_upload_path(), "C:/music/demo.mp3");
}

TEST_F(RoomScreenTest, EnterWithValidUploadPathStartsUploadAndShowsInitialProgress) {
    auto path = make_temp_mp3("room_screen_upload_test.mp3", "1234567890");
    auto room = make_screen();
    auto component = room.component();

    std::string raw = path.string();
    for (char ch : raw) {
        ASSERT_TRUE(component->OnEvent(Event::Character(std::string(1, ch))));
    }
    ASSERT_TRUE(component->OnEvent(Event::Return));

    EXPECT_EQ(uploaded_path, raw);
    EXPECT_TRUE(room.test_upload_path().empty());
    EXPECT_TRUE(room.test_upload_busy());
    EXPECT_TRUE(room.test_has_upload_progress());
    auto progress = room.test_upload_progress();
    EXPECT_EQ(progress.filename, path.filename().string());
    EXPECT_EQ(progress.bytes_sent, 0u);
    EXPECT_EQ(progress.total_bytes, 10u);

    std::filesystem::remove(path);
}

TEST_F(RoomScreenTest, InvalidExtensionDoesNotStartUploadAndKeepsTypedPath) {
    auto path = make_temp_mp3("room_screen_upload_test.txt", "1234");
    auto room = make_screen();
    auto component = room.component();

    std::string raw = path.string();
    for (char ch : raw) {
        ASSERT_TRUE(component->OnEvent(Event::Character(std::string(1, ch))));
    }
    ASSERT_TRUE(component->OnEvent(Event::Return));

    EXPECT_TRUE(uploaded_path.empty());
    EXPECT_EQ(room.test_upload_path(), raw);
    EXPECT_FALSE(room.test_upload_busy());
    EXPECT_FALSE(room.test_has_upload_progress());

    std::filesystem::remove(path);
}

TEST_F(RoomScreenTest, EnterWithEmptyUploadPathAddsSelectedLibraryTrack) {
    auto room = make_screen();
    room.set_tracks({{11, "one.mp3"}, {22, "two.mp3"}});
    auto component = room.component();

    EXPECT_TRUE(component->OnEvent(Event::ArrowDown));
    EXPECT_EQ(room.test_selected_library_track(), 1);
    EXPECT_TRUE(component->OnEvent(Event::Return));

    EXPECT_EQ(queued_track, 22);
    EXPECT_TRUE(uploaded_path.empty());
}

TEST_F(RoomScreenTest, EnterWithUploadPathDoesNotAddSelectedLibraryTrack) {
    auto path = make_temp_mp3("room_screen_upload_priority.mp3", "1234");
    auto room = make_screen();
    room.set_tracks({{11, "one.mp3"}});
    auto component = room.component();

    std::string raw = path.string();
    for (char ch : raw) {
        ASSERT_TRUE(component->OnEvent(Event::Character(std::string(1, ch))));
    }
    ASSERT_TRUE(component->OnEvent(Event::Return));

    EXPECT_EQ(uploaded_path, raw);
    EXPECT_EQ(queued_track, 0);

    std::filesystem::remove(path);
}

TEST_F(RoomScreenTest, MoodButtonAndF4CycleMoodWithoutChangingUploadText) {
    auto room = make_screen();
    auto component = room.component();

    for (char ch : std::string("C:/music/demo.mp3")) {
        ASSERT_TRUE(component->OnEvent(Event::Character(std::string(1, ch))));
    }
    ASSERT_TRUE(component->OnEvent(Event::F4));

    EXPECT_EQ(room.test_mood_index(), 1);
    EXPECT_EQ(room.test_upload_path(), "C:/music/demo.mp3");
}
