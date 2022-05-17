#pragma once

#include <vxlnetwork/lib/config.hpp>
#include <vxlnetwork/lib/stats.hpp>
#include <vxlnetwork/lib/work.hpp>
#include <vxlnetwork/node/active_transactions.hpp>
#include <vxlnetwork/node/blockprocessor.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap_attempt.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap_server.hpp>
#include <vxlnetwork/node/confirmation_height_processor.hpp>
#include <vxlnetwork/node/distributed_work_factory.hpp>
#include <vxlnetwork/node/election.hpp>
#include <vxlnetwork/node/election_scheduler.hpp>
#include <vxlnetwork/node/gap_cache.hpp>
#include <vxlnetwork/node/network.hpp>
#include <vxlnetwork/node/node_observers.hpp>
#include <vxlnetwork/node/nodeconfig.hpp>
#include <vxlnetwork/node/online_reps.hpp>
#include <vxlnetwork/node/portmapping.hpp>
#include <vxlnetwork/node/repcrawler.hpp>
#include <vxlnetwork/node/request_aggregator.hpp>
#include <vxlnetwork/node/signatures.hpp>
#include <vxlnetwork/node/telemetry.hpp>
#include <vxlnetwork/node/unchecked_map.hpp>
#include <vxlnetwork/node/vote_processor.hpp>
#include <vxlnetwork/node/wallet.hpp>
#include <vxlnetwork/node/write_database_queue.hpp>
#include <vxlnetwork/secure/ledger.hpp>
#include <vxlnetwork/secure/utility.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/latch.hpp>

#include <atomic>
#include <memory>
#include <vector>

