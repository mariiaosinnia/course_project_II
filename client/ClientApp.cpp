#include "ClientApp.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "Logger.h"
#include "PacketBuilder.h"
#include "PacketParser.h"
#include "UDPClient.h"

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
    : io_context_(io_context)
    , tcp_client_(TCPClient::create(io_context))
{
}

namespace {
    std::string trim_path(std::string path)
    {
        auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
        path.erase(path.begin(), std::find_if(path.begin(), path.end(),
            [&](char ch) { return !is_space(static_cast<unsigned char>(ch)); }));
        path.erase(std::find_if(path.rbegin(), path.rend(),
            [&](char ch) { return !is_space(static_cast<unsigned char>(ch)); }).base(), path.end());

        if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.size() - 2);
        }
        return path;
    }
}

void ClientApp::set_upload_status(const std::string& status, bool is_error)
{
    boost::asio::post(io_context_, [this, status, is_error] {
        if (on_upload_status_cb_) {
            on_upload_status_cb_(status, is_error);
        }
    });
}

void ClientApp::finish_upload_status(const std::string& status, bool is_error)
{
    {
        std::lock_guard<std::mutex> lock(upload_mutex_);
        pending_upload_filename_.clear();
    }
    state_.upload_in_progress = false;
    set_upload_status(status, is_error);
}

void ClientApp::set_pending_upload_filename(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(upload_mutex_);
    pending_upload_filename_ = filename;
}

bool ClientApp::consume_pending_upload_if_matches(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(upload_mutex_);
    if (pending_upload_filename_ != filename) {
        return false;
    }

    pending_upload_filename_.clear();
    return true;
}

void ClientApp::connect(const std::string& host, uint16_t port,
                         std::function<void(bool)> on_connected)
{
    server_host_ = host;
    tcp_client_->connect(host, port, std::move(on_connected));
}


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

void ClientApp::send_list_tracks()
{
    tcp_client_->send(PacketBuilder::list_tracks());
}

void ClientApp::send_track_select(uint16_t track_id)
{
    tcp_client_->send(PacketBuilder::track_select(track_id));
}

bool ClientApp::send_voice_start()
{
    if (state_.current_room_id.load() == 0 || !udp_client_) {
        Logger::print("cannot start voice: join a room first");
        return false;
    }

    if (udp_client_->is_voice_capturing()) {
        return true;
    }

    if (!udp_client_->start_voice_capture()) {
        Logger::print("failed to start voice capture");
        return false;
    }

    tcp_client_->send(PacketBuilder::voice_start());
    return true;
}

void ClientApp::send_voice_stop()
{
    if (!udp_client_ || !udp_client_->is_voice_capturing()) {
        return;
    }

    udp_client_->stop_voice_capture();
    tcp_client_->send(PacketBuilder::voice_stop());
}

bool ClientApp::toggle_voice()
{
    if (is_voice_active()) {
        send_voice_stop();
        return false;
    }
    return send_voice_start();
}

bool ClientApp::is_voice_active() const
{
    return udp_client_ && udp_client_->is_voice_capturing();
}

void ClientApp::send_ping()
{
    tcp_client_->send(PacketBuilder::ping());
}

