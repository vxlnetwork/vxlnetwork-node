#include <vxlnetwork/crypto_lib/random_pool.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap_attempt.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap_bulk_push.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap_frontier.hpp>
#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/node/node.hpp>
#include <vxlnetwork/node/transport/tcp.hpp>
#include <vxlnetwork/node/websocket.hpp>

#include <boost/format.hpp>

#include <algorithm>

constexpr unsigned vxlnetwork::bootstrap_limits::requeued_pulls_limit;
constexpr unsigned vxlnetwork::bootstrap_limits::requeued_pulls_limit_dev;

vxlnetwork::bootstrap_attempt::bootstrap_attempt (std::shared_ptr<vxlnetwork::node> const & node_a, vxlnetwork::bootstrap_mode mode_a, uint64_t incremental_id_a, std::string id_a) :
	node (node_a),
	incremental_id (incremental_id_a),
	id (id_a),
	mode (mode_a)
{
	if (id.empty ())
	{
		id = vxlnetwork::hardened_constants::get ().random_128.to_string ();
	}

	node->logger.always_log (boost::str (boost::format ("Starting %1% bootstrap attempt with ID %2%") % mode_text () % id));
	node->bootstrap_initiator.notify_listeners (true);
	if (node->websocket_server)
	{
		vxlnetwork::websocket::message_builder builder;
		node->websocket_server->broadcast (builder.bootstrap_started (id, mode_text ()));
	}
}

vxlnetwork::bootstrap_attempt::~bootstrap_attempt ()
{
	node->logger.always_log (boost::str (boost::format ("Exiting %1% bootstrap attempt with ID %2%") % mode_text () % id));
	node->bootstrap_initiator.notify_listeners (false);
	if (node->websocket_server)
	{
		vxlnetwork::websocket::message_builder builder;
		node->websocket_server->broadcast (builder.bootstrap_exited (id, mode_text (), attempt_start, total_blocks));
	}
}

bool vxlnetwork::bootstrap_attempt::should_log ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard (next_log_mutex);
	auto result (false);
	auto now (std::chrono::steady_clock::now ());
	if (next_log < now)
	{
		result = true;
		next_log = now + std::chrono::seconds (15);
	}
	return result;
}

bool vxlnetwork::bootstrap_attempt::still_pulling ()
{
	debug_assert (!mutex.try_lock ());
	auto running (!stopped);
	auto still_pulling (pulling > 0);
	return running && still_pulling;
}

void vxlnetwork::bootstrap_attempt::pull_started ()
{
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
		++pulling;
	}
	condition.notify_all ();
}

void vxlnetwork::bootstrap_attempt::pull_finished ()
{
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
		--pulling;
	}
	condition.notify_all ();
}

void vxlnetwork::bootstrap_attempt::stop ()
{
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		stopped = true;
	}
	condition.notify_all ();
	node->bootstrap_initiator.connections->clear_pulls (incremental_id);
}

std::string vxlnetwork::bootstrap_attempt::mode_text ()
{
	std::string mode_text;
	if (mode == vxlnetwork::bootstrap_mode::legacy)
	{
		mode_text = "legacy";
	}
	else if (mode == vxlnetwork::bootstrap_mode::lazy)
	{
		mode_text = "lazy";
	}
	else if (mode == vxlnetwork::bootstrap_mode::wallet_lazy)
	{
		mode_text = "wallet_lazy";
	}
	return mode_text;
}

void vxlnetwork::bootstrap_attempt::add_frontier (vxlnetwork::pull_info const &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::legacy);
}

void vxlnetwork::bootstrap_attempt::add_bulk_push_target (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::legacy);
}

bool vxlnetwork::bootstrap_attempt::request_bulk_push_target (std::pair<vxlnetwork::block_hash, vxlnetwork::block_hash> &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::legacy);
	return true;
}

void vxlnetwork::bootstrap_attempt::set_start_account (vxlnetwork::account const &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::legacy);
}

bool vxlnetwork::bootstrap_attempt::process_block (std::shared_ptr<vxlnetwork::block> const & block_a, vxlnetwork::account const & known_account_a, uint64_t pull_blocks_processed, vxlnetwork::bulk_pull::count_t max_blocks, bool block_expected, unsigned retry_limit)
{
	bool stop_pull (false);
	// If block already exists in the ledger, then we can avoid next part of long account chain
	if (pull_blocks_processed % vxlnetwork::bootstrap_limits::pull_count_per_check == 0 && node->ledger.block_or_pruned_exists (block_a->hash ()))
	{
		stop_pull = true;
	}
	else
	{
		vxlnetwork::unchecked_info info (block_a, known_account_a, vxlnetwork::signature_verification::unknown);
		node->block_processor.add (info);
	}
	return stop_pull;
}

bool vxlnetwork::bootstrap_attempt::lazy_start (vxlnetwork::hash_or_account const &, bool)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::lazy);
	return false;
}

void vxlnetwork::bootstrap_attempt::lazy_add (vxlnetwork::pull_info const &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::lazy);
}

void vxlnetwork::bootstrap_attempt::lazy_requeue (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::lazy);
}

uint32_t vxlnetwork::bootstrap_attempt::lazy_batch_size ()
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::lazy);
	return node->network_params.bootstrap.lazy_min_pull_blocks;
}

bool vxlnetwork::bootstrap_attempt::lazy_processed_or_exists (vxlnetwork::block_hash const &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::lazy);
	return false;
}

bool vxlnetwork::bootstrap_attempt::lazy_has_expired () const
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::lazy);
	return true;
}

void vxlnetwork::bootstrap_attempt::requeue_pending (vxlnetwork::account const &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::wallet_lazy);
}

void vxlnetwork::bootstrap_attempt::wallet_start (std::deque<vxlnetwork::account> &)
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::wallet_lazy);
}

std::size_t vxlnetwork::bootstrap_attempt::wallet_size ()
{
	debug_assert (mode == vxlnetwork::bootstrap_mode::wallet_lazy);
	return 0;
}
