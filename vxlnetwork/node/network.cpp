#include <vxlnetwork/crypto_lib/random_pool_shuffle.hpp>
#include <vxlnetwork/lib/threading.hpp>
#include <vxlnetwork/node/network.hpp>
#include <vxlnetwork/node/node.hpp>
#include <vxlnetwork/node/telemetry.hpp>
#include <vxlnetwork/secure/buffer.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/format.hpp>
#include <boost/variant/get.hpp>

#include <numeric>

vxlnetwork::network::network (vxlnetwork::node & node_a, uint16_t port_a) :
	id (vxlnetwork::network_constants::active_network),
	syn_cookies (node_a.network_params.network.max_peers_per_ip),
	inbound{ [this] (vxlnetwork::message const & message, std::shared_ptr<vxlnetwork::transport::channel> const & channel) {
		if (message.header.network == id)
		{
			process_message (message, channel);
		}
		else
		{
			this->node.stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::invalid_network);
		}
	} },
	buffer_container (node_a.stats, vxlnetwork::network::buffer_size, 4096), // 2Mb receive buffer
	resolver (node_a.io_ctx),
	limiter (node_a.config.bandwidth_limit_burst_ratio, node_a.config.bandwidth_limit),
	tcp_message_manager (node_a.config.tcp_incoming_connections_max),
	node (node_a),
	publish_filter (256 * 1024),
	udp_channels (node_a, port_a, inbound),
	tcp_channels (node_a, inbound),
	port (port_a),
	disconnect_observer ([] () {})
{
	if (!node.flags.disable_udp)
	{
		port = udp_channels.get_local_endpoint ().port ();
	}

	boost::thread::attributes attrs;
	vxlnetwork::thread_attributes::set (attrs);
	// UDP
	for (std::size_t i = 0; i < node.config.network_threads && !node.flags.disable_udp; ++i)
	{
		packet_processing_threads.emplace_back (attrs, [this] () {
			vxlnetwork::thread_role::set (vxlnetwork::thread_role::name::packet_processing);
			try
			{
				udp_channels.process_packets ();
			}
			catch (boost::system::error_code & ec)
			{
				this->node.logger.always_log (FATAL_LOG_PREFIX, ec.message ());
				release_assert (false);
			}
			catch (std::error_code & ec)
			{
				this->node.logger.always_log (FATAL_LOG_PREFIX, ec.message ());
				release_assert (false);
			}
			catch (std::runtime_error & err)
			{
				this->node.logger.always_log (FATAL_LOG_PREFIX, err.what ());
				release_assert (false);
			}
			catch (...)
			{
				this->node.logger.always_log (FATAL_LOG_PREFIX, "Unknown exception");
				release_assert (false);
			}
			if (this->node.config.logging.network_packet_logging ())
			{
				this->node.logger.try_log ("Exiting UDP packet processing thread");
			}
		});
	}
	// TCP
	for (std::size_t i = 0; i < node.config.network_threads && !node.flags.disable_tcp_realtime; ++i)
	{
		packet_processing_threads.emplace_back (attrs, [this] () {
			vxlnetwork::thread_role::set (vxlnetwork::thread_role::name::packet_processing);
			try
			{
				tcp_channels.process_messages ();
			}
			catch (boost::system::error_code & ec)
			{
				this->node.logger.always_log (FATAL_LOG_PREFIX, ec.message ());
				release_assert (false);
			}
			catch (std::error_code & ec)
			{
				this->node.logger.always_log (FATAL_LOG_PREFIX, ec.message ());
				release_assert (false);
			}
			catch (std::runtime_error & err)
			{
				this->node.logger.always_log (FATAL_LOG_PREFIX, err.what ());
				release_assert (false);
			}
			catch (...)
			{
				this->node.logger.always_log (FATAL_LOG_PREFIX, "Unknown exception");
				release_assert (false);
			}
			if (this->node.config.logging.network_packet_logging ())
			{
				this->node.logger.try_log ("Exiting TCP packet processing thread");
			}
		});
	}
}

vxlnetwork::network::~network ()
{
	stop ();
}

void vxlnetwork::network::start ()
{
	if (!node.flags.disable_connection_cleanup)
	{
		ongoing_cleanup ();
	}
	ongoing_syn_cookie_cleanup ();
	if (!node.flags.disable_udp)
	{
		udp_channels.start ();
		debug_assert (udp_channels.get_local_endpoint ().port () == port);
	}
	if (!node.flags.disable_tcp_realtime)
	{
		tcp_channels.start ();
	}
	ongoing_keepalive ();
}

void vxlnetwork::network::stop ()
{
	if (!stopped.exchange (true))
	{
		udp_channels.stop ();
		tcp_channels.stop ();
		resolver.cancel ();
		buffer_container.stop ();
		tcp_message_manager.stop ();
		port = 0;
		for (auto & thread : packet_processing_threads)
		{
			thread.join ();
		}
	}
}

void vxlnetwork::network::send_keepalive (std::shared_ptr<vxlnetwork::transport::channel> const & channel_a)
{
	vxlnetwork::keepalive message{ node.network_params.network };
	random_fill (message.peers);
	channel_a->send (message);
}

