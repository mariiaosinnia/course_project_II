#pragma once

#include <functional>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
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

struct QueueEntry {
    uint16_t track_id;
    std::string filename;
    bool is_current = false;
};

struct UploadProgressView {
    std::string filename;
    uintmax_t bytes_sent = 0;
    uintmax_t total_bytes = 0;
    bool waiting_for_server = false;
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
//  │  visualizer          │                   │
//  │  ▄▃▅▂▆▃▄▅▃▂▄▄▅▆▂▄   │                   │
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
    ~RoomScreen();

    ftxui::Component component();

    // Потокобезпечно оновити стан з мережевого потоку
    void set_room_name(const std::string& name);
    void set_track_name(const std::string& track);
    void set_users(std::vector<UserEntry> users);
    void add_user(UserEntry user);
    void remove_user(uint32_t client_id);
    void set_user_speaking(uint32_t client_id, bool speaking);
    void set_tracks(std::vector<TrackEntry> tracks);
    void reset_library();
    void set_queue(std::vector<QueueEntry> tracks);
    void reset_queue();
    void set_ping(int ms, float loss_percent);
    void set_visualizer_data(std::vector<float> bars);  // 0.0–1.0 per bar
    void set_muted(bool muted);
    void set_upload_status(const std::string& status, bool is_error = false);
    void set_upload_progress(const UploadProgressView& progress);
    void set_track_added_confirmation(uint16_t track_id, const std::string& filename);

#ifdef BUILD_TESTING
    std::string test_upload_path() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return upload_path_;
    }
    bool test_upload_busy() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return upload_busy_;
    }
    bool test_has_upload_progress() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return has_upload_progress_;
    }
    UploadProgressView test_upload_progress() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return upload_progress_;
    }
    int test_selected_library_track() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return selected_library_track_;
    }
    int test_mood_index() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return mood_index_;
    }
#endif

private:
    ftxui::ScreenInteractive& screen_;
    std::function<void()> on_leave_;
    std::function<void()> on_mute_toggle_;
    std::function<void(const std::string&)> on_upload_;
    std::function<void(uint16_t)> on_library_track_add_;
    std::function<void()> on_library_refresh_;

    mutable std::mutex data_mutex_;

    std::string room_name_ = "room";
    std::string track_name_ = "—";
    std::vector<UserEntry> users_;
    std::vector<TrackEntry> library_tracks_;
    std::vector<QueueEntry> queue_tracks_;
    int selected_library_track_ = 0;
    bool library_loading_ = true;
    bool queue_loading_ = true;
    int ping_ms_ = 0;
    float loss_ = 0.0f;
    std::vector<float> visualizer_bars_;
    bool muted_ = true;
    std::string upload_path_;
    std::string upload_status_;
    bool upload_status_is_error_ = false;
    bool upload_busy_ = false;
    bool upload_success_ = false;
    bool has_upload_progress_ = false;
    UploadProgressView upload_progress_;
    uint16_t added_track_id_ = 0;
    int added_queue_position_ = 0;
    std::string added_track_name_;
    int mood_index_ = 0;
    uint64_t mood_tick_ = 0;
    std::atomic<bool> mood_timer_running_{false};
    std::thread mood_timer_thread_;

    ftxui::Component component_;
    void build();

    // Рендер підсекцій (щоб Renderer не розростався)
    ftxui::Element render_now_playing(const std::string& track_name) const;
    ftxui::Element render_users(const std::vector<UserEntry>& users) const;
    ftxui::Element render_visualizer(const std::vector<float>& bars) const;
    ftxui::Element render_mood(int mood_index, uint64_t tick) const;
    ftxui::Element render_upload(const std::string& path, const std::string& status, bool is_error,
                                 bool busy, bool success, int queue_position,
                                 const std::string& added_track_name,
                                 bool has_progress,
                                 const UploadProgressView& progress) const;
    ftxui::Element render_library(const std::vector<TrackEntry>& tracks, int selected_track,
                                  bool loading,
                                  uint16_t added_track_id) const;
    ftxui::Element render_queue(const std::vector<QueueEntry>& tracks, bool loading) const;
    ftxui::Element render_footer(int ping_ms, float loss, bool muted) const;
};
