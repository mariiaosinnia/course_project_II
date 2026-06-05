#include <boost/endian/conversion.hpp>

#include "PacketBuilder.h"
#include "Protocol.h"

std::vector<uint8_t> PacketBuilder::connected(uint32_t client_id){
    ConnectedBody body;
    body.client_id = boost::endian::native_to_big(client_id);
    return build(PacketType::Connected, body);
}

std::vector<uint8_t> PacketBuilder::pong(){
    return build(PacketType::Pong);
}

std::vector<uint8_t> PacketBuilder::error(ErrorCode code){
    ErrorBody body;
    body.error_code = static_cast<uint8_t>(code);
    return build(PacketType::Error, body);
}

std::vector<uint8_t> PacketBuilder::build(PacketType type){
    PacketHeader header;
    header.type = static_cast<uint8_t>(type);
    header.payload_size = 0;

    std::vector<uint8_t> packet(sizeof(header));
    std::memcpy(packet.data(), &header, sizeof(header));
    return packet;
}