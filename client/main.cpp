#include <iostream>
#include <sstream>
#include <thread>

#include <boost/asio.hpp>

#include "ClientApp.h"
#include "Logger.h"
#include "Protocol.h"

namespace {

// Хост сервера - аргумент командного рядка для віддаленого тестування,
// з дефолтом на localhost для звичної локальної розробки.
std::string resolve_server_host(int argc, char** argv)
{
    if (argc >= 2) {
        return argv[1];
    }
    return "127.0.0.1";
}

const char* HELP_TEXT =
    "Commands:\n"
    "  /connect <username>\n"
    "  /create <room_name>\n"
    "  /join <room_id>\n"
    "  /leave\n"
    "  /list\n"
    "  /ping\n"
    "  /quit";

bool handle_line(const std::string& line, ClientApp& app)
{
    if (line.empty()) {
        return true;
    }

    std::istringstream iss(line);
    std::string command;
    iss >> command;

    if (command.empty() || command.front() != '/') {
        Logger::print("Unknown input, commands start with '/'. Type /help.");
        return true;
    }
    command.erase(0, 1);

    if (command == "quit") {
        return false;
    }
    if (command == "help") {
        Logger::print(HELP_TEXT);
        return true;
    }
    if (command == "connect") {
        std::string username;
        iss >> username;
        if (username.empty()) {
            Logger::print("Usage: /connect <username>");
            return true;
        }
        app.send_connect(username);
        return true;
    }
    if (command == "create") {
        std::string room_name;
        iss >> room_name;
        if (room_name.empty()) {
            Logger::print("Usage: /create <room_name>");
            return true;
        }
        app.send_create_room(room_name);
        return true;
    }
    if (command == "join") {
        std::string room_id_str;
        iss >> room_id_str;
        if (room_id_str.empty()) {
            Logger::print("Usage: /join <room_id>");
            return true;
        }
        try {
            app.send_join_room(static_cast<uint16_t>(std::stoi(room_id_str)));
        } catch (const std::exception&) {
            Logger::print("Invalid room_id");
        }
        return true;
    }
    if (command == "leave") {
        app.send_leave_room();
        return true;
    }
    if (command == "list") {
        app.send_list_rooms();
        return true;
    }
    if (command == "ping") {
        app.send_ping();
        return true;
    }

    Logger::print("Unknown command: /" + command + " (type /help)");
    return true;
}

void cli_loop(boost::asio::io_context& io_context, std::shared_ptr<ClientApp> app)
{
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;  // EOF / stdin закрито
        }
        if (!handle_line(line, *app)) {
            break;  // /quit
        }
    }
    io_context.stop();
}

}

int main(int argc, char** argv)
{
    const std::string server_host = resolve_server_host(argc, argv);
    const uint16_t server_port = static_cast<uint16_t>(TCP_SERVER_PORT);

    boost::asio::io_context io_context;
    auto app = ClientApp::create(io_context);

    app->connect(server_host, server_port, [](bool ok) {
        if (ok) {
            Logger::print("TCP connected. Type /connect <username> to register. (/help for commands)");
        } else {
            Logger::print("Failed to connect to server.");
        }
    });

    std::thread cli_thread(cli_loop, std::ref(io_context), app);

    io_context.run();

    if (cli_thread.joinable()) {
        cli_thread.join();
    }

    return 0;
}
