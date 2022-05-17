#pragma once

#include <vxlnetwork/boost/asio/ip/tcp.hpp>
#include <vxlnetwork/lib/logger_mt.hpp>
#include <vxlnetwork/lib/rpc_handler_interface.hpp>
#include <vxlnetwork/lib/rpcconfig.hpp>

namespace boost
{
namespace asio
{
	class io_context;
}
}

namespace vxlnetwork
{
class rpc_handler_interface;

class rpc
{
public:
	rpc (boost::asio::io_context & io_ctx_a, vxlnetwork::rpc_config config_a, vxlnetwork::rpc_handler_interface & rpc_handler_interface_a);
	virtual ~rpc ();
	void start ();
	virtual void accept ();
	void stop ();

	std::uint16_t listening_port ()
	{
		return acceptor.local_endpoint ().port ();
	}

	vxlnetwork::rpc_config config;
	boost::asio::ip::tcp::acceptor acceptor;
	vxlnetwork::logger_mt logger;
	boost::asio::io_context & io_ctx;
	vxlnetwork::rpc_handler_interface & rpc_handler_interface;
	bool stopped{ false };
};

/** Returns the correct RPC implementation based on TLS configuration */
std::unique_ptr<vxlnetwork::rpc> get_rpc (boost::asio::io_context & io_ctx_a, vxlnetwork::rpc_config const & config_a, vxlnetwork::rpc_handler_interface & rpc_handler_interface_a);
}
