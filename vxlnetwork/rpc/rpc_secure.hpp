#pragma once
#include <vxlnetwork/rpc/rpc.hpp>

namespace boost
{
namespace asio
{
	class io_context;
}
}

namespace vxlnetwork
{
/**
 * Specialization of vxlnetwork::rpc with TLS support
 */
class rpc_secure : public rpc
{
public:
	rpc_secure (boost::asio::io_context & context_a, vxlnetwork::rpc_config const & config_a, vxlnetwork::rpc_handler_interface & rpc_handler_interface_a);

	/** Starts accepting connections */
	void accept () override;
};
}
