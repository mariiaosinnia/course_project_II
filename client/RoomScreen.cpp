#include "RoomScreen.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace {
Color accent() { return Color::MediumPurple1; }
Color accent_soft() { return Color::Violet; }
Color soft_text() { return Color::GrayDark; }
Color section_text() { return Color::White; }
Color success_color() { return Color(0x63, 0xA8, 0x6B); }
Color error_color() { return Color(0xC5, 0x8B, 0x3A); }

const char* mood_name(int mood_index)
{
    static const char* names[] = {"sunny", "rainy", "night"};
    return names[mood_index % 3];
}

std::string trim_copy(std::string value)
{
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [&](char ch) { return !is_space(static_cast<unsigned char>(ch)); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [&](char ch) { return !is_space(static_cast<unsigned char>(ch)); }).base(), value.end());
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::string format_bytes(uintmax_t bytes)
{
    std::ostringstream oss;
    if (bytes >= 1024 * 1024) {
        oss << std::fixed << std::setprecision(1)
            << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MB";
    } else if (bytes >= 1024) {
        oss << std::fixed << std::setprecision(1)
            << static_cast<double>(bytes) / 1024.0 << " KB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

std::string file_name_from_path(const std::string& raw_path)
{
    std::filesystem::path path(trim_copy(raw_path));
    std::string filename = path.filename().string();
    return filename.empty() ? raw_path : filename;
}

std::string compact_path_for_display(const std::string& path)
{
    constexpr size_t max_len = 64;
    if (path.size() <= max_len) {
        return path;
    }
    return "..." + path.substr(path.size() - (max_len - 3));
}

int progress_from_status(const std::string& status)
{
    size_t percent = status.find('%');
    if (percent == std::string::npos) {
        if (status.find("upload sent") != std::string::npos) return 100;
        return 0;
    }
    size_t start = percent;
    while (start > 0 && std::isdigit(static_cast<unsigned char>(status[start - 1]))) {
        --start;
    }
    if (start == percent) return 0;
    return std::clamp(std::stoi(status.substr(start, percent - start)), 0, 100);
}

Element progress_bar(int percent)
{
    constexpr int width = 34;
    int filled = std::clamp((percent * width) / 100, 0, width);

    std::string bar;
    bar.reserve(width * 3); // Оптимізація пам'яті для UTF-8 символів
    for (int i = 0; i < filled; ++i) {
        bar += "━";
    }
    for (int i = 0; i < width - filled; ++i) {
        bar += "░";
    }

    return hbox({
        text("  uploading  ") | color(soft_text()),
        text(bar) | color(accent()),
        text("  " + std::to_string(percent) + "%") | color(soft_text()),
    });
}

Element centered_ascii_frame(const std::string& title, const std::vector<std::string>& lines)
{
    Elements rows;
    rows.push_back(text("  MOOD SCENE · " + title) | color(section_text()) | bold);
    rows.push_back(text(""));
    for (const auto& line : lines) {
        rows.push_back(hbox({
            filler(),
            text(line) | color(Color::White),
            filler(),
        }));
    }
    rows.push_back(filler());
    return vbox(std::move(rows)) | size(HEIGHT, GREATER_THAN, 8);
}

Element transfer_progress_bar(uintmax_t bytes_sent, uintmax_t total_bytes)
{
    int percent = total_bytes == 0
        ? 0
        : static_cast<int>(std::min<uintmax_t>(100, (bytes_sent * 100) / total_bytes));
    return progress_bar(percent);
}

std::string two_digit(size_t value)
{
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << value;
    return oss.str();
}
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
    , on_library_track_add_(std::move(on_track_select))
    , on_library_refresh_(std::move(on_tracks_refresh))
{
    mood_timer_running_ = true;
    mood_timer_thread_ = std::thread([this] {
        while (mood_timer_running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(450));
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                mood_tick_++;
            }
            screen_.PostEvent(Event::Custom);
        }
    });
    build();
}

