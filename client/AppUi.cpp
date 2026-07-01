#include "AppUI.h"
#include "ClientApp.h"

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
            // TODO: mute/unmute через VoiceStart/VoiceStop
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

    app_.set_on_room_joined([this](uint16_t /*room_id*/) {
        navigate_to(Screen::Room);
    });

    app_.set_on_room_list_updated([this] {
        // Список оновлено — передаємо в LobbyScreen
        // (дані вже в ClientApp::state через room_list_cache)
        const auto& rooms = app_.state().room_list_cache;
        std::vector<RoomEntry> entries;
        for (const auto& r : rooms) {
            entries.push_back({r.room_id, r.name, r.user_count, ""});
        }
        lobby_->set_rooms(std::move(entries));
    });

    app_.set_on_user_joined([this](uint32_t id, const std::string& name) {
        room_->add_user({id, name});
    });

    app_.set_on_user_left([this](uint32_t id) {
        room_->remove_user(id);
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
