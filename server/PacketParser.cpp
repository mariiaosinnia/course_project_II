#include <boost/endian/conversion.hpp>

#include "PacketParser.h"

std::optional<ConnectBody> PacketParser::parse_connect(const std::vector<uint8_t>& body)
{
	if (body.size() < sizeof(ConnectBody)) {
		return std::nullopt;
	}

	ConnectBody result;
	std::memcpy(&result, body.data(), sizeof(result));
	result.username[sizeof(result.username) - 1] = '\0';

	return result;
}

std::optional<JoinRoomBody> PacketParser::parse_join_room(const std::vector<uint8_t>& body)
{
	if (body.size() < sizeof(JoinRoomBody)) {
		return std::nullopt;
	}

	JoinRoomBody result;
	std::memcpy(&result, body.data(), sizeof(result));
	result.room_id = boost::endian::big_to_native(result.room_id);
	
	return result;
}

std::optional<CreateRoomBody> PacketParser::parse_create_room(const std::vector<uint8_t>& body)
{
	if (body.size() < sizeof(CreateRoomBody)) {
		return std::nullopt;
	}

	CreateRoomBody result;
	std::memcpy(&result, body.data(), sizeof(result));
	result.name[sizeof(result.name) - 1] = '\0';

	return result;
}

std::optional<UploadTrackBeginBody> PacketParser::parse_upload_begin(const std::vector<uint8_t>& body)
{
	if (body.size() < sizeof(UploadTrackBeginBody)) {
		return std::nullopt;
	}

	UploadTrackBeginBody result;
	std::memcpy(&result, body.data(), sizeof(result));
	result.room_id = boost::endian::big_to_native(result.room_id);
	result.file_size = boost::endian::big_to_native(result.file_size);
	result.filename[sizeof(result.filename) - 1] = '\0';

	return result;
}

