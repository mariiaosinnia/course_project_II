#include "Resampler.h"
#include <stdexcept>

bool Resampler::isSupportedRate(int rate) const {
    return rate == 8000  || rate == 12000 ||
           rate == 16000 || rate == 24000 ||
           rate == 48000;
}
void Resampler::resample(std::shared_ptr<Track> track, int target_rate) const {
    track->pcm_data = resampleLinear(
        track->pcm_data,
        track->total_frame_count,
        track->channels,
        track->sample_rate,
        target_rate
    );

    track->total_frame_count = track->pcm_data.size() / track->channels;
    track->sample_rate = target_rate;

}

std::vector<int16_t> Resampler::resampleLinear(
    const std::vector<int16_t>& input,
    uint64_t input_frames,
    int channels,
    int in_rate,
    int out_rate) const
{
    const double ratio = static_cast<double>(out_rate) / static_cast<double>(in_rate);
    const uint64_t output_frames =
        static_cast<uint64_t>(static_cast<double>(input_frames) * ratio);

    std::vector<int16_t> output(output_frames * channels);

    for (uint64_t out_i = 0; out_i < output_frames; ++out_i) {
        double src_pos = static_cast<double>(out_i) / ratio;
        uint64_t src_index = static_cast<uint64_t>(src_pos);
        double frac = src_pos - static_cast<double>(src_index);

        uint64_t src_index_next =
            (src_index + 1 < input_frames) ? src_index + 1 : src_index;

        for (int ch = 0; ch < channels; ++ch) {
            int16_t sample_a = input[src_index * channels + ch];
            int16_t sample_b = input[src_index_next * channels + ch];

            double interpolated = sample_a + (sample_b - sample_a) * frac;
            output[out_i * channels + ch] = static_cast<int16_t>(interpolated);
        }
    }

    return output;
}