#pragma once

#include <vxlnetwork/ipc_flatbuffers_lib/generated/flatbuffers/vxlnetworkapi_generated.h>

#include <memory>

namespace vxlnetwork
{
class amount;
class block;
class send_block;
class receive_block;
class change_block;
class open_block;
class state_block;
namespace ipc
{
	/**
	 * Utilities to convert between blocks and Flatbuffers equivalents
	 */
	class flatbuffers_builder
	{
	public:
		static vxlnetworkapi::BlockUnion block_to_union (vxlnetwork::block const & block_a, vxlnetwork::amount const & amount_a, bool is_state_send_a = false, bool is_state_epoch_a = false);
		static std::unique_ptr<vxlnetworkapi::BlockStateT> from (vxlnetwork::state_block const & block_a, vxlnetwork::amount const & amount_a, bool is_state_send_a, bool is_state_epoch_a);
		static std::unique_ptr<vxlnetworkapi::BlockSendT> from (vxlnetwork::send_block const & block_a);
		static std::unique_ptr<vxlnetworkapi::BlockReceiveT> from (vxlnetwork::receive_block const & block_a);
		static std::unique_ptr<vxlnetworkapi::BlockOpenT> from (vxlnetwork::open_block const & block_a);
		static std::unique_ptr<vxlnetworkapi::BlockChangeT> from (vxlnetwork::change_block const & block_a);
	};
}
}
