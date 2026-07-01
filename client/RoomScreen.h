#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>

struct UserEntry {
    uint32_t client_id;
    std::string name;
};

// Room view — екран кімнати в стилі:
//
//  ┌──────────────────────────────────────────┐
//  │  ←  rainy afternoon          ● live      │
//  ├──────────────────────┬───────────────────┤
//  │  now playing         │  online (3)       │
//  │  ☁  rain             │  ● alex           │
//  │  ▶  ──●──────────    │  ● marta          │
//  │  02:14 / ∞           │  ● oleg           │
//  ├──────────────────────┤                   │
//  │  volume              │                   │
//  │  ▌▌▌▌▌▌░░░░          ├───────────────────┤
//  ├──────────────────────┤  🍅  pomodoro     │
//  │  visualizer          │  20:14            │
//  │  ▄▃▅▂▆▃▄▅▃▂▄▄▅▆▂▄   │  [ pause ]        │
//  ├──────────────────────┴───────────────────┤
//  │  ping 24ms  ·  loss 0.2%        [ 🔇 ]  │
//  └──────────────────────────────────────────┘

class RoomScreen {
public:
    explicit RoomScreen(
        ftxui::ScreenInteractive& screen,
        std::function<void()> on_leave,
        std::function<void()> on_mute_toggle
    );

    ftxui::Component component();

    // Потокобезпечно оновити стан з мережевого потоку
    void set_room_name(const std::string& name);
    void set_track_name(const std::string& track);
    void set_users(std::vector<UserEntry> users);
    void add_user(UserEntry user);
    void remove_user(uint32_t client_id);
    void set_ping(int ms, float loss_percent);
    void set_visualizer_data(std::vector<float> bars);  // 0.0–1.0 per bar
    void set_muted(bool muted);
    void set_pomodoro(int seconds_remaining);  // -1 = вимкнено

private:
    ftxui::ScreenInteractive& screen_;
    std::function<void()> on_leave_;
    std::function<void()> on_mute_toggle_;

    mutable std::mutex data_mutex_;

    std::string room_name_ = "room";
    std::string track_name_ = "—";
    std::vector<UserEntry> users_;
    int ping_ms_ = 0;
    float loss_ = 0.0f;
    std::vector<float> visualizer_bars_;
    bool muted_ = false;
    int pomodoro_secs_ = -1;

    ftxui::Component component_;
    void build();

    // Рендер підсекцій (щоб Renderer не розростався)
    ftxui::Element render_now_playing() const;
    ftxui::Element render_users() const;
    ftxui::Element render_visualizer() const;
    ftxui::Element render_pomodoro() const;
    ftxui::Element render_footer() const;
};
