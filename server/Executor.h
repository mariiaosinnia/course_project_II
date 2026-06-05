#include <vector>

#include "Protocol.h"
#include "PacketParser.h"

class Executor
{
public:
	Executor() = default;
	~Executor() = default;
	std::vector<uint8_t> execute(uint32_t client_id, PacketType type, const std::vector<uint8_t>& body);
private:
	std::vector<uint8_t> handle_ping();
	std::vector<uint8_t> handle_connect(uint32_t client_id, const std::vector<uint8_t>& body);

	PacketParser parser;
};