void vxlnetwork::network::send_keepalive_self (std::shared_ptr<vxlnetwork::transport::channel> const & channel_a)
{
	vxlnetwork::keepalive message{ node.network_params.network };
	fill_keepalive_self (message.peers);
	channel_a->send (message);
}

void vxlnetwork::network::send_node_id_handshake (std::shared_ptr<vxlnetwork::transport::channel> const & channel_a, boost::optional<vxlnetwork::uint256_union> const & query, boost::optional<vxlnetwork::uint256_union> const & respond_to)
{
	boost::optional<std::pair<vxlnetwork::account, vxlnetwork::signature>> response (boost::none);
	if (respond_to)
	{
		response = std::make_pair (node.node_id.pub, vxlnetwork::sign_message (node.node_id.prv, node.node_id.pub, *respond_to));
		debug_assert (!vxlnetwork::validate_message (response->first, *respond_to, response->second));
	}
	vxlnetwork::node_id_handshake message{ node.network_params.network, query, response };
	if (node.config.logging.network_node_id_handshake_logging ())
	{
		node.logger.try_log (boost::str (boost::format ("Node ID handshake sent with node ID %1% to %2%: query %3%, respond_to %4% (signature %5%)") % node.node_id.pub.to_node_id () % channel_a->get_endpoint () % (query ? query->to_string () : std::string ("[none]")) % (respond_to ? respond_to->to_string () : std::string ("[none]")) % (response ? response->second.to_string () : std::string ("[none]"))));
	}
	channel_a->send (message);
}

void vxlnetwork::network::flood_message (vxlnetwork::message & message_a, vxlnetwork::buffer_drop_policy const drop_policy_a, float const scale_a)
{
	for (auto & i : list (fanout (scale_a)))
	{
		i->send (message_a, nullptr, drop_policy_a);
	}
}

void vxlnetwork::network::flood_keepalive (float const scale_a)
{
	vxlnetwork::keepalive message{ node.network_params.network };
	random_fill (message.peers);
	flood_message (message, vxlnetwork::buffer_drop_policy::limiter, scale_a);
}

void vxlnetwork::network::flood_keepalive_self (float const scale_a)
{
	vxlnetwork::keepalive message{ node.network_params.network };
	fill_keepalive_self (message.peers);
	flood_message (message, vxlnetwork::buffer_drop_policy::limiter, scale_a);
}

void vxlnetwork::network::flood_block (std::shared_ptr<vxlnetwork::block> const & block_a, vxlnetwork::buffer_drop_policy const drop_policy_a)
{
	vxlnetwork::publish message (node.network_params.network, block_a);
	flood_message (message, drop_policy_a);
}

void vxlnetwork::network::flood_block_initial (std::shared_ptr<vxlnetwork::block> const & block_a)
{
	vxlnetwork::publish message (node.network_params.network, block_a);
	for (auto const & i : node.rep_crawler.principal_representatives ())
	{
		i.channel->send (message, nullptr, vxlnetwork::buffer_drop_policy::no_limiter_drop);
	}
	for (auto & i : list_non_pr (fanout (1.0)))
	{
		i->send (message, nullptr, vxlnetwork::buffer_drop_policy::no_limiter_drop);
	}
}

void vxlnetwork::network::flood_vote (std::shared_ptr<vxlnetwork::vote> const & vote_a, float scale)
{
	vxlnetwork::confirm_ack message{ node.network_params.network, vote_a };
	for (auto & i : list (fanout (scale)))
	{
		i->send (message, nullptr);
	}
}

void vxlnetwork::network::flood_vote_pr (std::shared_ptr<vxlnetwork::vote> const & vote_a)
{
	vxlnetwork::confirm_ack message{ node.network_params.network, vote_a };
	for (auto const & i : node.rep_crawler.principal_representatives ())
	{
		i.channel->send (message, nullptr, vxlnetwork::buffer_drop_policy::no_limiter_drop);
	}
}

void vxlnetwork::network::flood_block_many (std::deque<std::shared_ptr<vxlnetwork::block>> blocks_a, std::function<void ()> callback_a, unsigned delay_a)
{
	if (!blocks_a.empty ())
	{
		auto block_l (blocks_a.front ());
		blocks_a.pop_front ();
		flood_block (block_l);
		if (!blocks_a.empty ())
		{
			std::weak_ptr<vxlnetwork::node> node_w (node.shared ());
			node.workers.add_timed_task (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a + std::rand () % delay_a), [node_w, blocks (std::move (blocks_a)), callback_a, delay_a] () {
				if (auto node_l = node_w.lock ())
				{
					node_l->network.flood_block_many (std::move (blocks), callback_a, delay_a);
				}
			});
		}
		else if (callback_a)
		{
			callback_a ();
		}
	}
}

void vxlnetwork::network::send_confirm_req (std::shared_ptr<vxlnetwork::transport::channel> const & channel_a, std::pair<vxlnetwork::block_hash, vxlnetwork::block_hash> const & hash_root_a)
{
	// Confirmation request with hash + root
	vxlnetwork::confirm_req req (node.network_params.network, hash_root_a.first, hash_root_a.second);
	channel_a->send (req);
}

