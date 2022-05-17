#include <vxlnetwork/secure/versioning.hpp>
#include <vxlnetwork/test_common/system.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST (wallets, open_create)
{
	vxlnetwork::system system (1);
	bool error (false);
	vxlnetwork::wallets wallets (error, *system.nodes[0]);
	ASSERT_FALSE (error);
	ASSERT_EQ (1, wallets.items.size ()); // it starts out with a default wallet
	auto id = vxlnetwork::random_wallet_id ();
	ASSERT_EQ (nullptr, wallets.open (id));
	auto wallet (wallets.create (id));
	ASSERT_NE (nullptr, wallet);
	ASSERT_EQ (wallet, wallets.open (id));
}

TEST (wallets, open_existing)
{
	vxlnetwork::system system (1);
	auto id (vxlnetwork::random_wallet_id ());
	{
		bool error (false);
		vxlnetwork::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
		auto wallet (wallets.create (id));
		ASSERT_NE (nullptr, wallet);
		ASSERT_EQ (wallet, wallets.open (id));
		vxlnetwork::raw_key password;
		password.clear ();
		system.deadline_set (10s);
		while (password == 0)
		{
			ASSERT_NO_ERROR (system.poll ());
			wallet->store.password.value (password);
		}
	}
	{
		bool error (false);
		vxlnetwork::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (2, wallets.items.size ());
		ASSERT_NE (nullptr, wallets.open (id));
	}
}

TEST (wallets, remove)
{
	vxlnetwork::system system (1);
	vxlnetwork::wallet_id one (1);
	{
		bool error (false);
		vxlnetwork::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
		auto wallet (wallets.create (one));
		ASSERT_NE (nullptr, wallet);
		ASSERT_EQ (2, wallets.items.size ());
		wallets.destroy (one);
		ASSERT_EQ (1, wallets.items.size ());
	}
	{
		bool error (false);
		vxlnetwork::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
	}
}

TEST (wallets, reload)
{
	vxlnetwork::system system (1);
	auto & node1 (*system.nodes[0]);
	vxlnetwork::wallet_id one (1);
	bool error (false);
	ASSERT_FALSE (error);
	ASSERT_EQ (1, node1.wallets.items.size ());
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock_wallet (node1.wallets.mutex);
		vxlnetwork::inactive_node node (node1.application_path, vxlnetwork::inactive_node_flag_defaults ());
		auto wallet (node.node->wallets.create (one));
		ASSERT_NE (wallet, nullptr);
	}
	ASSERT_TIMELY (5s, node1.wallets.open (one) != nullptr);
	ASSERT_EQ (2, node1.wallets.items.size ());
}

TEST (wallets, vote_minimum)
{
	vxlnetwork::system system (1);
	auto & node1 (*system.nodes[0]);
	vxlnetwork::keypair key1;
	vxlnetwork::keypair key2;
	vxlnetwork::state_block send1 (vxlnetwork::dev::genesis_key.pub, vxlnetwork::dev::genesis->hash (), vxlnetwork::dev::genesis_key.pub, std::numeric_limits<vxlnetwork::uint128_t>::max () - node1.config.vote_minimum.number (), key1.pub, vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub, *system.work.generate (vxlnetwork::dev::genesis->hash ()));
	ASSERT_EQ (vxlnetwork::process_result::progress, node1.process (send1).code);
	vxlnetwork::state_block open1 (key1.pub, 0, key1.pub, node1.config.vote_minimum.number (), send1.hash (), key1.prv, key1.pub, *system.work.generate (key1.pub));
	ASSERT_EQ (vxlnetwork::process_result::progress, node1.process (open1).code);
	// send2 with amount vote_minimum - 1 (not voting representative)
	vxlnetwork::state_block send2 (vxlnetwork::dev::genesis_key.pub, send1.hash (), vxlnetwork::dev::genesis_key.pub, std::numeric_limits<vxlnetwork::uint128_t>::max () - 2 * node1.config.vote_minimum.number () + 1, key2.pub, vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub, *system.work.generate (send1.hash ()));
	ASSERT_EQ (vxlnetwork::process_result::progress, node1.process (send2).code);
	vxlnetwork::state_block open2 (key2.pub, 0, key2.pub, node1.config.vote_minimum.number () - 1, send2.hash (), key2.prv, key2.pub, *system.work.generate (key2.pub));
	ASSERT_EQ (vxlnetwork::process_result::progress, node1.process (open2).code);
	auto wallet (node1.wallets.items.begin ()->second);
	vxlnetwork::unique_lock<vxlnetwork::mutex> representatives_lk (wallet->representatives_mutex);
	ASSERT_EQ (0, wallet->representatives.size ());
	representatives_lk.unlock ();
	wallet->insert_adhoc (vxlnetwork::dev::genesis_key.prv);
	wallet->insert_adhoc (key1.prv);
	wallet->insert_adhoc (key2.prv);
	node1.wallets.compute_reps ();
	representatives_lk.lock ();
	ASSERT_EQ (2, wallet->representatives.size ());
}

