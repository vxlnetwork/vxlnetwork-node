#pragma once

#include <vxlnetwork/lib/locks.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/node/transport/transport.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <condition_variable>
#include <thread>
#include <unordered_map>

namespace mi = boost::multi_index;

namespace vxlnetwork
{
class active_transactions;
class ledger;
class local_vote_history;
class node_config;
class stat;
class vote_generator;
class wallets;
/**
 * Pools together confirmation requests, separately for each endpoint.
 * Requests are added from network messages, and aggregated to minimize bandwidth and vote generation. Example:
 * * Two votes are cached, one for hashes {1,2,3} and another for hashes {4,5,6}
 * * A request arrives for hashes {1,4,5}. Another request arrives soon afterwards for hashes {2,3,6}
 * * The aggregator will reply with the two cached votes
 * Votes are generated for uncached hashes.
 */
class request_aggregator final
{
	/**
	 * Holds a buffer of incoming requests from an endpoint.
	 * Extends the lifetime of the corresponding channel. The channel is updated on a new request arriving from the same endpoint, such that only the newest channel is held
	 */
	struct channel_pool final
	{
		channel_pool () = delete;
		explicit channel_pool (std::shared_ptr<vxlnetwork::transport::channel> const & channel_a) :
			channel (channel_a),
			endpoint (vxlnetwork::transport::map_endpoint_to_v6 (channel_a->get_endpoint ()))
		{
		}
		std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::root>> hashes_roots;
		std::shared_ptr<vxlnetwork::transport::channel> channel;
		vxlnetwork::endpoint endpoint;
		std::chrono::steady_clock::time_point const start{ std::chrono::steady_clock::now () };
		std::chrono::steady_clock::time_point deadline;
	};

	// clang-format off
	class tag_endpoint {};
	class tag_deadline {};
	// clang-format on

public:
	request_aggregator (vxlnetwork::node_config const & config, vxlnetwork::stat & stats_a, vxlnetwork::vote_generator &, vxlnetwork::vote_generator &, vxlnetwork::local_vote_history &, vxlnetwork::ledger &, vxlnetwork::wallets &, vxlnetwork::active_transactions &);

	/** Add a new request by \p channel_a for hashes \p hashes_roots_a */
	void add (std::shared_ptr<vxlnetwork::transport::channel> const & channel_a, std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::root>> const & hashes_roots_a);
	void stop ();
	/** Returns the number of currently queued request pools */
	std::size_t size ();
	bool empty ();

	vxlnetwork::node_config const & config;
	std::chrono::milliseconds const max_delay;
	std::chrono::milliseconds const small_delay;
	std::size_t const max_channel_requests;

private:
	void run ();
	/** Remove duplicate requests **/
	void erase_duplicates (std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::root>> &) const;
	/** Aggregate \p requests_a and send cached votes to \p channel_a . Return the remaining hashes that need vote generation for each block for regular & final vote generators **/
	std::pair<std::vector<std::shared_ptr<vxlnetwork::block>>, std::vector<std::shared_ptr<vxlnetwork::block>>> aggregate (std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::root>> const & requests_a, std::shared_ptr<vxlnetwork::transport::channel> & channel_a) const;
	void reply_action (std::shared_ptr<vxlnetwork::vote> const & vote_a, std::shared_ptr<vxlnetwork::transport::channel> const & channel_a) const;

	vxlnetwork::stat & stats;
	vxlnetwork::local_vote_history & local_votes;
	vxlnetwork::ledger & ledger;
	vxlnetwork::wallets & wallets;
	vxlnetwork::active_transactions & active;
	vxlnetwork::vote_generator & generator;
	vxlnetwork::vote_generator & final_generator;

	// clang-format off
	boost::multi_index_container<channel_pool,
	mi::indexed_by<
		mi::hashed_unique<mi::tag<tag_endpoint>,
			mi::member<channel_pool, vxlnetwork::endpoint, &channel_pool::endpoint>>,
		mi::ordered_non_unique<mi::tag<tag_deadline>,
			mi::member<channel_pool, std::chrono::steady_clock::time_point, &channel_pool::deadline>>>>
	requests;
	// clang-format on

	bool stopped{ false };
	bool started{ false };
	vxlnetwork::condition_variable condition;
	vxlnetwork::mutex mutex{ mutex_identifier (mutexes::request_aggregator) };
	std::thread thread;

	friend std::unique_ptr<container_info_component> collect_container_info (request_aggregator &, std::string const &);
};
std::unique_ptr<container_info_component> collect_container_info (request_aggregator &, std::string const &);
}
