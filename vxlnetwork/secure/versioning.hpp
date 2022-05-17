#pragma once

#include <vxlnetwork/lib/blocks.hpp>
#include <vxlnetwork/secure/common.hpp>

struct MDB_val;

namespace vxlnetwork
{
class pending_info_v14 final
{
public:
	pending_info_v14 () = default;
	pending_info_v14 (vxlnetwork::account const &, vxlnetwork::amount const &, vxlnetwork::epoch);
	size_t db_size () const;
	bool deserialize (vxlnetwork::stream &);
	bool operator== (vxlnetwork::pending_info_v14 const &) const;
	vxlnetwork::account source{};
	vxlnetwork::amount amount{ 0 };
	vxlnetwork::epoch epoch{ vxlnetwork::epoch::epoch_0 };
};
class account_info_v14 final
{
public:
	account_info_v14 () = default;
	account_info_v14 (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &, vxlnetwork::block_hash const &, vxlnetwork::amount const &, uint64_t, uint64_t, uint64_t, vxlnetwork::epoch);
	size_t db_size () const;
	vxlnetwork::block_hash head{ 0 };
	vxlnetwork::block_hash rep_block{ 0 };
	vxlnetwork::block_hash open_block{ 0 };
	vxlnetwork::amount balance{ 0 };
	uint64_t modified{ 0 };
	uint64_t block_count{ 0 };
	uint64_t confirmation_height{ 0 };
	vxlnetwork::epoch epoch{ vxlnetwork::epoch::epoch_0 };
};
class block_sideband_v14 final
{
public:
	block_sideband_v14 () = default;
	block_sideband_v14 (vxlnetwork::block_type, vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::amount const &, uint64_t, uint64_t);
	void serialize (vxlnetwork::stream &) const;
	bool deserialize (vxlnetwork::stream &);
	static size_t size (vxlnetwork::block_type);
	vxlnetwork::block_type type{ vxlnetwork::block_type::invalid };
	vxlnetwork::block_hash successor{ 0 };
	vxlnetwork::account account{};
	vxlnetwork::amount balance{ 0 };
	uint64_t height{ 0 };
	uint64_t timestamp{ 0 };
};
class state_block_w_sideband_v14
{
public:
	std::shared_ptr<vxlnetwork::state_block> state_block;
	vxlnetwork::block_sideband_v14 sideband;
};
class block_sideband_v18 final
{
public:
	block_sideband_v18 () = default;
	block_sideband_v18 (vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::amount const &, uint64_t, uint64_t, vxlnetwork::block_details const &);
	block_sideband_v18 (vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::amount const &, uint64_t, uint64_t, vxlnetwork::epoch, bool is_send, bool is_receive, bool is_epoch);
	void serialize (vxlnetwork::stream &, vxlnetwork::block_type) const;
	bool deserialize (vxlnetwork::stream &, vxlnetwork::block_type);
	static size_t size (vxlnetwork::block_type);
	vxlnetwork::block_hash successor{ 0 };
	vxlnetwork::account account{};
	vxlnetwork::amount balance{ 0 };
	uint64_t height{ 0 };
	uint64_t timestamp{ 0 };
	vxlnetwork::block_details details;
};
}
