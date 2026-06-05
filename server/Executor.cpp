#include "Executor.h"
#include "PacketBuilder.h"

std::vector<uint8_t> Executor::execute(uint32_t client_id, PacketType type,
    const std::vector<uint8_t>& body) {
    switch (type) {
    case PacketType::Ping: return handle_ping();
    case PacketType::Connect: return handle_connect(client_id, body);
    default:
        return PacketBuilder::error(ErrorCode::Unknown);
    }
}

std::vector<uint8_t> Executor::handle_ping() {
    return PacketBuilder::pong();
}

std::vector<uint8_t> Executor::handle_connect(uint32_t client_id,
    const std::vector<uint8_t>& body) {
    auto parsed = PacketParser::parse_connect(body);
    if (!parsed) return PacketBuilder::error(ErrorCode::Unknown);
    return PacketBuilder::connected(client_id);
}