namespace vxlnetwork
{
namespace websocket
{
	class listener;
}
class node;
class telemetry;
class work_pool;
class block_arrival_info final
{
public:
	std::chrono::steady_clock::time_point arrival;
	vxlnetwork::block_hash hash;
};
// This class tracks blocks that are probably live because they arrived in a UDP packet
// This gives a fairly reliable way to differentiate between blocks being inserted via bootstrap or new, live blocks.
class block_arrival final
{
public:
	// Return `true' to indicated an error if the block has already been inserted
	bool add (vxlnetwork::block_hash const &);
	bool recent (vxlnetwork::block_hash const &);
	// clang-format off
	class tag_sequence {};
	class tag_hash {};
	boost::multi_index_container<vxlnetwork::block_arrival_info,
		boost::multi_index::indexed_by<
			boost::multi_index::sequenced<boost::multi_index::tag<tag_sequence>>,
			boost::multi_index::hashed_unique<boost::multi_index::tag<tag_hash>,
				boost::multi_index::member<vxlnetwork::block_arrival_info, vxlnetwork::block_hash, &vxlnetwork::block_arrival_info::hash>>>>
	arrival;
	// clang-format on
	vxlnetwork::mutex mutex{ mutex_identifier (mutexes::block_arrival) };
	static std::size_t constexpr arrival_size_min = 8 * 1024;
	static std::chrono::seconds constexpr arrival_time_min = std::chrono::seconds (300);
};

std::unique_ptr<container_info_component> collect_container_info (block_arrival & block_arrival, std::string const & name);

std::unique_ptr<container_info_component> collect_container_info (rep_crawler & rep_crawler, std::string const & name);

class node final : public std::enable_shared_from_this<vxlnetwork::node>
{
public:
	node (boost::asio::io_context &, uint16_t, boost::filesystem::path const &, vxlnetwork::logging const &, vxlnetwork::work_pool &, vxlnetwork::node_flags = vxlnetwork::node_flags (), unsigned seq = 0);
	node (boost::asio::io_context &, boost::filesystem::path const &, vxlnetwork::node_config const &, vxlnetwork::work_pool &, vxlnetwork::node_flags = vxlnetwork::node_flags (), unsigned seq = 0);
	~node ();
	template <typename T>
	void background (T action_a)
	{
		io_ctx.post (action_a);
	}
	bool copy_with_compaction (boost::filesystem::path const &);
	void keepalive (std::string const &, uint16_t);
	void start ();
	void stop ();
	std::shared_ptr<vxlnetwork::node> shared ();
	int store_version ();
	void receive_confirmed (vxlnetwork::transaction const & block_transaction_a, vxlnetwork::block_hash const & hash_a, vxlnetwork::account const & destination_a);
	void process_confirmed_data (vxlnetwork::transaction const &, std::shared_ptr<vxlnetwork::block> const &, vxlnetwork::block_hash const &, vxlnetwork::account &, vxlnetwork::uint128_t &, bool &, bool &, vxlnetwork::account &);
	void process_confirmed (vxlnetwork::election_status const &, uint64_t = 0);
	void process_active (std::shared_ptr<vxlnetwork::block> const &);
	[[nodiscard]] vxlnetwork::process_return process (vxlnetwork::block &);
	vxlnetwork::process_return process_local (std::shared_ptr<vxlnetwork::block> const &);
	void process_local_async (std::shared_ptr<vxlnetwork::block> const &);
	void keepalive_preconfigured (std::vector<std::string> const &);
	vxlnetwork::block_hash latest (vxlnetwork::account const &);
	vxlnetwork::uint128_t balance (vxlnetwork::account const &);
	std::shared_ptr<vxlnetwork::block> block (vxlnetwork::block_hash const &);
	std::pair<vxlnetwork::uint128_t, vxlnetwork::uint128_t> balance_pending (vxlnetwork::account const &, bool only_confirmed);
	vxlnetwork::uint128_t weight (vxlnetwork::account const &);
	vxlnetwork::block_hash rep_block (vxlnetwork::account const &);
	vxlnetwork::uint128_t minimum_principal_weight ();
	vxlnetwork::uint128_t minimum_principal_weight (vxlnetwork::uint128_t const &);
	void ongoing_rep_calculation ();
	void ongoing_bootstrap ();
	void ongoing_peer_store ();
	void ongoing_unchecked_cleanup ();
	void ongoing_backlog_population ();
	void backup_wallet ();
	void search_receivable_all ();
	void bootstrap_wallet ();
	void unchecked_cleanup ();
	bool collect_ledger_pruning_targets (std::deque<vxlnetwork::block_hash> &, vxlnetwork::account &, uint64_t const, uint64_t const, uint64_t const);
	void ledger_pruning (uint64_t const, bool, bool);
	void ongoing_ledger_pruning ();
	int price (vxlnetwork::uint128_t const &, int);
	// The default difficulty updates to base only when the first epoch_2 block is processed
	uint64_t default_difficulty (vxlnetwork::work_version const) const;
	uint64_t default_receive_difficulty (vxlnetwork::work_version const) const;
	uint64_t max_work_generate_difficulty (vxlnetwork::work_version const) const;
	bool local_work_generation_enabled () const;
	bool work_generation_enabled () const;
	bool work_generation_enabled (std::vector<std::pair<std::string, uint16_t>> const &) const;
	boost::optional<uint64_t> work_generate_blocking (vxlnetwork::block &, uint64_t);
	boost::optional<uint64_t> work_generate_blocking (vxlnetwork::work_version const, vxlnetwork::root const &, uint64_t, boost::optional<vxlnetwork::account> const & = boost::none);
	void work_generate (vxlnetwork::work_version const, vxlnetwork::root const &, uint64_t, std::function<void (boost::optional<uint64_t>)>, boost::optional<vxlnetwork::account> const & = boost::none, bool const = false);
	void add_initial_peers ();
	void block_confirm (std::shared_ptr<vxlnetwork::block> const &);
	bool block_confirmed (vxlnetwork::block_hash const &);
	bool block_confirmed_or_being_confirmed (vxlnetwork::transaction const &, vxlnetwork::block_hash const &);
	void do_rpc_callback (boost::asio::ip::tcp::resolver::iterator i_a, std::string const &, uint16_t, std::shared_ptr<std::string> const &, std::shared_ptr<std::string> const &, std::shared_ptr<boost::asio::ip::tcp::resolver> const &);
	void ongoing_online_weight_calculation ();
	void ongoing_online_weight_calculation_queue ();
	bool online () const;
	bool init_error () const;
	bool epoch_upgrader (vxlnetwork::raw_key const &, vxlnetwork::epoch, uint64_t, uint64_t);
	void set_bandwidth_params (std::size_t limit, double ratio);
	std::pair<uint64_t, decltype (vxlnetwork::ledger::bootstrap_weights)> get_bootstrap_weights () const;
	void populate_backlog ();
	uint64_t get_confirmation_height (vxlnetwork::transaction const &, vxlnetwork::account &);
	vxlnetwork::write_database_queue write_database_queue;
	boost::asio::io_context & io_ctx;
	boost::latch node_initialized_latch;
	vxlnetwork::node_config config;
	vxlnetwork::network_params & network_params;
	vxlnetwork::stat stats;
	vxlnetwork::thread_pool workers;
	std::shared_ptr<vxlnetwork::websocket::listener> websocket_server;
	vxlnetwork::node_flags flags;
	vxlnetwork::work_pool & work;
	vxlnetwork::distributed_work_factory distributed_work;
	vxlnetwork::logger_mt logger;
	std::unique_ptr<vxlnetwork::store> store_impl;
	vxlnetwork::store & store;
	vxlnetwork::unchecked_map unchecked;
	std::unique_ptr<vxlnetwork::wallets_store> wallets_store_impl;
	vxlnetwork::wallets_store & wallets_store;
	vxlnetwork::gap_cache gap_cache;
	vxlnetwork::ledger ledger;
	vxlnetwork::signature_checker checker;
	vxlnetwork::network network;
	std::shared_ptr<vxlnetwork::telemetry> telemetry;
	vxlnetwork::bootstrap_initiator bootstrap_initiator;
	vxlnetwork::bootstrap_listener bootstrap;
	boost::filesystem::path application_path;
	vxlnetwork::node_observers observers;
	vxlnetwork::port_mapping port_mapping;
	vxlnetwork::online_reps online_reps;
	vxlnetwork::rep_crawler rep_crawler;
	vxlnetwork::vote_processor vote_processor;
	unsigned warmed_up;
	vxlnetwork::block_processor block_processor;
	vxlnetwork::block_arrival block_arrival;
	vxlnetwork::local_vote_history history;
	vxlnetwork::keypair node_id;
	vxlnetwork::block_uniquer block_uniquer;
	vxlnetwork::vote_uniquer vote_uniquer;
	vxlnetwork::confirmation_height_processor confirmation_height_processor;
	vxlnetwork::active_transactions active;
	vxlnetwork::election_scheduler scheduler;
	vxlnetwork::request_aggregator aggregator;
	vxlnetwork::wallets wallets;
	std::chrono::steady_clock::time_point const startup_time;
	std::chrono::seconds unchecked_cutoff = std::chrono::seconds (7 * 24 * 60 * 60); // Week
	std::atomic<bool> unresponsive_work_peers{ false };
	std::atomic<bool> stopped{ false };
	static double constexpr price_max = 16.0;
	static double constexpr free_cutoff = 1024.0;
	// For tests only
	unsigned node_seq;
	// For tests only
	boost::optional<uint64_t> work_generate_blocking (vxlnetwork::block &);
	// For tests only
	boost::optional<uint64_t> work_generate_blocking (vxlnetwork::root const &, uint64_t);
	// For tests only
	boost::optional<uint64_t> work_generate_blocking (vxlnetwork::root const &);

private:
	void long_inactivity_cleanup ();
	void epoch_upgrader_impl (vxlnetwork::raw_key const &, vxlnetwork::epoch, uint64_t, uint64_t);
	vxlnetwork::locked<std::future<void>> epoch_upgrading;
};

std::unique_ptr<container_info_component> collect_container_info (node & node, std::string const & name);

vxlnetwork::node_flags const & inactive_node_flag_defaults ();

class node_wrapper final
{
public:
	node_wrapper (boost::filesystem::path const & path_a, boost::filesystem::path const & config_path_a, vxlnetwork::node_flags const & node_flags_a);
	~node_wrapper ();

	vxlnetwork::network_params network_params;
	std::shared_ptr<boost::asio::io_context> io_context;
	vxlnetwork::work_pool work;
	std::shared_ptr<vxlnetwork::node> node;
};

class inactive_node final
{
public:
	inactive_node (boost::filesystem::path const & path_a, vxlnetwork::node_flags const & node_flags_a);
	inactive_node (boost::filesystem::path const & path_a, boost::filesystem::path const & config_path_a, vxlnetwork::node_flags const & node_flags_a);

	vxlnetwork::node_wrapper node_wrapper;
	std::shared_ptr<vxlnetwork::node> node;
};
std::unique_ptr<vxlnetwork::inactive_node> default_inactive_node (boost::filesystem::path const &, boost::program_options::variables_map const &);
}
