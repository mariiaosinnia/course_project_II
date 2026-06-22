#pragma once

#include <string>
#include <vector>
#include "dr_mp3.h"

class UdpAudioServer {
public:
    UdpAudioServer(const std::string& mp3_path, unsigned short server_port);
    int run();

private:
    std::string mp3_path_;
    unsigned short server_port_;

    bool is_opus_supported_rate(int rate) const;

    std::vector<drmp3_int16> resample_linear(
        const drmp3_int16* input,
        drmp3_uint64 input_frames,
        int channels,
        int in_rate,
        int out_rate) const;
};