void vxlnetwork::network::broadcast_confirm_req (std::shared_ptr<vxlnetwork::block> const & block_a)
{
	auto list (std::make_shared<std::vector<std::shared_ptr<vxlnetwork::transport::channel>>> (node.rep_crawler.representative_endpoints (std::numeric_limits<std::size_t>::max ())));
	if (list->empty () || node.rep_crawler.total_weight () < node.online_reps.delta ())
	{
		// broadcast request to all peers (with max limit 2 * sqrt (peers count))
		auto peers (node.network.list (std::min<std::size_t> (100, node.network.fanout (2.0))));
		list->clear ();
		list->insert (list->end (), peers.begin (), peers.end ());
	}

	/*
	 * In either case (broadcasting to all representatives, or broadcasting to
	 * all peers because there are not enough connected representatives),
	 * limit each instance to a single random up-to-32 selection.  The invoker
	 * of "broadcast_confirm_req" will be responsible for calling it again
	 * if the votes for a block have not arrived in time.
	 */
	std::size_t const max_endpoints = 32;
	vxlnetwork::random_pool_shuffle (list->begin (), list->end ());
	if (list->size () > max_endpoints)
	{
		list->erase (list->begin () + max_endpoints, list->end ());
	}

	broadcast_confirm_req_base (block_a, list, 0);
}

void vxlnetwork::network::broadcast_confirm_req_base (std::shared_ptr<vxlnetwork::block> const & block_a, std::shared_ptr<std::vector<std::shared_ptr<vxlnetwork::transport::channel>>> const & endpoints_a, unsigned delay_a, bool resumption)
{
	std::size_t const max_reps = 10;
	if (!resumption && node.config.logging.network_logging ())
	{
		node.logger.try_log (boost::str (boost::format ("Broadcasting confirm req for block %1% to %2% representatives") % block_a->hash ().to_string () % endpoints_a->size ()));
	}
	auto count (0);
	while (!endpoints_a->empty () && count < max_reps)
	{
		auto channel (endpoints_a->back ());
		send_confirm_req (channel, std::make_pair (block_a->hash (), block_a->root ().as_block_hash ()));
		endpoints_a->pop_back ();
		count++;
	}
	if (!endpoints_a->empty ())
	{
		delay_a += std::rand () % broadcast_interval_ms;

		std::weak_ptr<vxlnetwork::node> node_w (node.shared ());
		node.workers.add_timed_task (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a), [node_w, block_a, endpoints_a, delay_a] () {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.broadcast_confirm_req_base (block_a, endpoints_a, delay_a, true);
			}
		});
	}
}

void vxlnetwork::network::broadcast_confirm_req_batched_many (std::unordered_map<std::shared_ptr<vxlnetwork::transport::channel>, std::deque<std::pair<vxlnetwork::block_hash, vxlnetwork::root>>> request_bundle_a, std::function<void ()> callback_a, unsigned delay_a, bool resumption_a)
{
	if (!resumption_a && node.config.logging.network_logging ())
	{
		node.logger.try_log (boost::str (boost::format ("Broadcasting batch confirm req to %1% representatives") % request_bundle_a.size ()));
	}

	for (auto i (request_bundle_a.begin ()), n (request_bundle_a.end ()); i != n;)
	{
		std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::root>> roots_hashes_l;
		// Limit max request size hash + root to 7 pairs
		while (roots_hashes_l.size () < confirm_req_hashes_max && !i->second.empty ())
		{
			// expects ordering by priority, descending
			roots_hashes_l.push_back (i->second.front ());
			i->second.pop_front ();
		}
		vxlnetwork::confirm_req req{ node.network_params.network, roots_hashes_l };
		i->first->send (req);
		if (i->second.empty ())
		{
			i = request_bundle_a.erase (i);
		}
		else
		{
			++i;
		}
	}
	if (!request_bundle_a.empty ())
	{
		std::weak_ptr<vxlnetwork::node> node_w (node.shared ());
		node.workers.add_timed_task (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a), [node_w, request_bundle_a, callback_a, delay_a] () {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.broadcast_confirm_req_batched_many (request_bundle_a, callback_a, delay_a, true);
			}
		});
	}
	else if (callback_a)
	{
		callback_a ();
	}
}

void vxlnetwork::network::broadcast_confirm_req_many (std::deque<std::pair<std::shared_ptr<vxlnetwork::block>, std::shared_ptr<std::vector<std::shared_ptr<vxlnetwork::transport::channel>>>>> requests_a, std::function<void ()> callback_a, unsigned delay_a)
{
	auto pair_l (requests_a.front ());
	requests_a.pop_front ();
	auto block_l (pair_l.first);
	// confirm_req to representatives
	auto endpoints (pair_l.second);
	if (!endpoints->empty ())
	{
		broadcast_confirm_req_base (block_l, endpoints, delay_a);
	}
	/* Continue while blocks remain
	Broadcast with random delay between delay_a & 2*delay_a */
	if (!requests_a.empty ())
	{
		std::weak_ptr<vxlnetwork::node> node_w (node.shared ());
		node.workers.add_timed_task (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a + std::rand () % delay_a), [node_w, requests_a, callback_a, delay_a] () {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.broadcast_confirm_req_many (requests_a, callback_a, delay_a);
			}
		});
	}
	else if (callback_a)
	{
		callback_a ();
	}
}

