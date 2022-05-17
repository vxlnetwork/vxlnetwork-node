#pragma once

#include <vxlnetwork/node/bootstrap/bootstrap_connections.hpp>
#include <vxlnetwork/node/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread/thread.hpp>

#include <atomic>
#include <queue>

namespace mi = boost::multi_index;

namespace vxlnetwork
{
class node;

class bootstrap_connections;
namespace transport
{
	class channel_tcp;
}
enum class bootstrap_mode
{
	legacy,
	lazy,
	wallet_lazy
};
enum class sync_result
{
	success,
	error,
	fork
};
class cached_pulls final
{
public:
	std::chrono::steady_clock::time_point time;
	vxlnetwork::uint512_union account_head;
	vxlnetwork::block_hash new_head;
};
class pulls_cache final
{
public:
	void add (vxlnetwork::pull_info const &);
	void update_pull (vxlnetwork::pull_info &);
	void remove (vxlnetwork::pull_info const &);
	vxlnetwork::mutex pulls_cache_mutex;
	class account_head_tag
	{
	};
	// clang-format off
	boost::multi_index_container<vxlnetwork::cached_pulls,
	mi::indexed_by<
		mi::ordered_non_unique<
			mi::member<vxlnetwork::cached_pulls, std::chrono::steady_clock::time_point, &vxlnetwork::cached_pulls::time>>,
		mi::hashed_unique<mi::tag<account_head_tag>,
			mi::member<vxlnetwork::cached_pulls, vxlnetwork::uint512_union, &vxlnetwork::cached_pulls::account_head>>>>
	cache;
	// clang-format on
	constexpr static std::size_t cache_size_max = 10000;
};

/**
 * Container for bootstrap sessions that are active. Owned by bootstrap_initiator.
 */
class bootstrap_attempts final
{
public:
	void add (std::shared_ptr<vxlnetwork::bootstrap_attempt>);
	void remove (uint64_t);
	void clear ();
	std::shared_ptr<vxlnetwork::bootstrap_attempt> find (uint64_t);
	std::size_t size ();
	std::atomic<uint64_t> incremental{ 0 };
	vxlnetwork::mutex bootstrap_attempts_mutex;
	std::map<uint64_t, std::shared_ptr<vxlnetwork::bootstrap_attempt>> attempts;
};

/**
 * Client side portion to initiate bootstrap sessions. Prevents multiple legacy-type bootstrap sessions from being started at the same time. Does permit
 * lazy/wallet bootstrap sessions to overlap with legacy sessions.
 */
class bootstrap_initiator final
{
public:
	explicit bootstrap_initiator (vxlnetwork::node &);
	~bootstrap_initiator ();
	void bootstrap (vxlnetwork::endpoint const &, bool add_to_peers = true, std::string id_a = "");
	void bootstrap (bool force = false, std::string id_a = "", uint32_t const frontiers_age_a = std::numeric_limits<uint32_t>::max (), vxlnetwork::account const & start_account_a = vxlnetwork::account{});
	bool bootstrap_lazy (vxlnetwork::hash_or_account const &, bool force = false, bool confirmed = true, std::string id_a = "");
	void bootstrap_wallet (std::deque<vxlnetwork::account> &);
	void run_bootstrap ();
	void lazy_requeue (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &);
	void notify_listeners (bool);
	void add_observer (std::function<void (bool)> const &);
	bool in_progress ();
	std::shared_ptr<vxlnetwork::bootstrap_connections> connections;
	std::shared_ptr<vxlnetwork::bootstrap_attempt> new_attempt ();
	bool has_new_attempts ();
	void remove_attempt (std::shared_ptr<vxlnetwork::bootstrap_attempt>);
	std::shared_ptr<vxlnetwork::bootstrap_attempt> current_attempt ();
	std::shared_ptr<vxlnetwork::bootstrap_attempt> current_lazy_attempt ();
	std::shared_ptr<vxlnetwork::bootstrap_attempt> current_wallet_attempt ();
	vxlnetwork::pulls_cache cache;
	vxlnetwork::bootstrap_attempts attempts;
	void stop ();

private:
	vxlnetwork::node & node;
	std::shared_ptr<vxlnetwork::bootstrap_attempt> find_attempt (vxlnetwork::bootstrap_mode);
	void stop_attempts ();
	std::vector<std::shared_ptr<vxlnetwork::bootstrap_attempt>> attempts_list;
	std::atomic<bool> stopped{ false };
	vxlnetwork::mutex mutex;
	vxlnetwork::condition_variable condition;
	vxlnetwork::mutex observers_mutex;
	std::vector<std::function<void (bool)>> observers;
	std::vector<boost::thread> bootstrap_initiator_threads;

	friend std::unique_ptr<container_info_component> collect_container_info (bootstrap_initiator & bootstrap_initiator, std::string const & name);
};

std::unique_ptr<container_info_component> collect_container_info (bootstrap_initiator & bootstrap_initiator, std::string const & name);

/**
 * Defines the numeric values for the bootstrap feature.
 */
class bootstrap_limits final
{
public:
	static constexpr double bootstrap_connection_scale_target_blocks = 10000.0;
	static constexpr double bootstrap_connection_warmup_time_sec = 5.0;
	static constexpr double bootstrap_minimum_blocks_per_sec = 10.0;
	static constexpr double bootstrap_minimum_elapsed_seconds_blockrate = 0.02;
	static constexpr double bootstrap_minimum_frontier_blocks_per_sec = 1000.0;
	static constexpr double bootstrap_minimum_termination_time_sec = 30.0;
	static constexpr unsigned bootstrap_max_new_connections = 32;
	static constexpr unsigned requeued_pulls_limit = 256;
	static constexpr unsigned requeued_pulls_limit_dev = 1;
	static constexpr unsigned requeued_pulls_processed_blocks_factor = 4096;
	static constexpr uint64_t pull_count_per_check = 8 * 1024;
	static constexpr unsigned bulk_push_cost_limit = 200;
	static constexpr std::chrono::seconds lazy_flush_delay_sec = std::chrono::seconds (5);
	static constexpr uint64_t lazy_batch_pull_count_resize_blocks_limit = 4 * 1024 * 1024;
	static constexpr double lazy_batch_pull_count_resize_ratio = 2.0;
	static constexpr std::size_t lazy_blocks_restart_limit = 1024 * 1024;
};
}
