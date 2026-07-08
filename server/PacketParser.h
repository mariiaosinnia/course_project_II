#pragma once
#include <optional>
#include <vector>
#include <cstdint>
#include "Protocol.h"

class PacketParser
{
public:
	static std::optional<ConnectBody> parse_connect(const std::vector<uint8_t>& body);
	static std::optional<JoinRoomBody> parse_join_room(const std::vector<uint8_t>& body);
	static std::optional<CreateRoomBody> parse_create_room(const std::vector<uint8_t>& body);
	static std::optional<UploadTrackBeginBody> parse_upload_begin(const std::vector<uint8_t>& body);
};