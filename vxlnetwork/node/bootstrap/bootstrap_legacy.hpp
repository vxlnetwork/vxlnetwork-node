#pragma once

#include <vxlnetwork/node/bootstrap/bootstrap_attempt.hpp>

#include <boost/property_tree/ptree_fwd.hpp>

#include <atomic>
#include <deque>
#include <memory>
#include <vector>

namespace vxlnetwork
{
class node;

/**
 * Legacy bootstrap session. This is made up of 3 phases: frontier requests, bootstrap pulls, bootstrap pushes.
 */
class bootstrap_attempt_legacy : public bootstrap_attempt
{
public:
	explicit bootstrap_attempt_legacy (std::shared_ptr<vxlnetwork::node> const & node_a, uint64_t const incremental_id_a, std::string const & id_a, uint32_t const frontiers_age_a, vxlnetwork::account const & start_account_a);
	void run () override;
	bool consume_future (std::future<bool> &);
	void stop () override;
	bool request_frontier (vxlnetwork::unique_lock<vxlnetwork::mutex> &, bool = false);
	void request_push (vxlnetwork::unique_lock<vxlnetwork::mutex> &);
	void add_frontier (vxlnetwork::pull_info const &) override;
	void add_bulk_push_target (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &) override;
	bool request_bulk_push_target (std::pair<vxlnetwork::block_hash, vxlnetwork::block_hash> &) override;
	void set_start_account (vxlnetwork::account const &) override;
	void run_start (vxlnetwork::unique_lock<vxlnetwork::mutex> &);
	void get_information (boost::property_tree::ptree &) override;
	vxlnetwork::tcp_endpoint endpoint_frontier_request;
	std::weak_ptr<vxlnetwork::frontier_req_client> frontiers;
	std::weak_ptr<vxlnetwork::bulk_push_client> push;
	std::deque<vxlnetwork::pull_info> frontier_pulls;
	std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::block_hash>> bulk_push_targets;
	vxlnetwork::account start_account{};
	std::atomic<unsigned> account_count{ 0 };
	uint32_t frontiers_age;
};
}
