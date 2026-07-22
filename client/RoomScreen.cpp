#include "RoomScreen.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

// Форматує секунди у "MM:SS"
static std::string fmt_time(int secs)
{
    if (secs < 0) return "--:--";
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << secs / 60
        << ":" << std::setw(2) << std::setfill('0') << secs % 60;
    return oss.str();
}

RoomScreen::RoomScreen(
    ScreenInteractive& screen,
    std::function<void()> on_leave,
    std::function<void()> on_mute_toggle,
    std::function<void(const std::string& path)> on_upload,
    std::function<void(uint16_t track_id)> on_track_select,
    std::function<void()> on_tracks_refresh)
    : screen_(screen)
    , on_leave_(std::move(on_leave))
    , on_mute_toggle_(std::move(on_mute_toggle))
    , on_upload_(std::move(on_upload))
    , on_track_select_(std::move(on_track_select))
    , on_tracks_refresh_(std::move(on_tracks_refresh))
{
    build();
}

Component RoomScreen::component() { return component_; }

// ---------- Потокобезпечні сеттери ----------

void RoomScreen::set_room_name(const std::string& name)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    room_name_ = name;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_track_name(const std::string& track)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    track_name_ = track;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_users(std::vector<UserEntry> users)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    users_ = std::move(users);
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::add_user(UserEntry user)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    users_.push_back(std::move(user));
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::remove_user(uint32_t client_id)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    users_.erase(
        std::remove_if(users_.begin(), users_.end(),
            [client_id](const UserEntry& u) { return u.client_id == client_id; }),
        users_.end());
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_user_speaking(uint32_t client_id, bool speaking)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    for (auto& user : users_) {
        if (user.client_id == client_id) {
            user.speaking = speaking;
            break;
        }
    }
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_tracks(std::vector<TrackEntry> tracks)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    tracks_ = std::move(tracks);
    if (selected_track_ >= static_cast<int>(tracks_.size())) {
        selected_track_ = 0;
    }
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_ping(int ms, float loss_percent)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    ping_ms_ = ms;
    loss_ = loss_percent;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_visualizer_data(std::vector<float> bars)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    visualizer_bars_ = std::move(bars);
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_muted(bool muted)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    muted_ = muted;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_pomodoro(int seconds_remaining)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    pomodoro_secs_ = seconds_remaining;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_upload_status(const std::string& status, bool is_error)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    upload_status_ = status;
    upload_status_is_error_ = is_error;
    upload_busy_ = !is_error && !status.empty() &&
        status.find("track added:") != 0;
    screen_.PostEvent(Event::Custom);
}

// ---------- Рендер підсекцій ----------

Element RoomScreen::render_now_playing(const std::string& track_name) const
{
    return vbox({
        text("  now playing") | color(Color::GrayDark),
        hbox({
            text("  ☁  "),
            text(track_name) | color(Color::Cyan),
        }),
        text("  ▶  ──●──────────────") | color(Color::GrayDark),
        text("  ∞") | color(Color::GrayDark),
    });
}

Element RoomScreen::render_users(const std::vector<UserEntry>& users) const
{
    Elements rows;
    rows.push_back(
        text("  online (" + std::to_string(users.size()) + ")")
            | color(Color::GrayDark)
    );
    for (const auto& u : users) {
        rows.push_back(hbox({
            text(u.speaking ? "  ◉ " : "  ● ") | color(u.speaking ? Color::Cyan : Color::Green),
            text(u.name),
        }));
    }
    return vbox(std::move(rows));
}

Element RoomScreen::render_visualizer(const std::vector<float>& bars_data) const
{
    // ASCII waveform: кожен бар = один символ висотою 0-8
    static const char* bars[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

    std::string wave = "  ";
    for (float v : bars_data) {
        int idx = static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 8.0f);
        wave += bars[idx];
    }
    if (bars_data.empty()) {
        wave += "▁▂▃▂▄▃▅▄▃▂▄▅▃▂▁▂▃▄▂▃▄▅▃▂▄";  // заглушка
    }

    return vbox({
        text("  visualizer") | color(Color::GrayDark),
        text(wave) | color(Color::Cyan),
    });
}

Element RoomScreen::render_upload(const std::string& status_text, bool is_error) const
{
    Element status = text("  paste mp3 path and press upload") | color(Color::GrayDark);
    if (!status_text.empty()) {
        status = text("  " + status_text)
            | color(is_error ? Color::Red : Color::GrayDark);
    }

    return vbox({
        text("  upload track") | color(Color::GrayDark),
        status,
    });
}

Element RoomScreen::render_tracks(const std::vector<TrackEntry>& tracks, int selected_track) const
{
    Elements rows;
    rows.push_back(text("  tracks") | color(Color::GrayDark));

    if (tracks.empty()) {
        rows.push_back(text("  no tracks") | color(Color::GrayDark));
        return vbox(std::move(rows));
    }

    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        bool selected = static_cast<int>(i) == selected_track;
        std::string label = "  " + std::string(selected ? ">" : " ") +
            " #" + std::to_string(track.track_id) + " " + track.filename;
        auto row = text(label);
        if (selected) {
            row = row | color(Color::Cyan) | bold;
        }
        rows.push_back(row);
    }

    return vbox(std::move(rows));
}

Element RoomScreen::render_pomodoro(int seconds_remaining) const
{
    if (seconds_remaining < 0) {
        return text("  🍅  —") | color(Color::GrayDark);
    }
    return vbox({
        hbox({
            text("  🍅  pomodoro") | color(Color::GrayDark),
        }),
        text("  " + fmt_time(seconds_remaining)) | bold | color(Color::Cyan),
    });
}

