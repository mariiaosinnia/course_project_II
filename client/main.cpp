#include <thread>
#include <boost/asio.hpp>

#include "AppUI.h"
#include "ClientApp.h"
#include "Protocol.h"

namespace {
std::string resolve_host(int argc, char** argv)
{
    return argc >= 2 ? argv[1] : "127.0.0.1";
}
}

int main(int argc, char** argv)
{
    boost::asio::io_context io_context;

    // ClientApp: мережева логіка + диспетчер пакетів
    auto app = ClientApp::create(io_context);

    // AppUI: всі три екрани + навігація між ними
    // Підключає app callbacks у своєму конструкторі
    AppUI ui(*app);

    // Зберігаємо host для з'єднання (AppUI викликатиме app.connect_to_server)
    const std::string host = resolve_host(argc, argv);
    const uint16_t port = static_cast<uint16_t>(TCP_SERVER_PORT);
    app->set_server_address(host, port);

    // io_context в окремому потоці — щоб не блокував FTXUI Loop()
    std::thread net([&io_context] { io_context.run(); });

    // FTXUI Loop() в main thread — вимога ОС
    ui.run();

    io_context.stop();
    if (net.joinable()) net.join();

    return 0;
}
