#include <memory>
#include <boost/asio.hpp>

class TCPConnection : public std::enable_shared_from_this<TCPConnection>
{
public:
	~TCPConnection() = default;

	static std::shared_ptr<TCPConnection> create(boost::asio::io_context& io_context);
	boost::asio::ip::tcp::socket& get_socket();

	void start();
	void read_header();
	void read_body(uint16_t);

private:
	TCPConnection(boost::asio::io_context& io_context);

	boost::asio::ip::tcp::socket socket;
};
