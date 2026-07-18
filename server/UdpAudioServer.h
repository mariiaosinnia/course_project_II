#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <filesystem>

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
    UdpAudioServer(unsigned short udp_port, RoomManager& room_manager,
                   std::filesystem::path resource_dir);

    uint16_t addTrack(const std::string& file_path);

    void startStreaming(uint16_t room_id);

    void stopStreaming();
    void stopRoomStreaming(uint16_t room_id);

    void registerClient(uint32_t client_id, const udp::endpoint& endpoint);

    void run();
    std::vector<TrackListEntry> getTrackList() const;
    bool isTrackExists(uint16_t track_id) const;


private:
    void receiveLoop();
    void schedulerLoop();
    void loadDefaultTracks();
    std::shared_ptr<Track> prepareTrack(const std::string& file_path);

    RoomManager&    room_manager_;
    UdpSocket       udp_socket_;
    Mp3Decoder      decoder_;
    Resampler       resampler_;

    std::unordered_map<uint16_t, std::shared_ptr<Track>> tracks_;
    uint16_t next_track_id_ = 1;
    std::mutex tracks_mutex_;

    std::unordered_map<uint16_t, PlaybackRoom> active_rooms_;
    std::mutex active_rooms_mutex_;

    std::filesystem::path resource_dir_;
    std::atomic<bool> running {true};
    std::thread scheduler_thread_;
};