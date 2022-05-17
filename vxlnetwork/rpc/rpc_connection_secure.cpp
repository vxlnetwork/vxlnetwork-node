#include <vxlnetwork/boost/asio/bind_executor.hpp>
#include <vxlnetwork/rpc/rpc_connection_secure.hpp>
#include <vxlnetwork/rpc/rpc_secure.hpp>

#include <boost/polymorphic_pointer_cast.hpp>

vxlnetwork::rpc_connection_secure::rpc_connection_secure (vxlnetwork::rpc_config const & rpc_config, boost::asio::io_context & io_ctx, vxlnetwork::logger_mt & logger, vxlnetwork::rpc_handler_interface & rpc_handler_interface, boost::asio::ssl::context & ssl_context) :
	vxlnetwork::rpc_connection (rpc_config, io_ctx, logger, rpc_handler_interface),
	stream (socket, ssl_context)
{
}

void vxlnetwork::rpc_connection_secure::parse_connection ()
{
	// Perform the SSL handshake
	auto this_l = std::static_pointer_cast<vxlnetwork::rpc_connection_secure> (shared_from_this ());
	stream.async_handshake (boost::asio::ssl::stream_base::server,
	boost::asio::bind_executor (this_l->strand, [this_l] (auto & ec) {
		this_l->handle_handshake (ec);
	}));
}

void vxlnetwork::rpc_connection_secure::on_shutdown (boost::system::error_code const & error)
{
	// No-op. We initiate the shutdown (since the RPC server kills the connection after each request)
	// and we'll thus get an expected EOF error. If the client disconnects, a short-read error will be expected.
}

void vxlnetwork::rpc_connection_secure::handle_handshake (boost::system::error_code const & error)
{
	if (!error)
	{
		read (stream);
	}
	else
	{
		logger.always_log ("TLS: Handshake error: ", error.message ());
	}
}

void vxlnetwork::rpc_connection_secure::write_completion_handler (std::shared_ptr<vxlnetwork::rpc_connection> const & rpc)
{
	auto rpc_connection_secure = boost::polymorphic_pointer_downcast<vxlnetwork::rpc_connection_secure> (rpc);
	rpc_connection_secure->stream.async_shutdown (boost::asio::bind_executor (rpc->strand, [rpc_connection_secure] (auto const & ec_shutdown) {
		rpc_connection_secure->on_shutdown (ec_shutdown);
	}));
}
