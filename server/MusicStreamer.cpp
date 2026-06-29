#include "MusicStreamer.h"
#include <chrono>
#include <cstring>

MusicStreamer::MusicStreamer(std::shared_ptr<Track> track,
                             RoomManager& room_manager,
                             uint16_t room_id,
                             UdpSocket& udp_socket)
    : track_(track),
      room_manager_(room_manager),
      room_id_(room_id),
      udp_socket_(udp_socket),
      encoder_(track->sample_rate, track->channels),
      running_(false),
      current_frame_(0),
      sequence_number_(0)
{
    samples_per_frame_ = track_->sample_rate * AudioEncoder::FRAME_DURATION_MS / 1000;
}

MusicStreamer::~MusicStreamer() {
    stop();
}

void MusicStreamer::start() {
    if (running_) return;
    running_ = true;
    stream_thread_ = std::thread(&MusicStreamer::streamLoop, this);
}

void MusicStreamer::stop() {
    if (!running_) return;
    running_ = false;
    if (stream_thread_.joinable()) {
        stream_thread_.join();
    }
}

bool MusicStreamer::isRunning() const {
    return running_;
}

uint32_t MusicStreamer::getCurrentPositionMs() const {
    return static_cast<uint32_t>(
        (current_frame_ * 1000) / track_->sample_rate
    );
}

void MusicStreamer::streamLoop() {
    auto stream_start_time = std::chrono::steady_clock::now();

    while (running_ &&
           current_frame_ + samples_per_frame_ <= track_->total_frame_count)
    {
        const int16_t* frame_start =
            track_->pcm_data.data() + (current_frame_ * track_->channels);

        auto opus_data = encoder_.encodeFrame(frame_start, samples_per_frame_);
        auto packet    = buildPacket(sequence_number_, opus_data);

        // Питаємо RoomManager хто зараз в кімнаті
        auto endpoints = room_manager_.get_udp_endpoints(room_id_);

        // Самі відправляємо кожному
        for (const auto& endpoint : endpoints) {
            udp_socket_.sendTo(packet, endpoint);
        }

        current_frame_  += samples_per_frame_;
        sequence_number_++;

        auto target_time = stream_start_time +
            std::chrono::milliseconds(
                static_cast<long long>(sequence_number_) *
                AudioEncoder::FRAME_DURATION_MS
            );
        std::this_thread::sleep_until(target_time);
    }

    running_ = false;
}

std::vector<uint8_t> MusicStreamer::buildPacket(
    uint32_t sequence_number,
    const std::vector<uint8_t>& opus_data) const
{
    std::vector<uint8_t> packet(4 + opus_data.size());

    packet[0] = static_cast<uint8_t>((sequence_number >> 24) & 0xFF);
    packet[1] = static_cast<uint8_t>((sequence_number >> 16) & 0xFF);
    packet[2] = static_cast<uint8_t>((sequence_number >> 8)  & 0xFF);
    packet[3] = static_cast<uint8_t>( sequence_number        & 0xFF);

    std::memcpy(packet.data() + 4, opus_data.data(), opus_data.size());

    return packet;
}