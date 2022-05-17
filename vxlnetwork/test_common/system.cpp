#include <vxlnetwork/crypto_lib/random_pool.hpp>
#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/node/transport/udp.hpp>
#include <vxlnetwork/test_common/system.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <cstdlib>

using namespace std::chrono_literals;

std::string vxlnetwork::error_system_messages::message (int ev) const
{
	switch (static_cast<vxlnetwork::error_system> (ev))
	{
		case vxlnetwork::error_system::generic:
			return "Unknown error";
		case vxlnetwork::error_system::deadline_expired:
			return "Deadline expired";
	}

	return "Invalid error code";
}

std::shared_ptr<vxlnetwork::node> vxlnetwork::system::add_node (vxlnetwork::node_flags node_flags_a, vxlnetwork::transport::transport_type type_a)
{
	return add_node (vxlnetwork::node_config (vxlnetwork::get_available_port (), logging), node_flags_a, type_a);
}

/** Returns the node added. */
std::shared_ptr<vxlnetwork::node> vxlnetwork::system::add_node (vxlnetwork::node_config const & node_config_a, vxlnetwork::node_flags node_flags_a, vxlnetwork::transport::transport_type type_a)
{
	auto node (std::make_shared<vxlnetwork::node> (io_ctx, vxlnetwork::unique_path (), node_config_a, work, node_flags_a, node_sequence++));
	for (auto i : initialization_blocks)
	{
		auto result = node->ledger.process (node->store.tx_begin_write (), *i);
		debug_assert (result.code == vxlnetwork::process_result::progress);
	}
	debug_assert (!node->init_error ());
	node->start ();
	node->wallets.create (vxlnetwork::random_wallet_id ());
	nodes.reserve (nodes.size () + 1);
	nodes.push_back (node);
	if (nodes.size () > 1)
	{
		debug_assert (nodes.size () - 1 <= node->network_params.network.max_peers_per_ip || node->flags.disable_max_peers_per_ip); // Check that we don't start more nodes than limit for single IP address
		auto begin = nodes.end () - 2;
		for (auto i (begin), j (begin + 1), n (nodes.end ()); j != n; ++i, ++j)
		{
			auto node1 (*i);
			auto node2 (*j);
			auto starting1 (node1->network.size ());
			size_t starting_listener1 (node1->bootstrap.realtime_count);
			decltype (starting1) new1;
			auto starting2 (node2->network.size ());
			size_t starting_listener2 (node2->bootstrap.realtime_count);
			decltype (starting2) new2;
			if (type_a == vxlnetwork::transport::transport_type::tcp)
			{
				(*j)->network.merge_peer ((*i)->network.endpoint ());
			}
			else
			{
				// UDP connection
				auto channel (std::make_shared<vxlnetwork::transport::channel_udp> ((*j)->network.udp_channels, (*i)->network.endpoint (), node1->network_params.network.protocol_version));
				(*j)->network.send_keepalive (channel);
			}
			do
			{
				poll ();
				new1 = node1->network.size ();
				new2 = node2->network.size ();
			} while (new1 == starting1 || new2 == starting2);
			if (type_a == vxlnetwork::transport::transport_type::tcp && node_config_a.tcp_incoming_connections_max != 0 && !node_flags_a.disable_tcp_realtime)
			{
				// Wait for initial connection finish
				decltype (starting_listener1) new_listener1;
				decltype (starting_listener2) new_listener2;
				do
				{
					poll ();
					new_listener1 = node1->bootstrap.realtime_count;
					new_listener2 = node2->bootstrap.realtime_count;
				} while (new_listener1 == starting_listener1 || new_listener2 == starting_listener2);
			}
		}
		auto iterations1 (0);
		while (std::any_of (begin, nodes.end (), [] (std::shared_ptr<vxlnetwork::node> const & node_a) { return node_a->bootstrap_initiator.in_progress (); }))
		{
			poll ();
			++iterations1;
			debug_assert (iterations1 < 10000);
		}
	}
	else
	{
		auto iterations1 (0);
		while (node->bootstrap_initiator.in_progress ())
		{
			poll ();
			++iterations1;
			debug_assert (iterations1 < 10000);
		}
	}

	return node;
}

vxlnetwork::system::system ()
{
	auto scale_str = std::getenv ("DEADLINE_SCALE_FACTOR");
	if (scale_str)
	{
		deadline_scaling_factor = std::stod (scale_str);
	}
	logging.init (vxlnetwork::unique_path ());
}

