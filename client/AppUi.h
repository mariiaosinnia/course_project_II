#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <ftxui/component/screen_interactive.hpp>

#include "LoginScreen.h"
#include "LobbyScreen.h"
#include "RoomScreen.h"

class ClientApp;

// AppUI керує навігацією між трьома екранами.
// Тримає один ScreenInteractive і перемикає активний Component.
//
// Навігація:
//   Login  → [Connected]  → Lobby
//   Lobby  → [RoomJoined] → Room
//   Room   → [RoomLeft]   → Lobby
//
// Всі екрани отримують спільний screen_ — щоб PostEvent з
// мережевого потоку правильно "будив" поточний активний екран.

class AppUI {
public:
    explicit AppUI(ClientApp& app);

    // Запускає FTXUI event loop. Блокує до виходу (q / Ctrl+C).
    // Має викликатись з main thread.
    void run();

    // Доступ до екранів для підписки на мережеві події
    LoginScreen& login_screen() { return *login_; }
    LobbyScreen& lobby_screen() { return *lobby_; }
    RoomScreen&  room_screen()  { return *room_; }

private:
    enum class Screen { Login, Lobby, Room };

    void navigate_to(Screen s);
    void toggle_room_voice();
    std::string room_display_name(uint16_t room_id) const;

    ClientApp& app_;
    ftxui::ScreenInteractive screen_;

    std::unique_ptr<LoginScreen> login_;
    std::unique_ptr<LobbyScreen> lobby_;
    std::unique_ptr<RoomScreen>  room_;

    Screen current_ = Screen::Login;
    std::string username_;
    std::string pending_created_room_name_;
    std::unordered_map<uint16_t, std::string> room_names_;
};