namespace
{
class network_message_visitor : public vxlnetwork::message_visitor
{
public:
	network_message_visitor (vxlnetwork::node & node_a, std::shared_ptr<vxlnetwork::transport::channel> const & channel_a) :
		node (node_a),
		channel (channel_a)
	{
	}
	void keepalive (vxlnetwork::keepalive const & message_a) override
	{
		if (node.config.logging.network_keepalive_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Received keepalive message from %1%") % channel->to_string ()));
		}
		node.stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::keepalive, vxlnetwork::stat::dir::in);
		node.network.merge_peers (message_a.peers);
		// Check for special node port data
		auto peer0 (message_a.peers[0]);
		if (peer0.address () == boost::asio::ip::address_v6{} && peer0.port () != 0)
		{
			vxlnetwork::endpoint new_endpoint (channel->get_tcp_endpoint ().address (), peer0.port ());
			node.network.merge_peer (new_endpoint);
		}
	}
	void publish (vxlnetwork::publish const & message_a) override
	{
		if (node.config.logging.network_message_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Publish message from %1% for %2%") % channel->to_string () % message_a.block->hash ().to_string ()));
		}
		node.stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::publish, vxlnetwork::stat::dir::in);
		if (!node.block_processor.full ())
		{
			node.process_active (message_a.block);
		}
		else
		{
			node.network.publish_filter.clear (message_a.digest);
			node.stats.inc (vxlnetwork::stat::type::drop, vxlnetwork::stat::detail::publish, vxlnetwork::stat::dir::in);
		}
	}
	void confirm_req (vxlnetwork::confirm_req const & message_a) override
	{
		if (node.config.logging.network_message_logging ())
		{
			if (!message_a.roots_hashes.empty ())
			{
				node.logger.try_log (boost::str (boost::format ("Confirm_req message from %1% for hashes:roots %2%") % channel->to_string () % message_a.roots_string ()));
			}
			else
			{
				node.logger.try_log (boost::str (boost::format ("Confirm_req message from %1% for %2%") % channel->to_string () % message_a.block->hash ().to_string ()));
			}
		}
		node.stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::confirm_req, vxlnetwork::stat::dir::in);
		// Don't load nodes with disabled voting
		if (node.config.enable_voting && node.wallets.reps ().voting > 0)
		{
			if (message_a.block != nullptr)
			{
				node.aggregator.add (channel, { { message_a.block->hash (), message_a.block->root () } });
			}
			else if (!message_a.roots_hashes.empty ())
			{
				node.aggregator.add (channel, message_a.roots_hashes);
			}
		}
	}
	void confirm_ack (vxlnetwork::confirm_ack const & message_a) override
	{
		if (node.config.logging.network_message_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Received confirm_ack message from %1% for %2% timestamp %3%") % channel->to_string () % message_a.vote->hashes_string () % std::to_string (message_a.vote->timestamp ())));
		}
		node.stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::confirm_ack, vxlnetwork::stat::dir::in);
		if (!message_a.vote->account.is_zero ())
		{
			if (message_a.header.block_type () != vxlnetwork::block_type::not_a_block)
			{
				for (auto & vote_block : message_a.vote->blocks)
				{
					if (!vote_block.which ())
					{
						auto const & block (boost::get<std::shared_ptr<vxlnetwork::block>> (vote_block));
						if (!node.block_processor.full ())
						{
							node.process_active (block);
						}
						else
						{
							node.stats.inc (vxlnetwork::stat::type::drop, vxlnetwork::stat::detail::confirm_ack, vxlnetwork::stat::dir::in);
						}
					}
				}
			}
			node.vote_processor.vote (message_a.vote, channel);
		}
	}
	void bulk_pull (vxlnetwork::bulk_pull const &) override
	{
		debug_assert (false);
	}
	void bulk_pull_account (vxlnetwork::bulk_pull_account const &) override
	{
		debug_assert (false);
	}
	void bulk_push (vxlnetwork::bulk_push const &) override
	{
		debug_assert (false);
	}
	void frontier_req (vxlnetwork::frontier_req const &) override
	{
		debug_assert (false);
	}
	void node_id_handshake (vxlnetwork::node_id_handshake const & message_a) override
	{
		node.stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::node_id_handshake, vxlnetwork::stat::dir::in);
	}
	void telemetry_req (vxlnetwork::telemetry_req const & message_a) override
	{
		if (node.config.logging.network_telemetry_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Telemetry_req message from %1%") % channel->to_string ()));
		}
		node.stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::telemetry_req, vxlnetwork::stat::dir::in);

		// Send an empty telemetry_ack if we do not want, just to acknowledge that we have received the message to
		// remove any timeouts on the server side waiting for a message.
		vxlnetwork::telemetry_ack telemetry_ack{ node.network_params.network };
		if (!node.flags.disable_providing_telemetry_metrics)
		{
			auto telemetry_data = vxlnetwork::local_telemetry_data (node.ledger, node.network, node.unchecked, node.config.bandwidth_limit, node.network_params, node.startup_time, node.default_difficulty (vxlnetwork::work_version::work_1), node.node_id);
			telemetry_ack = vxlnetwork::telemetry_ack{ node.network_params.network, telemetry_data };
		}
		channel->send (telemetry_ack, nullptr, vxlnetwork::buffer_drop_policy::no_socket_drop);
	}
	void telemetry_ack (vxlnetwork::telemetry_ack const & message_a) override
	{
		if (node.config.logging.network_telemetry_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Received telemetry_ack message from %1%") % channel->to_string ()));
		}
		node.stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::telemetry_ack, vxlnetwork::stat::dir::in);
		if (node.telemetry)
		{
			node.telemetry->set (message_a, *channel);
		}
	}
	vxlnetwork::node & node;
	std::shared_ptr<vxlnetwork::transport::channel> channel;
};
}

