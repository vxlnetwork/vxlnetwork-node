#include <vxlnetwork/node/confirmation_solicitor.hpp>
#include <vxlnetwork/node/election.hpp>
#include <vxlnetwork/node/nodeconfig.hpp>

using namespace std::chrono_literals;

vxlnetwork::confirmation_solicitor::confirmation_solicitor (vxlnetwork::network & network_a, vxlnetwork::node_config const & config_a) :
	max_block_broadcasts (config_a.network_params.network.is_dev_network () ? 4 : 30),
	max_election_requests (50),
	max_election_broadcasts (std::max<std::size_t> (network_a.fanout () / 2, 1)),
	network (network_a),
	config (config_a)
{
}

void vxlnetwork::confirmation_solicitor::prepare (std::vector<vxlnetwork::representative> const & representatives_a)
{
	debug_assert (!prepared);
	requests.clear ();
	rebroadcasted = 0;
	/** Two copies are required as representatives can be erased from \p representatives_requests */
	representatives_requests = representatives_a;
	representatives_broadcasts = representatives_a;
	prepared = true;
}

bool vxlnetwork::confirmation_solicitor::broadcast (vxlnetwork::election const & election_a)
{
	debug_assert (prepared);
	bool error (true);
	if (rebroadcasted++ < max_block_broadcasts)
	{
		auto const & hash (election_a.status.winner->hash ());
		vxlnetwork::publish winner{ config.network_params.network, election_a.status.winner };
		unsigned count = 0;
		// Directed broadcasting to principal representatives
		for (auto i (representatives_broadcasts.begin ()), n (representatives_broadcasts.end ()); i != n && count < max_election_broadcasts; ++i)
		{
			auto existing (election_a.last_votes.find (i->account));
			bool const exists (existing != election_a.last_votes.end ());
			bool const different (exists && existing->second.hash != hash);
			if (!exists || different)
			{
				i->channel->send (winner);
				count += different ? 0 : 1;
			}
		}
		// Random flood for block propagation
		network.flood_message (winner, vxlnetwork::buffer_drop_policy::limiter, 0.5f);
		error = false;
	}
	return error;
}

bool vxlnetwork::confirmation_solicitor::add (vxlnetwork::election const & election_a)
{
	debug_assert (prepared);
	bool error (true);
	unsigned count = 0;
	auto const max_channel_requests (config.confirm_req_batches_max * vxlnetwork::network::confirm_req_hashes_max);
	auto const & hash (election_a.status.winner->hash ());
	for (auto i (representatives_requests.begin ()); i != representatives_requests.end () && count < max_election_requests;)
	{
		bool full_queue (false);
		auto rep (*i);
		auto existing (election_a.last_votes.find (rep.account));
		bool const exists (existing != election_a.last_votes.end ());
		bool const is_final (exists && (!election_a.is_quorum.load () || existing->second.timestamp == std::numeric_limits<uint64_t>::max ()));
		bool const different (exists && existing->second.hash != hash);
		if (!exists || !is_final || different)
		{
			auto & request_queue (requests[rep.channel]);
			if (request_queue.size () < max_channel_requests)
			{
				request_queue.emplace_back (election_a.status.winner->hash (), election_a.status.winner->root ());
				count += different ? 0 : 1;
				error = false;
			}
			else
			{
				full_queue = true;
			}
		}
		i = !full_queue ? i + 1 : representatives_requests.erase (i);
	}
	return error;
}

void vxlnetwork::confirmation_solicitor::flush ()
{
	debug_assert (prepared);
	for (auto const & request_queue : requests)
	{
		auto const & channel (request_queue.first);
		std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::root>> roots_hashes_l;
		for (auto const & root_hash : request_queue.second)
		{
			roots_hashes_l.push_back (root_hash);
			if (roots_hashes_l.size () == vxlnetwork::network::confirm_req_hashes_max)
			{
				vxlnetwork::confirm_req req{ config.network_params.network, roots_hashes_l };
				channel->send (req);
				roots_hashes_l.clear ();
			}
		}
		if (!roots_hashes_l.empty ())
		{
			vxlnetwork::confirm_req req{ config.network_params.network, roots_hashes_l };
			channel->send (req);
		}
	}
	prepared = false;
}
