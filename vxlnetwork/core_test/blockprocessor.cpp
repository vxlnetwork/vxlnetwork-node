#include <vxlnetwork/lib/blockbuilders.hpp>
#include <vxlnetwork/node/node.hpp>
#include <vxlnetwork/node/nodeconfig.hpp>
#include <vxlnetwork/secure/common.hpp>
#include <vxlnetwork/secure/ledger.hpp>
#include <vxlnetwork/test_common/system.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST (block_processor, broadcast_block_on_arrival)
{
	vxlnetwork::system system;
	vxlnetwork::node_config config1{ vxlnetwork::get_available_port (), system.logging };
	// Deactivates elections on both nodes.
	config1.active_elections_size = 0;
	vxlnetwork::node_config config2{ vxlnetwork::get_available_port (), system.logging };
	config2.active_elections_size = 0;
	vxlnetwork::node_flags flags;
	// Disables bootstrap listener to make sure the block won't be shared by this channel.
	flags.disable_bootstrap_listener = true;
	auto node1 = system.add_node (config1, flags);
	auto node2 = system.add_node (config2, flags);
	vxlnetwork::state_block_builder builder;
	auto send1 = builder.make_block ()
				 .account (vxlnetwork::dev::genesis_key.pub)
				 .previous (vxlnetwork::dev::genesis->hash ())
				 .representative (vxlnetwork::dev::genesis_key.pub)
				 .balance (vxlnetwork::dev::constants.genesis_amount - vxlnetwork::Gxrb_ratio)
				 .link (vxlnetwork::dev::genesis_key.pub)
				 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
				 .work (*system.work.generate (vxlnetwork::dev::genesis->hash ()))
				 .build_shared ();
	// Adds a block to the first node. process_active() -> (calls) block_processor.add() -> add() ->
	// awakes process_block() -> process_batch() -> process_one() -> process_live()
	node1->process_active (send1);
	// Checks whether the block was broadcast.
	ASSERT_TIMELY (5s, node2->ledger.block_or_pruned_exists (send1->hash ()));
}