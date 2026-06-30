#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>

#include "Protocol.h"
#include "Track.h"
#include "Mp3Decoder.h"
#include "Resampler.h"
#include "AudioEncoder.h"
#include "UdpSocket.h"
#include "MusicStreamer.h"
#include "RoomManager.h"

struct PlaybackRoom {
    std::shared_ptr<Track> track;
    size_t current_packet_index = 0;
    uint32_t sequence_number = 0;
    std::chrono::steady_clock::time_point next_send_time;
};

class UdpAudioServer {
public:
    explicit UdpAudioServer(unsigned short udp_port, RoomManager& room_manager);

    void prepareTrack(int track_id);

    void startStreaming(uint16_t room_id);

    void stopStreaming();
    void stopRoomStreaming(uint16_t room_id);

    void registerClient(uint32_t client_id, const udp::endpoint& endpoint);

    void run();

private:
    void receiveLoop();
    void schedulerLoop();
    void loadDefaultTracks();

    RoomManager&    room_manager_;
    UdpSocket       udp_socket_;
    Mp3Decoder      decoder_;
    Resampler       resampler_;

    std::unordered_map<uint16_t, std::shared_ptr<Track>> tracks_;
    uint16_t next_track_id_ = 1;
    std::mutex tracks_mutex_;

    std::unordered_map<uint16_t, PlaybackRoom> active_rooms_;
    std::mutex active_rooms_mutex_;

    std::atomic<bool> running {true};
    std::thread scheduler_thread_;
};