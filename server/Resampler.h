#pragma once
#include "Track.h"
#include <memory>

class Resampler {
public:
    static constexpr int TARGET_RATE = 48000;
    void resample(std::shared_ptr<Track> track, int target_rate) const;

private:
    bool isSupportedRate(int rate) const;

    std::vector<int16_t> resampleLinear(
        const std::vector<int16_t>& input,
        uint64_t input_frames,
        int channels,
        int in_rate,
        int out_rate) const;
};