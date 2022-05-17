#include <vxlnetwork/lib/blocks.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/node/ipc/flatbuffers_util.hpp>
#include <vxlnetwork/secure/common.hpp>

std::unique_ptr<vxlnetworkapi::BlockStateT> vxlnetwork::ipc::flatbuffers_builder::from (vxlnetwork::state_block const & block_a, vxlnetwork::amount const & amount_a, bool is_state_send_a, bool is_state_epoch_a)
{
	auto block (std::make_unique<vxlnetworkapi::BlockStateT> ());
	block->account = block_a.account ().to_account ();
	block->hash = block_a.hash ().to_string ();
	block->previous = block_a.previous ().to_string ();
	block->representative = block_a.representative ().to_account ();
	block->balance = block_a.balance ().to_string_dec ();
	block->link = block_a.link ().to_string ();
	block->link_as_account = block_a.link ().to_account ();
	block_a.signature.encode_hex (block->signature);
	block->work = vxlnetwork::to_string_hex (block_a.work);

	if (is_state_send_a)
	{
		block->subtype = vxlnetworkapi::BlockSubType::BlockSubType_send;
	}
	else if (block_a.link ().is_zero ())
	{
		block->subtype = vxlnetworkapi::BlockSubType::BlockSubType_change;
	}
	else if (amount_a == 0 && is_state_epoch_a)
	{
		block->subtype = vxlnetworkapi::BlockSubType::BlockSubType_epoch;
	}
	else
	{
		block->subtype = vxlnetworkapi::BlockSubType::BlockSubType_receive;
	}
	return block;
}

std::unique_ptr<vxlnetworkapi::BlockSendT> vxlnetwork::ipc::flatbuffers_builder::from (vxlnetwork::send_block const & block_a)
{
	auto block (std::make_unique<vxlnetworkapi::BlockSendT> ());
	block->hash = block_a.hash ().to_string ();
	block->balance = block_a.balance ().to_string_dec ();
	block->destination = block_a.hashables.destination.to_account ();
	block->previous = block_a.previous ().to_string ();
	block_a.signature.encode_hex (block->signature);
	block->work = vxlnetwork::to_string_hex (block_a.work);
	return block;
}

std::unique_ptr<vxlnetworkapi::BlockReceiveT> vxlnetwork::ipc::flatbuffers_builder::from (vxlnetwork::receive_block const & block_a)
{
	auto block (std::make_unique<vxlnetworkapi::BlockReceiveT> ());
	block->hash = block_a.hash ().to_string ();
	block->source = block_a.source ().to_string ();
	block->previous = block_a.previous ().to_string ();
	block_a.signature.encode_hex (block->signature);
	block->work = vxlnetwork::to_string_hex (block_a.work);
	return block;
}

std::unique_ptr<vxlnetworkapi::BlockOpenT> vxlnetwork::ipc::flatbuffers_builder::from (vxlnetwork::open_block const & block_a)
{
	auto block (std::make_unique<vxlnetworkapi::BlockOpenT> ());
	block->hash = block_a.hash ().to_string ();
	block->source = block_a.source ().to_string ();
	block->account = block_a.account ().to_account ();
	block->representative = block_a.representative ().to_account ();
	block_a.signature.encode_hex (block->signature);
	block->work = vxlnetwork::to_string_hex (block_a.work);
	return block;
}

std::unique_ptr<vxlnetworkapi::BlockChangeT> vxlnetwork::ipc::flatbuffers_builder::from (vxlnetwork::change_block const & block_a)
{
	auto block (std::make_unique<vxlnetworkapi::BlockChangeT> ());
	block->hash = block_a.hash ().to_string ();
	block->previous = block_a.previous ().to_string ();
	block->representative = block_a.representative ().to_account ();
	block_a.signature.encode_hex (block->signature);
	block->work = vxlnetwork::to_string_hex (block_a.work);
	return block;
}

vxlnetworkapi::BlockUnion vxlnetwork::ipc::flatbuffers_builder::block_to_union (vxlnetwork::block const & block_a, vxlnetwork::amount const & amount_a, bool is_state_send_a, bool is_state_epoch_a)
{
	vxlnetworkapi::BlockUnion u;
	switch (block_a.type ())
	{
		case vxlnetwork::block_type::state:
		{
			u.Set (*from (dynamic_cast<vxlnetwork::state_block const &> (block_a), amount_a, is_state_send_a, is_state_epoch_a));
			break;
		}
		case vxlnetwork::block_type::send:
		{
			u.Set (*from (dynamic_cast<vxlnetwork::send_block const &> (block_a)));
			break;
		}
		case vxlnetwork::block_type::receive:
		{
			u.Set (*from (dynamic_cast<vxlnetwork::receive_block const &> (block_a)));
			break;
		}
		case vxlnetwork::block_type::open:
		{
			u.Set (*from (dynamic_cast<vxlnetwork::open_block const &> (block_a)));
			break;
		}
		case vxlnetwork::block_type::change:
		{
			u.Set (*from (dynamic_cast<vxlnetwork::change_block const &> (block_a)));
			break;
		}

		default:
			debug_assert (false);
	}
	return u;
}
