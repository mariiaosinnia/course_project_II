// udp_audio_client.cpp
//
// Мінімальний UDP audio клієнт:
//   1. Відкриває UDP сокет на вільному порту (порт виводиться в консоль —
//      саме його треба передати серверу як client_port)
//   2. У циклі приймає пакети: [4 байти sequence_number][opus payload]
//   3. Декодує Opus -> PCM
//   4. Кладе PCM у просту чергу (FIFO), з якої PortAudio callback забирає
//      дані для відтворення
//
// Без jitter buffer — пакети програються в тому порядку, в якому прийшли.

#include <opus.h>
#include <portaudio.h>
#include <boost/asio.hpp>

#include <iostream>
#include <vector>
#include <deque>
#include <mutex>
#include <cstring>
#include <atomic>

using boost::asio::ip::udp;

// ---------- Параметри аудіо ----------
// Мають збігатися з тим, що використовує сервер (виведе сам сервер при старті:
// sample_rate / channels). Якщо сервер покаже інші значення — поміняйте тут.
constexpr int SAMPLE_RATE = 48000;
constexpr int CHANNELS = 2;
constexpr int FRAME_DURATION_MS = 20;
constexpr int SAMPLES_PER_FRAME = SAMPLE_RATE * FRAME_DURATION_MS / 1000; // на канал

// ---------- Проста потокобезпечна черга PCM-фреймів ----------
class PcmQueue {
public:
    void push(std::vector<int16_t> frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(frame));
    }

    // Повертає true, якщо вдалось забрати фрейм; false якщо черга порожня
    bool pop(std::vector<int16_t>& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::mutex mutex_;
    std::deque<std::vector<int16_t>> queue_;
};

PcmQueue g_pcm_queue;
std::atomic<uint64_t> g_packets_received{0};
std::atomic<uint64_t> g_underruns{0};

// ---------- PortAudio callback ----------
// PortAudio викликає цю функцію сама, коли їй потрібні нові дані для відтворення
static int pa_callback(const void* /*input*/, void* output,
                        unsigned long frame_count,
                        const PaStreamCallbackTimeInfo* /*timeInfo*/,
                        PaStreamCallbackFlags /*statusFlags*/,
                        void* /*userData*/) {
    int16_t* out = static_cast<int16_t*>(output);

    static bool prebuffering = true;
    constexpr size_t PREBUFFER_FRAMES = 10; // ~200мс запасу при 20мс/фрейм

    if (prebuffering) {
        if (g_pcm_queue.size() < PREBUFFER_FRAMES) {
            // Ще не накопичили достатньо — граємо тишу, не чіпаємо чергу
            std::memset(out, 0, frame_count * CHANNELS * sizeof(int16_t));
            return paContinue;
        }
        prebuffering = false; // буфер заповнено, можна починати
    }

    std::vector<int16_t> frame;
    if (g_pcm_queue.pop(frame) && frame.size() == frame_count * CHANNELS) {
        std::memcpy(out, frame.data(), frame.size() * sizeof(int16_t));
    } else {
        // Немає даних (черга порожня або розмір не збігається) -> тиша,
        // щоб не було тріску/сміття в динаміках
        std::memset(out, 0, frame_count * CHANNELS * sizeof(int16_t));
        g_underruns++;

        // Якщо underrun стався - повертаємось у режим пре-буферизації,
        // щоб знову накопичити запас, а не "заїкатись" далі неперервно
        prebuffering = true;
    }

    return paContinue;
}

