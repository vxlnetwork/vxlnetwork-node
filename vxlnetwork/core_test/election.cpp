#include <vxlnetwork/node/election.hpp>
#include <vxlnetwork/test_common/system.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST (election, construction)
{
	vxlnetwork::system system (1);
	auto & node = *system.nodes[0];
	node.block_confirm (vxlnetwork::dev::genesis);
	node.scheduler.flush ();
	auto election = node.active.election (vxlnetwork::dev::genesis->qualified_root ());
	election->transition_active ();
}

TEST (election, quorum_minimum_flip_success)
{
	vxlnetwork::system system{};

	vxlnetwork::node_config node_config{ vxlnetwork::get_available_port (), system.logging };
	node_config.online_weight_minimum = vxlnetwork::dev::constants.genesis_amount;
	node_config.frontiers_confirmation = vxlnetwork::frontiers_confirmation_mode::disabled;

	auto & node1 = *system.add_node (node_config);
	auto const latest_hash = vxlnetwork::dev::genesis->hash ();
	vxlnetwork::state_block_builder builder{};

	vxlnetwork::keypair key1{};
	auto send1 = builder.make_block ()
				 .previous (latest_hash)
				 .account (vxlnetwork::dev::genesis_key.pub)
				 .representative (vxlnetwork::dev::genesis_key.pub)
				 .balance (node1.online_reps.delta ())
				 .link (key1.pub)
				 .work (*system.work.generate (latest_hash))
				 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
				 .build_shared ();

	vxlnetwork::keypair key2{};
	auto send2 = builder.make_block ()
				 .previous (latest_hash)
				 .account (vxlnetwork::dev::genesis_key.pub)
				 .representative (vxlnetwork::dev::genesis_key.pub)
				 .balance (node1.online_reps.delta ())
				 .link (key2.pub)
				 .work (*system.work.generate (latest_hash))
				 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
				 .build_shared ();

	node1.process_active (send1);
	ASSERT_TIMELY (5s, node1.active.election (send1->qualified_root ()) != nullptr)

	node1.process_active (send2);
	std::shared_ptr<vxlnetwork::election> election{};
	ASSERT_TIMELY (5s, (election = node1.active.election (send2->qualified_root ())) != nullptr)
	ASSERT_TIMELY (5s, election->blocks ().size () == 2);

	auto const vote1 = std::make_shared<vxlnetwork::vote> (vxlnetwork::dev::genesis_key.pub, vxlnetwork::dev::genesis_key.prv, vxlnetwork::vote::timestamp_max, vxlnetwork::vote::duration_max, send2);
	ASSERT_EQ (vxlnetwork::vote_code::vote, node1.active.vote (vote1));

	ASSERT_TIMELY (5s, election->confirmed ());
	auto const winner = election->winner ();
	ASSERT_NE (nullptr, winner);
	ASSERT_EQ (*winner, *send2);
}

TEST (election, quorum_minimum_flip_fail)
{
	vxlnetwork::system system{};

	vxlnetwork::node_config node_config{ vxlnetwork::get_available_port (), system.logging };
	node_config.online_weight_minimum = vxlnetwork::dev::constants.genesis_amount;
	node_config.frontiers_confirmation = vxlnetwork::frontiers_confirmation_mode::disabled;

	auto & node1 = *system.add_node (node_config);
	auto const latest_hash = vxlnetwork::dev::genesis->hash ();
	vxlnetwork::state_block_builder builder{};

	vxlnetwork::keypair key1{};
	auto send1 = builder.make_block ()
				 .previous (latest_hash)
				 .account (vxlnetwork::dev::genesis_key.pub)
				 .representative (vxlnetwork::dev::genesis_key.pub)
				 .balance (node1.online_reps.delta () - 1)
				 .link (key1.pub)
				 .work (*system.work.generate (latest_hash))
				 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
				 .build_shared ();

	vxlnetwork::keypair key2{};
	auto send2 = builder.make_block ()
				 .previous (latest_hash)
				 .account (vxlnetwork::dev::genesis_key.pub)
				 .representative (vxlnetwork::dev::genesis_key.pub)
				 .balance (node1.online_reps.delta () - 1)
				 .link (key2.pub)
				 .work (*system.work.generate (latest_hash))
				 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
				 .build_shared ();

	node1.process_active (send1);
	ASSERT_TIMELY (5s, node1.active.election (send1->qualified_root ()) != nullptr)

	node1.process_active (send2);
	std::shared_ptr<vxlnetwork::election> election{};
	ASSERT_TIMELY (5s, (election = node1.active.election (send2->qualified_root ())) != nullptr)
	ASSERT_TIMELY (5s, election->blocks ().size () == 2);

	auto const vote1 = std::make_shared<vxlnetwork::vote> (vxlnetwork::dev::genesis_key.pub, vxlnetwork::dev::genesis_key.prv, vxlnetwork::vote::timestamp_max, vxlnetwork::vote::duration_max, send2);
	ASSERT_EQ (vxlnetwork::vote_code::vote, node1.active.vote (vote1));

	// give the election 5 seconds before asserting it is not confirmed so that in case
	// it would be wrongfully confirmed, have that immediately fail instead of race
	//
	std::this_thread::sleep_for (5s);
	ASSERT_FALSE (election->confirmed ());
	ASSERT_FALSE (node1.block_confirmed (send2->hash ()));
}

