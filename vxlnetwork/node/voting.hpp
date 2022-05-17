#pragma once

#include <vxlnetwork/lib/locks.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/lib/utility.hpp>
#include <vxlnetwork/node/wallet.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace mi = boost::multi_index;

namespace vxlnetwork
{
class ledger;
class network;
class node_config;
class stat;
class vote_processor;
class wallets;
namespace transport
{
	class channel;
}

class vote_spacing final
{
	class entry
	{
	public:
		vxlnetwork::root root;
		std::chrono::steady_clock::time_point time;
		vxlnetwork::block_hash hash;
	};

	boost::multi_index_container<entry,
	mi::indexed_by<
	mi::hashed_non_unique<mi::tag<class tag_root>,
	mi::member<entry, vxlnetwork::root, &entry::root>>,
	mi::ordered_non_unique<mi::tag<class tag_time>,
	mi::member<entry, std::chrono::steady_clock::time_point, &entry::time>>>>
	recent;
	std::chrono::milliseconds const delay;
	void trim ();

public:
	vote_spacing (std::chrono::milliseconds const & delay) :
		delay{ delay }
	{
	}
	bool votable (vxlnetwork::root const & root_a, vxlnetwork::block_hash const & hash_a) const;
	void flag (vxlnetwork::root const & root_a, vxlnetwork::block_hash const & hash_a);
	std::size_t size () const;
};

class local_vote_history final
{
	class local_vote final
	{
	public:
		local_vote (vxlnetwork::root const & root_a, vxlnetwork::block_hash const & hash_a, std::shared_ptr<vxlnetwork::vote> const & vote_a) :
			root (root_a),
			hash (hash_a),
			vote (vote_a)
		{
		}
		vxlnetwork::root root;
		vxlnetwork::block_hash hash;
		std::shared_ptr<vxlnetwork::vote> vote;
	};

public:
	local_vote_history (vxlnetwork::voting_constants const & constants) :
		constants{ constants }
	{
	}
	void add (vxlnetwork::root const & root_a, vxlnetwork::block_hash const & hash_a, std::shared_ptr<vxlnetwork::vote> const & vote_a);
	void erase (vxlnetwork::root const & root_a);

	std::vector<std::shared_ptr<vxlnetwork::vote>> votes (vxlnetwork::root const & root_a, vxlnetwork::block_hash const & hash_a, bool const is_final_a = false) const;
	bool exists (vxlnetwork::root const &) const;
	std::size_t size () const;

private:
	// clang-format off
	boost::multi_index_container<local_vote,
	mi::indexed_by<
		mi::hashed_non_unique<mi::tag<class tag_root>,
			mi::member<local_vote, vxlnetwork::root, &local_vote::root>>,
		mi::sequenced<mi::tag<class tag_sequence>>>>
	history;
	// clang-format on

	vxlnetwork::voting_constants const & constants;
	void clean ();
	std::vector<std::shared_ptr<vxlnetwork::vote>> votes (vxlnetwork::root const & root_a) const;
	// Only used in Debug
	bool consistency_check (vxlnetwork::root const &) const;
	mutable vxlnetwork::mutex mutex;

	friend std::unique_ptr<container_info_component> collect_container_info (local_vote_history & history, std::string const & name);
	friend class local_vote_history_basic_Test;
};

std::unique_ptr<container_info_component> collect_container_info (local_vote_history & history, std::string const & name);

class vote_generator final
{
private:
	using candidate_t = std::pair<vxlnetwork::root, vxlnetwork::block_hash>;
	using request_t = std::pair<std::vector<candidate_t>, std::shared_ptr<vxlnetwork::transport::channel>>;

public:
	vote_generator (vxlnetwork::node_config const & config_a, vxlnetwork::ledger & ledger_a, vxlnetwork::wallets & wallets_a, vxlnetwork::vote_processor & vote_processor_a, vxlnetwork::local_vote_history & history_a, vxlnetwork::network & network_a, vxlnetwork::stat & stats_a, bool is_final_a);
	/** Queue items for vote generation, or broadcast votes already in cache */
	void add (vxlnetwork::root const &, vxlnetwork::block_hash const &);
	/** Queue blocks for vote generation, returning the number of successful candidates.*/
	std::size_t generate (std::vector<std::shared_ptr<vxlnetwork::block>> const & blocks_a, std::shared_ptr<vxlnetwork::transport::channel> const & channel_a);
	void set_reply_action (std::function<void (std::shared_ptr<vxlnetwork::vote> const &, std::shared_ptr<vxlnetwork::transport::channel> const &)>);
	void stop ();

private:
	void run ();
	void broadcast (vxlnetwork::unique_lock<vxlnetwork::mutex> &);
	void reply (vxlnetwork::unique_lock<vxlnetwork::mutex> &, request_t &&);
	void vote (std::vector<vxlnetwork::block_hash> const &, std::vector<vxlnetwork::root> const &, std::function<void (std::shared_ptr<vxlnetwork::vote> const &)> const &);
	void broadcast_action (std::shared_ptr<vxlnetwork::vote> const &) const;
	std::function<void (std::shared_ptr<vxlnetwork::vote> const &, std::shared_ptr<vxlnetwork::transport::channel> &)> reply_action; // must be set only during initialization by using set_reply_action
	vxlnetwork::node_config const & config;
	vxlnetwork::ledger & ledger;
	vxlnetwork::wallets & wallets;
	vxlnetwork::vote_processor & vote_processor;
	vxlnetwork::local_vote_history & history;
	vxlnetwork::vote_spacing spacing;
	vxlnetwork::network & network;
	vxlnetwork::stat & stats;
	mutable vxlnetwork::mutex mutex;
	vxlnetwork::condition_variable condition;
	static std::size_t constexpr max_requests{ 2048 };
	std::deque<request_t> requests;
	std::deque<candidate_t> candidates;
	std::atomic<bool> stopped{ false };
	bool started{ false };
	std::thread thread;
	bool is_final;

	friend std::unique_ptr<container_info_component> collect_container_info (vote_generator & vote_generator, std::string const & name);
};

std::unique_ptr<container_info_component> collect_container_info (vote_generator & generator, std::string const & name);

class vote_generator_session final
{
public:
	vote_generator_session (vote_generator & vote_generator_a);
	void add (vxlnetwork::root const &, vxlnetwork::block_hash const &);
	void flush ();

private:
	vxlnetwork::vote_generator & generator;
	std::vector<std::pair<vxlnetwork::root, vxlnetwork::block_hash>> items;
};
}