void vxlnetwork::network::process_message (vxlnetwork::message const & message_a, std::shared_ptr<vxlnetwork::transport::channel> const & channel_a)
{
	network_message_visitor visitor (node, channel_a);
	message_a.visit (visitor);
}

// Send keepalives to all the peers we've been notified of
void vxlnetwork::network::merge_peers (std::array<vxlnetwork::endpoint, 8> const & peers_a)
{
	for (auto i (peers_a.begin ()), j (peers_a.end ()); i != j; ++i)
	{
		merge_peer (*i);
	}
}

void vxlnetwork::network::merge_peer (vxlnetwork::endpoint const & peer_a)
{
	if (!reachout (peer_a, node.config.allow_local_peers))
	{
		std::weak_ptr<vxlnetwork::node> node_w (node.shared ());
		node.network.tcp_channels.start_tcp (peer_a);
	}
}

bool vxlnetwork::network::not_a_peer (vxlnetwork::endpoint const & endpoint_a, bool allow_local_peers)
{
	bool result (false);
	if (endpoint_a.address ().to_v6 ().is_unspecified ())
	{
		result = true;
	}
	else if (vxlnetwork::transport::reserved_address (endpoint_a, allow_local_peers))
	{
		result = true;
	}
	else if (endpoint_a == endpoint ())
	{
		result = true;
	}
	return result;
}

bool vxlnetwork::network::reachout (vxlnetwork::endpoint const & endpoint_a, bool allow_local_peers)
{
	// Don't contact invalid IPs
	bool error = not_a_peer (endpoint_a, allow_local_peers);
	if (!error)
	{
		error |= udp_channels.reachout (endpoint_a);
		error |= tcp_channels.reachout (endpoint_a);
	}
	return error;
}

std::deque<std::shared_ptr<vxlnetwork::transport::channel>> vxlnetwork::network::list (std::size_t count_a, uint8_t minimum_version_a, bool include_tcp_temporary_channels_a)
{
	std::deque<std::shared_ptr<vxlnetwork::transport::channel>> result;
	tcp_channels.list (result, minimum_version_a, include_tcp_temporary_channels_a);
	udp_channels.list (result, minimum_version_a);
	vxlnetwork::random_pool_shuffle (result.begin (), result.end ());
	if (result.size () > count_a)
	{
		result.resize (count_a, nullptr);
	}
	return result;
}

std::deque<std::shared_ptr<vxlnetwork::transport::channel>> vxlnetwork::network::list_non_pr (std::size_t count_a)
{
	std::deque<std::shared_ptr<vxlnetwork::transport::channel>> result;
	tcp_channels.list (result);
	udp_channels.list (result);
	vxlnetwork::random_pool_shuffle (result.begin (), result.end ());
	result.erase (std::remove_if (result.begin (), result.end (), [this] (std::shared_ptr<vxlnetwork::transport::channel> const & channel) {
		return this->node.rep_crawler.is_pr (*channel);
	}),
	result.end ());
	if (result.size () > count_a)
	{
		result.resize (count_a, nullptr);
	}
	return result;
}

// Simulating with sqrt_broadcast_simulate shows we only need to broadcast to sqrt(total_peers) random peers in order to successfully publish to everyone with high probability
std::size_t vxlnetwork::network::fanout (float scale) const
{
	return static_cast<std::size_t> (std::ceil (scale * size_sqrt ()));
}

std::unordered_set<std::shared_ptr<vxlnetwork::transport::channel>> vxlnetwork::network::random_set (std::size_t count_a, uint8_t min_version_a, bool include_temporary_channels_a) const
{
	std::unordered_set<std::shared_ptr<vxlnetwork::transport::channel>> result (tcp_channels.random_set (count_a, min_version_a, include_temporary_channels_a));
	std::unordered_set<std::shared_ptr<vxlnetwork::transport::channel>> udp_random (udp_channels.random_set (count_a, min_version_a));
	for (auto i (udp_random.begin ()), n (udp_random.end ()); i != n && result.size () < count_a * 1.5; ++i)
	{
		result.insert (*i);
	}
	while (result.size () > count_a)
	{
		result.erase (result.begin ());
	}
	return result;
}

