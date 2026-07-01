#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>

struct RoomEntry {
    uint16_t room_id;
    std::string name;
    uint8_t user_count;
    std::string track_name;  // ambient track назва (якщо є)
};

// Lobby-екран — список кімнат в стилі:
//
//  ┌─────────────────────────────────────────────┐
//  │  ♪  StudyRoom          alex_study  ▾        │
//  ├──────────────────┬──────────────────────────┤
//  │  rooms           │  [ + new ]               │
//  ├──────────────────┴──────────────────────────┤
//  │                                             │
//  │  ☁  rainy afternoon                         │
//  │     rain · 3 online · 00:14:22              │
//  │                                             │
//  │  ☕  café corner                             │
//  │     café · 1 online                         │
//  │                                             │
//  └─────────────────────────────────────────────┘

class LobbyScreen {
public:
    explicit LobbyScreen(
        ftxui::ScreenInteractive& screen,
        const std::string& username,
        std::function<void(const std::string& name)> on_create,
        std::function<void(uint16_t room_id)> on_join,
        std::function<void()> on_refresh
    );

    ftxui::Component component();

    // Потокобезпечно оновити список кімнат
    void set_rooms(std::vector<RoomEntry> rooms);
    void set_status(const std::string& msg, bool is_error = false);

private:
    ftxui::ScreenInteractive& screen_;
    std::string username_;

    std::function<void(const std::string&)> on_create_;
    std::function<void(uint16_t)> on_join_;
    std::function<void()> on_refresh_;

    std::string new_room_name_;
    std::string status_;
    bool status_is_error_ = false;

    mutable std::mutex rooms_mutex_;
    std::vector<RoomEntry> rooms_;
    int selected_ = 0;

    ftxui::Component component_;
    void build();
};