RoomScreen::~RoomScreen()
{
    mood_timer_running_ = false;
    if (mood_timer_thread_.joinable()) {
        mood_timer_thread_.join();
    }
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
    library_tracks_ = std::move(tracks);
    library_loading_ = false;
    if (selected_library_track_ >= static_cast<int>(library_tracks_.size())) {
        selected_library_track_ = library_tracks_.empty() ? 0 : static_cast<int>(library_tracks_.size()) - 1;
    }
    if (added_track_id_ != 0) {
        added_queue_position_ = 0;
        for (size_t i = 0; i < library_tracks_.size(); ++i) {
            if (library_tracks_[i].track_id == added_track_id_) {
                added_queue_position_ = static_cast<int>(i) + 1;
                break;
            }
        }
    }
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::reset_library()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    library_tracks_.clear();
    selected_library_track_ = 0;
    library_loading_ = true;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_queue(std::vector<QueueEntry> tracks)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    queue_tracks_ = std::move(tracks);
    queue_loading_ = false;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::reset_queue()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    queue_tracks_.clear();
    queue_loading_ = true;
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
    if (bars.empty()) {
        visualizer_bars_.clear();
        screen_.PostEvent(Event::Custom);
        return;
    }

    if (visualizer_bars_.size() != bars.size()) {
        visualizer_bars_.assign(bars.size(), 0.0f);
    }

    bool has_signal = std::any_of(bars.begin(), bars.end(),
        [](float value) { return value > 0.01f; });
    float incoming_weight = has_signal ? 0.45f : 0.0f;
    float decay = has_signal ? 0.55f : 0.78f;

    for (size_t i = 0; i < bars.size(); ++i) {
        float target = std::clamp(bars[i], 0.0f, 1.0f);
        visualizer_bars_[i] = std::clamp(
            visualizer_bars_[i] * decay + target * incoming_weight,
            0.0f,
            1.0f);
    }
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_muted(bool muted)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    muted_ = muted;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_upload_status(const std::string& status, bool is_error)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    upload_status_ = status;
    upload_status_is_error_ = is_error;
    if (!is_error && (status.rfind("uploading ", 0) == 0 ||
                      status.find("upload sent") != std::string::npos)) {
        upload_busy_ = true;
    }
    if (is_error) {
        upload_success_ = false;
        upload_busy_ = false;
    }
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_upload_progress(const UploadProgressView& progress)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    upload_progress_ = progress;
    has_upload_progress_ = true;
    upload_busy_ = true;
    upload_success_ = false;
    upload_status_is_error_ = false;
    upload_status_ = progress.waiting_for_server
        ? "upload sent, waiting for server..."
        : "uploading " + progress.filename;
    screen_.PostEvent(Event::Custom);
}

void RoomScreen::set_track_added_confirmation(uint16_t track_id, const std::string& filename)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    added_track_id_ = track_id;
    added_track_name_ = filename.empty() ? "track #" + std::to_string(track_id) : filename;
    upload_path_.clear();
    upload_success_ = true;
    upload_busy_ = false;
    upload_status_is_error_ = false;
    has_upload_progress_ = false;
    upload_status_ = added_track_name_ + " was added to the shared queue.";
    added_queue_position_ = 0;
    for (size_t i = 0; i < library_tracks_.size(); ++i) {
        if (library_tracks_[i].track_id == track_id) {
            added_queue_position_ = static_cast<int>(i) + 1;
            break;
        }
    }
    screen_.PostEvent(Event::Custom);
}

// ---------- Рендер підсекцій ----------

Element RoomScreen::render_now_playing(const std::string& track_name) const
{
    return vbox({
        text("  NOW PLAYING") | color(section_text()) | bold,
        hbox({
            text("  ◆  ") | color(accent_soft()),
            text(track_name) | color(section_text()) | bold,
        }),
        text("  synchronized room playback") | color(soft_text()),
    });
}

Element RoomScreen::render_users(const std::vector<UserEntry>& users) const
{
    Elements rows;
    rows.push_back(
        text("  ONLINE")
            | color(section_text()) | bold
    );
    for (const auto& u : users) {
        rows.push_back(hbox({
            text(u.speaking ? "  ◉ " : "  ● ") | color(u.speaking ? accent() : Color::Green),
            text(u.name),
        }));
    }
    return vbox(std::move(rows));
}

Element RoomScreen::render_visualizer(const std::vector<float>& bars_data) const
{
    static const char* bars[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

    std::string wave = "  ";
    const std::vector<float> silent(32, 0.0f);
    const auto& source = bars_data.empty() ? silent : bars_data;
    for (float v : source) {
        int idx = static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 8.0f);
        wave += bars[idx];
    }

    return vbox({
        text(wave) | color(accent_soft()),
    });
}

