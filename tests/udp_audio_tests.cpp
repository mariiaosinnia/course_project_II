#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <cmath>

#define private public
#define protected public
#include "MusicStreamer.h"
#include "AudioEncoder.h"
#include "UDPClient.h"
#include "UdpAudioServer.h"
#include "Resampler.h"
#undef private
#undef protected

// 1. MusicStreamer Test
TEST(MusicStreamerTest, BuildPacketSizesAndContents) {
    uint32_t seq = 12345;
    uint32_t track_pos = 98765;
    std::vector<uint8_t> opus = {0xAA, 0xBB, 0xCC, 0xDD};

    // Regression Test 1: buildPacket size must be exactly 9 + opus.size()
    auto music_packet = MusicStreamer::buildPacket(seq, opus, track_pos);
    EXPECT_EQ(music_packet.size(), 9 + opus.size());
    EXPECT_EQ(music_packet[0], static_cast<uint8_t>(UdpPacketType::Music));
    
    // Check big-endian values
    uint32_t parsed_seq = (music_packet[1] << 24) | (music_packet[2] << 16) | (music_packet[3] << 8) | music_packet[4];
    uint32_t parsed_pos = (music_packet[5] << 24) | (music_packet[6] << 16) | (music_packet[7] << 8) | music_packet[8];
    EXPECT_EQ(parsed_seq, seq);
    EXPECT_EQ(parsed_pos, track_pos);
    EXPECT_EQ(std::vector<uint8_t>(music_packet.begin() + 9, music_packet.end()), opus);

    // Regression Test 1: buildVoicePacket size must be exactly 9 + opus.size()
    uint32_t client_id = 999;
    auto voice_packet = MusicStreamer::buildVoicePacket(client_id, seq, opus);
    EXPECT_EQ(voice_packet.size(), 9 + opus.size());
    EXPECT_EQ(voice_packet[0], static_cast<uint8_t>(UdpPacketType::Voice));

    uint32_t parsed_client = (voice_packet[1] << 24) | (voice_packet[2] << 16) | (voice_packet[3] << 8) | voice_packet[4];
    uint32_t parsed_voice_seq = (voice_packet[5] << 24) | (voice_packet[6] << 16) | (voice_packet[7] << 8) | voice_packet[8];
    EXPECT_EQ(parsed_client, client_id);
    EXPECT_EQ(parsed_voice_seq, seq);
}

// 2. AudioEncoder Tests
TEST(AudioEncoderTest, SupportedSampleRates) {
    EXPECT_TRUE(AudioEncoder::isSupportedRate(48000));
    EXPECT_TRUE(AudioEncoder::isSupportedRate(24000));
    EXPECT_TRUE(AudioEncoder::isSupportedRate(16000));
    EXPECT_FALSE(AudioEncoder::isSupportedRate(44100));
    EXPECT_FALSE(AudioEncoder::isSupportedRate(0));
}