vxlnetwork::system::system (uint16_t count_a, vxlnetwork::transport::transport_type type_a, vxlnetwork::node_flags flags_a) :
	system ()
{
	nodes.reserve (count_a);
	for (uint16_t i (0); i < count_a; ++i)
	{
		vxlnetwork::node_config config (vxlnetwork::get_available_port (), logging);
		add_node (config, flags_a, type_a);
	}
}

vxlnetwork::system::~system ()
{
	for (auto & i : nodes)
	{
		i->stop ();
	}

#ifndef _WIN32
	// Windows cannot remove the log and data files while they are still owned by this process.
	// They will be removed later

	// Clean up tmp directories created by the tests. Since it's sometimes useful to
	// see log files after test failures, an environment variable is supported to
	// retain the files.
	if (std::getenv ("TEST_KEEP_TMPDIRS") == nullptr)
	{
		vxlnetwork::remove_temporary_directories ();
	}
#endif
}

void vxlnetwork::system::ledger_initialization_set (std::vector<vxlnetwork::keypair> const & reps, vxlnetwork::amount const & reserve)
{
	vxlnetwork::block_hash previous = vxlnetwork::dev::genesis->hash ();
	auto amount = (vxlnetwork::dev::constants.genesis_amount - reserve.number ()) / reps.size ();
	auto balance = vxlnetwork::dev::constants.genesis_amount;
	for (auto const & i : reps)
	{
		balance -= amount;
		vxlnetwork::state_block_builder builder;
		builder.account (vxlnetwork::dev::genesis_key.pub)
		.previous (previous)
		.representative (vxlnetwork::dev::genesis_key.pub)
		.link (i.pub)
		.balance (balance)
		.sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
		.work (*work.generate (previous));
		initialization_blocks.emplace_back (builder.build_shared ());
		previous = initialization_blocks.back ()->hash ();
		builder.make_block ();
		builder.account (i.pub)
		.previous (0)
		.representative (i.pub)
		.link (previous)
		.balance (amount)
		.sign (i.prv, i.pub)
		.work (*work.generate (i.pub));
		initialization_blocks.emplace_back (builder.build_shared ());
	}
}

std::shared_ptr<vxlnetwork::wallet> vxlnetwork::system::wallet (size_t index_a)
{
	debug_assert (nodes.size () > index_a);
	auto size (nodes[index_a]->wallets.items.size ());
	(void)size;
	debug_assert (size == 1);
	return nodes[index_a]->wallets.items.begin ()->second;
}

vxlnetwork::account vxlnetwork::system::account (vxlnetwork::transaction const & transaction_a, size_t index_a)
{
	auto wallet_l (wallet (index_a));
	auto keys (wallet_l->store.begin (transaction_a));
	debug_assert (keys != wallet_l->store.end ());
	auto result (keys->first);
	debug_assert (++keys == wallet_l->store.end ());
	return vxlnetwork::account (result);
}

uint64_t vxlnetwork::system::work_generate_limited (vxlnetwork::block_hash const & root_a, uint64_t min_a, uint64_t max_a)
{
	debug_assert (min_a > 0);
	uint64_t result = 0;
	do
	{
		result = *work.generate (root_a, min_a);
	} while (work.network_constants.work.difficulty (vxlnetwork::work_version::work_1, root_a, result) >= max_a);
	return result;
}

/** Initiate an epoch upgrade. Writes the epoch block into the ledger and leaves it to
 *  node background processes (e.g. frontiers confirmation) to cement the block.
 */
std::unique_ptr<vxlnetwork::state_block> vxlnetwork::upgrade_epoch (vxlnetwork::work_pool & pool_a, vxlnetwork::ledger & ledger_a, vxlnetwork::epoch epoch_a)
{
	auto transaction (ledger_a.store.tx_begin_write ());
	auto dev_genesis_key = vxlnetwork::dev::genesis_key;
	auto account = dev_genesis_key.pub;
	auto latest = ledger_a.latest (transaction, account);
	auto balance = ledger_a.account_balance (transaction, account);

	vxlnetwork::state_block_builder builder;
	std::error_code ec;
	auto epoch = builder
				 .account (dev_genesis_key.pub)
				 .previous (latest)
				 .balance (balance)
				 .link (ledger_a.epoch_link (epoch_a))
				 .representative (dev_genesis_key.pub)
				 .sign (dev_genesis_key.prv, dev_genesis_key.pub)
				 .work (*pool_a.generate (latest, pool_a.network_constants.work.threshold (vxlnetwork::work_version::work_1, vxlnetwork::block_details (epoch_a, false, false, true))))
				 .build (ec);

	bool error{ true };
	if (!ec && epoch)
	{
		error = ledger_a.process (transaction, *epoch).code != vxlnetwork::process_result::progress;
	}

	return !error ? std::move (epoch) : nullptr;
}

