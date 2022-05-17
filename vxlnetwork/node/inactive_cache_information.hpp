#pragma once

#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/node/inactive_cache_status.hpp>

#include <chrono>

namespace vxlnetwork
{
class inactive_cache_information final
{
public:
	inactive_cache_information () = default;
	inactive_cache_information (std::chrono::steady_clock::time_point arrival, vxlnetwork::block_hash hash, vxlnetwork::account initial_rep_a, uint64_t initial_timestamp_a, vxlnetwork::inactive_cache_status status) :
		arrival (arrival),
		hash (hash),
		status (status)
	{
		voters.reserve (8);
		voters.emplace_back (initial_rep_a, initial_timestamp_a);
	}

	std::chrono::steady_clock::time_point arrival;
	vxlnetwork::block_hash hash;
	vxlnetwork::inactive_cache_status status;
	std::vector<std::pair<vxlnetwork::account, uint64_t>> voters;

	bool needs_eval () const
	{
		return !status.bootstrap_started || !status.election_started || !status.confirmed;
	}

	std::string to_string () const;
};

}
