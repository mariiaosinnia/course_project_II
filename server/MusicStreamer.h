#pragma once
#include <cstdint>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

#include <boost/asio.hpp>
#include "UdpSocket.h"
#include "Track.h"
#include "AudioEncoder.h"
#include "RoomManager.h"

using boost::asio::ip::udp;

class MusicStreamer {
public:
    MusicStreamer(std::shared_ptr<Track> track,
                 RoomManager& room_manager,
                 uint16_t room_id,
                 UdpSocket& udp_socket);
    ~MusicStreamer();

    void start();
    void stop();
    bool isRunning() const;
    uint32_t getCurrentPositionMs() const;

private:
    void streamLoop();

    std::vector<uint8_t> buildPacket(
        uint32_t sequence_number,
        const std::vector<uint8_t>& opus_data) const;

    std::shared_ptr<Track>  track_;
    RoomManager&            room_manager_;
    uint16_t                room_id_;
    UdpSocket& udp_socket_;
    AudioEncoder            encoder_;

    std::thread             stream_thread_;
    std::atomic<bool>       running_;

    uint64_t                current_frame_;
    uint32_t                sequence_number_;
    int                     samples_per_frame_;
};