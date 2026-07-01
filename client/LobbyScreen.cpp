#include "LobbyScreen.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

// Іконки для кімнат (ротуємо по room_id)
static const char* room_icons[] = {"☁", "☕", "🌙", "🍅", "🎵", "⭐"};
static const char* room_icon(uint16_t room_id) {
    return room_icons[room_id % 6];
}

LobbyScreen::LobbyScreen(
    ScreenInteractive& screen,
    const std::string& username,
    std::function<void(const std::string&)> on_create,
    std::function<void(uint16_t)> on_join,
    std::function<void()> on_refresh)
    : screen_(screen)
    , username_(username)
    , on_create_(std::move(on_create))
    , on_join_(std::move(on_join))
    , on_refresh_(std::move(on_refresh))
{
    build();
}

void LobbyScreen::set_rooms(std::vector<RoomEntry> rooms)
{
    std::lock_guard<std::mutex> lock(rooms_mutex_);
    rooms_ = std::move(rooms);
    if (selected_ >= static_cast<int>(rooms_.size())) selected_ = 0;
    screen_.PostEvent(Event::Custom);
}

void LobbyScreen::set_status(const std::string& msg, bool is_error)
{
    status_ = msg;
    status_is_error_ = is_error;
    screen_.PostEvent(Event::Custom);
}

Component LobbyScreen::component() { return component_; }

void LobbyScreen::build()
{
    auto input_name = Input(&new_room_name_, "room name...");

    auto btn_create = Button("[ + new ]", [this] {
        if (new_room_name_.empty()) return;
        if (on_create_) on_create_(new_room_name_);
        new_room_name_.clear();
    });

    auto container = Container::Horizontal({input_name, btn_create});

    component_ = Renderer(container, [this, container] {
        std::vector<RoomEntry> rooms_copy;
        {
            std::lock_guard<std::mutex> lock(rooms_mutex_);
            rooms_copy = rooms_;
        }

        // Список кімнат
        Elements room_elements;
        if (rooms_copy.empty()) {
            room_elements.push_back(
                text("  no rooms yet") | color(Color::GrayDark) | italic
            );
        } else {
            for (size_t i = 0; i < rooms_copy.size(); ++i) {
                const auto& r = rooms_copy[i];
                bool selected = (static_cast<int>(i) == selected_);

                // Рядок 1: іконка + назва кімнати
                auto name_line = hbox({
                    text("  " + std::string(room_icon(r.room_id)) + "  "),
                    text(r.name) | (selected ? bold : nothing),
                }) | (selected ? color(Color::Cyan) : nothing);

                // Рядок 2: трек · кількість онлайн
                std::string meta = "     ";
                if (!r.track_name.empty()) meta += r.track_name + "  ·  ";
                meta += std::to_string(r.user_count) + " online";

                auto meta_line = text(meta) | color(Color::GrayDark);

                room_elements.push_back(name_line);
                room_elements.push_back(meta_line);
                room_elements.push_back(text(""));
            }
        }

        // Рядок створення кімнати
        auto create_row = hbox({
            text("  "),
            container->ChildAt(0)->Render()
                | size(WIDTH, EQUAL, 24),
            text("  "),
            container->ChildAt(1)->Render(),
        });

        // Статус
        Element footer = text("");
        if (!status_.empty()) {
            footer = text("  " + status_)
                | color(status_is_error_ ? Color::Red : Color::GrayDark);
        }

        return vbox({
            // Header
            hbox({
                text("  ♪  ") | color(Color::Cyan),
                text("StudyRoom") | bold | color(Color::Cyan),
                filler(),
                text(username_ + "  ▾") | color(Color::GrayDark),
                text("  "),
            }),
            separator(),

            // Rooms label + create
            hbox({
                text("  rooms") | color(Color::GrayDark),
                filler(),
                create_row,
                text("  "),
            }),
            separator(),

            // Список кімнат
            vbox(std::move(room_elements)) | flex,

            separator(),

            // Footer
            hbox({
                footer,
                filler(),
                text("  ↑↓ navigate  ·  enter to join  ") | color(Color::GrayDark),
            }),

        }) | border;
    });

    // Клавіатурна навігація по кімнатах
    component_ = CatchEvent(component_, [this](Event event) {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        if (rooms_.empty()) return false;

        if (event == Event::ArrowDown) {
            selected_ = std::min(selected_ + 1, (int)rooms_.size() - 1);
            screen_.PostEvent(Event::Custom);
            return true;
        }
        if (event == Event::ArrowUp) {
            selected_ = std::max(selected_ - 1, 0);
            screen_.PostEvent(Event::Custom);
            return true;
        }
        if (event == Event::Return) {
            if (on_join_) on_join_(rooms_[selected_].room_id);
            return true;
        }
        if (event == Event::Character('r')) {
            if (on_refresh_) on_refresh_();
            return true;
        }
        return false;
    });
}
