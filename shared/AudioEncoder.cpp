#include "AudioEncoder.h"
#include <stdexcept>

bool AudioEncoder::isSupportedRate(int rate) {
    return rate == 8000  || rate == 12000 ||
           rate == 16000 || rate == 24000 ||
           rate == 48000;
}

AudioEncoder::AudioEncoder(int sample_rate, int channels)
    : encoder_(nullptr)
{
    if (channels != 1 && channels != 2) {
        throw std::runtime_error("Opus supports only mono or stereo");
    }

    int error = 0;
    encoder_ = opus_encoder_create(
        sample_rate,
        channels,
        OPUS_APPLICATION_AUDIO,
        &error
    );

    if (error != OPUS_OK) {
        throw std::runtime_error(
            std::string("Failed to create Opus encoder: ") + opus_strerror(error));
    }

    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(BITRATE));
}

AudioEncoder::~AudioEncoder() {
    if (encoder_) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }
}

std::vector<uint8_t> AudioEncoder::encodeFrame(
    const int16_t* pcm_data,
    int samples_per_frame) const
{
    std::vector<uint8_t> opus_buffer(4000);

    int encoded_bytes = opus_encode(
        encoder_,
        pcm_data,
        samples_per_frame,
        opus_buffer.data(),
        static_cast<opus_int32>(opus_buffer.size())
    );

    if (encoded_bytes < 0) {
        throw std::runtime_error(
            std::string("Opus encode error: ") + opus_strerror(encoded_bytes));
    }

    return std::vector<uint8_t>(
        opus_buffer.begin(),
        opus_buffer.begin() + encoded_bytes
    );
}