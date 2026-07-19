#include "Server.h"
#include <filesystem>
#include <iostream>

Server::Server()
    : room_manager(user_manager)
    , tcp_server(io_context, room_manager, user_manager)
    , udp_server(UDP_SERVER_PORT, room_manager,
                 std::filesystem::path(__FILE__).parent_path().parent_path())
{
    room_manager.set_on_first_user_joined([this](uint16_t room_id) {
        udp_server.startStreaming(room_id);
    });

    room_manager.set_on_track_added([this](const std::string& track_path) {
        return udp_server.addTrack(track_path);
    });

    room_manager.set_list_tracks_fn([this]() -> std::vector<TrackListEntry> {
        // TODO: UDP team — реалізувати отримання списку треків з UdpAudioServer
        return {};
    });

    room_manager.set_track_exists_fn([this](uint16_t track_id) -> bool {
        // TODO: UDP team — перевірка чи трек існує в UdpAudioServer::tracks_
        return false;
    });

    room_manager.set_on_voice_start([this](uint32_t client_id, uint16_t room_id) {
        udp_server.onVoiceStart(client_id, room_id);
    });

    room_manager.set_on_voice_stop([this](uint32_t client_id, uint16_t room_id) {
        udp_server.onVoiceStop(client_id, room_id);
    });
}

void Server::run() {
    std::cout << "Server running...\n";

    std::thread tcp_thread([this] {
        io_context.run();
        });

    std::thread udp_thread([this] {
        udp_server.run();
        });

    tcp_thread.join();
    udp_thread.join();
}