void vxlnetwork::blocks_confirm (vxlnetwork::node & node_a, std::vector<std::shared_ptr<vxlnetwork::block>> const & blocks_a, bool const forced_a)
{
	// Finish processing all blocks
	node_a.block_processor.flush ();
	for (auto const & block : blocks_a)
	{
		auto disk_block (node_a.block (block->hash ()));
		// A sideband is required to start an election
		debug_assert (disk_block != nullptr);
		debug_assert (disk_block->has_sideband ());
		node_a.block_confirm (disk_block);
		if (forced_a)
		{
			auto election = node_a.active.election (disk_block->qualified_root ());
			debug_assert (election != nullptr);
			election->force_confirm ();
		}
	}
}

std::unique_ptr<vxlnetwork::state_block> vxlnetwork::system::upgrade_genesis_epoch (vxlnetwork::node & node_a, vxlnetwork::epoch const epoch_a)
{
	return upgrade_epoch (work, node_a.ledger, epoch_a);
}

void vxlnetwork::system::deadline_set (std::chrono::duration<double, std::nano> const & delta_a)
{
	deadline = std::chrono::steady_clock::now () + delta_a * deadline_scaling_factor;
}

std::error_code vxlnetwork::system::poll (std::chrono::nanoseconds const & wait_time)
{
#if VXLNETWORK_ASIO_HANDLER_TRACKING == 0
	io_ctx.run_one_for (wait_time);
#else
	vxlnetwork::timer<> timer;
	timer.start ();
	auto count = io_ctx.poll_one ();
	if (count == 0)
	{
		std::this_thread::sleep_for (wait_time);
	}
	else if (count == 1 && timer.since_start ().count () >= VXLNETWORK_ASIO_HANDLER_TRACKING)
	{
		auto timestamp = std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
		std::cout << (boost::format ("[%1%] io_thread held for %2%ms") % timestamp % timer.since_start ().count ()).str () << std::endl;
	}
#endif

	std::error_code ec;
	if (std::chrono::steady_clock::now () > deadline)
	{
		ec = vxlnetwork::error_system::deadline_expired;
		stop ();
	}
	return ec;
}

std::error_code vxlnetwork::system::poll_until_true (std::chrono::nanoseconds deadline_a, std::function<bool ()> predicate_a)
{
	std::error_code ec;
	deadline_set (deadline_a);
	while (!ec && !predicate_a ())
	{
		ec = poll ();
	}
	return ec;
}

/**
 * This function repetitively calls io_ctx.run_one_for until delay number of milliseconds elapse.
 * It can be used to sleep for a duration in unit tests whilst allowing the background io contexts to continue processing.
 * @param delay milliseconds of delay
 */
void vxlnetwork::system::delay_ms (std::chrono::milliseconds const & delay)
{
	auto now = std::chrono::steady_clock::now ();
	auto endtime = now + delay;
	while (now <= endtime)
	{
		io_ctx.run_one_for (endtime - now);
		now = std::chrono::steady_clock::now ();
	}
}

namespace
{
class traffic_generator : public std::enable_shared_from_this<traffic_generator>
{
public:
	traffic_generator (uint32_t count_a, uint32_t wait_a, std::shared_ptr<vxlnetwork::node> const & node_a, vxlnetwork::system & system_a) :
		count (count_a),
		wait (wait_a),
		node (node_a),
		system (system_a)
	{
	}
	void run ()
	{
		auto count_l (count - 1);
		count = count_l - 1;
		system.generate_activity (*node, accounts);
		if (count_l > 0)
		{
			auto this_l (shared_from_this ());
			node->workers.add_timed_task (std::chrono::steady_clock::now () + std::chrono::milliseconds (wait), [this_l] () { this_l->run (); });
		}
	}
	std::vector<vxlnetwork::account> accounts;
	uint32_t count;
	uint32_t wait;
	std::shared_ptr<vxlnetwork::node> node;
	vxlnetwork::system & system;
};
}

void vxlnetwork::system::generate_usage_traffic (uint32_t count_a, uint32_t wait_a)
{
	for (size_t i (0), n (nodes.size ()); i != n; ++i)
	{
		generate_usage_traffic (count_a, wait_a, i);
	}
}

