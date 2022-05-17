#pragma once

#include <vxlnetwork/ipc_flatbuffers_lib/generated/flatbuffers/vxlnetworkapi_generated.h>
#include <vxlnetwork/lib/errors.hpp>
#include <vxlnetwork/lib/ipc.hpp>
#include <vxlnetwork/node/ipc/ipc_access_config.hpp>
#include <vxlnetwork/node/ipc/ipc_broker.hpp>
#include <vxlnetwork/node/node_rpc_config.hpp>

#include <atomic>
#include <memory>
#include <mutex>

namespace flatbuffers
{
class Parser;
}
namespace vxlnetwork
{
class node;
class error;
namespace ipc
{
	class access;
	/** The IPC server accepts connections on one or more configured transports */
	class ipc_server final
	{
	public:
		ipc_server (vxlnetwork::node & node, vxlnetwork::node_rpc_config const & node_rpc_config);
		~ipc_server ();
		void stop ();

		std::optional<std::uint16_t> listening_tcp_port () const;

		vxlnetwork::node & node;
		vxlnetwork::node_rpc_config const & node_rpc_config;

		/** Unique counter/id shared across sessions */
		std::atomic<uint64_t> id_dispenser{ 1 };
		std::shared_ptr<vxlnetwork::ipc::broker> get_broker ();
		vxlnetwork::ipc::access & get_access ();
		vxlnetwork::error reload_access_config ();

	private:
		void setup_callbacks ();
		std::shared_ptr<vxlnetwork::ipc::broker> broker;
		vxlnetwork::ipc::access access;
		std::unique_ptr<dsock_file_remover> file_remover;
		std::vector<std::shared_ptr<vxlnetwork::ipc::transport>> transports;
	};
}
}