TEST (wallets, exists)
{
	vxlnetwork::system system (1);
	auto & node (*system.nodes[0]);
	vxlnetwork::keypair key1;
	vxlnetwork::keypair key2;
	{
		auto transaction (node.wallets.tx_begin_read ());
		ASSERT_FALSE (node.wallets.exists (transaction, key1.pub));
		ASSERT_FALSE (node.wallets.exists (transaction, key2.pub));
	}
	system.wallet (0)->insert_adhoc (key1.prv);
	{
		auto transaction (node.wallets.tx_begin_read ());
		ASSERT_TRUE (node.wallets.exists (transaction, key1.pub));
		ASSERT_FALSE (node.wallets.exists (transaction, key2.pub));
	}
	system.wallet (0)->insert_adhoc (key2.prv);
	{
		auto transaction (node.wallets.tx_begin_read ());
		ASSERT_TRUE (node.wallets.exists (transaction, key1.pub));
		ASSERT_TRUE (node.wallets.exists (transaction, key2.pub));
	}
}

TEST (wallets, search_receivable)
{
	for (auto search_all : { false, true })
	{
		vxlnetwork::system system;
		vxlnetwork::node_config config (vxlnetwork::get_available_port (), system.logging);
		config.enable_voting = false;
		config.frontiers_confirmation = vxlnetwork::frontiers_confirmation_mode::disabled;
		vxlnetwork::node_flags flags;
		flags.disable_search_pending = true;
		auto & node (*system.add_node (config, flags));

		vxlnetwork::unique_lock<vxlnetwork::mutex> lk (node.wallets.mutex);
		auto wallets = node.wallets.get_wallets ();
		lk.unlock ();
		ASSERT_EQ (1, wallets.size ());
		auto wallet_id = wallets.begin ()->first;
		auto wallet = wallets.begin ()->second;

		wallet->insert_adhoc (vxlnetwork::dev::genesis_key.prv);
		vxlnetwork::block_builder builder;
		auto send = builder.state ()
					.account (vxlnetwork::dev::genesis->account ())
					.previous (vxlnetwork::dev::genesis->hash ())
					.representative (vxlnetwork::dev::genesis->account ())
					.balance (vxlnetwork::dev::constants.genesis_amount - node.config.receive_minimum.number ())
					.link (vxlnetwork::dev::genesis->account ())
					.sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
					.work (*system.work.generate (vxlnetwork::dev::genesis->hash ()))
					.build ();
		ASSERT_EQ (vxlnetwork::process_result::progress, node.process (*send).code);

		// Pending search should start an election
		ASSERT_TRUE (node.active.empty ());
		if (search_all)
		{
			node.wallets.search_receivable_all ();
		}
		else
		{
			node.wallets.search_receivable (wallet_id);
		}
		auto election = node.active.election (send->qualified_root ());
		ASSERT_NE (nullptr, election);

		// Erase the key so the confirmation does not trigger an automatic receive
		wallet->store.erase (node.wallets.tx_begin_write (), vxlnetwork::dev::genesis->account ());

		// Now confirm the election
		election->force_confirm ();

		ASSERT_TIMELY (5s, node.block_confirmed (send->hash ()) && node.active.empty ());

		// Re-insert the key
		wallet->insert_adhoc (vxlnetwork::dev::genesis_key.prv);

		// Pending search should create the receive block
		ASSERT_EQ (2, node.ledger.cache.block_count);
		if (search_all)
		{
			node.wallets.search_receivable_all ();
		}
		else
		{
			node.wallets.search_receivable (wallet_id);
		}
		ASSERT_TIMELY (3s, node.balance (vxlnetwork::dev::genesis->account ()) == vxlnetwork::dev::constants.genesis_amount);
		auto receive_hash = node.ledger.latest (node.store.tx_begin_read (), vxlnetwork::dev::genesis->account ());
		auto receive = node.block (receive_hash);
		ASSERT_NE (nullptr, receive);
		ASSERT_EQ (receive->sideband ().height, 3);
		ASSERT_EQ (send->hash (), receive->link ().as_block_hash ());
	}
}
