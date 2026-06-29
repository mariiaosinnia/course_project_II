#pragma once
#include "Track.h"
#include <opus.h>
#include <vector>
#include "Protocol.h"
#include <cstdint>

class AudioEncoder {
public:
    AudioEncoder(int sample_rate, int channels);
    ~AudioEncoder();

    std::vector<uint8_t> encodeFrame(
        const int16_t* pcm_data,
        int samples_per_frame) const;

    static bool isSupportedRate(int rate);

private:
    ::OpusEncoder* encoder_;
};