void vxlnetwork::network::random_fill (std::array<vxlnetwork::endpoint, 8> & target_a) const
{
	auto peers (random_set (target_a.size (), 0, false)); // Don't include channels with ephemeral remote ports
	debug_assert (peers.size () <= target_a.size ());
	auto endpoint (vxlnetwork::endpoint (boost::asio::ip::address_v6{}, 0));
	debug_assert (endpoint.address ().is_v6 ());
	std::fill (target_a.begin (), target_a.end (), endpoint);
	auto j (target_a.begin ());
	for (auto i (peers.begin ()), n (peers.end ()); i != n; ++i, ++j)
	{
		debug_assert ((*i)->get_endpoint ().address ().is_v6 ());
		debug_assert (j < target_a.end ());
		*j = (*i)->get_endpoint ();
	}
}

void vxlnetwork::network::fill_keepalive_self (std::array<vxlnetwork::endpoint, 8> & target_a) const
{
	random_fill (target_a);
	// We will clobber values in index 0 and 1 and if there are only 2 nodes in the system, these are the only positions occupied
	// Move these items to index 2 and 3 so they propagate
	target_a[2] = target_a[0];
	target_a[3] = target_a[1];
	// Replace part of message with node external address or listening port
	target_a[1] = vxlnetwork::endpoint (boost::asio::ip::address_v6{}, 0); // For node v19 (response channels)
	if (node.config.external_address != boost::asio::ip::address_v6{}.to_string () && node.config.external_port != 0)
	{
		target_a[0] = vxlnetwork::endpoint (boost::asio::ip::make_address_v6 (node.config.external_address), node.config.external_port);
	}
	else
	{
		auto external_address (node.port_mapping.external_address ());
		if (external_address.address () != boost::asio::ip::address_v4::any ())
		{
			target_a[0] = vxlnetwork::endpoint (boost::asio::ip::address_v6{}, port);
			boost::system::error_code ec;
			auto external_v6 = boost::asio::ip::make_address_v6 (external_address.address ().to_string (), ec);
			target_a[1] = vxlnetwork::endpoint (external_v6, external_address.port ());
		}
		else
		{
			target_a[0] = vxlnetwork::endpoint (boost::asio::ip::address_v6{}, port);
		}
	}
}

vxlnetwork::tcp_endpoint vxlnetwork::network::bootstrap_peer (bool lazy_bootstrap)
{
	vxlnetwork::tcp_endpoint result (boost::asio::ip::address_v6::any (), 0);
	bool use_udp_peer (vxlnetwork::random_pool::generate_word32 (0, 1));
	if (use_udp_peer || tcp_channels.size () == 0)
	{
		result = udp_channels.bootstrap_peer (node.network_params.network.protocol_version_min);
	}
	if (result == vxlnetwork::tcp_endpoint (boost::asio::ip::address_v6::any (), 0))
	{
		result = tcp_channels.bootstrap_peer (node.network_params.network.protocol_version_min);
	}
	return result;
}

std::shared_ptr<vxlnetwork::transport::channel> vxlnetwork::network::find_channel (vxlnetwork::endpoint const & endpoint_a)
{
	std::shared_ptr<vxlnetwork::transport::channel> result (tcp_channels.find_channel (vxlnetwork::transport::map_endpoint_to_tcp (endpoint_a)));
	if (!result)
	{
		result = udp_channels.channel (endpoint_a);
	}
	return result;
}

std::shared_ptr<vxlnetwork::transport::channel> vxlnetwork::network::find_node_id (vxlnetwork::account const & node_id_a)
{
	std::shared_ptr<vxlnetwork::transport::channel> result (tcp_channels.find_node_id (node_id_a));
	if (!result)
	{
		result = udp_channels.find_node_id (node_id_a);
	}
	return result;
}

vxlnetwork::endpoint vxlnetwork::network::endpoint ()
{
	return vxlnetwork::endpoint (boost::asio::ip::address_v6::loopback (), port);
}

void vxlnetwork::network::cleanup (std::chrono::steady_clock::time_point const & cutoff_a)
{
	tcp_channels.purge (cutoff_a);
	udp_channels.purge (cutoff_a);
	if (node.network.empty ())
	{
		disconnect_observer ();
	}
}

void vxlnetwork::network::ongoing_cleanup ()
{
	cleanup (std::chrono::steady_clock::now () - node.network_params.network.cleanup_cutoff ());
	std::weak_ptr<vxlnetwork::node> node_w (node.shared ());
	node.workers.add_timed_task (std::chrono::steady_clock::now () + node.network_params.network.cleanup_period, [node_w] () {
		if (auto node_l = node_w.lock ())
		{
			node_l->network.ongoing_cleanup ();
		}
	});
}

void vxlnetwork::network::ongoing_syn_cookie_cleanup ()
{
	syn_cookies.purge (std::chrono::steady_clock::now () - vxlnetwork::transport::syn_cookie_cutoff);
	std::weak_ptr<vxlnetwork::node> node_w (node.shared ());
	node.workers.add_timed_task (std::chrono::steady_clock::now () + (vxlnetwork::transport::syn_cookie_cutoff * 2), [node_w] () {
		if (auto node_l = node_w.lock ())
		{
			node_l->network.ongoing_syn_cookie_cleanup ();
		}
	});
}

