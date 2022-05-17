#pragma once

#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/node/election.hpp>
#include <vxlnetwork/node/inactive_cache_information.hpp>
#include <vxlnetwork/node/inactive_cache_status.hpp>
#include <vxlnetwork/node/voting.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <boost/circular_buffer.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/optional.hpp>
#include <boost/thread/thread.hpp>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace mi = boost::multi_index;

namespace vxlnetwork
{
class node;
class block;
class block_sideband;
class election;
class election_scheduler;
class vote;
class transaction;
class confirmation_height_processor;
class stat;

class cementable_account final
{
public:
	cementable_account (vxlnetwork::account const & account_a, std::size_t blocks_uncemented_a);
	vxlnetwork::account account;
	uint64_t blocks_uncemented{ 0 };
};

class election_timepoint final
{
public:
	std::chrono::steady_clock::time_point time;
	vxlnetwork::qualified_root root;
};

class expired_optimistic_election_info final
{
public:
	expired_optimistic_election_info (std::chrono::steady_clock::time_point, vxlnetwork::account);

	std::chrono::steady_clock::time_point expired_time;
	vxlnetwork::account account;
	bool election_started{ false };
};

class frontiers_confirmation_info
{
public:
	bool can_start_elections () const;

	std::size_t max_elections{ 0 };
	bool aggressive_mode{ false };
};

class election_insertion_result final
{
public:
	std::shared_ptr<vxlnetwork::election> election;
	bool inserted{ false };
};

// Core class for determining consensus
// Holds all active blocks i.e. recently added blocks that need confirmation
class active_transactions final
{
	class conflict_info final
	{
	public:
		vxlnetwork::qualified_root root;
		std::shared_ptr<vxlnetwork::election> election;
		vxlnetwork::epoch epoch;
		vxlnetwork::uint128_t previous_balance;
	};

	friend class vxlnetwork::election;

	// clang-format off
	class tag_account {};
	class tag_random_access {};
	class tag_root {};
	class tag_sequence {};
	class tag_uncemented {};
	class tag_arrival {};
	class tag_hash {};
	class tag_expired_time {};
	class tag_election_started {};
	// clang-format on

public:
	// clang-format off
	using ordered_roots = boost::multi_index_container<conflict_info,
	mi::indexed_by<
		mi::random_access<mi::tag<tag_random_access>>,
		mi::hashed_unique<mi::tag<tag_root>,
			mi::member<conflict_info, vxlnetwork::qualified_root, &conflict_info::root>>>>;
	// clang-format on
	ordered_roots roots;
	using roots_iterator = active_transactions::ordered_roots::index_iterator<tag_root>::type;

	explicit active_transactions (vxlnetwork::node &, vxlnetwork::confirmation_height_processor &);
	~active_transactions ();
	// Distinguishes replay votes, cannot be determined if the block is not in any election
	vxlnetwork::vote_code vote (std::shared_ptr<vxlnetwork::vote> const &);
	// Is the root of this block in the roots container
	bool active (vxlnetwork::block const &);
	bool active (vxlnetwork::qualified_root const &);
	std::shared_ptr<vxlnetwork::election> election (vxlnetwork::qualified_root const &) const;
	std::shared_ptr<vxlnetwork::block> winner (vxlnetwork::block_hash const &) const;
	// Returns a list of elections sorted by difficulty
	std::vector<std::shared_ptr<vxlnetwork::election>> list_active (std::size_t = std::numeric_limits<std::size_t>::max ());
	void erase (vxlnetwork::block const &);
	void erase_hash (vxlnetwork::block_hash const &);
	void erase_oldest ();
	bool empty ();
	std::size_t size ();
	void stop ();
	bool publish (std::shared_ptr<vxlnetwork::block> const &);
	boost::optional<vxlnetwork::election_status_type> confirm_block (vxlnetwork::transaction const &, std::shared_ptr<vxlnetwork::block> const &);
	void block_cemented_callback (std::shared_ptr<vxlnetwork::block> const &);
	void block_already_cemented_callback (vxlnetwork::block_hash const &);

	int64_t vacancy () const;
	std::function<void ()> vacancy_update{ [] () {} };

	std::unordered_map<vxlnetwork::block_hash, std::shared_ptr<vxlnetwork::election>> blocks;
	std::deque<vxlnetwork::election_status> list_recently_cemented ();
	std::deque<vxlnetwork::election_status> recently_cemented;

