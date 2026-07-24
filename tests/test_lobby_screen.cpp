#include <gtest/gtest.h>

#include "LobbyScreen.h"

using ftxui::Event;
using ftxui::ScreenInteractive;

TEST(LobbyScreenTest, ArrowKeysSelectRoomAndEnterJoinsSelected) {
    auto screen = ScreenInteractive::FixedSize(100, 30);
    uint16_t joined_room = 0;
    LobbyScreen lobby(
        screen,
        "alice",
        [](const std::string&) {},
        [&](uint16_t room_id) { joined_room = room_id; },
        [] {});

    lobby.set_rooms({
        {1, "Alpha", 1, ""},
        {2, "Beta", 2, ""},
        {3, "Gamma", 1, ""},
    });

    auto component = lobby.component();
    EXPECT_TRUE(component->OnEvent(Event::ArrowDown));
    EXPECT_EQ(lobby.test_selected_index(), 1);
    EXPECT_TRUE(component->OnEvent(Event::ArrowDown));
    EXPECT_EQ(lobby.test_selected_index(), 2);
    EXPECT_TRUE(component->OnEvent(Event::ArrowDown));
    EXPECT_EQ(lobby.test_selected_index(), 2);

    EXPECT_TRUE(component->OnEvent(Event::Return));
    EXPECT_EQ(joined_room, 3);
}

TEST(LobbyScreenTest, EnterInNewRoomInputCreatesInsteadOfJoining) {
    auto screen = ScreenInteractive::FixedSize(100, 30);
    std::string created_room;
    uint16_t joined_room = 0;
    LobbyScreen lobby(
        screen,
        "alice",
        [&](const std::string& name) { created_room = name; },
        [&](uint16_t room_id) { joined_room = room_id; },
        [] {});

    lobby.set_rooms({{7, "Existing", 1, ""}});
    auto component = lobby.component();
    for (char ch : std::string("NewRoom")) {
        EXPECT_TRUE(component->OnEvent(Event::Character(std::string(1, ch))));
    }

    EXPECT_TRUE(component->OnEvent(Event::Return));
    EXPECT_EQ(created_room, "NewRoom");
    EXPECT_EQ(joined_room, 0);
    EXPECT_TRUE(lobby.test_new_room_name().empty());
}