Element RoomScreen::render_mood(int mood_index, uint64_t tick) const
{
    const int phase = static_cast<int>(tick % 2);
    switch (mood_index % 3) {
        case 0: {
            static const std::vector<std::string> frames[] = {
                {
                    "        \\ | /        ",
                    "      --  ☼  --      ",
                    "        / | \\        ",
                },
                {
                    "         \\|/         ",
                    "      --  ☼  --      ",
                    "         /|\\         ",
                },
            };
            return centered_ascii_frame("SUNNY", frames[phase]);
        }
        case 1: {
            static const std::vector<std::string> frames[] = {
                {
                    "          ☁  ☁         ",
                    "        |   .   |      ",
                    "      .   |   .        ",
                    "        |   .   |      ",
                },
                {
                    "          ☁  ☁         ",
                    "      .   |   .        ",
                    "        |   .   |      ",
                    "      |   .   |        ",
                },
            };
            return centered_ascii_frame("RAIN", frames[phase]);
        }
        default: {
            static const std::vector<std::string> frames[] = {
                {
                    "   ·    ✦      ·    ✧ ",
                    "      ✦      ·        ",
                    "          ☾           ",
                    "   ✧        ·     ✦   ",
                    "      ·        ✧    · ",
                },
                {
                    "   ✧      ·     ✦    ·",
                    "      ·       ✧       ",
                    "          ☾           ",
                    "   ·      ✦       ·   ",
                    "      ✧        ·    ✦ ",
                },
            };
            return centered_ascii_frame("NIGHT", frames[phase]);
        }
    }
}

Element RoomScreen::render_upload(const std::string& path, const std::string& status_text, bool is_error,
                                  bool busy, bool success, int queue_position,
                                  const std::string& added_track_name,
                                  bool has_progress,
                                  const UploadProgressView& progress) const
{
    std::string clean_path = trim_copy(path);
    bool has_path = !clean_path.empty();
    std::string title = has_path ? file_name_from_path(clean_path) : "Choose an MP3 track";
    std::string file_size = "size unavailable";
    bool file_ok = false;

    if (has_path) {
        std::error_code ec;
        std::filesystem::path fs_path(clean_path);
        if (std::filesystem::is_regular_file(fs_path, ec)) {
            uintmax_t bytes = std::filesystem::file_size(fs_path, ec);
            if (!ec) {
                file_ok = true;
                file_size = format_bytes(bytes);
            }
        }
    }
    if (busy && has_progress && !progress.filename.empty()) {
        title = progress.filename;
        if (progress.total_bytes > 0) {
            file_ok = true;
            file_size = format_bytes(progress.total_bytes);
        }
    }

    Elements rows;
    if (busy) {
        rows.push_back(text("  ADDING MUSIC TO THE ROOM") | bold | color(section_text()));
    } else if (success) {
        rows.push_back(text("  ✓ MUSIC ADDED") | bold | color(success_color()));
    } else {
        rows.push_back(text("  ADD MUSIC TO THE ROOM") | bold | color(section_text()));
    }
    if (!busy) {
        rows.push_back(text("  Choose an MP3 track to share with everyone in the room.") | color(soft_text()));
    }

    if (!has_path && !success && !busy) {
        rows.push_back(hbox({
            text("  ♪  ") | color(accent_soft()),
            text("Choose a track to make it part of the shared listening space.") | color(soft_text()),
        }));
    } else if (!busy) {
        rows.push_back(vbox({
            hbox({
                text("  ♪  ") | color(accent_soft()),
                text(success ? added_track_name : title) | bold | color(success ? success_color() : section_text()),
            }),
            text("     artist unavailable  ·  duration unavailable  ·  " + file_size)
                | color(soft_text()),
        }) | border | color(soft_text()));
    }

    if (busy) {
        if (has_progress && progress.total_bytes > 0) {
            uintmax_t sent = std::min(progress.bytes_sent, progress.total_bytes);
            rows.push_back(text("  " + progress.filename) | bold | color(section_text()));
            rows.push_back(transfer_progress_bar(sent, progress.total_bytes));
            rows.push_back(text("  " + format_bytes(sent) + " / " +
                format_bytes(progress.total_bytes) +
                (progress.waiting_for_server ? "  waiting for server" : ""))
                | color(soft_text()));
        } else {
            int percent = progress_from_status(status_text);
            rows.push_back(progress_bar(percent));
            rows.push_back(text("  Sharing the track with everyone in the room...") | color(soft_text()));
        }
    } else if (success) {
        std::string position = queue_position > 0
            ? "  Queue position: " + std::to_string(queue_position)
            : "  Queue position will appear after the shared queue refreshes.";
        rows.push_back(text("  " + added_track_name + " was added to the shared queue.")
            | color(success_color()));
        rows.push_back(text(position) | color(soft_text()));
        rows.push_back(text("  The path field is ready for another MP3.") | color(soft_text()));
    } else if (is_error && !status_text.empty()) {
        rows.push_back(text("  " + status_text) | color(error_color()));
    } else if (has_path && !file_ok) {
        rows.push_back(text("  The path will be validated before upload.") | color(soft_text()));
    }

    return vbox(std::move(rows));
}





