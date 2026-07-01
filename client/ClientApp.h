#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <functional>

#include "ClientState.h"
#include "Protocol.h"
#include "TCPClient.h"

class ClientApp : public std::enable_shared_from_this<ClientApp> {
public:
    static std::shared_ptr<ClientApp> create(boost::asio::io_context& io_context);

    void set_server_address(const std::string& host, uint16_t port);
    void connect_to_server(std::function<void(bool ok)> on_connected);
    void connect(const std::string& host, uint16_t port,
                 std::function<void(bool)> on_connected);

    void send_connect(const std::string& username);
    void send_create_room(const std::string& room_name);
    void send_join_room(uint16_t room_id);
    void send_leave_room();
    void send_list_rooms();
    void send_ping();

    ClientState& state() { return state_; }
    const ClientState& state() const { return state_; }

    void set_on_connected(std::function<void(uint32_t)> cb) { on_connected_cb_ = std::move(cb); }
    void set_on_disconnected(std::function<void()> cb) { on_disconnected_cb_ = std::move(cb); }
    void set_on_room_created(std::function<void(uint16_t)> cb) { on_room_created_cb_ = std::move(cb); }
    void set_on_room_joined(std::function<void(uint16_t)> cb) { on_room_joined_cb_ = std::move(cb); }
    void set_on_room_left(std::function<void()> cb) { on_room_left_cb_ = std::move(cb); }
    void set_on_room_list_updated(std::function<void()> cb) { on_room_list_updated_cb_ = std::move(cb); }
    void set_on_user_joined(std::function<void(uint32_t, const std::string&)> cb) { on_user_joined_cb_ = std::move(cb); }
    void set_on_user_left(std::function<void(uint32_t)> cb) { on_user_left_cb_ = std::move(cb); }

private:
    explicit ClientApp(boost::asio::io_context& io_context);

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

    std::string server_host_;
    uint16_t server_port_ = 0;
    std::shared_ptr<TCPClient> tcp_client_;
    std::shared_ptr<class UdpClient> udp_client_;
    ClientState state_;
    boost::asio::io_context& io_context_;

    std::function<void(uint32_t)> on_connected_cb_;
    std::function<void()> on_disconnected_cb_;
    std::function<void(uint16_t)> on_room_created_cb_;
    std::function<void(uint16_t)> on_room_joined_cb_;
    std::function<void()> on_room_left_cb_;
    std::function<void()> on_room_list_updated_cb_;
    std::function<void(uint32_t, const std::string&)> on_user_joined_cb_;
    std::function<void(uint32_t)> on_user_left_cb_;
};