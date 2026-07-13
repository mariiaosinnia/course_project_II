#pragma once
#include <cstdint>
#include <string>
#include <optional>
#include <boost/asio.hpp>
using boost::asio::ip::udp;


struct User {
	uint32_t id = 0;
	std::string name;
	uint16_t room_id = 0;
	bool is_speaking = false;
	std::optional<udp::endpoint> udp_endpoint;
};