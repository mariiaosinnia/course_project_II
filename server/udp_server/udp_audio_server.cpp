// udp_audio_server.cpp
//
// Мінімальний UDP audio сервер:
//   1. Декодує MP3 файл у PCM (dr_mp3)
//   2. Якщо MP3 не 48000 Hz або не stereo/mono підтримуваний Opus'ом — повідомляє і виходить
//   3. Нарізає PCM на фрейми фіксованого розміру (20 мс)
//   4. Кодує кожен фрейм через Opus
//   5. Шле UDP-пакетами: [4 байти sequence_number][opus payload]
//
// Без RoomManager, без реєстрації клієнтів, без jitter buffer —
// hardcoded IP:port отримувача, для першого наскрізного тесту.

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include <opus.h>
#include <boost/asio.hpp>

#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>

using boost::asio::ip::udp;

// Opus підтримує лише ці частоти дискретизації
bool is_opus_supported_rate(int rate) {
    return rate == 8000 || rate == 12000 || rate == 16000 ||
           rate == 24000 || rate == 48000;
}

// Простий ресемплінг через лінійну інтерполяцію.
// Якість нижча за повноцінні алгоритми (sinc-based тощо), але для
// тестового наскрізного прогону цього достатньо.
//
// input        — вхідний PCM (interleaved, якщо stereo: L,R,L,R,...)
// input_frames — кількість семплів НА КАНАЛ у input
// channels     — кількість каналів
// in_rate      — вхідна частота дискретизації
// out_rate     — бажана вихідна частота
// Повертає новий PCM-буфер (interleaved) у вихідній частоті.
std::vector<drmp3_int16> resample_linear(
    const drmp3_int16* input,
    drmp3_uint64 input_frames,
    int channels,
    int in_rate,
    int out_rate)
{
    const double ratio = static_cast<double>(out_rate) / static_cast<double>(in_rate);
    const drmp3_uint64 output_frames =
        static_cast<drmp3_uint64>(static_cast<double>(input_frames) * ratio);

    std::vector<drmp3_int16> output(output_frames * channels);

    for (drmp3_uint64 out_i = 0; out_i < output_frames; ++out_i) {
        // Позиція у вхідному сигналі (дробова — між двома реальними семплами)
        double src_pos = static_cast<double>(out_i) / ratio;

        drmp3_uint64 src_index = static_cast<drmp3_uint64>(src_pos);
        double frac = src_pos - static_cast<double>(src_index);

        drmp3_uint64 src_index_next =
            (src_index + 1 < input_frames) ? src_index + 1 : src_index;

        for (int ch = 0; ch < channels; ++ch) {
            drmp3_int16 sample_a = input[src_index * channels + ch];
            drmp3_int16 sample_b = input[src_index_next * channels + ch];

            // Лінійна інтерполяція між sample_a і sample_b
            double interpolated = sample_a + (sample_b - sample_a) * frac;
            output[out_i * channels + ch] = static_cast<drmp3_int16>(interpolated);
        }
    }

    return output;
}

