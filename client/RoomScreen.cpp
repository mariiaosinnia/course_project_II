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
    std::function<void()> on_mute_toggle)
    : screen_(screen)
    , on_leave_(std::move(on_leave))
    , on_mute_toggle_(std::move(on_mute_toggle))
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

// ---------- Рендер підсекцій ----------

Element RoomScreen::render_now_playing() const
{
    return vbox({
        text("  now playing") | color(Color::GrayDark),
        hbox({
            text("  ☁  "),
            text(track_name_) | color(Color::Cyan),
        }),
        text("  ▶  ──●──────────────") | color(Color::GrayDark),
        text("  ∞") | color(Color::GrayDark),
    });
}

Element RoomScreen::render_users() const
{
    Elements rows;
    rows.push_back(
        text("  online (" + std::to_string(users_.size()) + ")")
            | color(Color::GrayDark)
    );
    for (const auto& u : users_) {
        rows.push_back(hbox({
            text("  ● ") | color(Color::Green),
            text(u.name),
        }));
    }
    return vbox(std::move(rows));
}

Element RoomScreen::render_visualizer() const
{
    // ASCII waveform: кожен бар = один символ висотою 0-8
    static const char* bars[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

    std::string wave = "  ";
    for (float v : visualizer_bars_) {
        int idx = static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 8.0f);
        wave += bars[idx];
    }
    if (visualizer_bars_.empty()) {
        wave += "▁▂▃▂▄▃▅▄▃▂▄▅▃▂▁▂▃▄▂▃▄▅▃▂▄";  // заглушка
    }

    return vbox({
        text("  visualizer") | color(Color::GrayDark),
        text(wave) | color(Color::Cyan),
    });
}

Element RoomScreen::render_pomodoro() const
{
    if (pomodoro_secs_ < 0) {
        return text("  🍅  —") | color(Color::GrayDark);
    }
    return vbox({
        hbox({
            text("  🍅  pomodoro") | color(Color::GrayDark),
        }),
        text("  " + fmt_time(pomodoro_secs_)) | bold | color(Color::Cyan),
    });
}

Element RoomScreen::render_footer() const
{
    std::ostringstream oss;
    oss << "  ping " << ping_ms_ << "ms  ·  "
        << "loss " << std::fixed << std::setprecision(1) << loss_ << "%";

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

    auto btn_mute = Button("", [this] {
        if (on_mute_toggle_) on_mute_toggle_();
    });

    auto container = Container::Horizontal({btn_leave, btn_mute});

    component_ = Renderer(container, [this, container] {
        // Знімок даних під mutex'ом
        std::string room_name, track_name;
        std::vector<UserEntry> users;
        std::vector<float> viz;
        int ping_ms, pomo_secs;
        float loss;
        bool muted;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            room_name  = room_name_;
            track_name = track_name_;
            users      = users_;
            viz        = visualizer_bars_;
            ping_ms    = ping_ms_;
            loss       = loss_;
            muted      = muted_;
            pomo_secs  = pomodoro_secs_;
        }

        // Header
        auto header = hbox({
            container->ChildAt(0)->Render(),
            text("  " + room_name) | bold | color(Color::Cyan),
            filler(),
            text("● live") | color(Color::Green),
            text("  "),
        });

        // Ліва колонка: плеєр + візуалізатор
        auto left_col = vbox({
            render_now_playing(),
            separator() | color(Color::GrayDark),
            render_visualizer(),
        }) | flex;

        // Права колонка: юзери + помодоро
        auto right_col = vbox({
            render_users(),
            filler(),
            separator() | color(Color::GrayDark),
            render_pomodoro(),
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
            render_footer(),
        }) | border;
    });

    // 'm' — mute/unmute, 'q' — leave
    component_ = CatchEvent(component_, [this](Event event) {
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
