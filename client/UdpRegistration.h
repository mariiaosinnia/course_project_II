#pragma once

#include <cstdint>

// Ідея: клієнт після Connected{client_id}
// відкриває власний UDP-сокет і шле ОДИН такий пакет на UDP-порт
// сервера, щоб сервер зміг зв'язати client_id з (ip, port)
// відправника у своїй таблиці мапінгу.


#pragma pack(push, 1)
struct UdpRegisterPacket {
    uint8_t marker = 0xFF;
    uint32_t client_id;    // big-endian
};
#pragma pack(pop)

constexpr uint16_t DRAFT_UDP_PORT = 12346;
