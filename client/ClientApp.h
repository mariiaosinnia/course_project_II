#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>

#include "ClientState.h"
#include "Protocol.h"
#include "TCPClient.h"

// Головний клас клієнтського застосунку.
//
// Володіє TCPClient і ClientState, перетворює вхідні пакети на
// конкретні дії (on_connected, on_room_created, ...) замість одного
// великого switch у main(). Кожен тип пакета - окремий, тестований
// метод, що полегшує підтримку, коли протокол розростається.

class ClientApp : public std::enable_shared_from_this<ClientApp> {
public:
    static std::shared_ptr<ClientApp> create(boost::asio::io_context& io_context);

    // Підключитись до сервера. on_connected повідомляє результат
    // (успіх/невдача) тому, хто викликав connect - наприклад, щоб CLI
    // надрукував відповідне повідомлення.
    void connect(const std::string& host, uint16_t port,
                 std::function<void(bool)> on_connected);

    // Команди, що CLI транслює в мережеві запити. Кожна повертає
    // void - результат прийде асинхронно через packet-обробники.
    void send_connect(const std::string& username);
    void send_create_room(const std::string& room_name);
    void send_join_room(uint16_t room_id);
    void send_leave_room();
    void send_list_rooms();
    void send_ping();

    const ClientState& state() const { return state_; }

private:
    explicit ClientApp(boost::asio::io_context& io_context);

    // Диспетчер - єдине місце, де PacketType перетворюється на
    // конкретний обробник. Сам метод лише маршрутизує, логіка
    // кожного випадку - в окремому on_* методі нижче.
    void on_packet(PacketType type, const std::vector<uint8_t>& body);
    void on_disconnect();

    void on_connected(const std::vector<uint8_t>& body);
    void on_room_created(const std::vector<uint8_t>& body);
    void on_room_joined(const std::vector<uint8_t>& body);
    void on_room_left();
    void on_room_list(const std::vector<uint8_t>& body);
    void on_user_joined(const std::vector<uint8_t>& body);
    void on_user_left(const std::vector<uint8_t>& body);
    void on_pong();
    void on_error(const std::vector<uint8_t>& body);
    void on_unhandled(PacketType type);
    void start_udp(uint16_t udp_port);

    std::shared_ptr<TCPClient> tcp_client_;
    std::string server_host_;
    std::shared_ptr<class UdpClient> udp_client_;
    ClientState state_;
    boost::asio::io_context& io_context_;
};