void vxlnetwork::network::ongoing_keepalive ()
{
	flood_keepalive (0.75f);
	flood_keepalive_self (0.25f);
	std::weak_ptr<vxlnetwork::node> node_w (node.shared ());
	node.workers.add_timed_task (std::chrono::steady_clock::now () + node.network_params.network.cleanup_period_half (), [node_w] () {
		if (auto node_l = node_w.lock ())
		{
			node_l->network.ongoing_keepalive ();
		}
	});
}

std::size_t vxlnetwork::network::size () const
{
	return tcp_channels.size () + udp_channels.size ();
}

float vxlnetwork::network::size_sqrt () const
{
	return static_cast<float> (std::sqrt (size ()));
}

bool vxlnetwork::network::empty () const
{
	return size () == 0;
}

void vxlnetwork::network::erase (vxlnetwork::transport::channel const & channel_a)
{
	auto const channel_type = channel_a.get_type ();
	if (channel_type == vxlnetwork::transport::transport_type::tcp)
	{
		tcp_channels.erase (channel_a.get_tcp_endpoint ());
	}
	else if (channel_type != vxlnetwork::transport::transport_type::loopback)
	{
		udp_channels.erase (channel_a.get_endpoint ());
		udp_channels.clean_node_id (channel_a.get_node_id ());
	}
}

void vxlnetwork::network::set_bandwidth_params (double limit_burst_ratio_a, std::size_t limit_a)
{
	limiter.reset (limit_burst_ratio_a, limit_a);
}

vxlnetwork::message_buffer_manager::message_buffer_manager (vxlnetwork::stat & stats_a, std::size_t size, std::size_t count) :
	stats (stats_a),
	free (count),
	full (count),
	slab (size * count),
	entries (count),
	stopped (false)
{
	debug_assert (count > 0);
	debug_assert (size > 0);
	auto slab_data (slab.data ());
	auto entry_data (entries.data ());
	for (auto i (0); i < count; ++i, ++entry_data)
	{
		*entry_data = { slab_data + i * size, 0, vxlnetwork::endpoint () };
		free.push_back (entry_data);
	}
}

vxlnetwork::message_buffer * vxlnetwork::message_buffer_manager::allocate ()
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock (mutex);
	if (!stopped && free.empty () && full.empty ())
	{
		stats.inc (vxlnetwork::stat::type::udp, vxlnetwork::stat::detail::blocking, vxlnetwork::stat::dir::in);
		condition.wait (lock, [&stopped = stopped, &free = free, &full = full] { return stopped || !free.empty () || !full.empty (); });
	}
	vxlnetwork::message_buffer * result (nullptr);
	if (!free.empty ())
	{
		result = free.front ();
		free.pop_front ();
	}
	if (result == nullptr && !full.empty ())
	{
		result = full.front ();
		full.pop_front ();
		stats.inc (vxlnetwork::stat::type::udp, vxlnetwork::stat::detail::overflow, vxlnetwork::stat::dir::in);
	}
	release_assert (result || stopped);
	return result;
}

void vxlnetwork::message_buffer_manager::enqueue (vxlnetwork::message_buffer * data_a)
{
	debug_assert (data_a != nullptr);
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		full.push_back (data_a);
	}
	condition.notify_all ();
}

vxlnetwork::message_buffer * vxlnetwork::message_buffer_manager::dequeue ()
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock (mutex);
	while (!stopped && full.empty ())
	{
		condition.wait (lock);
	}
	vxlnetwork::message_buffer * result (nullptr);
	if (!full.empty ())
	{
		result = full.front ();
		full.pop_front ();
	}
	return result;
}

void vxlnetwork::message_buffer_manager::release (vxlnetwork::message_buffer * data_a)
{
	debug_assert (data_a != nullptr);
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		free.push_back (data_a);
	}
	condition.notify_all ();
}

void vxlnetwork::message_buffer_manager::stop ()
{
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		stopped = true;
	}
	condition.notify_all ();
}

vxlnetwork::tcp_message_manager::tcp_message_manager (unsigned incoming_connections_max_a) :
	max_entries (incoming_connections_max_a * vxlnetwork::tcp_message_manager::max_entries_per_connection + 1)
{
	debug_assert (max_entries > 0);
}

void vxlnetwork::tcp_message_manager::put_message (vxlnetwork::tcp_message_item const & item_a)
{
	{
		vxlnetwork::unique_lock<vxlnetwork::mutex> lock (mutex);
		while (entries.size () >= max_entries && !stopped)
		{
			producer_condition.wait (lock);
		}
		entries.push_back (item_a);
	}
	consumer_condition.notify_one ();
}

