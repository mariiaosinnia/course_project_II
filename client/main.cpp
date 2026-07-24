#include <thread>

#include <boost/asio/io_context.hpp>

#include "AppUi.h"
#include "ClientApp.h"
#include "Protocol.h"

int main()
{
    boost::asio::io_context io;
    auto work_guard = boost::asio::make_work_guard(io);
    auto app = ClientApp::create(io);

    app->set_server_address("138.199.165.208", TCP_SERVER_PORT);

    std::thread io_thread([&io] {
        io.run();
    });

    AppUI ui(*app);
    ui.run();

    work_guard.reset();
    io.stop();
    if (io_thread.joinable()) {
        io_thread.join();
    }

    return 0;
}