Element RoomScreen::render_footer(int ping_ms, float loss, bool muted) const
{
    std::ostringstream oss;
    oss << "  ping " << ping_ms << "ms  ·  "
        << "loss " << std::fixed << std::setprecision(1) << loss << "%";

    return hbox({
        text(oss.str()) | color(Color::GrayDark),
        filler(),
        text(muted_ ? "  [ 🔇 ]  " : "  [ 🔊 ]  ")
            | color(muted_ ? Color::Red : Color::GrayDark),
    });
}

// ---------- Build ----------

void RoomScreen::build()
{
    auto btn_leave = Button("← leave", [this] {
        if (on_leave_) on_leave_();
    });

    auto btn_mute = Button("mic", [this] {
        if (on_mute_toggle_) on_mute_toggle_();
    });

    auto input_upload = Input(&upload_path_, "path to .mp3");

    auto btn_upload = Button("upload", [this] {
        if (upload_path_.empty()) {
            set_upload_status("choose a file first", true);
            return;
        }
        if (on_upload_) {
            on_upload_(upload_path_);
        }
    });

    auto btn_select_track = Button("play", [this] {
        uint16_t track_id = 0;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (tracks_.empty() || selected_track_ >= static_cast<int>(tracks_.size())) {
                return;
            }
            track_id = tracks_[selected_track_].track_id;
        }
        if (on_track_select_) {
            on_track_select_(track_id);
        }
    });

    auto btn_refresh_tracks = Button("tracks", [this] {
        if (on_tracks_refresh_) on_tracks_refresh_();
    });

    auto container = Container::Vertical({
        Container::Horizontal({btn_leave, btn_mute}),
        Container::Horizontal({input_upload, btn_upload}),
        Container::Horizontal({btn_select_track, btn_refresh_tracks}),
    });

    component_ = Renderer(container, [this, container] {
        // Знімок даних під mutex'ом
        std::string room_name, track_name;
        std::vector<UserEntry> users;
        std::vector<float> viz;
        std::string upload_status;
        std::vector<TrackEntry> tracks;
        int ping_ms, pomo_secs;
        int selected_track;
        float loss;
        bool muted, upload_status_is_error, upload_busy;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            room_name  = room_name_;
            track_name = track_name_;
            users      = users_;
            tracks     = tracks_;
            selected_track = selected_track_;
            viz        = visualizer_bars_;
            upload_status = upload_status_;
            ping_ms    = ping_ms_;
            loss       = loss_;
            muted      = muted_;
            pomo_secs  = pomodoro_secs_;
            upload_status_is_error = upload_status_is_error_;
            upload_busy = upload_busy_;
        }

        // Header
        auto header = hbox({
            container->ChildAt(0)->ChildAt(0)->Render(),
            text("  " + room_name) | bold | color(Color::Cyan),
            filler(),
            text("● live") | color(Color::Green),
            text("  "),
        });

        // Ліва колонка: плеєр + візуалізатор
        auto left_col = vbox({
            render_now_playing(track_name),
            separator() | color(Color::GrayDark),
            render_visualizer(viz),
            separator() | color(Color::GrayDark),
            render_upload(upload_status, upload_status_is_error),
            hbox({
                text("  "),
                container->ChildAt(1)->ChildAt(0)->Render() | size(WIDTH, EQUAL, 36),
                text("  "),
                container->ChildAt(1)->ChildAt(1)->Render()
                    | (upload_busy ? dim : nothing),
            }),
            separator() | color(Color::GrayDark),
            render_tracks(tracks, selected_track),
            hbox({
                text("  "),
                container->ChildAt(2)->ChildAt(0)->Render(),
                text("  "),
                container->ChildAt(2)->ChildAt(1)->Render(),
            }),
        }) | flex;

        // Права колонка: юзери + помодоро
        auto right_col = vbox({
            render_users(users),
            filler(),
            separator() | color(Color::GrayDark),
            render_pomodoro(pomo_secs),
        }) | size(WIDTH, EQUAL, 20);

        return vbox({
            header,
            separator(),
            hbox({
                left_col,
                separator() | color(Color::GrayDark),
                right_col,
            }) | flex,
            separator(),
            render_footer(ping_ms, loss, muted),
        }) | border;
    });

    // 'm' — mute/unmute, 'q' — leave. When the upload input is focused,
    // printable characters must go to the input instead of global shortcuts.
    component_ = CatchEvent(component_, [this, input_upload](Event event) {
        if (input_upload->Focused() && event.is_character()) {
            return false;
        }
        if (event == Event::ArrowDown) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (!tracks_.empty()) {
                selected_track_ = std::min(selected_track_ + 1, static_cast<int>(tracks_.size()) - 1);
                screen_.PostEvent(Event::Custom);
                return true;
            }
        }
        if (event == Event::ArrowUp) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (!tracks_.empty()) {
                selected_track_ = std::max(selected_track_ - 1, 0);
                screen_.PostEvent(Event::Custom);
                return true;
            }
        }
        if (event == Event::Return) {
            uint16_t track_id = 0;
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                if (!tracks_.empty() && selected_track_ < static_cast<int>(tracks_.size())) {
                    track_id = tracks_[selected_track_].track_id;
                }
            }
            if (track_id != 0 && on_track_select_) {
                on_track_select_(track_id);
            }
            if (track_id != 0) {
                return true;
            }
        }
        if (event == Event::Character('r')) {
            if (on_tracks_refresh_) on_tracks_refresh_();
            return true;
        }
        if (event == Event::Character('m')) {
            if (on_mute_toggle_) on_mute_toggle_();
            return true;
        }
        if (event == Event::Character('q')) {
            if (on_leave_) on_leave_();
            return true;
        }
        return false;
    });
}
