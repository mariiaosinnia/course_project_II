#include "UDPClient.h"

#include <iostream>
#include <cstring>

using boost::asio::ip::udp;

// ---------- PcmQueue Implementation ----------

void PcmQueue::push(std::vector<int16_t> frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(frame));
}

bool PcmQueue::pop(std::vector<int16_t>& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

size_t PcmQueue::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

// ---------- UdpAudioClient Implementation ----------

UdpClient::UdpClient(const std::string& server_ip, unsigned short server_port, uint32_t client_id)
    : server_ip_(server_ip),
      server_port_(server_port),
      client_id_(client_id),
      socket_(io_context_, udp::endpoint(udp::v4(), 0)) // 0 = OS automatically chooses a local port
{
}

UdpClient::~UdpClient() {
    stop();
}

bool UdpClient::start() {
    // 1. Configure Opus decoder
    int opus_error = 0;
    decoder_ = opus_decoder_create(SAMPLE_RATE, CHANNELS, &opus_error);
    if (opus_error != OPUS_OK) {
        std::cerr << "Failed to create Opus decoder: " << opus_strerror(opus_error) << "\n";
        return false;
    }

    // 2. Configure PortAudio
    PaError pa_err = Pa_Initialize();
    if (pa_err != paNoError) {
        std::cerr << "PortAudio init error: " << Pa_GetErrorText(pa_err) << "\n";
        return false;
    }

    pa_err = Pa_OpenDefaultStream(
        &stream_,
        0,                  // no input channels
        CHANNELS,           // output channels
        paInt16,            // sample format
        SAMPLE_RATE,
        SAMPLES_PER_FRAME,  // frames per buffer
        &UdpClient::pa_callback_wrapper, // Static wrapper function
        this                // Pass a pointer to our object (userData)
    );

    if (pa_err != paNoError) {
        std::cerr << "Failed to open PortAudio stream: " << Pa_GetErrorText(pa_err) << "\n";
        return false;
    }

    pa_err = Pa_StartStream(stream_);
    if (pa_err != paNoError) {
        std::cerr << "Failed to start PortAudio stream: " << Pa_GetErrorText(pa_err) << "\n";
        return false;
    }

    // 3. Send registration packets
    send_registration();

    // 4. Start network loop in a separate thread
    running_ = true;
    receive_thread_ = std::thread(&UdpClient::receive_loop, this);

    std::cout << "Client started successfully. Waiting for audio...\n\n";
    return true;
}

void UdpClient::stop() {
    if (!running_) return;
    running_ = false;

    // Close the socket to interrupt the blocking socket_.receive_from
    boost::system::error_code ec;
    socket_.close(ec);

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    Pa_Terminate();

    if (decoder_) {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }
}

void UdpClient::send_registration() {
    udp::endpoint server_endpoint(boost::asio::ip::make_address(server_ip_), server_port_);

    std::cout << "========================================\n";
    std::cout << "Register on server " << server_ip_ << ":" << server_port_
              << " with client_id=" << client_id_ << "\n";
    std::cout << "========================================\n";

    // Пакет 1: маркер пінгу
    std::vector<unsigned char> ping_packet{0xFF};
    socket_.send_to(boost::asio::buffer(ping_packet), server_endpoint);

    // Пакет 2: client_id (big-endian)
    std::vector<unsigned char> id_packet(4);
    id_packet[0] = static_cast<unsigned char>((client_id_ >> 24) & 0xFF);
    id_packet[1] = static_cast<unsigned char>((client_id_ >> 16) & 0xFF);
    id_packet[2] = static_cast<unsigned char>((client_id_ >> 8) & 0xFF);
    id_packet[3] = static_cast<unsigned char>(client_id_ & 0xFF);
    socket_.send_to(boost::asio::buffer(id_packet), server_endpoint);
}
int UdpClient::pa_callback_wrapper(const void* input, void* output,
                                        unsigned long frame_count,
                                        const PaStreamCallbackTimeInfo* timeInfo,
                                        PaStreamCallbackFlags statusFlags,
                                        void* userData) {
    // Restore the pointer to our object and call its method
    auto* client = static_cast<UdpClient*>(userData);
    return client->process_audio(output, frame_count);
}

int UdpClient::process_audio(void* output, unsigned long frame_count) {
    int16_t* out = static_cast<int16_t*>(output);

    if (prebuffering_) {
        if (pcm_queue_.size() < PREBUFFER_FRAMES) {
            std::memset(out, 0, frame_count * CHANNELS * sizeof(int16_t));
            return paContinue;
        }
        prebuffering_ = false;
    }

    std::vector<int16_t> frame;
    if (pcm_queue_.pop(frame) && frame.size() == frame_count * CHANNELS) {
        std::memcpy(out, frame.data(), frame.size() * sizeof(int16_t));
    } else {
        std::memset(out, 0, frame_count * CHANNELS * sizeof(int16_t));
        underruns_++;
        prebuffering_ = true; // Return to buffering
    }

    return paContinue;
}

void UdpClient::receive_loop() {
    std::vector<unsigned char> recv_buffer(4000);
    udp::endpoint sender_endpoint;
    uint32_t expected_seq = 0;
    bool first_packet = true;

    while (running_) {
        boost::system::error_code ec;
        size_t bytes_received = socket_.receive_from(
            boost::asio::buffer(recv_buffer), sender_endpoint, 0, ec);

        // If the socket was closed via stop(), exit the loop
        if (ec == boost::asio::error::operation_aborted || !socket_.is_open()) {
            break;
        }

        if (ec) {
            std::cerr << "Receive error: " << ec.message() << "\n";
            continue;
        }

        if (bytes_received < 4) {
            continue;
        }

        uint32_t seq =
            (static_cast<uint32_t>(recv_buffer[0]) << 24) |
            (static_cast<uint32_t>(recv_buffer[1]) << 16) |
            (static_cast<uint32_t>(recv_buffer[2]) << 8) |
            (static_cast<uint32_t>(recv_buffer[3]));

        if (!first_packet && seq != expected_seq) {
            std::cout << "[warning] sequence gap: expected " << expected_seq
                      << ", got " << seq << "\n";
        }
        expected_seq = seq + 1;
        first_packet = false;

        const unsigned char* opus_payload = recv_buffer.data() + 4;
        int opus_payload_size = static_cast<int>(bytes_received - 4);

        std::vector<int16_t> pcm_frame(SAMPLES_PER_FRAME * CHANNELS);
        int decoded_samples = opus_decode(
            decoder_,
            opus_payload,
            opus_payload_size,
            pcm_frame.data(),
            SAMPLES_PER_FRAME,
            0
        );

        if (decoded_samples < 0) {
            std::cerr << "Opus decode error: " << opus_strerror(decoded_samples) << "\n";
            continue;
        }

        pcm_queue_.push(std::move(pcm_frame));
        packets_received_++;

        if (packets_received_ % 50 == 0) {
            std::cout << "Packets received: " << packets_received_
                      << " | underruns: " << underruns_
                      << " | in queue: " << pcm_queue_.size() << "\n";
        }
    }
}