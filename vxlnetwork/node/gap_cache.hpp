#pragma once

#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/lib/utility.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace vxlnetwork
{
class node;
class transaction;

/** For each gap in account chains, track arrival time and voters */
class gap_information final
{
public:
	std::chrono::steady_clock::time_point arrival;
	vxlnetwork::block_hash hash;
	std::vector<vxlnetwork::account> voters;
	bool bootstrap_started{ false };
};

/** Maintains voting and arrival information for gaps (missing source or previous blocks in account chains) */
class gap_cache final
{
public:
	explicit gap_cache (vxlnetwork::node &);
	void add (vxlnetwork::block_hash const &, std::chrono::steady_clock::time_point = std::chrono::steady_clock::now ());
	void erase (vxlnetwork::block_hash const & hash_a);
	void vote (std::shared_ptr<vxlnetwork::vote> const &);
	bool bootstrap_check (std::vector<vxlnetwork::account> const &, vxlnetwork::block_hash const &);
	void bootstrap_start (vxlnetwork::block_hash const & hash_a);
	vxlnetwork::uint128_t bootstrap_threshold ();
	std::size_t size ();
	// clang-format off
	class tag_arrival {};
	class tag_hash {};
	using ordered_gaps = boost::multi_index_container<vxlnetwork::gap_information,
	boost::multi_index::indexed_by<
		boost::multi_index::ordered_non_unique<boost::multi_index::tag<tag_arrival>,
			boost::multi_index::member<gap_information, std::chrono::steady_clock::time_point, &gap_information::arrival>>,
		boost::multi_index::hashed_unique<boost::multi_index::tag<tag_hash>,
			boost::multi_index::member<gap_information, vxlnetwork::block_hash, &gap_information::hash>>>>;
	ordered_gaps blocks;
	// clang-format on
	std::size_t const max = 256;
	vxlnetwork::mutex mutex{ mutex_identifier (mutexes::gap_cache) };
	vxlnetwork::node & node;
};

std::unique_ptr<container_info_component> collect_container_info (gap_cache & gap_cache, std::string const & name);
}
