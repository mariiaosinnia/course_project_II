#include "Executor.h"
#include "RoomManager.h"
#include "PacketBuilder.h"

#include <iostream>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

Executor::Executor(RoomManager& rm)
    : room_manager(rm)
{
}

void Executor::cleanup() {
    upload_state_.reset();
}

std::vector<uint8_t> Executor::execute(User& user, PacketType type,
    const std::vector<uint8_t>& body) {
    switch (type) {
        case PacketType::Ping: return handle_ping();
        case PacketType::Connect: return handle_connect(user, body);
        case PacketType::CreateRoom: return handle_create_room(user, body);
        case PacketType::JoinRoom: return handle_join_room(user, body);
        case PacketType::LeaveRoom: return handle_leave_room(user);
        case PacketType::ListRooms: return handle_list_rooms();
        case PacketType::UploadTrackBegin: return handle_upload_begin(user, body);
        case PacketType::UploadTrackData: return handle_upload_data(user, body);
        case PacketType::UploadTrackEnd: return handle_upload_end(user);
        default: return PacketBuilder::error(StatusCode::Unknown);
    }
}

std::vector<uint8_t> Executor::handle_ping() {
    return PacketBuilder::pong();
}

std::vector<uint8_t> Executor::handle_connect(User& user, const std::vector<uint8_t>& body) {
    std::optional<ConnectBody> parsed = PacketParser::parse_connect(body);
    if (!parsed) {
        return PacketBuilder::error(StatusCode::Unknown);
    }
    user.name = std::string(parsed->username, strnlen(parsed->username, USERNAME_MAX_LEN));
    return PacketBuilder::connected(user.id);
}

std::vector<uint8_t> Executor::handle_create_room(User& user, const std::vector<uint8_t>& body) {
    std::optional<CreateRoomBody> parsed = PacketParser::parse_create_room(body);
    if (!parsed) {
        return PacketBuilder::error(StatusCode::Unknown);
    }
    std::string name(parsed->name, strnlen(parsed->name, ROOM_NAME_MAX_LEN));
    uint16_t room_id = room_manager.create_room(name);
    return PacketBuilder::room_created(room_id);
}

std::vector<uint8_t> Executor::handle_join_room(User& user, const std::vector<uint8_t>& body) {
    std::optional<JoinRoomBody> parsed = PacketParser::parse_join_room(body);
    if (!parsed) {
        return PacketBuilder::error(StatusCode::Unknown);
    }

    StatusCode status = room_manager.join_room(user, parsed->room_id);
    if (status != StatusCode::Success) {
        return PacketBuilder::error(status);
    }

    std::vector<UserInfo> users_in_room = room_manager.get_users_in_room(parsed->room_id);

    RoomJoinedHeader rj_header;
    rj_header.room_id = boost::endian::native_to_big(parsed->room_id);
    rj_header.udp_port = boost::endian::native_to_big(static_cast<uint16_t>(UDP_SERVER_PORT));
    rj_header.track_id = boost::endian::native_to_big(room_manager.get_current_track_id(parsed->room_id));
    rj_header.track_position_ms = boost::endian::native_to_big(room_manager.get_current_position(parsed->room_id));
    rj_header.user_count = static_cast<uint8_t>(users_in_room.size());

    std::vector<uint8_t> packet = PacketBuilder::user_joined(user.id, user.name.c_str());
    room_manager.broadcast_to_room(parsed->room_id, packet, user);

    return PacketBuilder::room_joined(rj_header, users_in_room);
}

std::vector<uint8_t> Executor::handle_leave_room(User& user) {
    if (user.room_id == 0) {
        return PacketBuilder::error(StatusCode::NotInRoom);
    }

    uint16_t room_id = user.room_id;
    StatusCode status = room_manager.leave_room(user, room_id);
    if (status != StatusCode::Success) return PacketBuilder::error(status);

    std::vector<uint8_t>
        packet = PacketBuilder::user_left(user.id);
    room_manager.broadcast_to_room(room_id, packet, user);

    return PacketBuilder::room_left();
}

