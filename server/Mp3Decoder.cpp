#include "Mp3Decoder.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#include <stdexcept>

std::shared_ptr<Track> Mp3Decoder::decode(const std::string& file_path) const {
    drmp3_config config;
    drmp3_uint64 total_frame_count = 0;

    drmp3_int16* raw = drmp3_open_file_and_read_pcm_frames_s16(
        file_path.c_str(), &config, &total_frame_count, nullptr);

    if (raw == nullptr) {
        throw std::runtime_error("Failed to decode MP3: " + file_path);
    }

    auto track = std::make_shared<Track>();
    track->name              = file_path;
    track->channels          = static_cast<int>(config.channels);
    track->sample_rate       = static_cast<int>(config.sampleRate);
    track->total_frame_count = total_frame_count;

    const size_t total_samples = total_frame_count * config.channels;
    track->pcm_data.assign(raw, raw + total_samples);

    drmp3_free(raw, nullptr);

    return track;
}