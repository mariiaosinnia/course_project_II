#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Одна кімната в кеші списку
struct RoomCacheEntry {
    uint16_t room_id;
    std::string name;
    uint8_t user_count;
};

// Стан TCP-сесії. Чистий контейнер даних без логіки.
// atomic-поля читаються з будь-якого потоку без блокування.
// room_list_cache захищений room_list_mutex.
struct ClientState {
    std::atomic<uint32_t> client_id{0};
    std::atomic<bool> connected{false};
    std::atomic<uint16_t> current_room_id{0};

    // Кеш списку кімнат — оновлюється при кожному RoomList від сервера.
    // AppUI читає його для передачі в LobbyScreen.
    mutable std::mutex room_list_mutex;
    std::vector<RoomCacheEntry> room_list_cache;
};
