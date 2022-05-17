#pragma once

#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/lib/utility.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace vxlnetwork
{
class signature_checker;
class active_transactions;
class store;
class node_observers;
class stats;
class node_config;
class logger_mt;
class online_reps;
class rep_crawler;
class ledger;
class network_params;
class node_flags;
class stat;

class transaction;
namespace transport
{
	class channel;
}

class vote_processor final
{
public:
	explicit vote_processor (vxlnetwork::signature_checker & checker_a, vxlnetwork::active_transactions & active_a, vxlnetwork::node_observers & observers_a, vxlnetwork::stat & stats_a, vxlnetwork::node_config & config_a, vxlnetwork::node_flags & flags_a, vxlnetwork::logger_mt & logger_a, vxlnetwork::online_reps & online_reps_a, vxlnetwork::rep_crawler & rep_crawler_a, vxlnetwork::ledger & ledger_a, vxlnetwork::network_params & network_params_a);
	/** Returns false if the vote was processed */
	bool vote (std::shared_ptr<vxlnetwork::vote> const &, std::shared_ptr<vxlnetwork::transport::channel> const &);
	/** Note: node.active.mutex lock is required */
	vxlnetwork::vote_code vote_blocking (std::shared_ptr<vxlnetwork::vote> const &, std::shared_ptr<vxlnetwork::transport::channel> const &, bool = false);
	void verify_votes (std::deque<std::pair<std::shared_ptr<vxlnetwork::vote>, std::shared_ptr<vxlnetwork::transport::channel>>> const &);
	void flush ();
	/** Block until the currently active processing cycle finishes */
	void flush_active ();
	std::size_t size ();
	bool empty ();
	bool half_full ();
	void calculate_weights ();
	void stop ();
	std::atomic<uint64_t> total_processed{ 0 };

private:
	void process_loop ();

	vxlnetwork::signature_checker & checker;
	vxlnetwork::active_transactions & active;
	vxlnetwork::node_observers & observers;
	vxlnetwork::stat & stats;
	vxlnetwork::node_config & config;
	vxlnetwork::logger_mt & logger;
	vxlnetwork::online_reps & online_reps;
	vxlnetwork::rep_crawler & rep_crawler;
	vxlnetwork::ledger & ledger;
	vxlnetwork::network_params & network_params;
	std::size_t max_votes;
	std::deque<std::pair<std::shared_ptr<vxlnetwork::vote>, std::shared_ptr<vxlnetwork::transport::channel>>> votes;
	/** Representatives levels for random early detection */
	std::unordered_set<vxlnetwork::account> representatives_1;
	std::unordered_set<vxlnetwork::account> representatives_2;
	std::unordered_set<vxlnetwork::account> representatives_3;
	vxlnetwork::condition_variable condition;
	vxlnetwork::mutex mutex{ mutex_identifier (mutexes::vote_processor) };
	bool started;
	bool stopped;
	bool is_active;
	std::thread thread;

	friend std::unique_ptr<container_info_component> collect_container_info (vote_processor & vote_processor, std::string const & name);
	friend class vote_processor_weights_Test;
};

std::unique_ptr<container_info_component> collect_container_info (vote_processor & vote_processor, std::string const & name);
}
