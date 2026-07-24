#include "LobbyScreen.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace {
Color accent() { return Color::MediumPurple1; }
Color accent_soft() { return Color::Violet; }
Color soft_text() { return Color::GrayDark; }
Color section_text() { return Color::White; }
}

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

void LobbyScreen::set_username(const std::string& username)
{
    username_ = username;
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

    auto create_room = [this] {
        if (new_room_name_.empty()) return;
        if (on_create_) on_create_(new_room_name_);
        new_room_name_.clear();
    };

    auto btn_create = Button("+ room", create_room);

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
                text("  no rooms yet") | color(Color::GrayDark)
            );
        } else {
            for (size_t i = 0; i < rooms_copy.size(); ++i) {
                const auto& r = rooms_copy[i];
                bool selected = (static_cast<int>(i) == selected_);

                // Рядок 1: іконка + назва кімнати
                auto name_line = hbox({
                    text("  " + std::string(selected ? ">" : " ") + " "),
                    text(std::string(room_icon(r.room_id)) + "  "),
                    text(r.name) | (selected ? bold : nothing),
                }) | (selected ? color(accent()) : nothing);

                // Рядок 2: трек · кількість онлайн
                std::string meta = "       ";
                if (!r.track_name.empty()) meta += r.track_name + "  ·  ";
                meta += std::to_string(r.user_count) + " online";

                auto meta_line = text(meta) | color(soft_text());

                room_elements.push_back(name_line);
                room_elements.push_back(meta_line);
                room_elements.push_back(text(""));
            }
        }

        // Рядок створення кімнати
        auto create_row = hbox({
            text("  new room: ") | color(soft_text()),
            text("  "),
            container->ChildAt(0)->Render()
                | size(WIDTH, EQUAL, 26)
                | border
                | color(soft_text()),
            text("  "),
            container->ChildAt(1)->Render(),
        });

        // Статус
        Element footer = text("");
        if (!status_.empty()) {
            footer = text("  " + status_)
                | color(status_is_error_ ? accent() : soft_text());
        }

        return vbox({
            // Header
            hbox({
                text("  /ᐠ｡ꞈ｡ᐟ\\  ") | color(accent_soft()),
                text("StudyRoom") | bold | color(section_text()),
                filler(),
                text(username_ + "  ▾") | color(soft_text()),
                text("  "),
            }),
            separator(),

            // Rooms label + create
            hbox({
                text("  rooms") | color(soft_text()),
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
                text("  ↑↓ choose room  ·  enter joins selected  ·  r refresh  ") | color(soft_text()),
            }),

        }) | border;
    });

    // Клавіатурна навігація по кімнатах
    component_ = CatchEvent(component_, [this, input_name, create_room](Event event) {
        if (input_name->Focused() && event.is_character()) {
            return false;
        }
        if (input_name->Focused() && event == Event::Return && !new_room_name_.empty()) {
            create_room();
            return true;
        }
        if (event == Event::Character('r')) {
            if (on_refresh_) on_refresh_();
            return true;
        }
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
        return false;
    });
}