int main() {
    const std::string server_ip = "127.0.0.1";
    const unsigned short server_port = 9001;
    const uint32_t my_client_id = 1;

    // ---------- 1. Налаштовуємо Opus decoder ----------
    int opus_error = 0;
    OpusDecoder* decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &opus_error);
    if (opus_error != OPUS_OK) {
        std::cerr << "Не вдалося створити Opus decoder: " << opus_strerror(opus_error) << "\n";
        return 1;
    }

    // ---------- 2. Налаштовуємо PortAudio ----------
    PaError pa_err = Pa_Initialize();
    if (pa_err != paNoError) {
        std::cerr << "PortAudio init error: " << Pa_GetErrorText(pa_err) << "\n";
        return 1;
    }

    PaStream* stream = nullptr;
    pa_err = Pa_OpenDefaultStream(
        &stream,
        0,                  // вхідних каналів немає (ми не пишемо з мікрофона)
        CHANNELS,           // вихідних каналів
        paInt16,            // формат семплів
        SAMPLE_RATE,
        SAMPLES_PER_FRAME,  // фреймів на буфер
        pa_callback,
        nullptr
    );

    if (pa_err != paNoError) {
        std::cerr << "Не вдалося відкрити PortAudio stream: " << Pa_GetErrorText(pa_err) << "\n";
        Pa_Terminate();
        return 1;
    }

    pa_err = Pa_StartStream(stream);
    if (pa_err != paNoError) {
        std::cerr << "Не вдалося запустити PortAudio stream: " << Pa_GetErrorText(pa_err) << "\n";
        Pa_Terminate();
        return 1;
    }

    // ---------- 3. Відкриваємо UDP сокет і реєструємось на сервері ----------
    boost::asio::io_context io_context;
    udp::socket socket(io_context, udp::endpoint(udp::v4(), 0)); // 0 = ОС сама обирає локальний порт

    udp::endpoint server_endpoint(
        boost::asio::ip::make_address(server_ip), server_port);

    std::cout << "========================================\n";
    std::cout << "Реєструюсь на сервері " << server_ip << ":" << server_port
              << " з client_id=" << my_client_id << "\n";
    std::cout << "========================================\n";

    // Пакет 1: маркер пінгу — рівно 1 байт, значення 0xFF
    {
        std::vector<unsigned char> ping_packet{0xFF};
        socket.send_to(boost::asio::buffer(ping_packet), server_endpoint);
    }

    // Пакет 2: client_id — рівно 4 байти, big-endian
    {
        std::vector<unsigned char> id_packet(4);
        id_packet[0] = static_cast<unsigned char>((my_client_id >> 24) & 0xFF);
        id_packet[1] = static_cast<unsigned char>((my_client_id >> 16) & 0xFF);
        id_packet[2] = static_cast<unsigned char>((my_client_id >> 8) & 0xFF);
        id_packet[3] = static_cast<unsigned char>(my_client_id & 0xFF);
        socket.send_to(boost::asio::buffer(id_packet), server_endpoint);
    }

    std::cout << "Реєстрацію надіслано. Очікую аудіо... (Ctrl+C для зупинки)\n\n";

    // ---------- 4. Цикл прийому пакетів ----------
    std::vector<unsigned char> recv_buffer(4000);
    udp::endpoint sender_endpoint;

    uint32_t expected_seq = 0;
    bool first_packet = true;

    while (true) {
        boost::system::error_code ec;
        size_t bytes_received = socket.receive_from(
            boost::asio::buffer(recv_buffer), sender_endpoint, 0, ec);

        if (ec) {
            std::cerr << "Помилка прийому: " << ec.message() << "\n";
            continue;
        }

        if (bytes_received < 4) {
            std::cerr << "Пакет закороткий (менше 4 байт заголовка), ігнорую\n";
            continue;
        }

        // Розбираємо заголовок: 4 байти sequence_number (big-endian)
        uint32_t seq =
            (static_cast<uint32_t>(recv_buffer[0]) << 24) |
            (static_cast<uint32_t>(recv_buffer[1]) << 16) |
            (static_cast<uint32_t>(recv_buffer[2]) << 8) |
            (static_cast<uint32_t>(recv_buffer[3]));

        if (!first_packet && seq != expected_seq) {
            std::cout << "[увага] sequence gap: очікував " << expected_seq
                      << ", отримав " << seq << " (можлива втрата пакету)\n";
        }
        expected_seq = seq + 1;
        first_packet = false;

        // Декодуємо Opus payload -> PCM
        const unsigned char* opus_payload = recv_buffer.data() + 4;
        int opus_payload_size = static_cast<int>(bytes_received - 4);

        std::vector<int16_t> pcm_frame(SAMPLES_PER_FRAME * CHANNELS);
        int decoded_samples = opus_decode(
            decoder,
            opus_payload,
            opus_payload_size,
            pcm_frame.data(),
            SAMPLES_PER_FRAME,  // максимум семплів на канал, який можемо прийняти
            0                    // FEC вимкнено
        );

        if (decoded_samples < 0) {
            std::cerr << "Помилка Opus decode: " << opus_strerror(decoded_samples) << "\n";
            continue;
        }

        g_pcm_queue.push(std::move(pcm_frame));
        g_packets_received++;

        if (g_packets_received % 50 == 0) {
            std::cout << "Прийнято пакетів: " << g_packets_received
                      << " | underruns: " << g_underruns
                      << " | у черзі: " << g_pcm_queue.size() << "\n";
        }
    }

    // (недосяжно при Ctrl+C, але для повноти)
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    opus_decoder_destroy(decoder);

    return 0;
}