TEST(AudioEncoderTest, EncodeDecodeRoundtrip) {
    // Opus requires stereo/mono and supported rate
    int sample_rate = 48000;
    int channels = 2;
    int samples_per_frame = sample_rate * FRAME_DURATION_MS / 1000; // 960

    AudioEncoder encoder(sample_rate, channels);
    
    // 1. Test with silence - should decode back to very low/zero values
    std::vector<int16_t> silence_in(samples_per_frame * channels, 0);
    std::vector<uint8_t> opus_silence = encoder.encodeFrame(silence_in.data(), samples_per_frame);
    EXPECT_FALSE(opus_silence.empty());

    int error = 0;
    OpusDecoder* decoder = opus_decoder_create(sample_rate, channels, &error);
    ASSERT_EQ(error, OPUS_OK);
    ASSERT_NE(decoder, nullptr);

    std::vector<int16_t> silence_out(samples_per_frame * channels);
    int decoded = opus_decode(decoder, opus_silence.data(), static_cast<opus_int32>(opus_silence.size()), silence_out.data(), samples_per_frame, 0);
    EXPECT_EQ(decoded, samples_per_frame);

    double sum_sq_diff = 0;
    for (size_t i = 0; i < silence_in.size(); ++i) {
        double diff = silence_in[i] - silence_out[i];
        sum_sq_diff += diff * diff;
    }
    double rmse = std::sqrt(sum_sq_diff / silence_in.size());
    EXPECT_LT(rmse, 50.0); // Pure silence should remain extremely close to 0

    // Reset decoder state for the next run
    opus_decoder_ctl(decoder, OPUS_RESET_STATE);

    // 2. Test with signal (sine wave) - verify that it successfully decodes to a signal with similar energy
    std::vector<int16_t> sine_in(samples_per_frame * channels);
    double in_energy = 0;
    for (int i = 0; i < samples_per_frame; ++i) {
        int16_t sample = static_cast<int16_t>(10000.0 * std::sin(2.0 * M_PI * 1000.0 * i / sample_rate));
        sine_in[i * 2] = sample;
        sine_in[i * 2 + 1] = sample;
        in_energy += static_cast<double>(sample) * sample;
    }
    in_energy = std::sqrt(in_energy / sine_in.size());

    std::vector<uint8_t> opus_sine = encoder.encodeFrame(sine_in.data(), samples_per_frame);
    EXPECT_FALSE(opus_sine.empty());

    std::vector<int16_t> sine_out(samples_per_frame * channels);
    decoded = opus_decode(decoder, opus_sine.data(), static_cast<opus_int32>(opus_sine.size()), sine_out.data(), samples_per_frame, 0);
    EXPECT_EQ(decoded, samples_per_frame);

    double out_energy = 0;
    for (size_t i = 0; i < sine_out.size(); ++i) {
        out_energy += static_cast<double>(sine_out[i]) * sine_out[i];
    }
    out_energy = std::sqrt(out_energy / sine_out.size());

    // Energy of output signal should be very close to input signal energy (within 25%)
    EXPECT_NEAR(in_energy, out_energy, in_energy * 0.25);

    opus_decoder_destroy(decoder);
}