void vxlnetwork::system::generate_usage_traffic (uint32_t count_a, uint32_t wait_a, size_t index_a)
{
	debug_assert (nodes.size () > index_a);
	debug_assert (count_a > 0);
	auto generate (std::make_shared<traffic_generator> (count_a, wait_a, nodes[index_a], *this));
	generate->run ();
}

void vxlnetwork::system::generate_rollback (vxlnetwork::node & node_a, std::vector<vxlnetwork::account> & accounts_a)
{
	auto transaction (node_a.store.tx_begin_write ());
	debug_assert (std::numeric_limits<CryptoPP::word32>::max () > accounts_a.size ());
	auto index (random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (accounts_a.size () - 1)));
	auto account (accounts_a[index]);
	vxlnetwork::account_info info;
	auto error (node_a.store.account.get (transaction, account, info));
	if (!error)
	{
		auto hash (info.open_block);
		if (hash != node_a.network_params.ledger.genesis->hash ())
		{
			accounts_a[index] = accounts_a[accounts_a.size () - 1];
			accounts_a.pop_back ();
			std::vector<std::shared_ptr<vxlnetwork::block>> rollback_list;
			auto error = node_a.ledger.rollback (transaction, hash, rollback_list);
			(void)error;
			debug_assert (!error);
			for (auto & i : rollback_list)
			{
				node_a.active.erase (*i);
			}
		}
	}
}

void vxlnetwork::system::generate_receive (vxlnetwork::node & node_a)
{
	std::shared_ptr<vxlnetwork::block> send_block;
	{
		auto transaction (node_a.store.tx_begin_read ());
		vxlnetwork::account random_account;
		random_pool::generate_block (random_account.bytes.data (), sizeof (random_account.bytes));
		auto i (node_a.store.pending.begin (transaction, vxlnetwork::pending_key (random_account, 0)));
		if (i != node_a.store.pending.end ())
		{
			vxlnetwork::pending_key const & send_hash (i->first);
			send_block = node_a.store.block.get (transaction, send_hash.hash);
		}
	}
	if (send_block != nullptr)
	{
		auto receive_error (wallet (0)->receive_sync (send_block, vxlnetwork::dev::genesis->account (), std::numeric_limits<vxlnetwork::uint128_t>::max ()));
		(void)receive_error;
	}
}

void vxlnetwork::system::generate_activity (vxlnetwork::node & node_a, std::vector<vxlnetwork::account> & accounts_a)
{
	auto what (random_pool::generate_byte ());
	if (what < 0x1)
	{
		generate_rollback (node_a, accounts_a);
	}
	else if (what < 0x10)
	{
		generate_change_known (node_a, accounts_a);
	}
	else if (what < 0x20)
	{
		generate_change_unknown (node_a, accounts_a);
	}
	else if (what < 0x70)
	{
		generate_receive (node_a);
	}
	else if (what < 0xc0)
	{
		generate_send_existing (node_a, accounts_a);
	}
	else
	{
		generate_send_new (node_a, accounts_a);
	}
}

vxlnetwork::account vxlnetwork::system::get_random_account (std::vector<vxlnetwork::account> & accounts_a)
{
	debug_assert (std::numeric_limits<CryptoPP::word32>::max () > accounts_a.size ());
	auto index (random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (accounts_a.size () - 1)));
	auto result (accounts_a[index]);
	return result;
}

vxlnetwork::uint128_t vxlnetwork::system::get_random_amount (vxlnetwork::transaction const & transaction_a, vxlnetwork::node & node_a, vxlnetwork::account const & account_a)
{
	vxlnetwork::uint128_t balance (node_a.ledger.account_balance (transaction_a, account_a));
	vxlnetwork::uint128_union random_amount;
	vxlnetwork::random_pool::generate_block (random_amount.bytes.data (), sizeof (random_amount.bytes));
	return (((vxlnetwork::uint256_t{ random_amount.number () } * balance) / vxlnetwork::uint256_t{ std::numeric_limits<vxlnetwork::uint128_t>::max () }).convert_to<vxlnetwork::uint128_t> ());
}

