#pragma once

#include <opus.h>
#include <portaudio.h>
#include <boost/asio.hpp>

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include "Protocol.h"

constexpr int SAMPLE_RATE = 48000;
constexpr int CHANNELS = 2;
constexpr int SAMPLES_PER_FRAME = SAMPLE_RATE * FRAME_DURATION_MS / 1000;

// Проста потокобезпечна черга PCM-фреймів
class PcmQueue {
public:
    void push(std::vector<int16_t> frame);
    bool pop(std::vector<int16_t>& out);
    size_t size();

private:
    std::mutex mutex_;
    std::deque<std::vector<int16_t>> queue_;
};

class UdpClient {
public:
    UdpClient(const std::string& server_ip, unsigned short server_port, uint32_t client_id);
    ~UdpClient();

    bool start();
    void stop();

private:
    // C-style callback для PortAudio, який перенаправляє виклик у process_audio
    static int pa_callback_wrapper(const void* input, void* output,
                                   unsigned long frame_count,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void* userData);

    // Внутрішній метод для обробки аудіо (має доступ до членів класу)
    int process_audio(void* output, unsigned long frame_count);

    // Цикл прийому UDP пакетів
    void receive_loop();
    void send_registration();

    std::string server_ip_;
    unsigned short server_port_;
    uint32_t client_id_;

    boost::asio::io_context io_context_;
    boost::asio::ip::udp::socket socket_;

    OpusDecoder* decoder_{nullptr};
    PaStream* stream_{nullptr};

    std::thread receive_thread_;
    std::atomic<bool> running_{false};

    PcmQueue pcm_queue_;
    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> underruns_{0};

    // Стан пре-буферизації
    bool prebuffering_{true};
    static constexpr size_t PREBUFFER_FRAMES = 10;
};


