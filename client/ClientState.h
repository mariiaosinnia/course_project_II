#pragma once

#include <atomic>
#include <cstdint>

// Стан TCP-сесії клієнта. Чистий контейнер даних - жодної логіки тут.
// std::atomic, бо мережевий потік (TCPClient callbacks) і потік CLI
// можуть читати/писати ці поля одночасно.
struct ClientState {
    std::atomic<uint32_t> client_id{0};
    std::atomic<bool> connected{false};
    std::atomic<uint16_t> current_room_id{0};  // 0 = не в кімнаті
};
