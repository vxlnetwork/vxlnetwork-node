#include <vxlnetwork/lib/jsonconfig.hpp>
#include <vxlnetwork/node/confirmation_solicitor.hpp>
#include <vxlnetwork/test_common/system.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST (confirmation_solicitor, batches)
{
	vxlnetwork::system system;
	vxlnetwork::node_flags node_flags;
	node_flags.disable_request_loop = true;
	node_flags.disable_rep_crawler = true;
	node_flags.disable_udp = false;
	auto & node1 = *system.add_node (node_flags);
	node_flags.disable_request_loop = true;
	auto & node2 = *system.add_node (node_flags);
	auto channel1 (node2.network.udp_channels.create (node1.network.endpoint ()));
	// Solicitor will only solicit from this representative
	vxlnetwork::representative representative (vxlnetwork::dev::genesis_key.pub, vxlnetwork::dev::constants.genesis_amount, channel1);
	std::vector<vxlnetwork::representative> representatives{ representative };
	vxlnetwork::confirmation_solicitor solicitor (node2.network, node2.config);
	solicitor.prepare (representatives);
	// Ensure the representatives are correct
	ASSERT_EQ (1, representatives.size ());
	ASSERT_EQ (channel1, representatives.front ().channel);
	ASSERT_EQ (vxlnetwork::dev::genesis_key.pub, representatives.front ().account);
	ASSERT_TIMELY (3s, node2.network.size () == 1);
	auto send (std::make_shared<vxlnetwork::send_block> (vxlnetwork::dev::genesis->hash (), vxlnetwork::keypair ().pub, vxlnetwork::dev::constants.genesis_amount - 100, vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub, *system.work.generate (vxlnetwork::dev::genesis->hash ())));
	send->sideband_set ({});
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (node2.active.mutex);
		for (size_t i (0); i < vxlnetwork::network::confirm_req_hashes_max; ++i)
		{
			auto election (std::make_shared<vxlnetwork::election> (node2, send, nullptr, nullptr, vxlnetwork::election_behavior::normal));
			ASSERT_FALSE (solicitor.add (*election));
		}
		// Reached the maximum amount of requests for the channel
		auto election (std::make_shared<vxlnetwork::election> (node2, send, nullptr, nullptr, vxlnetwork::election_behavior::normal));
		ASSERT_TRUE (solicitor.add (*election));
		// Broadcasting should be immediate
		ASSERT_EQ (0, node2.stats.count (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::publish, vxlnetwork::stat::dir::out));
		ASSERT_FALSE (solicitor.broadcast (*election));
	}
	// One publish through directed broadcasting and another through random flooding
	ASSERT_EQ (2, node2.stats.count (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::publish, vxlnetwork::stat::dir::out));
	solicitor.flush ();
	ASSERT_EQ (1, node2.stats.count (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::confirm_req, vxlnetwork::stat::dir::out));
}

