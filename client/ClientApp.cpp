#include "ClientApp.h"

#include <sstream>

#include "Logger.h"
#include "PacketBuilder.h"
#include "PacketParser.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>


std::shared_ptr<ClientApp> ClientApp::create(boost::asio::io_context& io_context)
{
    auto app = std::shared_ptr<ClientApp>(new ClientApp(io_context));

    std::weak_ptr<ClientApp> self = app;

    app->tcp_client_->set_packet_callback(
        [self](PacketType type, const std::vector<uint8_t>& body) {
            if (auto locked = self.lock()) {
                locked->on_packet(type, body);
            }
        });

    app->tcp_client_->set_disconnect_callback([self]() {
        if (auto locked = self.lock()) {
            locked->on_disconnect();
        }
    });

    return app;
}

ClientApp::ClientApp(boost::asio::io_context& io_context)
    : tcp_client_(TCPClient::create(io_context))
{
}

void ClientApp::connect(const std::string& host, uint16_t port,
                         std::function<void(bool)> on_connected)
{
    tcp_client_->connect(host, port, std::move(on_connected));
}

// ---------- Команди (CLI -> мережа) ----------

void ClientApp::send_connect(const std::string& username)
{
    tcp_client_->send(PacketBuilder::connect(username));
}

void ClientApp::send_create_room(const std::string& room_name)
{
    tcp_client_->send(PacketBuilder::create_room(room_name));
}

void ClientApp::send_join_room(uint16_t room_id)
{
    tcp_client_->send(PacketBuilder::join_room(room_id));
}

void ClientApp::send_leave_room()
{
    tcp_client_->send(PacketBuilder::leave_room());
}

void ClientApp::send_list_rooms()
{
    tcp_client_->send(PacketBuilder::list_rooms());
}

void ClientApp::send_ping()
{
    tcp_client_->send(PacketBuilder::ping());
}


void ClientApp::on_packet(PacketType type, const std::vector<uint8_t>& body)
{
    switch (type) {
        case PacketType::Connected:    on_connected(body);    break;
        case PacketType::RoomCreated:  on_room_created(body); break;
        case PacketType::RoomJoined:   on_room_joined(body);  break;
        case PacketType::RoomLeft:     on_room_left();        break;
        case PacketType::RoomList:     on_room_list(body);    break;
        case PacketType::UserJoined:   on_user_joined(body);  break;
        case PacketType::UserLeft:     on_user_left(body);    break;
        case PacketType::Pong:         on_pong();             break;
        case PacketType::Error:        on_error(body);        break;
        default:                       on_unhandled(type);    break;
    }
}

void ClientApp::on_disconnect()
{
    state_.connected = false;
    Logger::print("Disconnected from server");
}


void ClientApp::on_connected(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_connected(body);
    if (!parsed) {
        Logger::print("bad Connected body");
        return;
    }
    state_.client_id = parsed->client_id;
    state_.connected = true;
    Logger::print("Connected, client_id=" + std::to_string(parsed->client_id));
}

void ClientApp::on_room_created(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_room_created(body);
    if (!parsed) {
        Logger::print("bad RoomCreated body");
        return;
    }
    Logger::print("RoomCreated, room_id=" + std::to_string(parsed->room_id) +
                   " (use /join " + std::to_string(parsed->room_id) + " to enter)");
}

void ClientApp::on_room_joined(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_room_joined(body);
    if (!parsed) {
        Logger::print("bad RoomJoined body");
        return;
    }
    state_.current_room_id = parsed->header.room_id;

    std::string msg = "RoomJoined room_id=" + std::to_string(parsed->header.room_id) +
                       " track_position_ms=" + std::to_string(parsed->header.track_position_ms) +
                       " users(" + std::to_string(parsed->users.size()) + "):";
    for (const auto& user : parsed->users) {
        msg += "\n  id=" + std::to_string(user.client_id) + " name=" + user.username;
    }
    Logger::print(msg);
}

void ClientApp::on_room_left()
{
    state_.current_room_id = 0;
    Logger::print("RoomLeft");
}

void ClientApp::on_room_list(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_room_list(body);
    if (!parsed) {
        Logger::print("bad RoomList body");
        return;
    }
    std::string msg = "RoomList (" + std::to_string(parsed->rooms.size()) + "):";
    for (const auto& room : parsed->rooms) {
        msg += "\n  room_id=" + std::to_string(room.room_id) +
               " name=" + room.name +
               " users=" + std::to_string(room.user_count);
    }
    Logger::print(msg);
}

void ClientApp::on_user_joined(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_user_joined(body);
    if (!parsed) {
        Logger::print("bad UserJoined body");
        return;
    }
    Logger::print(std::string("UserJoined: ") + parsed->username +
                   " (id=" + std::to_string(parsed->client_id) + ")");
}

void ClientApp::on_user_left(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_user_left(body);
    if (!parsed) {
        Logger::print("bad UserLeft body");
        return;
    }
    Logger::print("UserLeft: id=" + std::to_string(parsed->client_id));
}

void ClientApp::on_pong()
{
    Logger::print("Pong received");
}

void ClientApp::on_error(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_error(body);
    if (!parsed) {
        Logger::print("bad Error body");
        return;
    }
    std::ostringstream oss;
    oss << "Server error, code=0x" << std::hex << static_cast<int>(parsed->error_code);
    Logger::print(oss.str());
}

void ClientApp::on_unhandled(PacketType type)
{
    std::ostringstream oss;
    oss << "Unhandled packet type: 0x" << std::hex << static_cast<int>(type);
    Logger::print(oss.str());
}