	void add_recently_cemented (vxlnetwork::election_status const &);
	void add_recently_confirmed (vxlnetwork::qualified_root const &, vxlnetwork::block_hash const &);
	void erase_recently_confirmed (vxlnetwork::block_hash const &);
	void add_inactive_votes_cache (vxlnetwork::unique_lock<vxlnetwork::mutex> &, vxlnetwork::block_hash const &, vxlnetwork::account const &, uint64_t const);
	// Inserts an election if conditions are met
	void trigger_inactive_votes_cache_election (std::shared_ptr<vxlnetwork::block> const &);
	vxlnetwork::inactive_cache_information find_inactive_votes_cache (vxlnetwork::block_hash const &);
	void erase_inactive_votes_cache (vxlnetwork::block_hash const &);
	vxlnetwork::election_scheduler & scheduler;
	vxlnetwork::confirmation_height_processor & confirmation_height_processor;
	vxlnetwork::node & node;
	mutable vxlnetwork::mutex mutex{ mutex_identifier (mutexes::active) };
	std::size_t priority_cementable_frontiers_size ();
	std::size_t priority_wallet_cementable_frontiers_size ();
	std::size_t inactive_votes_cache_size ();
	std::size_t election_winner_details_size ();
	void add_election_winner_details (vxlnetwork::block_hash const &, std::shared_ptr<vxlnetwork::election> const &);
	void remove_election_winner_details (vxlnetwork::block_hash const &);

	vxlnetwork::vote_generator generator;
	vxlnetwork::vote_generator final_generator;

#ifdef MEMORY_POOL_DISABLED
	using allocator = std::allocator<vxlnetwork::inactive_cache_information>;
#else
	using allocator = boost::fast_pool_allocator<vxlnetwork::inactive_cache_information>;
#endif

	// clang-format off
	using ordered_cache = boost::multi_index_container<vxlnetwork::inactive_cache_information,
	mi::indexed_by<
		mi::ordered_non_unique<mi::tag<tag_arrival>,
			mi::member<vxlnetwork::inactive_cache_information, std::chrono::steady_clock::time_point, &vxlnetwork::inactive_cache_information::arrival>>,
		mi::hashed_unique<mi::tag<tag_hash>,
			mi::member<vxlnetwork::inactive_cache_information, vxlnetwork::block_hash, &vxlnetwork::inactive_cache_information::hash>>>, allocator>;
	// clang-format on

private:
	vxlnetwork::mutex election_winner_details_mutex{ mutex_identifier (mutexes::election_winner_details) };

	std::unordered_map<vxlnetwork::block_hash, std::shared_ptr<vxlnetwork::election>> election_winner_details;

	// Call action with confirmed block, may be different than what we started with
	// clang-format off
	vxlnetwork::election_insertion_result insert_impl (vxlnetwork::unique_lock<vxlnetwork::mutex> &, std::shared_ptr<vxlnetwork::block> const&, boost::optional<vxlnetwork::uint128_t> const & = boost::none, vxlnetwork::election_behavior = vxlnetwork::election_behavior::normal, std::function<void(std::shared_ptr<vxlnetwork::block>const&)> const & = nullptr);
	// clang-format on
	void request_loop ();
	void request_confirm (vxlnetwork::unique_lock<vxlnetwork::mutex> &);
	void erase (vxlnetwork::qualified_root const &);
	// Erase all blocks from active and, if not confirmed, clear digests from network filters
	void cleanup_election (vxlnetwork::unique_lock<vxlnetwork::mutex> & lock_a, vxlnetwork::election const &);
	// Returns a list of elections sorted by difficulty, mutex must be locked
	std::vector<std::shared_ptr<vxlnetwork::election>> list_active_impl (std::size_t) const;

	vxlnetwork::condition_variable condition;
	bool started{ false };
	std::atomic<bool> stopped{ false };

	// Maximum time an election can be kept active if it is extending the container
	std::chrono::seconds const election_time_to_live;

	static std::size_t constexpr recently_confirmed_size{ 65536 };
	using recent_confirmation = std::pair<vxlnetwork::qualified_root, vxlnetwork::block_hash>;
	// clang-format off
	boost::multi_index_container<recent_confirmation,
	mi::indexed_by<
		mi::sequenced<mi::tag<tag_sequence>>,
		mi::hashed_unique<mi::tag<tag_root>,
			mi::member<recent_confirmation, vxlnetwork::qualified_root, &recent_confirmation::first>>,
		mi::hashed_unique<mi::tag<tag_hash>,
			mi::member<recent_confirmation, vxlnetwork::block_hash, &recent_confirmation::second>>>>
	recently_confirmed;
	using prioritize_num_uncemented = boost::multi_index_container<vxlnetwork::cementable_account,
	mi::indexed_by<
		mi::hashed_unique<mi::tag<tag_account>,
			mi::member<vxlnetwork::cementable_account, vxlnetwork::account, &vxlnetwork::cementable_account::account>>,
		mi::ordered_non_unique<mi::tag<tag_uncemented>,
			mi::member<vxlnetwork::cementable_account, uint64_t, &vxlnetwork::cementable_account::blocks_uncemented>,
			std::greater<uint64_t>>>>;

