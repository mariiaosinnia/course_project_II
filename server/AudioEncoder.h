#pragma once
#include "Track.h"
#include <opus.h>
#include <vector>
#include <cstdint>

class AudioEncoder {
public:
    AudioEncoder(int sample_rate, int channels);
    ~AudioEncoder();

    std::vector<uint8_t> encodeFrame(
        const int16_t* pcm_data,
        int samples_per_frame) const;

    static bool isSupportedRate(int rate);
    static constexpr int TARGET_RATE = 48000;
    static constexpr int FRAME_DURATION_MS = 20;
    static constexpr int BITRATE = 64000;

private:
    ::OpusEncoder* encoder_;
};