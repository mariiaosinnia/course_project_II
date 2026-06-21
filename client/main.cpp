#include <iostream>

#include <boost/asio.hpp>
#include <boost/endian/conversion.hpp>

#include "PacketBuilder.h"
#include "PacketParser.h"
#include "Protocol.h"
#include "TCPClient.h"
#include "UdpRegistration.h"

namespace {

constexpr const char* SERVER_HOST = "127.0.0.1";
constexpr uint16_t SERVER_TCP_PORT = 12345;

// Стан застосунку. У реальному клієнті це окремий клас ClientState
// з mutex'ом, бо до нього лізтиме і мережевий потік, і UI-потік (FTXUI).
// Тут навмисно спрощено - лише щоб показати логіку диспетчера.
struct ClientState {
    uint32_t client_id = 0;
};

// Реєстрація на UDP - окрема дія, не через TCPClient.
// Відкриває власний UDP-сокет і шле один пакет на UDP-порт сервера.
void send_udp_registration(boost::asio::io_context& io_context, uint32_t client_id)
{
    using boost::asio::ip::udp;

    udp::socket udp_socket(io_context, udp::endpoint(udp::v4(), 0));
    udp::endpoint server_endpoint(
        boost::asio::ip::make_address(SERVER_HOST), DRAFT_UDP_PORT);

    UdpRegisterPacket packet;
    packet.client_id = boost::endian::native_to_big(client_id);

    boost::system::error_code ec;
    udp_socket.send_to(boost::asio::buffer(&packet, sizeof(packet)), server_endpoint, 0, ec);

    if (ec) {
        std::cerr << "UDP registration failed: " << ec.message() << "\n";
    }
    else {
        std::cout << "UDP registration sent for client_id=" << client_id << "\n";
    }

    // У реальному клієнті цей сокет має жити далі (зберігатись у
    // AudioBridge чи окремому UdpClient), а не закриватись тут -
    // він знадобиться, щоб ПРИЙМАТИ аудіо. Це поки лише демонстрація
    // самого факту реєстрації.
}

} // namespace

int main()
{
    boost::asio::io_context io_context;
    ClientState state;

    auto tcp_client = TCPClient::create(io_context);

    // Диспетчер вхідних пакетів. ВАЖЛИВО: це не "одна відповідь на
    // один запит" - UserJoined/UserLeft можуть прилетіти будь-коли,
    // тому кожен тип обробляється незалежно.
    tcp_client->set_packet_callback(
        [&io_context, &state, tcp_client](PacketType type, const std::vector<uint8_t>& body) {
            switch (type) {
            case PacketType::Connected: {
                auto parsed = PacketParser::parse_connected(body);
                if (!parsed) {
                    std::cerr << "bad Connected body\n";
                    return;
                }
                state.client_id = parsed->client_id;
                std::cout << "Connected, client_id=" << state.client_id << "\n";

                send_udp_registration(io_context, state.client_id);
                break;
            }
            case PacketType::RoomCreated: {
                auto parsed = PacketParser::parse_room_created(body);
                if (!parsed) {
                    std::cerr << "bad RoomCreated body\n";
                    return;
                }
                std::cout << "RoomCreated, room_id=" << parsed->room_id << "\n";

                // Договір з командою: клієнт сам приєднується до
                // щойно створеної кімнати, сервер цього не робить.
                tcp_client->send(PacketBuilder::join_room(parsed->room_id));
                break;
            }
            case PacketType::UserJoined: {
                auto parsed = PacketParser::parse_user_joined(body);
                if (parsed) {
                    std::cout << "UserJoined: " << parsed->username
                        << " (id=" << parsed->client_id << ")\n";
                }
                break;
            }
            case PacketType::UserLeft: {
                auto parsed = PacketParser::parse_user_left(body);
                if (parsed) {
                    std::cout << "UserLeft: id=" << parsed->client_id << "\n";
                }
                break;
            }
            case PacketType::Error: {
                auto parsed = PacketParser::parse_error(body);
                if (parsed) {
                    std::cerr << "Server error, code=0x" << std::hex
                        << static_cast<int>(parsed->error_code) << std::dec << "\n";
                }
                break;
            }
            default:
                std::cout << "Unhandled packet type: 0x" << std::hex
                    << static_cast<int>(type) << std::dec << "\n";
                break;
            }
        });

    tcp_client->set_disconnect_callback([]() {
        std::cout << "Disconnected from server\n";
        });

    tcp_client->connect(SERVER_HOST, SERVER_TCP_PORT, [tcp_client](bool ok) {
        if (!ok) {
            return;
        }
        tcp_client->send(PacketBuilder::connect("daryna"));
        });

    io_context.run();
    return 0;
}