int main() {
    const std::string mp3_path = "summer.mp3";
    const unsigned short server_port = 9001;

    // ---------- 1. Декодуємо MP3 у PCM ----------
    drmp3_config mp3_config;
    drmp3_uint64 total_frame_count = 0;

    drmp3_int16* pcm_data = drmp3_open_file_and_read_pcm_frames_s16(
        mp3_path.c_str(), &mp3_config, &total_frame_count, nullptr);

    if (pcm_data == nullptr) {
        std::cerr << "Не вдалося прочитати/декодувати MP3 файл: " << mp3_path << "\n";
        return 1;
    }

    const int sample_rate = static_cast<int>(mp3_config.sampleRate);
    const int channels = static_cast<int>(mp3_config.channels);

    std::cout << "MP3 декодовано:\n";
    std::cout << "  sample_rate = " << sample_rate << " Hz\n";
    std::cout << "  channels    = " << channels << "\n";
    std::cout << "  frame_count = " << total_frame_count << " (семплів на канал)\n";

    int working_sample_rate = sample_rate;
    drmp3_int16* working_pcm_data = pcm_data;
    drmp3_uint64 working_frame_count = total_frame_count;
    std::vector<drmp3_int16> resampled_buffer; // тримає пам'ять, якщо ресемплінг відбувся

    if (!is_opus_supported_rate(sample_rate)) {
        const int target_rate = 48000;
        std::cout << "\nSample rate " << sample_rate << " Hz не підтримується Opus напряму.\n";
        std::cout << "Виконую ресемплінг (лінійна інтерполяція) до " << target_rate << " Hz...\n";

        resampled_buffer = resample_linear(
            pcm_data, total_frame_count, channels, sample_rate, target_rate);

        working_sample_rate = target_rate;
        working_pcm_data = resampled_buffer.data();
        working_frame_count = resampled_buffer.size() / channels;

        std::cout << "Ресемплінг завершено: " << working_frame_count
                  << " семплів/канал на " << working_sample_rate << " Hz\n\n";
    }

    if (channels != 1 && channels != 2) {
        std::cerr << "ПОМИЛКА: Opus у цьому тесті підтримує лише mono(1) або stereo(2), отримано: "
                  << channels << "\n";
        drmp3_free(pcm_data, nullptr);
        return 1;
    }

    // ---------- 2. Налаштовуємо Opus encoder ----------
    int opus_error = 0;
    OpusEncoder* encoder = opus_encoder_create(working_sample_rate, channels, OPUS_APPLICATION_AUDIO, &opus_error);
    if (opus_error != OPUS_OK) {
        std::cerr << "Не вдалося створити Opus encoder: " << opus_strerror(opus_error) << "\n";
        drmp3_free(pcm_data, nullptr);
        return 1;
    }

    // Бітрейт для музики (можна підлаштувати пізніше)
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(64000));

    // ---------- 3. Параметри нарізки на фрейми ----------
    const int frame_duration_ms = 20;
    const int samples_per_frame = working_sample_rate * frame_duration_ms / 1000; // семплів НА КАНАЛ
    const int samples_per_frame_total = samples_per_frame * channels;     // з урахуванням каналів

    std::cout << "  frame_size  = " << samples_per_frame << " семплів/канал ("
              << frame_duration_ms << " мс)\n\n";

    // Буфер під закодований Opus-пакет (з запасом)
    std::vector<unsigned char> opus_buffer(4000);

    // ---------- 4. Налаштовуємо UDP сокет ----------
    // Сервер слухає на ФІКСОВАНОМУ порту, бо клієнт має знати заздалегідь,
    // куди надсилати реєстраційні пакети (пінг).
    boost::asio::io_context io_context;
    udp::socket socket(io_context, udp::endpoint(udp::v4(), server_port));

    std::cout << "UDP сервер слухає на порту: " << server_port << "\n";
    std::cout << "Очікую реєстрацію клієнта (0xFF пінг + client_id)...\n\n";

    // ---------- 4.1 Очікуємо реєстрацію клієнта ----------
    // Протокол реєстрації — ДВА окремих UDP-пакети від клієнта:
    //   1) [1 байт: 0xFF]              — маркер "це пінг/реєстрація"
    //   2) [4 байти: client_id, BE]    — ідентифікатор клієнта (як у TCP)
    // Endpoint (ip:port) клієнта дізнаємось із самого факту прийому пакету —
    // саме так його надсилач і визначається в UDP.
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
                std::cerr << "Помилка прийому реєстраційного пакету: " << ec.message() << "\n";
                continue;
            }

            if (!got_ping) {
                // Очікуємо перший пакет: рівно 1 байт, значення 0xFF
                if (bytes == 1 && reg_buffer[0] == 0xFF) {
                    client_endpoint = sender_endpoint; // запам'ятовуємо джерело
                    got_ping = true;
                    std::cout << "Отримано пінг (0xFF) від "
                              << sender_endpoint.address().to_string()
                              << ":" << sender_endpoint.port() << "\n";
                } else {
                    std::cerr << "Очікував 0xFF пінг, отримав пакет розміром "
                              << bytes << " байт — ігнорую\n";
                }
                continue;
            }

            if (!got_id) {
                // Очікуємо другий пакет: рівно 4 байти client_id (big-endian)
                if (bytes == 4 && sender_endpoint == client_endpoint) {
                    registered_client_id =
                        (static_cast<uint32_t>(reg_buffer[0]) << 24) |
                        (static_cast<uint32_t>(reg_buffer[1]) << 16) |
                        (static_cast<uint32_t>(reg_buffer[2]) << 8) |
                        (static_cast<uint32_t>(reg_buffer[3]));
                    got_id = true;
                    std::cout << "Отримано client_id: " << registered_client_id << "\n";
                } else {
                    std::cerr << "Очікував 4-байтовий client_id з того ж endpoint, "
                              << "отримав " << bytes << " байт — ігнорую\n";
                }
            }
        }
    }

    std::cout << "\nКлієнт зареєстрований: " << client_endpoint.address().to_string()
              << ":" << client_endpoint.port()
              << " (id=" << registered_client_id << ")\n";
    std::cout << "Починаю надсилати аудіо...\n\n";

    // ---------- 5. Формат UDP-пакету ----------
    // [0..3]  sequence_number (uint32_t, big-endian)
    // [4..]   opus encoded payload
    uint32_t sequence_number = 0;

    drmp3_uint64 frames_sent = 0;
    const drmp3_uint64 total_audio_frames = working_frame_count; // семплів на канал

    // Рахуємо абсолютний "ідеальний" час відправки кожного пакету від старту,
    // а не sleep(20ms) щоразу окремо — так компенсуємо накопичену похибку
    // (encode + send теж займають час, який інакше додавався б зверху).
    auto stream_start_time = std::chrono::steady_clock::now();

    while (frames_sent + samples_per_frame <= total_audio_frames) {
        const drmp3_int16* frame_start = working_pcm_data + (frames_sent * channels);

        // Кодуємо фрейм PCM -> Opus
        int encoded_bytes = opus_encode(
            encoder,
            frame_start,
            samples_per_frame,           // кількість семплів НА КАНАЛ
            opus_buffer.data(),
            static_cast<opus_int32>(opus_buffer.size())
        );

        if (encoded_bytes < 0) {
            std::cerr << "Помилка Opus encode: " << opus_strerror(encoded_bytes) << "\n";
            break;
        }

        // Формуємо UDP-пакет: заголовок + payload
        std::vector<unsigned char> packet(4 + encoded_bytes);
        packet[0] = static_cast<unsigned char>((sequence_number >> 24) & 0xFF);
        packet[1] = static_cast<unsigned char>((sequence_number >> 16) & 0xFF);
        packet[2] = static_cast<unsigned char>((sequence_number >> 8) & 0xFF);
        packet[3] = static_cast<unsigned char>(sequence_number & 0xFF);
        std::memcpy(packet.data() + 4, opus_buffer.data(), encoded_bytes);

        socket.send_to(boost::asio::buffer(packet), client_endpoint);

        sequence_number++;
        frames_sent += samples_per_frame;

        // Цільовий момент часу для наступного пакету = старт + (номер_пакету * 20мс)
        auto target_time = stream_start_time +
            std::chrono::milliseconds(static_cast<long long>(sequence_number) * frame_duration_ms);
        std::this_thread::sleep_until(target_time);

        if (sequence_number % 50 == 0) {
            std::cout << "Надіслано пакетів: " << sequence_number << "\n";
        }
    }

    std::cout << "\nГотово. Всього надіслано пакетів: " << sequence_number << "\n";

    opus_encoder_destroy(encoder);
    drmp3_free(pcm_data, nullptr);

    return 0;
}