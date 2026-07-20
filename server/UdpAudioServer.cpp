#include "UdpAudioServer.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cstring>



UdpAudioServer::UdpAudioServer(unsigned short udp_port, RoomManager& room_manager,
                                std::filesystem::path resource_dir)
    : room_manager_(room_manager)
    , udp_socket_(udp_port)
    , resource_dir_(std::move(resource_dir))
{
}

uint16_t UdpAudioServer::addTrack(const std::string& file_path) {
    auto track = prepareTrack(file_path);
    if (!track) return 0;

    std::lock_guard<std::mutex> lock(tracks_mutex_);
    uint16_t id = next_track_id_++;
    track->id = id;
    tracks_[id] = track;
    return id;
}

std::shared_ptr<Track> UdpAudioServer::prepareTrack(const std::string& file_path) {
    auto track = std::make_shared<Track>();
    track->path = file_path;

    decoder_.decode(*track);

    if (!AudioEncoder::isSupportedRate(track->sample_rate)) {
        resampler_.resample(track, TARGET_RATE);
    }

    AudioEncoder encoder(track->sample_rate, track->channels);
    int samples_per_frame = (track->sample_rate * FRAME_DURATION_MS) / 1000;
    size_t total_samples = track->pcm_data.size() / track->channels;
    size_t total_frames = total_samples / samples_per_frame;

    track->opus_packets.reserve(total_frames);
    for (size_t i = 0; i < total_frames; ++i) {
        const int16_t* frame_start = track->pcm_data.data() +
                                      (i * samples_per_frame * track->channels);
        track->opus_packets.push_back(encoder.encodeFrame(frame_start, samples_per_frame));
    }

    track->pcm_data.clear();
    track->pcm_data.shrink_to_fit();

    std::cout << "Track loaded: " << file_path
              << " | rate=" << track->sample_rate
              << " | channels=" << track->channels << "\n";

    return track;
}

struct RoomStreamState {
    std::unique_ptr<MusicStreamer> streamer;
    std::chrono::steady_clock::time_point next_send_time;
};

void UdpAudioServer::schedulerLoop() {
    while (running) {
        auto now = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(active_rooms_mutex_);
            for (auto it = active_rooms_.begin(); it != active_rooms_.end(); ) {
                auto& state = it->second;

                if (now >= state.next_send_time) {

                    // 1. Перевіряємо чи закінчився трек
                    if (state.current_packet_index >= state.track->opus_packets.size()) {

                        // Перемикаємо на наступний трек і передаємо поточний `now`,
                        // який ми захопили на початку ітерації циклу!
                        uint16_t next_track_id = room_manager_.advance_track(it->first, now);

                        if (next_track_id == 0) {
                            it = active_rooms_.erase(it);
                            continue;
                        }

                        std::lock_guard<std::mutex> track_lock(tracks_mutex_);
                        auto next_it = tracks_.find(next_track_id);

                        if (next_it != tracks_.end()) {
                            state.track = next_it->second;
                            state.current_packet_index = 0;
                            // Базова прив'язка часу для нового треку теж стає `now`
                            state.next_send_time = now;

                            std::cout << "Room " << it->first << " switched to track " << next_track_id << "\n";
                        } else {
                            std::cerr << "Error: Next track " << next_track_id << " not loaded.\n";
                            it = active_rooms_.erase(it);
                            continue;
                        }
                    }

                    // 2. Якщо трек ще грає (або ми щойно його перемкнули), беремо пакет
                    const auto& opus_data = state.track->opus_packets[state.current_packet_index];
                    // додаю час треку всередині пакету для drift correction
                    uint32_t track_position_ms = state.current_packet_index * FRAME_DURATION_MS;
                    auto packet = MusicStreamer::buildPacket(state.sequence_number, opus_data, track_position_ms);

                    // 3. Розсилаємо
                    auto endpoints = room_manager_.get_udp_endpoints(it->first);
                    for (const auto& endpoint : endpoints) {
                        udp_socket_.sendTo(packet, endpoint);
                    }

                    // 4. Плануємо наступний кадр
                    state.current_packet_index++;
                    state.sequence_number++;
                    state.next_send_time += std::chrono::milliseconds(FRAME_DURATION_MS);
                }
                ++it;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void UdpAudioServer::stopStreaming() {
    running = false;
}

void UdpAudioServer::startStreaming(uint16_t room_id) {
    uint16_t track_id = room_manager_.get_current_track_id(room_id);
    if (track_id == 0) {
        std::cerr << "Cannot start streaming: room " << room_id << " has no tracks.\n";
        return;
    }

    std::shared_ptr<Track> track;
    {
        std::lock_guard<std::mutex> lock(tracks_mutex_);
        auto it = tracks_.find(track_id);
        if (it == tracks_.end()) return;
        track = it->second;
    }

    auto start_time = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(active_rooms_mutex_);

    PlaybackRoom state;
    state.track = track;
    state.current_packet_index = 0;
    state.sequence_number = 0;
    state.next_send_time = start_time;

    active_rooms_[room_id] = state;

    room_manager_.start_playback(room_id, start_time);

    std::cout << "Streaming started for room " << room_id << " with track " << track_id << "\n";
}

void UdpAudioServer::registerClient(uint32_t client_id, const udp::endpoint& endpoint) {
    room_manager_.registerUdpEndpoint(client_id, endpoint);
    std::cout << "UDP registered: client_id=" << client_id
              << " endpoint=" << endpoint.address().to_string()
              << ":" << endpoint.port() << "\n";
}

void UdpAudioServer::run() {
    std::cout << "UDP AudioServer listening...\n";
    std::thread scheduler(&UdpAudioServer::schedulerLoop, this);
    scheduler.detach();
    receiveLoop();
}

void UdpAudioServer::receiveLoop() {
    std::vector<uint8_t> buffer(16);

    while (running) {
        udp::endpoint sender;
        size_t bytes = udp_socket_.receiveFrom(buffer, sender);

        if (bytes == 0) continue;

        if (bytes == 5 && buffer[0] == 0xFF) {
            uint32_t client_id =
                (static_cast<uint32_t>(buffer[1]) << 24) |
                (static_cast<uint32_t>(buffer[2]) << 16) |
                (static_cast<uint32_t>(buffer[3]) << 8)  |
                (static_cast<uint32_t>(buffer[4]));

            registerClient(client_id, sender);
        }
    }
}

std::vector<TrackListEntry> UdpAudioServer::getTrackList() const {
    std::vector<TrackListEntry> result;
    for (const auto& [id, track] : tracks_) {
        TrackListEntry element{};
        element.track_id = id;
        std::strncpy(element.filename, track->name.c_str(), sizeof(element.filename) - 1);
        result.push_back(element);
    }
    return result;
}

bool UdpAudioServer::isTrackExists(uint16_t track_id) const {
    return tracks_.find(track_id) != tracks_.end();
}