Element RoomScreen::render_library(const std::vector<TrackEntry>& tracks, int selected_track,
                                   bool loading,
                                   uint16_t added_track_id) const
{
    Elements rows;
    rows.push_back(text("  SHARED LIBRARY") | color(section_text()) | bold);

    if (loading) {
        rows.push_back(text("  Loading server tracks...") | color(soft_text()));
        return vbox(std::move(rows));
    }

    if (tracks.empty()) {
        rows.push_back(text("  No tracks on the server yet.") | color(soft_text()));
        return vbox(std::move(rows));
    }

    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        bool selected = static_cast<int>(i) == selected_track;
        auto row = text("  " + std::string(selected ? ">" : " ") +
            " " + two_digit(i + 1) + "  " + track.filename);
        if (selected) {
            row = row | color(accent()) | bold | focus;
        } else if (track.track_id == added_track_id) {
            row = row | color(accent_soft()) | bold;
        }
        rows.push_back(row);
    }
    rows.push_back(text("  Enter adds selected to queue") | color(soft_text()));

    return vbox(std::move(rows))
        | vscroll_indicator
        | frame
        | size(HEIGHT, LESS_THAN, 10);
}

Element RoomScreen::render_queue(const std::vector<QueueEntry>& tracks, bool loading) const
{
    Elements rows;
    rows.push_back(text("  SHARED QUEUE") | color(section_text()) | bold);

    if (loading) {
        rows.push_back(text("  Loading shared queue...") | color(soft_text()));
        return vbox(std::move(rows));
    }

    if (tracks.empty()) {
        rows.push_back(text("  The queue is empty.") | color(soft_text()));
        return vbox(std::move(rows));
    }

    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        std::string marker = track.is_current ? "▶" : " ";
        std::string state = track.is_current ? "playing" : (i == 1 ? "up next" : "queued");
        auto row = text("  " + marker + " " + two_digit(i + 1) + "  " +
            track.filename + "  " + state);
        if (track.is_current) {
            row = row | color(accent()) | bold;
        } else {
            row = row | color(soft_text());
        }
        rows.push_back(row);
    }

    rows.push_back(text("  " + std::to_string(tracks.size()) + " tracks")
        | color(soft_text()));
    return vbox(std::move(rows))
        | vscroll_indicator
        | frame
        | size(HEIGHT, LESS_THAN, 8);
}

Element RoomScreen::render_footer(int ping_ms, float loss, bool muted) const
{
    std::ostringstream oss;
    oss << "  ping " << ping_ms << "ms  ·  "
        << "loss " << std::fixed << std::setprecision(1) << loss << "%";

    return hbox({
        text(oss.str()) | color(soft_text()),
        text("  ·  ") | color(soft_text()),
        text(muted ? "  🎙 off  " : "  🎙 on  ")
            | color(muted ? accent_soft() : accent()),
    });
}

// ---------- Build ----------

