#pragma once

#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/lib/utility.hpp>
#include <vxlnetwork/node/active_transactions.hpp>
#include <vxlnetwork/node/transport/transport.hpp>

namespace vxlnetwork
{
class telemetry;
class node_observers final
{
public:
	using blocks_t = vxlnetwork::observer_set<vxlnetwork::election_status const &, std::vector<vxlnetwork::vote_with_weight_info> const &, vxlnetwork::account const &, vxlnetwork::uint128_t const &, bool, bool>;
	blocks_t blocks;
	vxlnetwork::observer_set<bool> wallet;
	vxlnetwork::observer_set<std::shared_ptr<vxlnetwork::vote>, std::shared_ptr<vxlnetwork::transport::channel>, vxlnetwork::vote_code> vote;
	vxlnetwork::observer_set<vxlnetwork::block_hash const &> active_stopped;
	vxlnetwork::observer_set<vxlnetwork::account const &, bool> account_balance;
	vxlnetwork::observer_set<std::shared_ptr<vxlnetwork::transport::channel>> endpoint;
	vxlnetwork::observer_set<> disconnect;
	vxlnetwork::observer_set<vxlnetwork::root const &> work_cancel;
	vxlnetwork::observer_set<vxlnetwork::telemetry_data const &, vxlnetwork::endpoint const &> telemetry;
};

std::unique_ptr<container_info_component> collect_container_info (node_observers & node_observers, std::string const & name);
}
