#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>

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

// Захищає std::cout від одночасного запису з мережевого потоку
// (callback-и io_context.run()) і з потоку CLI (cliLoop).
std::mutex cout_mutex;

void print(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << msg << "\n";
}

// Стан застосунку. У реальному клієнті (з FTXUI) сюди ж писатиме
// мережевий потік, а UI-потік читатиме - тут спрощено, бо немає UI,
// лише CLI-потік сам формує команди.
struct ClientState {
    std::atomic<uint32_t> client_id{0};
    std::atomic<bool> connected{false};
};

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
        print("UDP registration failed: " + ec.message());
    } else {
        print("UDP registration sent for client_id=" + std::to_string(client_id));
    }
}

// Обробка одного рядка з консолі. Повертає false, якщо треба завершити цикл (/quit).
bool handle_line(const std::string& line, std::shared_ptr<TCPClient> client, ClientState& state)
{
    if (line.empty()) {
        return true;
    }

    std::istringstream iss(line);
    std::string command;
    iss >> command;

    if (command.empty() || command.front() != '/') {
        print("Unknown input, commands start with '/'. Type /help.");
        return true;
    }
    command.erase(0, 1);  // прибрати '/'

    if (command == "quit") {
        return false;
    }

    if (command == "help") {
        print(
            "Commands:\n"
            "  /connect <username>\n"
            "  /create <room_name>\n"
            "  /join <room_id>\n"
            "  /leave\n"
            "  /list\n"
            "  /ping\n"
            "  /quit");
        return true;
    }

    if (command == "connect") {
        std::string username;
        iss >> username;
        if (username.empty()) {
            print("Usage: /connect <username>");
            return true;
        }
        client->send(PacketBuilder::connect(username));
        return true;
    }

    if (command == "create") {
        std::string room_name;
        iss >> room_name;
        if (room_name.empty()) {
            print("Usage: /create <room_name>");
            return true;
        }
        client->send(PacketBuilder::create_room(room_name));
        return true;
    }

    if (command == "join") {
        std::string room_id_str;
        iss >> room_id_str;
        if (room_id_str.empty()) {
            print("Usage: /join <room_id>");
            return true;
        }
        try {
            uint16_t room_id = static_cast<uint16_t>(std::stoi(room_id_str));
            client->send(PacketBuilder::join_room(room_id));
        } catch (const std::exception&) {
            print("Invalid room_id");
        }
        return true;
    }

    if (command == "leave") {
        client->send(PacketBuilder::leave_room());
        return true;
    }

    if (command == "list") {
        client->send(PacketBuilder::list_rooms());
        return true;
    }

    if (command == "ping") {
        client->send(PacketBuilder::ping());
        return true;
    }

    print("Unknown command: /" + command + " (type /help)");
    return true;
}

// CLI-цикл - працює в окремому потоці, читає std::cin, формує і шле пакети.
void cli_loop(boost::asio::io_context& io_context,
              std::shared_ptr<TCPClient> client,
              ClientState& state)
{
    std::string line;
    while (true) {
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "> " << std::flush;
        }

        if (!std::getline(std::cin, line)) {
            break;  // EOF / stdin закрито
        }

        if (!handle_line(line, client, state)) {
            break;  // /quit
        }
    }

    // Зупиняємо io_context, щоб io_context.run() в main-потоці теж завершився.
    io_context.stop();
}

}  // namespace

int main()
{
    boost::asio::io_context io_context;
    ClientState state;

    auto tcp_client = TCPClient::create(io_context);

    tcp_client->set_packet_callback(
        [&io_context, &state](PacketType type, const std::vector<uint8_t>& body) {
            switch (type) {
            case PacketType::Connected: {
                auto parsed = PacketParser::parse_connected(body);
                if (!parsed) {
                    print("bad Connected body");
                    return;
                }
                state.client_id = parsed->client_id;
                state.connected = true;
                print("Connected, client_id=" + std::to_string(parsed->client_id));
                send_udp_registration(io_context, parsed->client_id);
                break;
            }
            case PacketType::RoomCreated: {
                auto parsed = PacketParser::parse_room_created(body);
                if (!parsed) {
                    print("bad RoomCreated body");
                    return;
                }
                print("RoomCreated, room_id=" + std::to_string(parsed->room_id) +
                      " (use /join " + std::to_string(parsed->room_id) + " to enter)");
                break;
            }
            case PacketType::UserJoined: {
                auto parsed = PacketParser::parse_user_joined(body);
                if (parsed) {
                    print(std::string("UserJoined: ") + parsed->username +
                          " (id=" + std::to_string(parsed->client_id) + ")");
                }
                break;
            }
            case PacketType::UserLeft: {
                auto parsed = PacketParser::parse_user_left(body);
                if (parsed) {
                    print("UserLeft: id=" + std::to_string(parsed->client_id));
                }
                break;
            }
            case PacketType::RoomList: {
                auto parsed = PacketParser::parse_room_list(body);
                if (!parsed) {
                    print("bad RoomList body");
                    return;
                }
                std::string msg = "RoomList (" + std::to_string(parsed->rooms.size()) + "):";
                for (const auto& room : parsed->rooms) {
                    msg += "\n  room_id=" + std::to_string(room.room_id) +
                           " name=" + room.name +
                           " users=" + std::to_string(room.user_count);
                }
                print(msg);
                break;
            }
            case PacketType::RoomJoined: {
                auto parsed = PacketParser::parse_room_joined(body);
                if (!parsed) {
                    print("bad RoomJoined body");
                    return;
                }
                std::string msg = "RoomJoined room_id=" + std::to_string(parsed->header.room_id) +
                                   " track_position_ms=" + std::to_string(parsed->header.track_position_ms) +
                                   " users(" + std::to_string(parsed->users.size()) + "):";
                for (const auto& user : parsed->users) {
                    msg += "\n  id=" + std::to_string(user.client_id) +
                           " name=" + user.username;
                }
                print(msg);
                break;
            }
            case PacketType::RoomLeft: {
                print("RoomLeft");
                break;
            }
            case PacketType::Pong: {
                print("Pong received");
                break;
            }
            case PacketType::Error: {
                auto parsed = PacketParser::parse_error(body);
                if (parsed) {
                    std::ostringstream oss;
                    oss << "Server error, code=0x" << std::hex
                        << static_cast<int>(parsed->error_code);
                    print(oss.str());
                }
                break;
            }
            default: {
                std::ostringstream oss;
                oss << "Unhandled packet type: 0x" << std::hex
                    << static_cast<int>(type);
                print(oss.str());
                break;
            }
            }
        });

    tcp_client->set_disconnect_callback([&state]() {
        state.connected = false;
        print("Disconnected from server");
    });

    tcp_client->connect(SERVER_HOST, SERVER_TCP_PORT, [](bool ok) {
        if (ok) {
            print("TCP connected. Type /connect <username> to register. (/help for commands)");
        } else {
            print("Failed to connect to server.");
        }
    });

    // CLI працює у власному потоці, мережа - в io_context.run() тут, у main-потоці.
    std::thread cli_thread(cli_loop, std::ref(io_context), tcp_client, std::ref(state));

    io_context.run();

    if (cli_thread.joinable()) {
        cli_thread.join();
    }

    return 0;
}