#define DR_MP3_IMPLEMENTATION
#include "UdpAudioServer.h"

#include <opus.h>
#include <boost/asio.hpp>

#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>

using boost::asio::ip::udp;

// --- Constructor Implementation ---
UdpAudioServer::UdpAudioServer(const std::string& mp3_path, unsigned short server_port)
    : mp3_path_(mp3_path), server_port_(server_port) {}


// --- Helper Methods Implementation ---
bool UdpAudioServer::is_opus_supported_rate(int rate) const {
    return rate == 8000 || rate == 12000 || rate == 16000 ||
           rate == 24000 || rate == 48000;
}

std::vector<drmp3_int16> UdpAudioServer::resample_linear(
    const drmp3_int16* input,
    drmp3_uint64 input_frames,
    int channels,
    int in_rate,
    int out_rate) const
{
    const double ratio = static_cast<double>(out_rate) / static_cast<double>(in_rate);
    const drmp3_uint64 output_frames =
        static_cast<drmp3_uint64>(static_cast<double>(input_frames) * ratio);

    std::vector<drmp3_int16> output(output_frames * channels);

    for (drmp3_uint64 out_i = 0; out_i < output_frames; ++out_i) {
        // Position in the input signal (fractional — between two real samples)
        double src_pos = static_cast<double>(out_i) / ratio;

        drmp3_uint64 src_index = static_cast<drmp3_uint64>(src_pos);
        double frac = src_pos - static_cast<double>(src_index);

        drmp3_uint64 src_index_next =
            (src_index + 1 < input_frames) ? src_index + 1 : src_index;

        for (int ch = 0; ch < channels; ++ch) {
            drmp3_int16 sample_a = input[src_index * channels + ch];
            drmp3_int16 sample_b = input[src_index_next * channels + ch];

            // Linear interpolation between sample_a and sample_b
            double interpolated = sample_a + (sample_b - sample_a) * frac;
            output[out_i * channels + ch] = static_cast<drmp3_int16>(interpolated);
        }
    }

    return output;
}

