#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct Track {
    uint16_t id;
    std::string name;
    std::string path;
    int channels;
    int sample_rate;
    uint64_t total_frame_count;
    std::vector<int16_t> pcm_data; // maybe should be deleted because unused
    std::vector<std::vector<uint8_t>> opus_packets;
};