TEST (election, quorum_minimum_confirm_success)
{
	vxlnetwork::system system;
	vxlnetwork::node_config node_config (vxlnetwork::get_available_port (), system.logging);
	node_config.online_weight_minimum = vxlnetwork::dev::constants.genesis_amount;
	node_config.frontiers_confirmation = vxlnetwork::frontiers_confirmation_mode::disabled;
	auto & node1 = *system.add_node (node_config);
	vxlnetwork::keypair key1;
	vxlnetwork::block_builder builder;
	auto send1 = builder.state ()
				 .account (vxlnetwork::dev::genesis_key.pub)
				 .previous (vxlnetwork::dev::genesis->hash ())
				 .representative (vxlnetwork::dev::genesis_key.pub)
				 .balance (node1.online_reps.delta ())
				 .link (key1.pub)
				 .work (0)
				 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	node1.process_active (send1);
	node1.block_processor.flush ();
	node1.scheduler.activate (vxlnetwork::dev::genesis_key.pub, node1.store.tx_begin_read ());
	node1.scheduler.flush ();
	auto election = node1.active.election (send1->qualified_root ());
	ASSERT_NE (nullptr, election);
	ASSERT_EQ (1, election->blocks ().size ());
	auto vote1 (std::make_shared<vxlnetwork::vote> (vxlnetwork::dev::genesis_key.pub, vxlnetwork::dev::genesis_key.prv, vxlnetwork::vote::timestamp_max, vxlnetwork::vote::duration_max, send1));
	ASSERT_EQ (vxlnetwork::vote_code::vote, node1.active.vote (vote1));
	node1.block_processor.flush ();
	ASSERT_NE (nullptr, node1.block (send1->hash ()));
	ASSERT_TRUE (election->confirmed ());
}

TEST (election, quorum_minimum_confirm_fail)
{
	vxlnetwork::system system;
	vxlnetwork::node_config node_config (vxlnetwork::get_available_port (), system.logging);
	node_config.online_weight_minimum = vxlnetwork::dev::constants.genesis_amount;
	node_config.frontiers_confirmation = vxlnetwork::frontiers_confirmation_mode::disabled;
	auto & node1 = *system.add_node (node_config);
	vxlnetwork::keypair key1;
	vxlnetwork::block_builder builder;
	auto send1 = builder.state ()
				 .account (vxlnetwork::dev::genesis_key.pub)
				 .previous (vxlnetwork::dev::genesis->hash ())
				 .representative (vxlnetwork::dev::genesis_key.pub)
				 .balance (node1.online_reps.delta () - 1)
				 .link (key1.pub)
				 .work (0)
				 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	node1.process_active (send1);
	node1.block_processor.flush ();
	node1.scheduler.activate (vxlnetwork::dev::genesis_key.pub, node1.store.tx_begin_read ());
	node1.scheduler.flush ();
	auto election = node1.active.election (send1->qualified_root ());
	ASSERT_NE (nullptr, election);
	ASSERT_EQ (1, election->blocks ().size ());
	auto vote1 (std::make_shared<vxlnetwork::vote> (vxlnetwork::dev::genesis_key.pub, vxlnetwork::dev::genesis_key.prv, vxlnetwork::vote::timestamp_max, vxlnetwork::vote::duration_max, send1));
	ASSERT_EQ (vxlnetwork::vote_code::vote, node1.active.vote (vote1));
	node1.block_processor.flush ();
	ASSERT_NE (nullptr, node1.block (send1->hash ()));
	ASSERT_FALSE (election->confirmed ());
}