void ClientApp::send_upload_track(const std::string& path)
{
    uint16_t room_id = state_.current_room_id.load();
    if (room_id == 0) {
        set_upload_status("join a room before uploading", true);
        return;
    }

    bool expected = false;
    if (!state_.upload_in_progress.compare_exchange_strong(expected, true)) {
        set_upload_status("upload already in progress", true);
        return;
    }

    auto tcp = tcp_client_;
    std::weak_ptr<ClientApp> self = shared_from_this();

    std::thread([self, tcp, room_id, path = trim_path(path)] {
        namespace fs = std::filesystem;
        constexpr size_t chunk_size = 60 * 1024;

        auto finish = [&self](const std::string& status, bool is_error) {
            if (auto app = self.lock()) {
                app->finish_upload_status(status, is_error);
            }
        };
        auto report = [&self](const std::string& status, bool is_error) {
            if (auto app = self.lock()) {
                app->set_upload_status(status, is_error);
            }
        };

        if (path.empty()) {
            finish("choose a file first", true);
            return;
        }

        std::error_code ec;
        fs::path file_path(path);
        if (!fs::is_regular_file(file_path, ec)) {
            finish(ec ? "cannot access file: " + ec.message() : "path is not a file", true);
            return;
        }

        uintmax_t size = fs::file_size(file_path, ec);
        if (ec) {
            finish("cannot read file size: " + ec.message(), true);
            return;
        }
        if (size == 0 || size > MAX_UPLOAD_SIZE) {
            finish("file must be 1 byte to 50 MB", true);
            return;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            finish("cannot open file", true);
            return;
        }

        std::string filename = file_path.filename().string();
        if (filename.empty()) {
            finish("file name is empty", true);
            return;
        }
        if (filename.size() >= FILENAME_MAX_LEN) {
            finish("file name is too long", true);
            return;
        }

        if (auto app = self.lock()) {
            app->set_pending_upload_filename(filename);
        }

        report("uploading " + filename + "...", false);
        tcp->send(PacketBuilder::upload_track_begin(
            room_id,
            static_cast<uint32_t>(size),
            filename));

        std::array<char, chunk_size> buffer{};
        while (file.good()) {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            std::streamsize read = file.gcount();
            if (read <= 0) {
                break;
            }

            std::vector<uint8_t> chunk(
                reinterpret_cast<uint8_t*>(buffer.data()),
                reinterpret_cast<uint8_t*>(buffer.data()) + read);
            tcp->send(PacketBuilder::upload_track_data(chunk));
        }

        if (file.bad()) {
            finish("file read failed", true);
            return;
        }

        tcp->send(PacketBuilder::upload_track_end());
        report("upload sent, waiting for server...", false);
    }).detach();
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
        case PacketType::TrackAdded:   on_track_added(body);  break;
        case PacketType::TrackList:    on_track_list(body);   break;
        case PacketType::VoiceStarted: on_voice_started(body); break;
        case PacketType::VoiceStopped: on_voice_stopped(body); break;
        case PacketType::Pong:         on_pong();             break;
        case PacketType::Error:        on_error(body);        break;
        default:                       on_unhandled(type);    break;
    }
}

void ClientApp::on_disconnect()
{
    state_.connected = false;
    state_.current_room_id = 0;
    state_.upload_in_progress = false;
    if (udp_client_) {
        udp_client_->stop_voice_capture();
        udp_client_->stop();
        udp_client_.reset();
    }
    Logger::print("Disconnected from server");
    if (on_disconnected_cb_) on_disconnected_cb_();
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

    if (on_connected_cb_) on_connected_cb_(parsed->client_id);
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

    if (on_room_created_cb_) on_room_created_cb_(parsed->room_id);
}

void ClientApp::on_room_joined(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_room_joined(body);
    if (!parsed) {
        Logger::print("bad RoomJoined body");
        return;
    }
    state_.current_room_id = parsed->header.room_id;
    state_.upload_in_progress = false;

    std::string msg = "RoomJoined room_id=" + std::to_string(parsed->header.room_id) +
                       " track_position_ms=" + std::to_string(parsed->header.track_position_ms) +
                       " users(" + std::to_string(parsed->users.size()) + "):";
    for (const auto& user : parsed->users) {
        msg += "\n  id=" + std::to_string(user.client_id) + " name=" + user.username;
    }
    Logger::print(msg);

    start_udp(parsed->header.udp_port);

    if (on_room_joined_cb_) on_room_joined_cb_(parsed->header.room_id);

    for (const auto& user : parsed->users) {
        if (on_user_joined_cb_) {
            on_user_joined_cb_(user.client_id, std::string(user.username));
        }
    }

    send_list_tracks();
}

void ClientApp::start_udp(uint16_t udp_port)
{
    if (state_.client_id == 0) {
        Logger::print("cannot start UDP: client_id unknown");
        return;
    }

    if (udp_client_) {
        udp_client_->stop();
        udp_client_.reset();
    }

    udp_client_ = std::make_shared<UdpClient>(
        io_context_,
        server_host_,
        udp_port,
        state_.client_id.load()
    );

    if (!udp_client_->start()) {
        Logger::print("Failed to start UDP client");
        udp_client_.reset();
    }
}

