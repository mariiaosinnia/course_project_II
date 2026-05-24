#include <boost/asio.hpp>

class TCPServer
{
public:
	TCPServer(boost::asio::io_context& io_cntxt);
	~TCPServer() = default;
	void run();
private:
	const int port = 12345;

	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::acceptor acceptor;

	void start_accept();
};