namespace vxlnetwork
{
TEST (confirmation_solicitor, different_hash)
{
	vxlnetwork::system system;
	vxlnetwork::node_flags node_flags;
	node_flags.disable_request_loop = true;
	node_flags.disable_rep_crawler = true;
	node_flags.disable_udp = false;
	auto & node1 = *system.add_node (node_flags);
	auto & node2 = *system.add_node (node_flags);
	auto channel1 (node2.network.udp_channels.create (node1.network.endpoint ()));
	// Solicitor will only solicit from this representative
	vxlnetwork::representative representative (vxlnetwork::dev::genesis_key.pub, vxlnetwork::dev::constants.genesis_amount, channel1);
	std::vector<vxlnetwork::representative> representatives{ representative };
	vxlnetwork::confirmation_solicitor solicitor (node2.network, node2.config);
	solicitor.prepare (representatives);
	// Ensure the representatives are correct
	ASSERT_EQ (1, representatives.size ());
	ASSERT_EQ (channel1, representatives.front ().channel);
	ASSERT_EQ (vxlnetwork::dev::genesis_key.pub, representatives.front ().account);
	ASSERT_TIMELY (3s, node2.network.size () == 1);
	auto send (std::make_shared<vxlnetwork::send_block> (vxlnetwork::dev::genesis->hash (), vxlnetwork::keypair ().pub, vxlnetwork::dev::constants.genesis_amount - 100, vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub, *system.work.generate (vxlnetwork::dev::genesis->hash ())));
	send->sideband_set ({});
	auto election (std::make_shared<vxlnetwork::election> (node2, send, nullptr, nullptr, vxlnetwork::election_behavior::normal));
	// Add a vote for something else, not the winner
	election->last_votes[representative.account] = { std::chrono::steady_clock::now (), 1, 1 };
	// Ensure the request and broadcast goes through
	ASSERT_FALSE (solicitor.add (*election));
	ASSERT_FALSE (solicitor.broadcast (*election));
	// One publish through directed broadcasting and another through random flooding
	ASSERT_EQ (2, node2.stats.count (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::publish, vxlnetwork::stat::dir::out));
	solicitor.flush ();
	ASSERT_EQ (1, node2.stats.count (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::confirm_req, vxlnetwork::stat::dir::out));
}

TEST (confirmation_solicitor, bypass_max_requests_cap)
{
	vxlnetwork::system system;
	vxlnetwork::node_flags node_flags;
	node_flags.disable_request_loop = true;
	node_flags.disable_rep_crawler = true;
	node_flags.disable_udp = false;
	auto & node1 = *system.add_node (node_flags);
	auto & node2 = *system.add_node (node_flags);
	vxlnetwork::confirmation_solicitor solicitor (node2.network, node2.config);
	std::vector<vxlnetwork::representative> representatives;
	auto max_representatives = std::max<size_t> (solicitor.max_election_requests, solicitor.max_election_broadcasts);
	representatives.reserve (max_representatives + 1);
	for (auto i (0); i < max_representatives + 1; ++i)
	{
		auto channel (node2.network.udp_channels.create (node1.network.endpoint ()));
		vxlnetwork::representative representative (vxlnetwork::account (i), i, channel);
		representatives.push_back (representative);
	}
	ASSERT_EQ (max_representatives + 1, representatives.size ());
	solicitor.prepare (representatives);
	ASSERT_TIMELY (3s, node2.network.size () == 1);
	auto send (std::make_shared<vxlnetwork::send_block> (vxlnetwork::dev::genesis->hash (), vxlnetwork::keypair ().pub, vxlnetwork::dev::constants.genesis_amount - 100, vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub, *system.work.generate (vxlnetwork::dev::genesis->hash ())));
	send->sideband_set ({});
	auto election (std::make_shared<vxlnetwork::election> (node2, send, nullptr, nullptr, vxlnetwork::election_behavior::normal));
	// Add a vote for something else, not the winner
	for (auto const & rep : representatives)
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (election->mutex);
		election->last_votes[rep.account] = { std::chrono::steady_clock::now (), 1, 1 };
	}
	ASSERT_FALSE (solicitor.add (*election));
	ASSERT_FALSE (solicitor.broadcast (*election));
	solicitor.flush ();
	// All requests went through, the last one would normally not go through due to the cap but a vote for a different hash does not count towards the cap
	ASSERT_EQ (max_representatives + 1, node2.stats.count (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::confirm_req, vxlnetwork::stat::dir::out));

	solicitor.prepare (representatives);
	auto election2 (std::make_shared<vxlnetwork::election> (node2, send, nullptr, nullptr, vxlnetwork::election_behavior::normal));
	ASSERT_FALSE (solicitor.add (*election2));
	ASSERT_FALSE (solicitor.broadcast (*election2));

	solicitor.flush ();

	// All requests but one went through, due to the cap
	ASSERT_EQ (2 * max_representatives + 1, node2.stats.count (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::confirm_req, vxlnetwork::stat::dir::out));
}
}