void ClientApp::on_room_left()
{
    state_.current_room_id = 0;
    state_.upload_in_progress = false;
    if (udp_client_) {
        udp_client_->stop_voice_capture();
        udp_client_->stop();
        udp_client_.reset();
    }
    {
        std::lock_guard<std::mutex> lock(state_.track_list_mutex);
        state_.track_list_cache.clear();
    }
    Logger::print("RoomLeft");
    set_upload_status("", false);
    if (on_room_left_cb_) on_room_left_cb_();
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

    {
        std::lock_guard<std::mutex> lock(state_.room_list_mutex);
        state_.room_list_cache.clear();
        state_.room_list_cache.reserve(parsed->rooms.size());
        for (const auto& room : parsed->rooms) {
            state_.room_list_cache.push_back(room);
        }
    }

    if (on_room_list_updated_cb_) on_room_list_updated_cb_();
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
    if (on_user_joined_cb_) {
        on_user_joined_cb_(parsed->client_id, std::string(parsed->username));
    }
}

void ClientApp::on_user_left(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_user_left(body);
    if (!parsed) {
        Logger::print("bad UserLeft body");
        return;
    }
    Logger::print("UserLeft: id=" + std::to_string(parsed->client_id));
    if (on_user_left_cb_) {
        on_user_left_cb_(parsed->client_id);
    }
}

void ClientApp::on_track_added(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_track_added(body);
    if (!parsed) {
        Logger::print("bad TrackAdded body");
        return;
    }

    std::string filename(parsed->filename);
    Logger::print("TrackAdded: " + filename +
                  " (id=" + std::to_string(parsed->track_id) + ")");

    if (consume_pending_upload_if_matches(filename)) {
        state_.upload_in_progress = false;
        if (on_upload_status_cb_) {
            on_upload_status_cb_("track added: " + filename, false);
        }
    }

    if (on_track_added_cb_) {
        on_track_added_cb_(parsed->room_id, parsed->track_id, filename);
    }

    send_list_tracks();
}

void ClientApp::on_track_list(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_track_list(body);
    if (!parsed) {
        Logger::print("bad TrackList body");
        return;
    }

    std::string msg = "TrackList (" + std::to_string(parsed->tracks.size()) + "):";
    for (const auto& track : parsed->tracks) {
        msg += "\n  track_id=" + std::to_string(track.track_id) +
               " filename=" + track.filename;
    }
    Logger::print(msg);

    {
        std::lock_guard<std::mutex> lock(state_.track_list_mutex);
        state_.track_list_cache = parsed->tracks;
    }

    if (on_track_list_updated_cb_) on_track_list_updated_cb_();
}

void ClientApp::on_voice_started(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_voice_started(body);
    if (!parsed) {
        Logger::print("bad VoiceStarted body");
        return;
    }

    Logger::print("VoiceStarted: id=" + std::to_string(parsed->client_id));
    if (on_voice_started_cb_) on_voice_started_cb_(parsed->client_id);
}

void ClientApp::on_voice_stopped(const std::vector<uint8_t>& body)
{
    auto parsed = PacketParser::parse_voice_stopped(body);
    if (!parsed) {
        Logger::print("bad VoiceStopped body");
        return;
    }

    Logger::print("VoiceStopped: id=" + std::to_string(parsed->client_id));
    if (on_voice_stopped_cb_) on_voice_stopped_cb_(parsed->client_id);
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

    StatusCode code = static_cast<StatusCode>(parsed->error_code);
    if (code == StatusCode::UploadFailed ||
        code == StatusCode::FileTooLarge ||
        code == StatusCode::NoUploadInProgress ||
        code == StatusCode::UploadAlreadyInProgress ||
        code == StatusCode::NotInRoom) {
        finish_upload_status(status_to_string(code), true);
    }
}

void ClientApp::on_unhandled(PacketType type)
{
    std::ostringstream oss;
    oss << "Unhandled packet type: 0x" << std::hex << static_cast<int>(type);
    Logger::print(oss.str());
}
void ClientApp::set_server_address(const std::string& host, uint16_t port)
{
    server_host_ = host;
    server_port_ = port;
    }

void ClientApp::connect_to_server(std::function<void(bool)> on_connected)
{
    connect(server_host_, server_port_, std::move(on_connected));
    }

std::string ClientApp::status_to_string(StatusCode code)
{
    switch (code) {
        case StatusCode::NotInRoom:
            return "join a room before uploading";
        case StatusCode::UploadFailed:
            return "upload failed on server";
        case StatusCode::FileTooLarge:
            return "file is too large";
        case StatusCode::NoUploadInProgress:
            return "server has no upload in progress";
        case StatusCode::UploadAlreadyInProgress:
            return "server upload already in progress";
        default:
            return "server error";
    }
}