	boost::multi_index_container<vxlnetwork::expired_optimistic_election_info,
	mi::indexed_by<
		mi::ordered_non_unique<mi::tag<tag_expired_time>,
			mi::member<expired_optimistic_election_info, std::chrono::steady_clock::time_point, &expired_optimistic_election_info::expired_time>>,
		mi::hashed_unique<mi::tag<tag_account>,
			mi::member<expired_optimistic_election_info, vxlnetwork::account, &expired_optimistic_election_info::account>>,
		mi::ordered_non_unique<mi::tag<tag_election_started>,
			mi::member<expired_optimistic_election_info, bool, &expired_optimistic_election_info::election_started>, std::greater<bool>>>>
	expired_optimistic_election_infos;
	// clang-format on
	std::atomic<uint64_t> expired_optimistic_election_infos_size{ 0 };

	// Frontiers confirmation
	vxlnetwork::frontiers_confirmation_info get_frontiers_confirmation_info ();
	void confirm_prioritized_frontiers (vxlnetwork::transaction const &, uint64_t, uint64_t &);
	void confirm_expired_frontiers_pessimistically (vxlnetwork::transaction const &, uint64_t, uint64_t &);
	void frontiers_confirmation (vxlnetwork::unique_lock<vxlnetwork::mutex> &);
	bool insert_election_from_frontiers_confirmation (std::shared_ptr<vxlnetwork::block> const &, vxlnetwork::account const &, vxlnetwork::uint128_t, vxlnetwork::election_behavior);
	vxlnetwork::account next_frontier_account{};
	std::chrono::steady_clock::time_point next_frontier_check{ std::chrono::steady_clock::now () };
	constexpr static std::size_t max_active_elections_frontier_insertion{ 1000 };
	prioritize_num_uncemented priority_wallet_cementable_frontiers;
	prioritize_num_uncemented priority_cementable_frontiers;
	std::unordered_set<vxlnetwork::wallet_id> wallet_ids_already_iterated;
	std::unordered_map<vxlnetwork::wallet_id, vxlnetwork::account> next_wallet_id_accounts;
	bool skip_wallets{ false };
	std::atomic<unsigned> optimistic_elections_count{ 0 };
	void prioritize_frontiers_for_confirmation (vxlnetwork::transaction const &, std::chrono::milliseconds, std::chrono::milliseconds);
	bool prioritize_account_for_confirmation (prioritize_num_uncemented &, std::size_t &, vxlnetwork::account const &, vxlnetwork::account_info const &, uint64_t);
	unsigned max_optimistic ();
	void set_next_frontier_check (bool);
	void add_expired_optimistic_election (vxlnetwork::election const &);
	bool should_do_frontiers_confirmation () const;
	static std::size_t constexpr max_priority_cementable_frontiers{ 100000 };
	static std::size_t constexpr confirmed_frontiers_max_pending_size{ 10000 };
	static std::chrono::minutes constexpr expired_optimistic_election_info_cutoff{ 30 };
	ordered_cache inactive_votes_cache;
	vxlnetwork::inactive_cache_status inactive_votes_bootstrap_check (vxlnetwork::unique_lock<vxlnetwork::mutex> &, std::vector<std::pair<vxlnetwork::account, uint64_t>> const &, vxlnetwork::block_hash const &, vxlnetwork::inactive_cache_status const &);
	vxlnetwork::inactive_cache_status inactive_votes_bootstrap_check (vxlnetwork::unique_lock<vxlnetwork::mutex> &, vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::inactive_cache_status const &);
	vxlnetwork::inactive_cache_status inactive_votes_bootstrap_check_impl (vxlnetwork::unique_lock<vxlnetwork::mutex> &, vxlnetwork::uint128_t const &, std::size_t, vxlnetwork::block_hash const &, vxlnetwork::inactive_cache_status const &);
	vxlnetwork::inactive_cache_information find_inactive_votes_cache_impl (vxlnetwork::block_hash const &);
	boost::thread thread;

	friend class election;
	friend class election_scheduler;
	friend std::unique_ptr<container_info_component> collect_container_info (active_transactions &, std::string const &);

	friend class active_transactions_vote_replays_Test;
	friend class frontiers_confirmation_prioritize_frontiers_Test;
	friend class frontiers_confirmation_prioritize_frontiers_max_optimistic_elections_Test;
	friend class confirmation_height_prioritize_frontiers_overwrite_Test;
	friend class active_transactions_confirmation_consistency_Test;
	friend class node_deferred_dependent_elections_Test;
	friend class active_transactions_pessimistic_elections_Test;
	friend class frontiers_confirmation_expired_optimistic_elections_removal_Test;
};

bool purge_singleton_inactive_votes_cache_pool_memory ();
std::unique_ptr<container_info_component> collect_container_info (active_transactions & active_transactions, std::string const & name);
}
