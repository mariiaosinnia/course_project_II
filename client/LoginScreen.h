#pragma once

#include <functional>
#include <string>

#include <ftxui/component/screen_interactive.hpp>

// Логін-екран — мінімалістичний стиль:
//
//  ┌────────────────────────────────────┐
//  │  ♪  StudyRoom                      │
//  │     focus together                 │
//  ├────────────────────────────────────┤
//  │                                    │
//  │     what should we call you?       │
//  │                                    │
//  │     ┌──────────────────────────┐   │
//  │     │ daryna_                  │   │
//  │     └──────────────────────────┘   │
//  │                                    │
//  │         ┌──────────────────┐       │
//  │         │     join →       │       │
//  │         └──────────────────┘       │
//  │                                    │
//  │     no account needed ♥            │
//  └────────────────────────────────────┘

class LoginScreen {
public:
    explicit LoginScreen(
        ftxui::ScreenInteractive& screen,
        std::function<void(const std::string& username)> on_connect
    );

    ftxui::Component component();

    // Потокобезпечно оновити статус (викликається з мережевого потоку)
    void set_status(const std::string& status, bool is_error = false);

private:
    ftxui::ScreenInteractive& screen_;
    std::function<void(const std::string&)> on_connect_;

    std::string username_;
    std::string status_;
    bool status_is_error_ = false;

    ftxui::Component component_;
};
