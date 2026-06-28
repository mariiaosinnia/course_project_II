#define DR_MP3_IMPLEMENTATION
#include "UdpAudioServer.h"
#include <iostream>
#include <stdexcept>

UdpAudioServer::UdpAudioServer(unsigned short udp_port, RoomManager& room_manager)
    : room_manager_(room_manager),
      udp_socket_(udp_port)
{}

uint16_t UdpAudioServer::loadTrack(const std::string& file_path) {
    // Декодуємо MP3
    auto track = decoder_.decode(file_path);

    // Ресемплюємо якщо треба
    if (!AudioEncoder::isSupportedRate(track->sample_rate)) {
        resampler_.resample(track, AudioEncoder::TARGET_RATE);
    }

    std::lock_guard<std::mutex> lock(tracks_mutex_);
    uint16_t id = next_track_id_++;
    track->id = id;
    tracks_[id] = track;

    std::cout << "Track loaded: " << file_path
              << " | id=" << id
              << " | rate=" << track->sample_rate
              << " | channels=" << track->channels << "\n";

    return id;
}

void UdpAudioServer::startStreaming(uint16_t room_id, uint16_t track_id) {
    std::shared_ptr<Track> track;

    {
        std::lock_guard<std::mutex> lock(tracks_mutex_);
        auto it = tracks_.find(track_id);
        if (it == tracks_.end()) {
            throw std::runtime_error("Track not found: " + std::to_string(track_id));
        }
        track = it->second;
    }

    std::lock_guard<std::mutex> lock(streamers_mutex_);

    // Зупиняємо попередній стрімер якщо є
    auto existing = streamers_.find(room_id);
    if (existing != streamers_.end()) {
        existing->second->stop();
        streamers_.erase(existing);
    }

    // Створюємо новий стрімер для кімнати
    auto streamer = std::make_unique<MusicStreamer>(
        track, room_manager_, room_id, udp_socket_);

    streamer->start();
    streamers_[room_id] = std::move(streamer);

    std::cout << "Streaming started for room " << room_id
              << " track " << track_id << "\n";
}

void UdpAudioServer::stopStreaming(uint16_t room_id) {
    std::lock_guard<std::mutex> lock(streamers_mutex_);

    auto it = streamers_.find(room_id);
    if (it == streamers_.end()) return;

    it->second->stop();
    streamers_.erase(it);

    std::cout << "Streaming stopped for room " << room_id << "\n";
}

void UdpAudioServer::registerClient(uint32_t client_id, const udp::endpoint& endpoint) {
    room_manager_.registerUdpEndpoint(client_id, endpoint);
    std::cout << "UDP registered: client_id=" << client_id
              << " endpoint=" << endpoint.address().to_string()
              << ":" << endpoint.port() << "\n";
}

void UdpAudioServer::run() {
    std::cout << "UDP AudioServer listening...\n";
    receiveLoop();
}

void UdpAudioServer::receiveLoop() {
    std::vector<uint8_t> buffer(16);

    while (true) {
        udp::endpoint sender;
        size_t bytes = udp_socket_.receiveFrom(buffer, sender);

        if (bytes == 0) continue;

        // Пакет реєстрації: 1 байт 0xFF = ping
        if (bytes == 1 && buffer[0] == 0xFF) {
            std::cout << "Ping from "
                      << sender.address().to_string()
                      << ":" << sender.port() << "\n";
            continue;
        }

        // Пакет реєстрації: 4 байти = client_id
        if (bytes == 4) {
            uint32_t client_id =
                (static_cast<uint32_t>(buffer[0]) << 24) |
                (static_cast<uint32_t>(buffer[1]) << 16) |
                (static_cast<uint32_t>(buffer[2]) << 8)  |
                (static_cast<uint32_t>(buffer[3]));

            registerClient(client_id, sender);
        }
    }
}