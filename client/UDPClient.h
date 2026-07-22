#pragma once

#include <opus.h>
#include <portaudio.h>
#include <boost/asio.hpp>
#include "../shared/AudioEncoder.h"
#include "../shared/MusicStreamer.h"

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include "Protocol.h"

constexpr int SAMPLE_RATE = 48000;
constexpr int CHANNELS = 2;
constexpr int VOICE_CHANNELS = 1;
constexpr int SAMPLES_PER_FRAME = SAMPLE_RATE * FRAME_DURATION_MS / 1000;

struct AudioFrame {
    std::vector<int16_t> pcm;
    uint32_t pts_ms{0};
};

class PcmQueue {
public:
    void push(AudioFrame frame);
    bool pop(AudioFrame& out);
    size_t size();
private:
    std::deque<AudioFrame> queue_;
    std::mutex mutex_;
};

class UdpClient : public std::enable_shared_from_this<UdpClient> {
public:
    UdpClient(boost::asio::io_context& io_context,
              const std::string& server_ip,
              unsigned short server_port,
              uint32_t client_id);
    ~UdpClient();

    bool start();
    void stop();
    bool start_voice_capture();
    void stop_voice_capture();
    bool is_voice_capturing() const { return voice_capturing_; }

private:
    static int pa_callback_wrapper(const void* input, void* output,
                                   unsigned long frame_count,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void* userData);

    int process_audio(void* output, unsigned long frame_count);

    void send_registration();
    void start_receive();
    void handle_receive(const boost::system::error_code& ec, size_t bytes_received);
    void handle_music_packet(size_t bytes_received);
    void handle_voice_packet(size_t bytes_received);

    boost::asio::io_context& io_context_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint server_endpoint_;
    boost::asio::ip::udp::endpoint sender_endpoint_;

    std::string server_ip_;
    unsigned short server_port_;
    uint32_t client_id_;

    OpusDecoder* decoder_{nullptr};
    PaStream* stream_{nullptr};

    OpusDecoder* voice_decoder_{nullptr};
    PcmQueue voice_queue_;

    static int voice_pa_callback_wrapper(const void* input, void* output,
                                          unsigned long frame_count,
                                          const PaStreamCallbackTimeInfo* timeInfo,
                                          PaStreamCallbackFlags statusFlags,
                                          void* userData);
    void process_voice_capture(const void* input, unsigned long frame_count);

    PaStream* voice_stream_ = nullptr;
    std::unique_ptr<AudioEncoder> voice_encoder_;
    std::atomic<bool> voice_capturing_{false};
    uint32_t voice_seq_ = 0;

    std::atomic<bool> running_{false};

    std::vector<unsigned char> recv_buffer_;
    uint32_t expected_seq_ = 0;
    bool first_packet_ = true;

    PcmQueue pcm_queue_;
    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> underruns_{0};
    int consecutive_underruns_ = 0;

    std::atomic<uint32_t> server_position_ms_{0};
    std::atomic<uint32_t> playback_position_ms_{0};

    std::vector<int16_t> last_frame_;

    bool prebuffering_{true};
    static constexpr size_t PREBUFFER_FRAMES = 10;

    static constexpr size_t PREBUFFER_FRAMES_VOICE = 3;
    bool voice_prebuffering_ = true;

    uint32_t expected_voice_seq_ = 0;
    bool first_voice_packet_ = true;
};