namespace vxlnetwork
{
TEST (election, quorum_minimum_update_weight_before_quorum_checks)
{
	vxlnetwork::system system{};

	vxlnetwork::node_config node_config{ vxlnetwork::get_available_port (), system.logging };
	node_config.frontiers_confirmation = vxlnetwork::frontiers_confirmation_mode::disabled;

	auto & node1 = *system.add_node (node_config);
	system.wallet (0)->insert_adhoc (vxlnetwork::dev::genesis_key.prv);

	vxlnetwork::keypair key1{};
	vxlnetwork::send_block_builder builder{};
	auto const amount = ((vxlnetwork::uint256_t (node_config.online_weight_minimum.number ()) * vxlnetwork::online_reps::online_weight_quorum) / 100).convert_to<vxlnetwork::uint128_t> () - 1;

	auto const latest = node1.latest (vxlnetwork::dev::genesis_key.pub);
	auto const send1 = builder.make_block ()
					   .previous (latest)
					   .destination (key1.pub)
					   .balance (amount)
					   .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
					   .work (*system.work.generate (latest))
					   .build_shared ();
	node1.process_active (send1);

	auto const open1 = vxlnetwork::open_block_builder{}.make_block ().account (key1.pub).source (send1->hash ()).representative (key1.pub).sign (key1.prv, key1.pub).work (*system.work.generate (key1.pub)).build_shared ();
	ASSERT_EQ (vxlnetwork::process_result::progress, node1.process (*open1).code);

	vxlnetwork::keypair key2{};
	auto const send2 = builder.make_block ()
					   .previous (open1->hash ())
					   .destination (key2.pub)
					   .balance (3)
					   .sign (key1.prv, key1.pub)
					   .work (*system.work.generate (open1->hash ()))
					   .build_shared ();
	ASSERT_EQ (vxlnetwork::process_result::progress, node1.process (*send2).code);
	ASSERT_TIMELY (5s, node1.ledger.cache.block_count == 4);

	node_config.peering_port = vxlnetwork::get_available_port ();
	auto & node2 = *system.add_node (node_config);

	system.wallet (1)->insert_adhoc (key1.prv);
	ASSERT_TIMELY (10s, node2.ledger.cache.block_count == 4);

	std::shared_ptr<vxlnetwork::election> election{};
	ASSERT_TIMELY (5s, (election = node1.active.election (send1->qualified_root ())) != nullptr);
	ASSERT_EQ (1, election->blocks ().size ());

	auto const vote1 = std::make_shared<vxlnetwork::vote> (vxlnetwork::dev::genesis_key.pub, vxlnetwork::dev::genesis_key.prv, vxlnetwork::vote::timestamp_max, vxlnetwork::vote::duration_max, send1);
	ASSERT_EQ (vxlnetwork::vote_code::vote, node1.active.vote (vote1));

	auto channel = node1.network.find_channel (node2.network.endpoint ());
	ASSERT_NE (channel, nullptr);

	auto const vote2 = std::make_shared<vxlnetwork::vote> (key1.pub, key1.prv, vxlnetwork::vote::timestamp_max, vxlnetwork::vote::duration_max, send1);
	ASSERT_TIMELY (10s, !node1.rep_crawler.response (channel, vote2));

	ASSERT_FALSE (election->confirmed ());
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (node1.online_reps.mutex);
		// Modify online_m for online_reps to more than is available, this checks that voting below updates it to current online reps.
		node1.online_reps.online_m = node_config.online_weight_minimum.number () + 20;
	}
	ASSERT_EQ (vxlnetwork::vote_code::vote, node1.active.vote (vote2));
	ASSERT_TRUE (election->confirmed ());
	ASSERT_NE (nullptr, node1.block (send1->hash ()));
}
}
