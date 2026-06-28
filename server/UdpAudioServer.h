#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "Track.h"
#include "Mp3Decoder.h"
#include "Resampler.h"
#include "AudioEncoder.h"
#include "UdpSocket.h"
#include "MusicStreamer.h"
#include "RoomManager.h"

class UdpAudioServer {
public:
    explicit UdpAudioServer(unsigned short udp_port, RoomManager& room_manager);

    // Завантажує трек з файлу і повертає його id
    uint16_t loadTrack(const std::string& file_path);

    // Запускає стрімінг треку для кімнати
    void startStreaming(uint16_t room_id, uint16_t track_id);

    // Зупиняє стрімінг для кімнати
    void stopStreaming(uint16_t room_id);

    // Реєструє UDP endpoint клієнта
    void registerClient(uint32_t client_id, const udp::endpoint& endpoint);

    // Головний цикл отримання пакетів реєстрації
    void run();

private:
    void receiveLoop();

    RoomManager&    room_manager_;
    UdpSocket       udp_socket_;
    Mp3Decoder      decoder_;
    Resampler       resampler_;

    std::unordered_map<uint16_t, std::shared_ptr<Track>> tracks_;
    uint16_t next_track_id_ = 1;
    std::mutex tracks_mutex_;

    std::unordered_map<uint16_t, std::unique_ptr<MusicStreamer>> streamers_;
    std::mutex streamers_mutex_;
};