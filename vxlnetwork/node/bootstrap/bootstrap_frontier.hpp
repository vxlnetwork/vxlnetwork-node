#pragma once

#include <vxlnetwork/node/common.hpp>

#include <deque>
#include <future>

namespace vxlnetwork
{
class bootstrap_attempt;
class bootstrap_client;

/**
 * Client side of a frontier request. Created to send and listen for frontier sequences from the server.
 */
class frontier_req_client final : public std::enable_shared_from_this<vxlnetwork::frontier_req_client>
{
public:
	explicit frontier_req_client (std::shared_ptr<vxlnetwork::bootstrap_client> const &, std::shared_ptr<vxlnetwork::bootstrap_attempt> const &);
	void run (vxlnetwork::account const & start_account_a, uint32_t const frontiers_age_a, uint32_t const count_a);
	void receive_frontier ();
	void received_frontier (boost::system::error_code const &, std::size_t);
	bool bulk_push_available ();
	void unsynced (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &);
	void next ();
	std::shared_ptr<vxlnetwork::bootstrap_client> connection;
	std::shared_ptr<vxlnetwork::bootstrap_attempt> attempt;
	vxlnetwork::account current;
	vxlnetwork::block_hash frontier;
	unsigned count;
	vxlnetwork::account last_account{ std::numeric_limits<vxlnetwork::uint256_t>::max () }; // Using last possible account stop further frontier requests
	std::chrono::steady_clock::time_point start_time;
	std::promise<bool> promise;
	/** A very rough estimate of the cost of `bulk_push`ing missing blocks */
	uint64_t bulk_push_cost;
	std::deque<std::pair<vxlnetwork::account, vxlnetwork::block_hash>> accounts;
	uint32_t frontiers_age{ std::numeric_limits<uint32_t>::max () };
	uint32_t count_limit{ std::numeric_limits<uint32_t>::max () };
	static std::size_t constexpr size_frontier = sizeof (vxlnetwork::account) + sizeof (vxlnetwork::block_hash);
};
class bootstrap_server;
class frontier_req;

/**
 * Server side of a frontier request. Created when a bootstrap_server receives a frontier_req message and exited when end-of-list is reached.
 */
class frontier_req_server final : public std::enable_shared_from_this<vxlnetwork::frontier_req_server>
{
public:
	frontier_req_server (std::shared_ptr<vxlnetwork::bootstrap_server> const &, std::unique_ptr<vxlnetwork::frontier_req>);
	void send_next ();
	void sent_action (boost::system::error_code const &, std::size_t);
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, std::size_t);
	void next ();
	bool send_confirmed ();
	std::shared_ptr<vxlnetwork::bootstrap_server> connection;
	vxlnetwork::account current;
	vxlnetwork::block_hash frontier;
	std::unique_ptr<vxlnetwork::frontier_req> request;
	std::size_t count;
	std::deque<std::pair<vxlnetwork::account, vxlnetwork::block_hash>> accounts;
};
}
