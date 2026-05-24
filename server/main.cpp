#include <boost/asio.hpp>

#include "TCPServer.h"
#include <iostream>

int main()
{
    try
    {
        boost::asio::io_context io_context;
        TCPServer server(io_context);
        std::cout << "io_context running..." << std::endl;
        io_context.run();
        std::cout << "io_context stopped" << std::endl;
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
	return 0;
}