vxlnetwork::tcp_message_item vxlnetwork::tcp_message_manager::get_message ()
{
	vxlnetwork::tcp_message_item result;
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock (mutex);
	while (entries.empty () && !stopped)
	{
		consumer_condition.wait (lock);
	}
	if (!entries.empty ())
	{
		result = std::move (entries.front ());
		entries.pop_front ();
	}
	else
	{
		result = vxlnetwork::tcp_message_item{ nullptr, vxlnetwork::tcp_endpoint (boost::asio::ip::address_v6::any (), 0), 0, nullptr };
	}
	lock.unlock ();
	producer_condition.notify_one ();
	return result;
}

void vxlnetwork::tcp_message_manager::stop ()
{
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		stopped = true;
	}
	consumer_condition.notify_all ();
	producer_condition.notify_all ();
}

vxlnetwork::syn_cookies::syn_cookies (std::size_t max_cookies_per_ip_a) :
	max_cookies_per_ip (max_cookies_per_ip_a)
{
}

boost::optional<vxlnetwork::uint256_union> vxlnetwork::syn_cookies::assign (vxlnetwork::endpoint const & endpoint_a)
{
	auto ip_addr (endpoint_a.address ());
	debug_assert (ip_addr.is_v6 ());
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (syn_cookie_mutex);
	unsigned & ip_cookies = cookies_per_ip[ip_addr];
	boost::optional<vxlnetwork::uint256_union> result;
	if (ip_cookies < max_cookies_per_ip)
	{
		if (cookies.find (endpoint_a) == cookies.end ())
		{
			vxlnetwork::uint256_union query;
			random_pool::generate_block (query.bytes.data (), query.bytes.size ());
			syn_cookie_info info{ query, std::chrono::steady_clock::now () };
			cookies[endpoint_a] = info;
			++ip_cookies;
			result = query;
		}
	}
	return result;
}

bool vxlnetwork::syn_cookies::validate (vxlnetwork::endpoint const & endpoint_a, vxlnetwork::account const & node_id, vxlnetwork::signature const & sig)
{
	auto ip_addr (endpoint_a.address ());
	debug_assert (ip_addr.is_v6 ());
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (syn_cookie_mutex);
	auto result (true);
	auto cookie_it (cookies.find (endpoint_a));
	if (cookie_it != cookies.end () && !vxlnetwork::validate_message (node_id, cookie_it->second.cookie, sig))
	{
		result = false;
		cookies.erase (cookie_it);
		unsigned & ip_cookies = cookies_per_ip[ip_addr];
		if (ip_cookies > 0)
		{
			--ip_cookies;
		}
		else
		{
			debug_assert (false && "More SYN cookies deleted than created for IP");
		}
	}
	return result;
}

void vxlnetwork::syn_cookies::purge (std::chrono::steady_clock::time_point const & cutoff_a)
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (syn_cookie_mutex);
	auto it (cookies.begin ());
	while (it != cookies.end ())
	{
		auto info (it->second);
		if (info.created_at < cutoff_a)
		{
			unsigned & per_ip = cookies_per_ip[it->first.address ()];
			if (per_ip > 0)
			{
				--per_ip;
			}
			else
			{
				debug_assert (false && "More SYN cookies deleted than created for IP");
			}
			it = cookies.erase (it);
		}
		else
		{
			++it;
		}
	}
}

std::size_t vxlnetwork::syn_cookies::cookies_size ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (syn_cookie_mutex);
	return cookies.size ();
}

std::unique_ptr<vxlnetwork::container_info_component> vxlnetwork::collect_container_info (network & network, std::string const & name)
{
	auto composite = std::make_unique<container_info_composite> (name);
	composite->add_component (network.tcp_channels.collect_container_info ("tcp_channels"));
	composite->add_component (network.udp_channels.collect_container_info ("udp_channels"));
	composite->add_component (network.syn_cookies.collect_container_info ("syn_cookies"));
	composite->add_component (collect_container_info (network.excluded_peers, "excluded_peers"));
	return composite;
}

std::unique_ptr<vxlnetwork::container_info_component> vxlnetwork::syn_cookies::collect_container_info (std::string const & name)
{
	std::size_t syn_cookies_count;
	std::size_t syn_cookies_per_ip_count;
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> syn_cookie_guard (syn_cookie_mutex);
		syn_cookies_count = cookies.size ();
		syn_cookies_per_ip_count = cookies_per_ip.size ();
	}
	auto composite = std::make_unique<container_info_composite> (name);
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "syn_cookies", syn_cookies_count, sizeof (decltype (cookies)::value_type) }));
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "syn_cookies_per_ip", syn_cookies_per_ip_count, sizeof (decltype (cookies_per_ip)::value_type) }));
	return composite;
}

std::string vxlnetwork::network::to_string (vxlnetwork::networks network)
{
	switch (network)
	{
		case vxlnetwork::networks::invalid:
			return "invalid";
		case vxlnetwork::networks::vxlnetwork_beta_network:
			return "beta";
		case vxlnetwork::networks::vxlnetwork_dev_network:
			return "dev";
		case vxlnetwork::networks::vxlnetwork_live_network:
			return "live";
		case vxlnetwork::networks::vxlnetwork_test_network:
			return "test";
			// default case intentionally omitted to cause warnings for unhandled enums
	}

	return "n/a";
}
