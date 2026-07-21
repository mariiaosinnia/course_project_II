#include "UDPClient.h"
#include "Logger.h"
#include <cstring>

using boost::asio::ip::udp;

void PcmQueue::push(AudioFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(frame));
}

bool PcmQueue::pop(AudioFrame& out) {
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

    int voice_opus_error = 0;
    voice_decoder_ = opus_decoder_create(SAMPLE_RATE, CHANNELS, &voice_opus_error);
    if (voice_opus_error != OPUS_OK) {
        Logger::print(std::string("Failed to create Opus decoder for voice: ") + opus_strerror(voice_opus_error));
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
    if (voice_decoder_) {
        opus_decoder_destroy(voice_decoder_);
        voice_decoder_ = nullptr;
    }
}

void UdpClient::send_registration() {
    Logger::print("Register on UDP server " + server_ip_ + ":" +
                   std::to_string(server_port_) + " with client_id=" +
                   std::to_string(client_id_));

    std::vector<unsigned char> packet(5);
    packet[0] = static_cast<unsigned char>(UdpPacketType::Registration);
    packet[1] = static_cast<unsigned char>((client_id_ >> 24) & 0xFF);
    packet[2] = static_cast<unsigned char>((client_id_ >> 16) & 0xFF);
    packet[3] = static_cast<unsigned char>((client_id_ >> 8)  & 0xFF);
    packet[4] = static_cast<unsigned char>( client_id_        & 0xFF);

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

    if (bytes_received < 1) {
        start_receive();
        return;
    }

    UdpPacketType type = static_cast<UdpPacketType>(recv_buffer_[0]);

    switch (type) {
        case UdpPacketType::Music:
            handle_music_packet(bytes_received);
            break;
        case UdpPacketType::Voice:
            handle_voice_packet(bytes_received);
            break;
        default:
            Logger::print("[warning] unknown UDP packet type received");
            break;
    }

    start_receive();
}

void UdpClient::handle_music_packet(size_t bytes_received) {
    if (bytes_received < 9) {  // 1 (type) + 4 (seq) + 4 (position)
        return;
    }

    uint32_t seq =
        (static_cast<uint32_t>(recv_buffer_[1]) << 24) |
        (static_cast<uint32_t>(recv_buffer_[2]) << 16) |
        (static_cast<uint32_t>(recv_buffer_[3]) << 8)  |
        (static_cast<uint32_t>(recv_buffer_[4]));

    uint32_t prev_server_position = server_position_ms_.load();

    uint32_t new_position =
        (static_cast<uint32_t>(recv_buffer_[5]) << 24) |
        (static_cast<uint32_t>(recv_buffer_[6]) << 16) |
        (static_cast<uint32_t>(recv_buffer_[7]) << 8)  |
        (static_cast<uint32_t>(recv_buffer_[8]));

    if (new_position + 500 < prev_server_position) {
        if (decoder_) {
            opus_decoder_ctl(decoder_, OPUS_RESET_STATE);
        }
    }

    server_position_ms_.store(new_position);

    if (!first_packet_ && seq != expected_seq_) {
        Logger::print("[warning] sequence gap: expected " + std::to_string(expected_seq_) +
                       ", got " + std::to_string(seq));
    }
    expected_seq_ = seq + 1;
    first_packet_ = false;

    const unsigned char* opus_payload = recv_buffer_.data() + 9;   // було +8
    int opus_payload_size = static_cast<int>(bytes_received - 9);  // було -8

    AudioFrame frame;
    frame.pcm.resize(SAMPLES_PER_FRAME * CHANNELS);
    frame.pts_ms = new_position;

    int decoded_samples = opus_decode(
        decoder_, opus_payload, opus_payload_size,
        frame.pcm.data(), SAMPLES_PER_FRAME, 0);

    if (decoded_samples < 0) {
        Logger::print(std::string("Opus decode error: ") + opus_strerror(decoded_samples));
        return;
    }

    pcm_queue_.push(std::move(frame));
    packets_received_++;

    if (packets_received_ % 50 == 0) {
        Logger::print("Packets received: " + std::to_string(packets_received_.load()) +
                       " | underruns: " + std::to_string(underruns_.load()) +
                       " | in queue: " + std::to_string(pcm_queue_.size()));
    }
}


void UdpClient::handle_voice_packet(size_t bytes_received) {
    if (bytes_received < 9) return;

    uint32_t sender_id =
        (static_cast<uint32_t>(recv_buffer_[1]) << 24) |
        (static_cast<uint32_t>(recv_buffer_[2]) << 16) |
        (static_cast<uint32_t>(recv_buffer_[3]) << 8)  |
        (static_cast<uint32_t>(recv_buffer_[4]));

    if (sender_id == client_id_) {
        return;  // страховка: не програвати власний голос
    }

    uint32_t seq =
        (static_cast<uint32_t>(recv_buffer_[5]) << 24) |
        (static_cast<uint32_t>(recv_buffer_[6]) << 16) |
        (static_cast<uint32_t>(recv_buffer_[7]) << 8)  |
        (static_cast<uint32_t>(recv_buffer_[8]));

    if (voice_queue_.size() == 0) {
        first_voice_packet_ = true;
    }

    if (!first_voice_packet_ && seq != expected_voice_seq_) {
        Logger::print("[warning] voice sequence gap: expected " +
                       std::to_string(expected_voice_seq_) +
                       ", got " + std::to_string(seq));
    }
    expected_voice_seq_ = seq + 1;
    first_voice_packet_ = false;

    const unsigned char* opus_payload = recv_buffer_.data() + 9;
    int opus_payload_size = static_cast<int>(bytes_received - 9);

    AudioFrame frame;
    frame.pcm.resize(SAMPLES_PER_FRAME * CHANNELS);

    int decoded_samples = opus_decode(
        voice_decoder_, opus_payload, opus_payload_size,
        frame.pcm.data(), SAMPLES_PER_FRAME, 0);

    if (decoded_samples < 0) {
        Logger::print(std::string("Voice Opus decode error: ") + opus_strerror(decoded_samples));
        return;
    }

    voice_queue_.push(std::move(frame));
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

    int32_t current_queue_size = static_cast<int32_t>(pcm_queue_.size());
    int32_t drift_frames = current_queue_size - static_cast<int32_t>(PREBUFFER_FRAMES);

    if (drift_frames > 3) {
        // Черга переповнюється — непомітно пропускаємо 1 фрейм
        AudioFrame dummy;
        pcm_queue_.pop(dummy);
    } else if (drift_frames < -3) {
        // Черга спорожнюється — повторюємо останній фрейм для пригальмовування
        if (!last_frame_.empty() && last_frame_.size() == frame_count * CHANNELS) {
            std::memcpy(out, last_frame_.data(), last_frame_.size() * sizeof(int16_t));
            return paContinue;
        }
    }

    AudioFrame frame;
    if (pcm_queue_.pop(frame) && frame.pcm.size() == frame_count * CHANNELS) {
        std::memcpy(out, frame.pcm.data(), frame.pcm.size() * sizeof(int16_t));
        last_frame_ = frame.pcm;

        // Позиція відтворення стає РІВНОЮ PTS кадру, який прямо зараз пішов у колонки!
        playback_position_ms_.store(frame.pts_ms);

        consecutive_underruns_ = 0;
    } else {
        // Якщо сталася втрата даних (underrun)
        std::memset(out, 0, frame_count * CHANNELS * sizeof(int16_t));
        underruns_++;
        consecutive_underruns_++;

        if (consecutive_underruns_ >= 3) {
            prebuffering_ = true; // Запитати новий накопичувальний буфер
            consecutive_underruns_ = 0;
        }
    }

    if (voice_queue_.size() == 0) {
        voice_prebuffering_ = true;
    } else if (voice_prebuffering_) {
        if (voice_queue_.size() < PREBUFFER_FRAMES_VOICE) {
            return paContinue;
        } else {
            voice_prebuffering_ = false;
        }
    }

    if (!voice_prebuffering_) {
        AudioFrame voice_frame;
        if (voice_queue_.pop(voice_frame) && voice_frame.pcm.size() == frame_count * CHANNELS) {
            for (size_t i = 0; i < frame_count * CHANNELS; ++i) {
                int32_t mixed = static_cast<int32_t>(out[i]) + static_cast<int32_t>(voice_frame.pcm[i]);
                out[i] = static_cast<int16_t>(std::clamp(mixed, -32768, 32767));
            }
        }
    }

    return paContinue;
}

bool UdpClient::start_voice_capture() {
    if (voice_capturing_) return true;

    voice_encoder_ = std::make_unique<AudioEncoder>(SAMPLE_RATE, CHANNELS);

    PaError err = Pa_OpenDefaultStream(
        &voice_stream_,
        CHANNELS, 0,
        paInt16, SAMPLE_RATE, SAMPLES_PER_FRAME,
        &UdpClient::voice_pa_callback_wrapper, this);

    if (err != paNoError) {
        Logger::print(std::string("Failed to open voice input stream: ") + Pa_GetErrorText(err));
        voice_stream_ = nullptr;
        return false;
    }

    err = Pa_StartStream(voice_stream_);
    if (err != paNoError) {
        Logger::print(std::string("Failed to start voice input stream: ") + Pa_GetErrorText(err));
        Pa_CloseStream(voice_stream_);
        voice_stream_ = nullptr;
        return false;
    }

    voice_seq_ = 0;
    voice_capturing_ = true;
    Logger::print("Voice capture started");
    return true;
}

void UdpClient::stop_voice_capture() {
    if (!voice_capturing_) return;
    voice_capturing_ = false;

    if (voice_stream_) {
        Pa_StopStream(voice_stream_);
        Pa_CloseStream(voice_stream_);
        voice_stream_ = nullptr;
    }
    voice_encoder_.reset();
    Logger::print("Voice capture stopped");
}

int UdpClient::voice_pa_callback_wrapper(const void* input, void* output,
                                          unsigned long frame_count,
                                          const PaStreamCallbackTimeInfo* timeInfo,
                                          PaStreamCallbackFlags statusFlags,
                                          void* userData) {
    auto* client = static_cast<UdpClient*>(userData);
    client->process_voice_capture(input, frame_count);
    return paContinue;
}

void UdpClient::process_voice_capture(const void* input, unsigned long frame_count) {
    if (!voice_capturing_ || !input) return;

    const int16_t* in = static_cast<const int16_t*>(input);

    auto opus_data = voice_encoder_->encodeFrame(in, static_cast<int>(frame_count));

    auto packet = MusicStreamer::buildVoicePacket(client_id_, voice_seq_++, opus_data);

    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(packet), server_endpoint_, 0, ec);
}