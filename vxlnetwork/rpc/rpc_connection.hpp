#pragma once

#include <vxlnetwork/boost/asio/ip/tcp.hpp>
#include <vxlnetwork/boost/asio/strand.hpp>
#include <vxlnetwork/boost/beast/core/flat_buffer.hpp>
#include <vxlnetwork/boost/beast/http.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include <atomic>

/* Boost v1.70 introduced breaking changes; the conditional compilation allows 1.6x to be supported as well. */
#if BOOST_VERSION < 107000
using socket_type = boost::asio::ip::tcp::socket;
#else
using socket_type = boost::asio::basic_stream_socket<boost::asio::ip::tcp, boost::asio::io_context::executor_type>;
#endif

namespace vxlnetwork
{
class logger_mt;
class rpc_config;
class rpc_handler_interface;

class rpc_connection : public std::enable_shared_from_this<vxlnetwork::rpc_connection>
{
public:
	rpc_connection (vxlnetwork::rpc_config const & rpc_config, boost::asio::io_context & io_ctx, vxlnetwork::logger_mt & logger, vxlnetwork::rpc_handler_interface & rpc_handler_interface_a);
	virtual ~rpc_connection () = default;
	virtual void parse_connection ();
	virtual void write_completion_handler (std::shared_ptr<vxlnetwork::rpc_connection> const & rpc_connection);
	void prepare_head (unsigned version, boost::beast::http::status status = boost::beast::http::status::ok);
	void write_result (std::string body, unsigned version, boost::beast::http::status status = boost::beast::http::status::ok);

	socket_type socket;
	boost::beast::flat_buffer buffer;
	boost::beast::http::response<boost::beast::http::string_body> res;
	boost::asio::strand<boost::asio::io_context::executor_type> strand;
	std::atomic_flag responded;
	boost::asio::io_context & io_ctx;
	vxlnetwork::logger_mt & logger;
	vxlnetwork::rpc_config const & rpc_config;
	vxlnetwork::rpc_handler_interface & rpc_handler_interface;

protected:
	template <typename STREAM_TYPE>
	void read (STREAM_TYPE & stream);

	template <typename STREAM_TYPE>
	void parse_request (STREAM_TYPE & stream, std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>> const & header_parser);
};
}
