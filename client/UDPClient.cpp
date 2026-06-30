#include "UDPClient.h"
#include "Logger.h"
#include <cstring>

using boost::asio::ip::udp;

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

UdpClient::UdpClient(boost::asio::io_context& io_context,
                      const std::string& server_ip,
                      unsigned short server_port,
                      uint32_t client_id)
    : io_context_(io_context)
    , socket_(io_context, udp::endpoint(udp::v4(), 0))
    , server_ip_(server_ip)
    , server_port_(server_port)
    , client_id_(client_id)
    , recv_buffer_(4000)
{
}

UdpClient::~UdpClient() {
    stop();
}

bool UdpClient::start() {
    int opus_error = 0;
    decoder_ = opus_decoder_create(SAMPLE_RATE, CHANNELS, &opus_error);
    if (opus_error != OPUS_OK) {
        Logger::print(std::string("Failed to create Opus decoder: ") + opus_strerror(opus_error));
        return false;
    }

    PaError pa_err = Pa_Initialize();
    if (pa_err != paNoError) {
        Logger::print(std::string("PortAudio init error: ") + Pa_GetErrorText(pa_err));
        return false;
    }

    pa_err = Pa_OpenDefaultStream(
        &stream_, 0, CHANNELS, paInt16, SAMPLE_RATE, SAMPLES_PER_FRAME,
        &UdpClient::pa_callback_wrapper, this);
    if (pa_err != paNoError) {
        Logger::print(std::string("Failed to open PortAudio stream: ") + Pa_GetErrorText(pa_err));
        return false;
    }

    pa_err = Pa_StartStream(stream_);
    if (pa_err != paNoError) {
        Logger::print(std::string("Failed to start PortAudio stream: ") + Pa_GetErrorText(pa_err));
        return false;
    }

    boost::system::error_code resolve_ec;
    server_endpoint_ = udp::endpoint(boost::asio::ip::make_address(server_ip_, resolve_ec), server_port_);
    if (resolve_ec) {
        Logger::print("bad server ip: " + resolve_ec.message());
        return false;
    }

    running_ = true;
    send_registration();
    start_receive();

    Logger::print("UDP client started, registering with server...");
    return true;
}

void UdpClient::stop() {
    if (!running_) return;
    running_ = false;

    boost::system::error_code ec;
    socket_.close(ec);

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
    Logger::print("Register on UDP server " + server_ip_ + ":" +
                   std::to_string(server_port_) + " with client_id=" +
                   std::to_string(client_id_));

    std::vector<unsigned char> packet(5);
    packet[0] = 0xFF;
    packet[1] = static_cast<unsigned char>((client_id_ >> 24) & 0xFF);
    packet[2] = static_cast<unsigned char>((client_id_ >> 16) & 0xFF);
    packet[3] = static_cast<unsigned char>((client_id_ >> 8)  & 0xFF);
    packet[4] = static_cast<unsigned char>( client_id_        & 0xFF);

    // Синхронна відправка — гарантовано йде одразу
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(packet), server_endpoint_, 0, ec);
    if (ec) {
        Logger::print("UDP registration send error: " + ec.message());
    } else {
        Logger::print("UDP registration packet sent successfully");
    }
}

void UdpClient::start_receive() {
    if (!running_) return;

    socket_.async_receive_from(
        boost::asio::buffer(recv_buffer_), sender_endpoint_,
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t bytes) {
            self->handle_receive(ec, bytes);
        });
}

void UdpClient::handle_receive(const boost::system::error_code& ec, size_t bytes_received) {
    if (!running_ || ec == boost::asio::error::operation_aborted) {
        return;
    }

    if (ec) {
        Logger::print("UDP receive error: " + ec.message());
        start_receive();
        return;
    }

    if (bytes_received < 4) {
        start_receive();
        return;
    }

    uint32_t seq =
        (static_cast<uint32_t>(recv_buffer_[0]) << 24) |
        (static_cast<uint32_t>(recv_buffer_[1]) << 16) |
        (static_cast<uint32_t>(recv_buffer_[2]) << 8) |
        (static_cast<uint32_t>(recv_buffer_[3]));

    if (!first_packet_ && seq != expected_seq_) {
        Logger::print("[warning] sequence gap: expected " + std::to_string(expected_seq_) +
                       ", got " + std::to_string(seq));
    }
    expected_seq_ = seq + 1;
    first_packet_ = false;

    const unsigned char* opus_payload = recv_buffer_.data() + 4;
    int opus_payload_size = static_cast<int>(bytes_received - 4);

    std::vector<int16_t> pcm_frame(SAMPLES_PER_FRAME * CHANNELS);
    int decoded_samples = opus_decode(
        decoder_, opus_payload, opus_payload_size,
        pcm_frame.data(), SAMPLES_PER_FRAME, 0);

    if (decoded_samples < 0) {
        Logger::print(std::string("Opus decode error: ") + opus_strerror(decoded_samples));
        start_receive();
        return;
    }

    pcm_queue_.push(std::move(pcm_frame));
    packets_received_++;

    if (packets_received_ % 50 == 0) {
        Logger::print("Packets received: " + std::to_string(packets_received_.load()) +
                       " | underruns: " + std::to_string(underruns_.load()) +
                       " | in queue: " + std::to_string(pcm_queue_.size()));
    }

    start_receive();
}

int UdpClient::pa_callback_wrapper(const void* input, void* output,
                                    unsigned long frame_count,
                                    const PaStreamCallbackTimeInfo* timeInfo,
                                    PaStreamCallbackFlags statusFlags,
                                    void* userData) {
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
        prebuffering_ = true;
    }

    return paContinue;
}