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
    bool speaking = false;
};

struct TrackEntry {
    uint16_t track_id;
    std::string filename;
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
        std::function<void()> on_mute_toggle,
        std::function<void(const std::string& path)> on_upload,
        std::function<void(uint16_t track_id)> on_track_select,
        std::function<void()> on_tracks_refresh
    );

    ftxui::Component component();

    // Потокобезпечно оновити стан з мережевого потоку
    void set_room_name(const std::string& name);
    void set_track_name(const std::string& track);
    void set_users(std::vector<UserEntry> users);
    void add_user(UserEntry user);
    void remove_user(uint32_t client_id);
    void set_user_speaking(uint32_t client_id, bool speaking);
    void set_tracks(std::vector<TrackEntry> tracks);
    void set_ping(int ms, float loss_percent);
    void set_visualizer_data(std::vector<float> bars);  // 0.0–1.0 per bar
    void set_muted(bool muted);
    void set_pomodoro(int seconds_remaining);  // -1 = вимкнено
    void set_upload_status(const std::string& status, bool is_error = false);

private:
    ftxui::ScreenInteractive& screen_;
    std::function<void()> on_leave_;
    std::function<void()> on_mute_toggle_;
    std::function<void(const std::string&)> on_upload_;
    std::function<void(uint16_t)> on_track_select_;
    std::function<void()> on_tracks_refresh_;

    mutable std::mutex data_mutex_;

    std::string room_name_ = "room";
    std::string track_name_ = "—";
    std::vector<UserEntry> users_;
    std::vector<TrackEntry> tracks_;
    int selected_track_ = 0;
    int ping_ms_ = 0;
    float loss_ = 0.0f;
    std::vector<float> visualizer_bars_;
    bool muted_ = true;
    int pomodoro_secs_ = -1;
    std::string upload_path_;
    std::string upload_status_;
    bool upload_status_is_error_ = false;
    bool upload_busy_ = false;

    ftxui::Component component_;
    void build();

    // Рендер підсекцій (щоб Renderer не розростався)
    ftxui::Element render_now_playing(const std::string& track_name) const;
    ftxui::Element render_users(const std::vector<UserEntry>& users) const;
    ftxui::Element render_visualizer(const std::vector<float>& bars) const;
    ftxui::Element render_upload(const std::string& status, bool is_error) const;
    ftxui::Element render_tracks(const std::vector<TrackEntry>& tracks, int selected_track) const;
    ftxui::Element render_pomodoro(int seconds_remaining) const;
    ftxui::Element render_footer(int ping_ms, float loss, bool muted) const;
};
