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
            pending_created_room_name_ = name;
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
            toggle_room_voice();
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
        current_room_id_ = 0;
        current_playing_track_id_ = 0;
        room_names_.clear();
        room_->set_visualizer_data({});
        navigate_to(Screen::Login);
    });

    app_.set_on_room_created([this](uint16_t room_id) {
        // Кімнату створено — одразу заходимо в неї
        if (!pending_created_room_name_.empty()) {
            room_names_[room_id] = pending_created_room_name_;
        }
        app_.send_join_room(room_id);
        app_.send_list_rooms();
    });

    app_.set_on_room_joined([this](uint16_t room_id) {
        current_room_id_ = room_id;
        current_playing_track_id_ = 0;
        room_->set_room_name(room_display_name(room_id));
        room_->set_track_name("—");
        room_->set_users({});
        room_->reset_library();
        room_->reset_queue();
        room_->set_muted(!app_.is_voice_active());
        pending_created_room_name_.clear();
        navigate_to(Screen::Room);
    });

    app_.set_on_room_left([this] {
        if (current_room_id_ != 0) {
            room_names_.erase(current_room_id_);
        }
        current_room_id_ = 0;
        current_playing_track_id_ = 0;
        room_->set_users({});
        room_->reset_library();
        room_->reset_queue();
        room_->set_visualizer_data({});
        navigate_to(Screen::Lobby);
        app_.send_list_rooms();
    });

    app_.set_on_room_list_updated([this] {
        auto& state = app_.state();
        std::vector<RoomEntry> entries;
        std::unordered_map<uint16_t, std::string> fresh_room_names;
        {
            std::lock_guard<std::mutex> lock(state.room_list_mutex);
            for (const auto& r : state.room_list_cache) {
                fresh_room_names[r.room_id] = r.name;
                entries.push_back({r.room_id, r.name, r.user_count, ""});
            }
        }
        room_names_ = std::move(fresh_room_names);
        lobby_->set_status("", false);
        lobby_->set_rooms(std::move(entries));
    });

    app_.set_on_error([this](StatusCode code) {
        if (code == StatusCode::RoomNotFound) {
            if (current_ == Screen::Room) {
                current_room_id_ = 0;
                current_playing_track_id_ = 0;
                room_->set_users({});
                room_->reset_library();
                room_->reset_queue();
                room_->set_visualizer_data({});
                navigate_to(Screen::Lobby);
            }
            lobby_->set_status("room no longer exists", true);
            app_.send_list_rooms();
        }
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

    app_.set_on_upload_progress([this](const UploadProgress& progress) {
        room_->set_upload_progress({
            progress.filename,
            progress.bytes_sent,
            progress.total_bytes,
            progress.waiting_for_server,
        });
    });

    app_.set_on_visualizer_data([this](std::vector<float> bars) {
        room_->set_visualizer_data(std::move(bars));
    });

    app_.set_on_track_added([this](uint16_t room_id, uint16_t track_id, const std::string& filename) {
        room_->set_track_added_confirmation(track_id, filename);
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

    app_.set_on_queue_list_updated([this] {
        auto& state = app_.state();
        std::vector<QueueEntry> entries;
        {
            std::lock_guard<std::mutex> lock(state.queue_list_mutex);
            for (const auto& track : state.queue_list_cache) {
                entries.push_back({track.track_id, track.filename, track.is_current != 0});
            }
        }
        bool has_current = false;
        for (const auto& entry : entries) {
            if (entry.is_current) {
                has_current = true;
                if (entry.track_id == current_playing_track_id_) {
                    break;
                }
                current_playing_track_id_ = entry.track_id;
                std::string display = entry.filename.empty()
                    ? "track #" + std::to_string(entry.track_id)
                    : entry.filename + " (#" + std::to_string(entry.track_id) + ")";
                room_->set_track_name(display);
                break;
            }
        }
        if (!has_current && current_playing_track_id_ != 0) {
            current_playing_track_id_ = 0;
            room_->set_track_name("—");
        }
        room_->set_queue(std::move(entries));
    });
}

void AppUI::navigate_to(Screen s)
{
    current_ = s;
    // PostEvent змусить FTXUI перемалювати і наш Renderer
    // побачить оновлений current_
    screen_.PostEvent(Event::Custom);
}

void AppUI::toggle_room_voice()
{
    bool was_voice_active = app_.is_voice_active();
    bool voice_active = app_.toggle_voice();
    room_->set_muted(!voice_active);
    if (!was_voice_active && !voice_active) {
        room_->set_upload_status("microphone unavailable; check client.log", true);
    } else if (voice_active) {
        room_->set_upload_status("speaking", false);
    } else {
        room_->set_upload_status("", false);
    }
}

std::string AppUI::room_display_name(uint16_t room_id) const
{
    auto it = room_names_.find(room_id);
    if (it != room_names_.end() && !it->second.empty()) {
        return it->second;
    }
    return "room #" + std::to_string(room_id);
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
        if (current_ == Screen::Room && event == Event::F2) {
            toggle_room_voice();
            return true;
        }

        switch (current_) {
            case Screen::Login: return login_->component()->OnEvent(event);
            case Screen::Lobby: return lobby_->component()->OnEvent(event);
            case Screen::Room:  return room_->component()->OnEvent(event);
        }
        return false;
    });

    screen_.Loop(root_with_events);
}