// 3. PcmQueue Thread-Safety & FIFO Tests
TEST(PcmQueueTest, FifoAndThreadSafety) {
    PcmQueue queue;
    
    // Test FIFO
    AudioFrame f1; f1.pts_ms = 100; f1.pcm = {1, 2, 3};
    AudioFrame f2; f2.pts_ms = 200; f2.pcm = {4, 5, 6};

    queue.push(f1);
    queue.push(f2);
    EXPECT_EQ(queue.size(), 2);

    AudioFrame out1, out2;
    EXPECT_TRUE(queue.pop(out1));
    EXPECT_EQ(out1.pts_ms, 100);
    EXPECT_EQ(out1.pcm[0], 1);

    EXPECT_TRUE(queue.pop(out2));
    EXPECT_EQ(out2.pts_ms, 200);
    EXPECT_EQ(out2.pcm[0], 4);

    EXPECT_FALSE(queue.pop(out1)); // Empty queue pop

    // Multi-threaded stress test
    std::atomic<bool> start_flag{false};
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    const int count_per_thread = 500;
    const int num_threads = 4;

    for (int t = 0; t < num_threads; ++t) {
        producers.emplace_back([&queue, &start_flag, count_per_thread]() {
            while (!start_flag) std::this_thread::yield();
            for (int i = 0; i < count_per_thread; ++i) {
                AudioFrame frame;
                frame.pts_ms = i;
                queue.push(std::move(frame));
            }
        });
    }

    std::atomic<int> total_consumed{0};
    for (int t = 0; t < num_threads; ++t) {
        consumers.emplace_back([&queue, &start_flag, &total_consumed, count_per_thread, num_threads]() {
            while (!start_flag) std::this_thread::yield();
            // Try to consume items until we get all of them
            int consumed = 0;
            const int target = count_per_thread * num_threads;
            while (total_consumed.load() < target) {
                AudioFrame out;
                if (queue.pop(out)) {
                    consumed++;
                    total_consumed++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    start_flag = true;
    for (auto& th : producers) th.join();
    for (auto& th : consumers) th.join();

    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(total_consumed.load(), num_threads * count_per_thread);
}

// 4. Resampler Tests
TEST(ResamplerTest, LinearResampleScale) {
    Resampler resampler;
    auto track = std::make_shared<Track>();
    track->channels = 2;
    track->sample_rate = 44100;
    track->total_frame_count = 44100; // 1 second
    track->pcm_data.resize(track->total_frame_count * track->channels, 500); // filled with 500s

    resampler.resample(track, 48000);
    EXPECT_EQ(track->sample_rate, 48000);
    // 44100 frames at 44100Hz scaled to 48000Hz should result in 48000 frames
    EXPECT_EQ(track->total_frame_count, 48000);
    EXPECT_EQ(track->pcm_data.size(), 48000 * 2);
    // Values should be preserved (linear interpolation of flat signal is flat)
    EXPECT_EQ(track->pcm_data[0], 500);
}

// 5. UdpAudioServer handleVoicePacket Broadcast Exclusion (Regression Test 4)
TEST(UdpAudioServerTest, HandleVoicePacketBroadcastExcludesSender) {
    UserManager user_manager;
    RoomManager room_manager(user_manager);

    UdpAudioServer server(0, room_manager, ".");

    // Create room
    uint16_t room_id = room_manager.create_room("TestRoom");

    // Add users
    User* sender_user = user_manager.add(101);
    ASSERT_NE(sender_user, nullptr);
    sender_user->name = "sender";
    sender_user->udp_endpoint = udp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 5001);

    User* receiver_user = user_manager.add(102);
    ASSERT_NE(receiver_user, nullptr);
    receiver_user->name = "receiver";
    receiver_user->udp_endpoint = udp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 5002);

    // Join room in room manager
    room_manager.join_room(*sender_user, room_id);
    room_manager.join_room(*receiver_user, room_id);

    // Build voice packet buffer
    std::vector<uint8_t> opus = {1, 2, 3};
    auto packet = MusicStreamer::buildVoicePacket(101, 5, opus);

    // Call handleVoicePacket
    auto result_opt = server.handleVoicePacket(packet, packet.size(), sender_user->udp_endpoint.value());
    ASSERT_TRUE(result_opt.has_value());

    // Verify broadcast target endpoints: should contain receiver, but NOT sender
    EXPECT_EQ(result_opt->endpoints.size(), 1);
    EXPECT_EQ(result_opt->endpoints[0], receiver_user->udp_endpoint.value());
    EXPECT_NE(result_opt->endpoints[0], sender_user->udp_endpoint.value());
}

// 6. UdpClient Track Change: server_position_ms_ оновлюється при стрибку позиції (Regression Test 2)
// NOTE: цей тест перевіряє ТІЛЬКИ оновлення server_position_ms_.
// Виклик opus_decoder_ctl(OPUS_RESET_STATE) відбувається всередині, але перевірити
// його без моку неможливо — тест не намагається це зробити.
TEST(UdpClientTest, TrackChangePositionResync) {
    boost::asio::io_context io_context;
    UdpClient client(io_context, "127.0.0.1", 12345, 777);

    // Initialize Opus decoder (потрібен щоб opus_decode не крешнув з nullptr)
    int error = 0;
    client.decoder_ = opus_decoder_create(SAMPLE_RATE, CHANNELS, &error);
    ASSERT_EQ(error, OPUS_OK);

    // Попередня позиція сервера висока — симулюємо перехід на новий трек
    client.server_position_ms_.store(5000);

    // Incoming music packet: new_position = 100, тобто new_position + 500 (600) < 5000
    std::vector<uint8_t> opus_payload = {0xFC, 0xFF, 0x55};
    auto packet = MusicStreamer::buildPacket(0, opus_payload, 100);
    client.recv_buffer_.assign(packet.begin(), packet.end());

    client.handle_music_packet(packet.size());

    // server_position_ms_ має оновитися до нової позиції (100)
    EXPECT_EQ(client.server_position_ms_.load(), 100u);

    opus_decoder_destroy(client.decoder_);
    client.decoder_ = nullptr;
}

// 7. UdpClient Voice Sequence Gap reset on seq=0 (Regression Test 3)
TEST(UdpClientTest, VoiceSequenceGapResetOnSeqZero) {
    boost::asio::io_context io_context;
    UdpClient client(io_context, "127.0.0.1", 12345, 777);

    int error = 0;
    client.voice_decoder_ = opus_decoder_create(SAMPLE_RATE, CHANNELS, &error);
    ASSERT_EQ(error, OPUS_OK);

    // Add 1 frame to voice queue so it is not empty
    AudioFrame dummy_frame;
    dummy_frame.pcm.resize(SAMPLES_PER_FRAME * CHANNELS, 0);
    client.voice_queue_.push(dummy_frame);

    client.first_voice_packet_ = false;
    client.expected_voice_seq_ = 100;

    // Construct voice packet with seq = 0 from sender 999
    std::vector<uint8_t> opus_payload(10, 0);
    auto packet = MusicStreamer::buildVoicePacket(999, 0, opus_payload);

    // Copy to client's buffer
    client.recv_buffer_.assign(packet.begin(), packet.end());

    // Process voice packet - should reset first_voice_packet_ to true and NOT trigger sequence gap warning
    client.handle_voice_packet(packet.size());

    // Expect first_voice_packet_ was reset to false (after processing it sets to false, but skipped the gap check!)
    EXPECT_FALSE(client.first_voice_packet_);
    EXPECT_EQ(client.expected_voice_seq_, 1);

    opus_decoder_destroy(client.voice_decoder_);
    client.voice_decoder_ = nullptr;
}

// 8. UdpClient process_audio Mixer and Prebuffering tests
TEST(UdpClientTest, ProcessAudioMixAndPrebuffer) {
    boost::asio::io_context io_context;
    UdpClient client(io_context, "127.0.0.1", 12345, 777);

    // Test mixing (music + voice)
    std::vector<int16_t> output_buffer(SAMPLES_PER_FRAME * CHANNELS, 0);

    // 1. Queue is empty. We are in prebuffering. So it should output 0.
    client.prebuffering_ = true;
    client.process_audio(output_buffer.data(), SAMPLES_PER_FRAME);
    for (auto val : output_buffer) {
        EXPECT_EQ(val, 0);
    }

    // Disable prebuffering manually
    client.prebuffering_ = false;

    // Populate music queue with 1 frame of value 100
    AudioFrame music_frame;
    music_frame.pcm.resize(SAMPLES_PER_FRAME * CHANNELS, 100);
    music_frame.pts_ms = 40;
    client.pcm_queue_.push(music_frame);

    // Populate voice queue with 1 frame of value 200
    AudioFrame voice_frame;
    voice_frame.pcm.resize(SAMPLES_PER_FRAME * CHANNELS, 200);
    client.voice_queue_.push(voice_frame);
    client.voice_prebuffering_ = false;

    // Process audio
    client.process_audio(output_buffer.data(), SAMPLES_PER_FRAME);

    // Expecting sum = 300
    EXPECT_EQ(output_buffer[0], 300);
    EXPECT_EQ(output_buffer[output_buffer.size() - 1], 300);
    EXPECT_EQ(client.playback_position_ms_.load(), 40);
}
// 9. handle_music_packet: sequence gap detection
// Перевіряє що expected_seq_ оновлюється коректно і що перший пакет
// не тригерить gap warning (first_packet_ flag).
TEST(UdpClientTest, MusicPacketSequenceTracking) {
    boost::asio::io_context io_context;
    UdpClient client(io_context, "127.0.0.1", 12345, 777);

    int error = 0;
    client.decoder_ = opus_decoder_create(SAMPLE_RATE, CHANNELS, &error);
    ASSERT_EQ(error, OPUS_OK);

    // Стан за замовчуванням: first_packet_ = true
    EXPECT_TRUE(client.first_packet_);

    // Перший пакет seq=5. first_packet_=true → gap check пропускається.
    // Після: first_packet_=false, expected_seq_=6.
    auto pkt1 = MusicStreamer::buildPacket(5, {0xFC, 0xFF, 0x55}, 0);
    client.recv_buffer_.assign(pkt1.begin(), pkt1.end());
    client.handle_music_packet(pkt1.size());
    EXPECT_FALSE(client.first_packet_);
    EXPECT_EQ(client.expected_seq_, 6u);

    // Другий пакет seq=6 (очікуваний) → expected_seq_ стає 7.
    auto pkt2 = MusicStreamer::buildPacket(6, {0xFC, 0xFF, 0x55}, 20);
    client.recv_buffer_.assign(pkt2.begin(), pkt2.end());
    client.handle_music_packet(pkt2.size());
    EXPECT_EQ(client.expected_seq_, 7u);

    // Третій пакет seq=99 (пропуск!). expected_seq_ оновлюється до 100.
    // Gap warning логується, але expected_seq_ все одно оновлюється.
    auto pkt3 = MusicStreamer::buildPacket(99, {0xFC, 0xFF, 0x55}, 40);
    client.recv_buffer_.assign(pkt3.begin(), pkt3.end());
    client.handle_music_packet(pkt3.size());
    EXPECT_EQ(client.expected_seq_, 100u);

    opus_decoder_destroy(client.decoder_);
    client.decoder_ = nullptr;
}

// 10. process_audio: drift compensation — черга переповнюється (drift_frames > 3)
// При queue_size > PREBUFFER_FRAMES + 3 (тобто > 13), один фрейм непомітно дропається.
TEST(UdpClientTest, ProcessAudioDriftSkipsFrameOnOverflow) {
    boost::asio::io_context io_context;
    UdpClient client(io_context, "127.0.0.1", 12345, 777);
    client.prebuffering_ = false;

    // PREBUFFER_FRAMES=10. drift_frames = queue_size - 10.
    // Треба drift_frames > 3 → queue_size > 13. Заповнюємо 14 фреймів.
    // pcm value = i+1, pts_ms = i — щоб відстежити який саме фрейм вийшов.
    const size_t fill = 14;
    for (size_t i = 0; i < fill; ++i) {
        AudioFrame f;
        f.pcm.resize(SAMPLES_PER_FRAME * CHANNELS, static_cast<int16_t>(i + 1));
        f.pts_ms = static_cast<uint32_t>(i);
        client.pcm_queue_.push(f);
    }
    EXPECT_EQ(client.pcm_queue_.size(), 14u);

    std::vector<int16_t> out(SAMPLES_PER_FRAME * CHANNELS, 0);
    client.process_audio(out.data(), SAMPLES_PER_FRAME);

    // drift_frames = 14-10 = 4 > 3 → фрейм i=0 (value=1) дропнуто,
    // читається i=1 (value=2). Після pop-dummy + pop-frame залишається 12.
    EXPECT_EQ(client.pcm_queue_.size(), 12u);
    EXPECT_EQ(out[0], 2);
    EXPECT_EQ(out[SAMPLES_PER_FRAME * CHANNELS - 1], 2);
}

// 11. process_audio: drift < -3, є last_frame_ → повторюємо last_frame_, рання return
TEST(UdpClientTest, ProcessAudioDriftRepeatsLastFrameOnUnderflow) {
    boost::asio::io_context io_context;
    UdpClient client(io_context, "127.0.0.1", 12345, 777);
    client.prebuffering_ = false;

    // Встановлюємо last_frame_ з відомими значеннями
    client.last_frame_.assign(SAMPLES_PER_FRAME * CHANNELS, 42);

    // Черга порожня: drift_frames = 0-10 = -10 < -3
    EXPECT_EQ(client.pcm_queue_.size(), 0u);

    std::vector<int16_t> out(SAMPLES_PER_FRAME * CHANNELS, 0);
    client.process_audio(out.data(), SAMPLES_PER_FRAME);

    // last_frame_ скопійовано у вивід, рання return — нічого з черги не pop-нули
    EXPECT_EQ(out[0], 42);
    EXPECT_EQ(out[SAMPLES_PER_FRAME * CHANNELS - 1], 42);
    EXPECT_EQ(client.pcm_queue_.size(), 0u);
    // playback_position_ms_ НЕ оновлюється — рання return до pop
    EXPECT_EQ(client.playback_position_ms_.load(), 0u);
}

// 12. process_audio: drift < -3, last_frame_ порожній → silent output + underrun
TEST(UdpClientTest, ProcessAudioDriftSilentUnderflowNoLastFrame) {
    boost::asio::io_context io_context;
    UdpClient client(io_context, "127.0.0.1", 12345, 777);
    client.prebuffering_ = false;

    // last_frame_ порожній за замовчуванням
    EXPECT_TRUE(client.last_frame_.empty());

    // Черга порожня → drift < -3, last_frame_ порожній → падає в загальну гілку underrun
    std::vector<int16_t> out(SAMPLES_PER_FRAME * CHANNELS, 99);
    client.process_audio(out.data(), SAMPLES_PER_FRAME);

    // Output — тиша (memset 0)
    EXPECT_EQ(out[0], 0);
    EXPECT_EQ(out[SAMPLES_PER_FRAME * CHANNELS - 1], 0);
    EXPECT_EQ(client.underruns_.load(), 1u);
    EXPECT_EQ(client.consecutive_underruns_, 1);
}

// 13. process_audio: 3 послідовні underruns → prebuffering_ скидається в true
TEST(UdpClientTest, ProcessAudioConsecutiveUnderrunsResetsPrebuffering) {
    boost::asio::io_context io_context;
    UdpClient client(io_context, "127.0.0.1", 12345, 777);
    client.prebuffering_ = false;
    // last_frame_ порожній → underrun path при порожній черзі

    std::vector<int16_t> out(SAMPLES_PER_FRAME * CHANNELS, 0);

    client.process_audio(out.data(), SAMPLES_PER_FRAME);
    EXPECT_FALSE(client.prebuffering_);
    EXPECT_EQ(client.consecutive_underruns_, 1);

    client.process_audio(out.data(), SAMPLES_PER_FRAME);
    EXPECT_FALSE(client.prebuffering_);
    EXPECT_EQ(client.consecutive_underruns_, 2);

    // 3-й underrun → consecutive_underruns_ >= 3 → prebuffering_=true, лічильник скидається
    client.process_audio(out.data(), SAMPLES_PER_FRAME);
    EXPECT_TRUE(client.prebuffering_);
    EXPECT_EQ(client.consecutive_underruns_, 0);
    EXPECT_EQ(client.underruns_.load(), 3u);
}

// 14. handleVoicePacket edge case: bytes < 9 → повертає nullopt
TEST(UdpAudioServerTest, HandleVoicePacketRejectsShortPacket) {
    UserManager user_manager;
    RoomManager room_manager(user_manager);
    UdpAudioServer server(0, room_manager, ".");

    // Пакет лише 5 байт — менший за мінімальний заголовок (9)
    std::vector<uint8_t> short_buf = {0x02, 0x00, 0x00, 0x00, 0x65};
    udp::endpoint dummy_ep(boost::asio::ip::make_address("127.0.0.1"), 9000);

    auto result = server.handleVoicePacket(short_buf, short_buf.size(), dummy_ep);
    EXPECT_FALSE(result.has_value());
}

// 15. handleVoicePacket edge case: відправник не в жодній кімнаті (room_id==0) → nullopt
TEST(UdpAudioServerTest, HandleVoicePacketRejectsUnknownSender) {
    UserManager user_manager;
    RoomManager room_manager(user_manager);
    UdpAudioServer server(0, room_manager, ".");

    // client_id=999 не зареєстровано в жодній кімнаті
    std::vector<uint8_t> opus = {1, 2, 3};
    auto packet = MusicStreamer::buildVoicePacket(999, 0, opus);
    udp::endpoint dummy_ep(boost::asio::ip::make_address("127.0.0.1"), 9001);

    auto result = server.handleVoicePacket(packet, packet.size(), dummy_ep);
    EXPECT_FALSE(result.has_value());
}
