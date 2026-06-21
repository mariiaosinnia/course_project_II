#pragma once
#include <cstdint>
#include <string>

struct User {
	uint32_t id = 0;
	std::string name;
	uint16_t room_id = 0;
};