void vxlnetwork::system::generate_send_existing (vxlnetwork::node & node_a, std::vector<vxlnetwork::account> & accounts_a)
{
	vxlnetwork::uint128_t amount;
	vxlnetwork::account destination;
	vxlnetwork::account source;
	{
		vxlnetwork::account account;
		random_pool::generate_block (account.bytes.data (), sizeof (account.bytes));
		auto transaction (node_a.store.tx_begin_read ());
		vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> entry (node_a.store.account.begin (transaction, account));
		if (entry == node_a.store.account.end ())
		{
			entry = node_a.store.account.begin (transaction);
		}
		debug_assert (entry != node_a.store.account.end ());
		destination = vxlnetwork::account (entry->first);
		source = get_random_account (accounts_a);
		amount = get_random_amount (transaction, node_a, source);
	}
	if (!amount.is_zero ())
	{
		auto hash (wallet (0)->send_sync (source, destination, amount));
		(void)hash;
		debug_assert (!hash.is_zero ());
	}
}

void vxlnetwork::system::generate_change_known (vxlnetwork::node & node_a, std::vector<vxlnetwork::account> & accounts_a)
{
	vxlnetwork::account source (get_random_account (accounts_a));
	if (!node_a.latest (source).is_zero ())
	{
		vxlnetwork::account destination (get_random_account (accounts_a));
		auto change_error (wallet (0)->change_sync (source, destination));
		(void)change_error;
		debug_assert (!change_error);
	}
}

void vxlnetwork::system::generate_change_unknown (vxlnetwork::node & node_a, std::vector<vxlnetwork::account> & accounts_a)
{
	vxlnetwork::account source (get_random_account (accounts_a));
	if (!node_a.latest (source).is_zero ())
	{
		vxlnetwork::keypair key;
		vxlnetwork::account destination (key.pub);
		auto change_error (wallet (0)->change_sync (source, destination));
		(void)change_error;
		debug_assert (!change_error);
	}
}

void vxlnetwork::system::generate_send_new (vxlnetwork::node & node_a, std::vector<vxlnetwork::account> & accounts_a)
{
	debug_assert (node_a.wallets.items.size () == 1);
	vxlnetwork::uint128_t amount;
	vxlnetwork::account source;
	{
		auto transaction (node_a.store.tx_begin_read ());
		source = get_random_account (accounts_a);
		amount = get_random_amount (transaction, node_a, source);
	}
	if (!amount.is_zero ())
	{
		auto pub (node_a.wallets.items.begin ()->second->deterministic_insert ());
		accounts_a.push_back (pub);
		auto hash (wallet (0)->send_sync (source, pub, amount));
		(void)hash;
		debug_assert (!hash.is_zero ());
	}
}

void vxlnetwork::system::generate_mass_activity (uint32_t count_a, vxlnetwork::node & node_a)
{
	std::vector<vxlnetwork::account> accounts;
	auto dev_genesis_key = vxlnetwork::dev::genesis_key;
	wallet (0)->insert_adhoc (dev_genesis_key.prv);
	accounts.push_back (dev_genesis_key.pub);
	auto previous (std::chrono::steady_clock::now ());
	for (uint32_t i (0); i < count_a; ++i)
	{
		if ((i & 0xff) == 0)
		{
			auto now (std::chrono::steady_clock::now ());
			auto us (std::chrono::duration_cast<std::chrono::microseconds> (now - previous).count ());
			auto count = node_a.ledger.cache.block_count.load ();
			std::cerr << boost::str (boost::format ("Mass activity iteration %1% us %2% us/t %3% block count: %4%\n") % i % us % (us / 256) % count);
			previous = now;
		}
		generate_activity (node_a, accounts);
	}
}

void vxlnetwork::system::stop ()
{
	for (auto i : nodes)
	{
		i->stop ();
	}
	work.stop ();
}

uint16_t vxlnetwork::get_available_port ()
{
	// Maximum possible sockets which may feasibly be used in 1 test
	constexpr auto max = 200;
	static uint16_t current = 0;
	// Read the TEST_BASE_PORT environment and override the default base port if it exists
	auto base_str = std::getenv ("TEST_BASE_PORT");
	uint16_t base_port = 24000;
	if (base_str)
	{
		base_port = boost::lexical_cast<uint16_t> (base_str);
	}

	uint16_t const available_port = base_port + current;
	++current;
	// Reset port number once we have reached the maximum
	if (current == max)
	{
		current = 0;
	}

	return available_port;
}

void vxlnetwork::cleanup_dev_directories_on_exit ()
{
	// Makes sure everything is cleaned up
	vxlnetwork::logging::release_file_sink ();
	// Clean up tmp directories created by the tests. Since it's sometimes useful to
	// see log files after test failures, an environment variable is supported to
	// retain the files.
	if (std::getenv ("TEST_KEEP_TMPDIRS") == nullptr)
	{
		vxlnetwork::remove_temporary_directories ();
	}
}
