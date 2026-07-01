#include <thread>

#include <boost/asio/io_context.hpp>

#include "AppUi.h"
#include "ClientApp.h"

int main()
{
    boost::asio::io_context io;
    auto work_guard = boost::asio::make_work_guard(io);
    auto app = ClientApp::create(io);

    app->set_server_address("127.0.0.1", 12345);

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