std::vector<uint8_t> Executor::handle_list_rooms() {
    std::vector<RoomListEntry> rooms = room_manager.list_rooms();
    return PacketBuilder::room_list(rooms);
}
std::vector<uint8_t> Executor::handle_upload_begin(User& user, const std::vector<uint8_t>& body) {
    if (user.room_id == 0) {
        return PacketBuilder::error(StatusCode::NotInRoom);
    }

    if (upload_state_.has_value()) {
        return PacketBuilder::error(StatusCode::UploadAlreadyInProgress);
    }

    std::optional<UploadTrackBeginBody> parsed = PacketParser::parse_upload_begin(body);
    if (!parsed) {
        return PacketBuilder::error(StatusCode::Unknown);
    }

ї    if (parsed->room_id != user.room_id) {
        return PacketBuilder::error(StatusCode::NotInRoom);
    }

    if (parsed->file_size == 0 || parsed->file_size > MAX_UPLOAD_SIZE) {
        return PacketBuilder::error(StatusCode::FileTooLarge);
    }

    std::string filename(parsed->filename, strnlen(parsed->filename, FILENAME_MAX_LEN));
    if (filename.empty()) {
        return PacketBuilder::error(StatusCode::Unknown);
    }

    std::string dir = "uploads/room_" + std::to_string(parsed->room_id);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        std::cerr << "Failed to create upload dir " << dir << ": " << ec.message() << "\n";
        return PacketBuilder::error(StatusCode::UploadFailed);
    }

    auto now = std::chrono::system_clock::now();
    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    std::string local_name = std::to_string(user.id) + "_"
        + std::to_string(epoch_ms) + "_" + filename;
    std::string local_path = dir + "/" + local_name;

    UploadState state;
    state.room_id = parsed->room_id;
    state.filename = filename;
    state.local_path = local_path;
    state.expected_size = parsed->file_size;
    state.bytes_received = 0;
    state.file_stream.open(local_path, std::ios::binary);

    if (!state.file_stream.is_open()) {
        std::cerr << "Failed to open file for upload: " << local_path << "\n";
        return PacketBuilder::error(StatusCode::UploadFailed);
    }

    upload_state_ = std::move(state);

    std::cout << "Upload started: user=" << user.id
              << " file=" << filename
              << " size=" << parsed->file_size
              << " -> " << local_path << "\n";

    return {};
}

std::vector<uint8_t> Executor::handle_upload_data(User& user, const std::vector<uint8_t>& body) {
    if (!upload_state_.has_value()) {
        return PacketBuilder::error(StatusCode::NoUploadInProgress);
    }

    auto& state = *upload_state_;

    if (state.bytes_received + body.size() > state.expected_size) {
        std::cerr << "Upload overflow: received " << state.bytes_received + body.size()
                  << " > expected " << state.expected_size << "\n";
        upload_state_.reset();
        return PacketBuilder::error(StatusCode::UploadFailed);
    }

    state.file_stream.write(reinterpret_cast<const char*>(body.data()), body.size());
    if (!state.file_stream.good()) {
        std::cerr << "Write error during upload to " << state.local_path << "\n";
        upload_state_.reset();
        return PacketBuilder::error(StatusCode::UploadFailed);
    }

    state.bytes_received += static_cast<uint32_t>(body.size());

    return {};
}

std::vector<uint8_t> Executor::handle_upload_end(User& user) {
    if (!upload_state_.has_value()) {
        return PacketBuilder::error(StatusCode::NoUploadInProgress);
    }

    auto& state = *upload_state_;

    if (state.bytes_received != state.expected_size) {
        std::cerr << "Upload size mismatch: got " << state.bytes_received
                  << ", expected " << state.expected_size << "\n";
        upload_state_.reset();
        return PacketBuilder::error(StatusCode::UploadFailed);
    }

    state.file_stream.close();

    uint16_t track_id = room_manager.add_track_to_room(state.room_id, state.local_path);
    if (track_id == 0) {
        std::cerr << "Failed to register uploaded track: " << state.local_path << "\n";
        upload_state_.reset();
        return PacketBuilder::error(StatusCode::UploadFailed);
    }

    state.completed = true;

    uint16_t room_id = state.room_id;
    std::string filename = state.filename;

    upload_state_.reset();

    std::cout << "Upload complete: track_id=" << track_id
              << " room=" << room_id
              << " file=" << filename << "\n";

    std::vector<uint8_t> packet = PacketBuilder::track_added(room_id, track_id, filename.c_str());
    room_manager.broadcast_to_room(room_id, packet, user);
    return packet;
}