void RoomScreen::build()
{
    auto btn_leave = Button("← leave", [this] {
        if (on_leave_) on_leave_();
    });

    auto btn_mute = Button("🎙 talk", [this] {
        if (on_mute_toggle_) on_mute_toggle_();
    });

    auto btn_mood = Button("mood ▾", [this] {
        std::lock_guard<std::mutex> lock(data_mutex_);
        mood_index_ = (mood_index_ + 1) % 3;
        mood_tick_++;
        screen_.PostEvent(Event::Custom);
    });

    auto add_selected_to_queue = [this] {
        uint16_t track_id = 0;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (library_tracks_.empty() ||
                selected_library_track_ >= static_cast<int>(library_tracks_.size())) {
                return;
            }
            track_id = library_tracks_[selected_library_track_].track_id;
        }
        if (track_id != 0 && on_library_track_add_) {
            on_library_track_add_(track_id);
        }
    };

    InputOption upload_input_option;
    upload_input_option.content = &upload_path_;
    upload_input_option.placeholder = "Paste or type the full path here";
    upload_input_option.multiline = false;
    upload_input_option.transform = [](InputState state) {
        return state.element
            | color(state.is_placeholder ? soft_text() : Color::White);
    };
    auto input_upload = Input(upload_input_option);

    auto submit_upload = [this] {
        std::string normalized = trim_copy(upload_path_);
        if (normalized.empty()) {
            set_upload_status("choose a file first", true);
            return;
        }
        std::filesystem::path path(normalized);
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (extension != ".mp3") {
            set_upload_status("unsupported format: choose an MP3 file", true);
            return;
        }

        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            set_upload_status(ec ? "cannot access file: " + ec.message() : "path is not a file", true);
            return;
        }
        uintmax_t total_bytes = std::filesystem::file_size(path, ec);
        if (ec) {
            set_upload_status("cannot read file size: " + ec.message(), true);
            return;
        }
        if (total_bytes == 0) {
            set_upload_status("file is empty", true);
            return;
        }

        std::string filename = path.filename().string();
        if (filename.empty()) {
            filename = file_name_from_path(normalized);
        }

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (upload_busy_) {
                return;
            }
            upload_path_.clear();
            upload_success_ = false;
            upload_status_is_error_ = false;
            upload_busy_ = true;
            upload_status_ = "uploading " + filename;
            upload_progress_ = {filename, 0, total_bytes, false};
            has_upload_progress_ = true;
        }
        screen_.PostEvent(Event::Custom);
        if (on_upload_) {
            on_upload_(normalized);
        }
    };

    auto btn_upload = Button("Upload to room", submit_upload);

    auto container = Container::Vertical({
        Container::Horizontal({btn_leave, btn_mood}),
        Container::Horizontal({input_upload, btn_upload}),
        Container::Horizontal({btn_mute}),
    });

    component_ = Renderer(container, [this, container] {
        // Знімок даних під mutex'ом
        std::string room_name, track_name;
        std::vector<UserEntry> users;
        std::vector<float> viz;
        std::string upload_path;
        std::string upload_status;
        std::string added_track_name;
        std::vector<TrackEntry> library_tracks;
        std::vector<QueueEntry> queue_tracks;
        UploadProgressView upload_progress;
        int ping_ms, selected_track, added_queue_position;
        int mood_index;
        uint64_t mood_tick;
        float loss;
        uint16_t added_track_id;
        bool library_loading, queue_loading;
        bool muted, upload_status_is_error, upload_busy, upload_success, has_upload_progress;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            room_name  = room_name_;
            track_name = track_name_;
            users      = users_;
            library_tracks = library_tracks_;
            queue_tracks = queue_tracks_;
            selected_track = selected_library_track_;
            library_loading = library_loading_;
            queue_loading = queue_loading_;
            viz        = visualizer_bars_;
            upload_path = upload_path_;
            upload_status = upload_status_;
            added_track_name = added_track_name_;
            upload_progress = upload_progress_;
            ping_ms    = ping_ms_;
            added_track_id = added_track_id_;
            added_queue_position = added_queue_position_;
            mood_index = mood_index_;
            mood_tick = mood_tick_;
            loss       = loss_;
            muted      = muted_;
            upload_status_is_error = upload_status_is_error_;
            upload_busy = upload_busy_;
            upload_success = upload_success_;
            has_upload_progress = has_upload_progress_;
        }

        std::string normalized_upload_path = trim_copy(upload_path);
        std::filesystem::path upload_fs_path(normalized_upload_path);
        std::string upload_extension = upload_fs_path.extension().string();
        std::transform(upload_extension.begin(), upload_extension.end(), upload_extension.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        bool can_upload = !upload_busy && !normalized_upload_path.empty() && upload_extension == ".mp3";
        bool upload_input_focused = container->ChildAt(1)->ChildAt(0)->Focused();

        // Header
        auto header = hbox({
            container->ChildAt(0)->ChildAt(0)->Render(),
            text("   "),
            text("ROOM: " + room_name) | bold | color(section_text()),
            filler(),
            text("[ ") | color(soft_text()),
            container->ChildAt(0)->ChildAt(1)->Render(),
            text(": " + std::string(mood_name(mood_index)) + " ]") | color(accent_soft()),
            filler(),
            text("● live") | color(Color::Green),
            text(" · " + std::to_string(users.size()) + " online") | color(soft_text()),
            text("  "),
        });

        // Ліва колонка: плеєр + візуалізатор
        auto left_col = vbox({
            render_now_playing(track_name),
            separator() | color(soft_text()),
            render_visualizer(viz),
            render_mood(mood_index, mood_tick),
            separator() | color(soft_text()),
            render_library(library_tracks, selected_track, library_loading, added_track_id),
            separator() | color(soft_text()),
            render_upload(upload_path, upload_status, upload_status_is_error,
                          upload_busy, upload_success, added_queue_position,
                          added_track_name, has_upload_progress, upload_progress),
            vbox({
                hbox({
                    text("  MP3 PATH") | color(section_text()) | bold,
                    filler(),
                    text(upload_busy ? "uploading" : (upload_input_focused ? "active" : "ready"))
                        | color(soft_text()),
                    text("  "),
                }),
                hbox({
                    text("  "),
                    container->ChildAt(1)->ChildAt(0)->Render()
                        | color(Color::White)
                        | size(WIDTH, EQUAL, 48),
                    text("  "),
                }) | borderStyled(soft_text()),
                normalized_upload_path.empty() || upload_busy
                    ? text("")
                    : text("  Typed: " + compact_path_for_display(normalized_upload_path))
                        | color(Color::White),
                text("  Start typing or paste a full .mp3 path, then press Enter to upload.")
                    | color(soft_text()),
                hbox({
                    filler(),
                    container->ChildAt(1)->ChildAt(1)->Render()
                        | (can_upload ? nothing : dim),
                    text("  "),
                }),
            }),
            separator() | color(soft_text()),
            render_queue(queue_tracks, queue_loading),
        }) | flex;

        // Права колонка: юзери + помодоро
        auto right_col = vbox({
            render_users(users),
            filler(),
        }) | size(WIDTH, EQUAL, 20);

        return vbox({
            header,
            separator(),
            hbox({
                left_col,
                separator() | color(soft_text()),
                right_col,
            }) | flex,
            separator(),
            hbox({
                render_footer(ping_ms, loss, muted),
                filler(),
                container->ChildAt(2)->ChildAt(0)->Render(),
                text("  "),
            }),
        }) | border;
    });

    // Printable characters go to the upload input while it is focused.
    // F2 is handled by AppUI as the talk shortcut.
    component_ = CatchEvent(component_, [this, input_upload, submit_upload, add_selected_to_queue](Event event) {
        if (input_upload->Focused() && event.is_character()) {
            return false;
        }
        if (event == Event::Return && !trim_copy(upload_path_).empty()) {
            submit_upload();
            return true;
        }
        if (!input_upload->Focused() && event == Event::Return) {
            add_selected_to_queue();
            return true;
        }
        if (!input_upload->Focused() && event.is_character()) {
            std::string value = event.character();
            if (!value.empty() && value[0] >= 32 && value[0] != 'q') {
                input_upload->TakeFocus();
                return input_upload->OnEvent(event);
            }
        }
        if (event == Event::ArrowDown) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (!library_tracks_.empty()) {
                selected_library_track_ = std::min(
                    selected_library_track_ + 1,
                    static_cast<int>(library_tracks_.size()) - 1);
                screen_.PostEvent(Event::Custom);
                return true;
            }
        }
        if (event == Event::ArrowUp) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (!library_tracks_.empty()) {
                selected_library_track_ = std::max(selected_library_track_ - 1, 0);
                screen_.PostEvent(Event::Custom);
                return true;
            }
        }
        if (event == Event::F4) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            mood_index_ = (mood_index_ + 1) % 3;
            mood_tick_++;
            screen_.PostEvent(Event::Custom);
            return true;
        }
        if (event == Event::Character('q')) {
            if (on_leave_) on_leave_();
            return true;
        }
        return false;
    });
}
