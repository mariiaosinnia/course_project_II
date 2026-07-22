#include "AppUi.h"
#include "ClientApp.h"

#include <mutex>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

AppUI::AppUI(ClientApp& app)
    : app_(app)
    , screen_(ScreenInteractive::Fullscreen())
{
    // --- Login screen ---
    login_ = std::make_unique<LoginScreen>(
        screen_,
        [this](const std::string& username) {
            // Зберігаємо username для передачі в Lobby
            username_ = username;
            // Підключаємось до сервера
            app_.connect_to_server([this](bool ok) {
                if (!ok) {
                    login_->set_status("connection failed", true);
                    return;
                }
                // TCP з'єднання є — відправляємо Connect-пакет
                app_.send_connect(username_);
                // Статус "connecting..." — оновиться на "connected"
                // коли ClientApp отримає Connected від сервера
                login_->set_status("connecting...");
            });
        }
    );

    // --- Lobby screen ---
    lobby_ = std::make_unique<LobbyScreen>(
        screen_,
        username_,  // оновиться пізніше через navigate_to(Lobby)
        [this](const std::string& name) {
            app_.send_create_room(name);
        },
        [this](uint16_t room_id) {
            app_.send_join_room(room_id);
        },
        [this] {
            app_.send_list_rooms();
        }
    );

    // --- Room screen ---
    room_ = std::make_unique<RoomScreen>(
        screen_,
        [this] {
            app_.send_leave_room();
        },
        [this] {
            bool voice_active = app_.toggle_voice();
            room_->set_muted(!voice_active);
        },
        [this](const std::string& path) {
            app_.send_upload_track(path);
        },
        [this](uint16_t track_id) {
            app_.send_track_select(track_id);
        },
        [this] {
            app_.send_list_tracks();
        }
    );

    // --- Підписуємось на мережеві події через ClientApp ---

    app_.set_on_connected([this](uint32_t /*client_id*/) {
        // Connected прийшов — переходимо в Lobby
        // Запитуємо список кімнат одразу
        app_.send_list_rooms();
        navigate_to(Screen::Lobby);
    });

    app_.set_on_disconnected([this] {
        // Дисконнект — назад на логін
        login_->set_status("disconnected", true);
        navigate_to(Screen::Login);
    });

    app_.set_on_room_created([this](uint16_t room_id) {
        // Кімнату створено — одразу заходимо в неї
        app_.send_join_room(room_id);
        app_.send_list_rooms();
    });

    app_.set_on_room_joined([this](uint16_t room_id) {
        room_->set_room_name("room #" + std::to_string(room_id));
        room_->set_track_name("track #" + std::to_string(room_id));
        room_->set_users({});
        room_->set_muted(!app_.is_voice_active());
        navigate_to(Screen::Room);
    });

    app_.set_on_room_left([this] {
        room_->set_users({});
        navigate_to(Screen::Lobby);
    });

    app_.set_on_room_list_updated([this] {
        auto& state = app_.state();
        std::vector<RoomEntry> entries;
        {
            std::lock_guard<std::mutex> lock(state.room_list_mutex);
            for (const auto& r : state.room_list_cache) {
                entries.push_back({r.room_id, r.name, r.user_count, ""});
            }
        }
        lobby_->set_rooms(std::move(entries));
    });

    app_.set_on_user_joined([this](uint32_t id, const std::string& name) {
        room_->add_user({id, name});
    });

    app_.set_on_user_left([this](uint32_t id) {
        room_->remove_user(id);
    });

    app_.set_on_voice_started([this](uint32_t id) {
        room_->set_user_speaking(id, true);
    });

    app_.set_on_voice_stopped([this](uint32_t id) {
        room_->set_user_speaking(id, false);
    });

    app_.set_on_upload_status([this](const std::string& status, bool is_error) {
        room_->set_upload_status(status, is_error);
    });

    app_.set_on_track_added([this](uint16_t room_id, uint16_t track_id, const std::string& filename) {
        std::string display = filename.empty()
            ? "track #" + std::to_string(track_id)
            : filename + " (#" + std::to_string(track_id) + ")";
        room_->set_track_name(display);
    });

    app_.set_on_track_list_updated([this] {
        auto& state = app_.state();
        std::vector<TrackEntry> entries;
        {
            std::lock_guard<std::mutex> lock(state.track_list_mutex);
            for (const auto& track : state.track_list_cache) {
                entries.push_back({track.track_id, track.filename});
            }
        }
        room_->set_tracks(std::move(entries));
    });
}

void AppUI::navigate_to(Screen s)
{
    current_ = s;
    // PostEvent змусить FTXUI перемалювати і наш Renderer
    // побачить оновлений current_
    screen_.PostEvent(Event::Custom);
}

void AppUI::run()
{
    // Головний Renderer: показує той екран, який зараз активний.
    // При navigate_to(X) — current_ змінюється, PostEvent будить
    // цей Renderer, і він повертає вже інший Component.

    auto root = Renderer([this] {
        switch (current_) {
            case Screen::Login: return login_->component()->Render();
            case Screen::Lobby: return lobby_->component()->Render();
            case Screen::Room:  return room_->component()->Render();
        }
        return text("") | nothing;
    });

    // Пробрасуємо події в активний Component (для Tab, стрілок тощо)
    auto root_with_events = CatchEvent(root, [this](Event event) {
        switch (current_) {
            case Screen::Login: return login_->component()->OnEvent(event);
            case Screen::Lobby: return lobby_->component()->OnEvent(event);
            case Screen::Room:  return room_->component()->OnEvent(event);
        }
        return false;
    });

    screen_.Loop(root_with_events);
}