// --- Main Loop Implementation ---
int UdpAudioServer::run() {
    // ---------- 1. Decode MP3 to PCM ----------
    drmp3_config mp3_config;
    drmp3_uint64 total_frame_count = 0;

    drmp3_int16* pcm_data = drmp3_open_file_and_read_pcm_frames_s16(
        mp3_path_.c_str(), &mp3_config, &total_frame_count, nullptr);

    if (pcm_data == nullptr) {
        std::cerr << "Failed to read/decode MP3 file: " << mp3_path_ << "\n";
        return 1;
    }

    const int sample_rate = static_cast<int>(mp3_config.sampleRate);
    const int channels = static_cast<int>(mp3_config.channels);

    std::cout << "MP3 decoded:\n";
    std::cout << "  sample_rate = " << sample_rate << " Hz\n";
    std::cout << "  channels    = " << channels << "\n";
    std::cout << "  frame_count = " << total_frame_count << " (samples per channel)\n";

    int working_sample_rate = sample_rate;
    drmp3_int16* working_pcm_data = pcm_data;
    drmp3_uint64 working_frame_count = total_frame_count;
    std::vector<drmp3_int16> resampled_buffer; // holds memory if resampling occurred

    if (!is_opus_supported_rate(sample_rate)) {
        const int target_rate = 48000;
        std::cout << "\nSample rate " << sample_rate << " Hz is not directly supported by Opus.\n";
        std::cout << "Performing resampling (linear interpolation) to " << target_rate << " Hz...\n";

        resampled_buffer = resample_linear(
            pcm_data, total_frame_count, channels, sample_rate, target_rate);

        working_sample_rate = target_rate;
        working_pcm_data = resampled_buffer.data();
        working_frame_count = resampled_buffer.size() / channels;

        std::cout << "Resampling complete: " << working_frame_count
                  << " samples/channel at " << working_sample_rate << " Hz\n\n";
    }

    if (channels != 1 && channels != 2) {
        std::cerr << "ERROR: Opus in this test only supports mono(1) or stereo(2), received: "
                  << channels << "\n";
        drmp3_free(pcm_data, nullptr);
        return 1;
    }

    // ---------- 2. Configure Opus encoder ----------
    int opus_error = 0;
    OpusEncoder* encoder = opus_encoder_create(working_sample_rate, channels, OPUS_APPLICATION_AUDIO, &opus_error);
    if (opus_error != OPUS_OK) {
        std::cerr << "Failed to create Opus encoder: " << opus_strerror(opus_error) << "\n";
        drmp3_free(pcm_data, nullptr);
        return 1;
    }

    // Bitrate for music (can be adjusted later)
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(64000));

    // ---------- 3. Frame slicing parameters ----------
    const int frame_duration_ms = 20;
    const int samples_per_frame = working_sample_rate * frame_duration_ms / 1000; // samples PER CHANNEL
    const int samples_per_frame_total = samples_per_frame * channels;     // considering channels

    std::cout << "  frame_size  = " << samples_per_frame << " samples/channel ("
              << frame_duration_ms << " ms)\n\n";

    // Buffer for encoded Opus packet (with margin)
    std::vector<unsigned char> opus_buffer(4000);

    // ---------- 4. Configure UDP socket ----------
    boost::asio::io_context io_context;
    udp::socket socket(io_context, udp::endpoint(udp::v4(), server_port_));

    std::cout << "UDP server listening on port: " << server_port_ << "\n";
    std::cout << "Waiting for client registration (0xFF ping + client_id)...\n\n";

    // ---------- 4.1 Wait for client registration ----------
    udp::endpoint client_endpoint;
    uint32_t registered_client_id = 0;

    {
        std::vector<unsigned char> reg_buffer(16);
        bool got_ping = false;
        bool got_id = false;

        while (!got_ping || !got_id) {
            udp::endpoint sender_endpoint;
            boost::system::error_code ec;
            size_t bytes = socket.receive_from(
                boost::asio::buffer(reg_buffer), sender_endpoint, 0, ec);

            if (ec) {
                std::cerr << "Error receiving registration packet: " << ec.message() << "\n";
                continue;
            }

            if (!got_ping) {
                // Expecting first packet: exactly 1 byte, value 0xFF
                if (bytes == 1 && reg_buffer[0] == 0xFF) {
                    client_endpoint = sender_endpoint; // remember the source
                    got_ping = true;
                    std::cout << "Received ping (0xFF) from "
                              << sender_endpoint.address().to_string()
                              << ":" << sender_endpoint.port() << "\n";
                } else {
                    std::cerr << "Expected 0xFF ping, received packet of size "
                              << bytes << " bytes — ignoring\n";
                }
                continue;
            }

            if (!got_id) {
                // Expecting second packet: exactly 4 bytes client_id (big-endian)
                if (bytes == 4 && sender_endpoint == client_endpoint) {
                    registered_client_id =
                        (static_cast<uint32_t>(reg_buffer[0]) << 24) |
                        (static_cast<uint32_t>(reg_buffer[1]) << 16) |
                        (static_cast<uint32_t>(reg_buffer[2]) << 8) |
                        (static_cast<uint32_t>(reg_buffer[3]));
                    got_id = true;
                    std::cout << "Received client_id: " << registered_client_id << "\n";
                } else {
                    std::cerr << "Expected 4-byte client_id from the same endpoint, "
                              << "received " << bytes << " bytes — ignoring\n";
                }
            }
        }
    }

    std::cout << "\nClient registered: " << client_endpoint.address().to_string()
              << ":" << client_endpoint.port()
              << " (id=" << registered_client_id << ")\n";
    std::cout << "Starting to send audio...\n\n";

    // ---------- 5. UDP packet format ----------
    uint32_t sequence_number = 0;
    drmp3_uint64 frames_sent = 0;
    const drmp3_uint64 total_audio_frames = working_frame_count;

    auto stream_start_time = std::chrono::steady_clock::now();

    while (frames_sent + samples_per_frame <= total_audio_frames) {
        const drmp3_int16* frame_start = working_pcm_data + (frames_sent * channels);

        // Encode PCM frame -> Opus
        int encoded_bytes = opus_encode(
            encoder,
            frame_start,
            samples_per_frame,
            opus_buffer.data(),
            static_cast<opus_int32>(opus_buffer.size())
        );

        if (encoded_bytes < 0) {
            std::cerr << "Opus encode error: " << opus_strerror(encoded_bytes) << "\n";
            break;
        }

        // Build UDP packet
        std::vector<unsigned char> packet(4 + encoded_bytes);
        packet[0] = static_cast<unsigned char>((sequence_number >> 24) & 0xFF);
        packet[1] = static_cast<unsigned char>((sequence_number >> 16) & 0xFF);
        packet[2] = static_cast<unsigned char>((sequence_number >> 8) & 0xFF);
        packet[3] = static_cast<unsigned char>(sequence_number & 0xFF);
        std::memcpy(packet.data() + 4, opus_buffer.data(), encoded_bytes);

        socket.send_to(boost::asio::buffer(packet), client_endpoint);

        sequence_number++;
        frames_sent += samples_per_frame;

        // Target timestamp
        auto target_time = stream_start_time +
            std::chrono::milliseconds(static_cast<long long>(sequence_number) * frame_duration_ms);
        std::this_thread::sleep_until(target_time);

        if (sequence_number % 50 == 0) {
            std::cout << "Packets sent: " << sequence_number << "\n";
        }
    }

    std::cout << "\nDone. Total packets sent: " << sequence_number << "\n";

    opus_encoder_destroy(encoder);
    drmp3_free(pcm_data, nullptr);

    return 0;
}