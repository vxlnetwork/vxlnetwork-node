#include <vxlnetwork/node/bootstrap/bootstrap_bulk_push.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap_frontier.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap_server.hpp>
#include <vxlnetwork/node/node.hpp>
#include <vxlnetwork/node/transport/tcp.hpp>

#include <boost/format.hpp>
#include <boost/variant/get.hpp>

vxlnetwork::bootstrap_listener::bootstrap_listener (uint16_t port_a, vxlnetwork::node & node_a) :
	node (node_a),
	port (port_a)
{
}

void vxlnetwork::bootstrap_listener::start ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	on = true;
	listening_socket = std::make_shared<vxlnetwork::server_socket> (node, boost::asio::ip::tcp::endpoint (boost::asio::ip::address_v6::any (), port), node.config.tcp_incoming_connections_max);
	boost::system::error_code ec;
	listening_socket->start (ec);
	if (ec)
	{
		node.logger.always_log (boost::str (boost::format ("Network: Error while binding for incoming TCP/bootstrap on port %1%: %2%") % listening_socket->listening_port () % ec.message ()));
		throw std::runtime_error (ec.message ());
	}

	// the user can either specify a port value in the config or it can leave the choice up to the OS;
	// independently of user's port choice, he may have also opted to disable UDP or not; this gives us 4 possibilities:
	// (1): UDP enabled, port specified
	// (2): UDP enabled, port not specified
	// (3): UDP disabled, port specified
	// (4): UDP disabled, port not specified
	//
	const auto listening_port = listening_socket->listening_port ();
	if (!node.flags.disable_udp)
	{
		// (1) and (2) -- no matter if (1) or (2), since UDP socket binding happens before this TCP socket binding,
		// we must have already been constructed with a valid port value, so check that it really is the same everywhere
		//
		debug_assert (port == listening_port);
		debug_assert (port == node.network.port);
		debug_assert (port == node.network.endpoint ().port ());
	}
	else
	{
		// (3) -- nothing to do, just check that port values match everywhere
		//
		if (port == listening_port)
		{
			debug_assert (port == node.network.port);
			debug_assert (port == node.network.endpoint ().port ());
		}
		// (4) -- OS port choice happened at TCP socket bind time, so propagate this port value back;
		// the propagation is done here for the `bootstrap_listener` itself, whereas for `network`, the node does it
		// after calling `bootstrap_listener.start ()`
		//
		else
		{
			port = listening_port;
		}
	}

	listening_socket->on_connection ([this] (std::shared_ptr<vxlnetwork::socket> const & new_connection, boost::system::error_code const & ec_a) {
		if (!ec_a)
		{
			accept_action (ec_a, new_connection);
		}
		return true;
	});
}

void vxlnetwork::bootstrap_listener::stop ()
{
	decltype (connections) connections_l;
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		on = false;
		connections_l.swap (connections);
	}
	if (listening_socket)
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		listening_socket->close ();
		listening_socket = nullptr;
	}
}

std::size_t vxlnetwork::bootstrap_listener::connection_count ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	return connections.size ();
}

void vxlnetwork::bootstrap_listener::accept_action (boost::system::error_code const & ec, std::shared_ptr<vxlnetwork::socket> const & socket_a)
{
	if (!node.network.excluded_peers.check (socket_a->remote_endpoint ()))
	{
		auto connection (std::make_shared<vxlnetwork::bootstrap_server> (socket_a, node.shared ()));
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		connections[connection.get ()] = connection;
		connection->receive ();
	}
	else
	{
		node.stats.inc (vxlnetwork::stat::type::tcp, vxlnetwork::stat::detail::tcp_excluded);
		if (node.config.logging.network_rejected_logging ())
		{
			node.logger.try_log ("Rejected connection from excluded peer ", socket_a->remote_endpoint ());
		}
	}
}

boost::asio::ip::tcp::endpoint vxlnetwork::bootstrap_listener::endpoint ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	if (on && listening_socket)
	{
		return boost::asio::ip::tcp::endpoint (boost::asio::ip::address_v6::loopback (), port);
	}
	else
	{
		return boost::asio::ip::tcp::endpoint (boost::asio::ip::address_v6::loopback (), 0);
	}
}

std::unique_ptr<vxlnetwork::container_info_component> vxlnetwork::collect_container_info (bootstrap_listener & bootstrap_listener, std::string const & name)
{
	auto sizeof_element = sizeof (decltype (bootstrap_listener.connections)::value_type);
	auto composite = std::make_unique<container_info_composite> (name);
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "connections", bootstrap_listener.connection_count (), sizeof_element }));
	return composite;
}

vxlnetwork::bootstrap_server::bootstrap_server (std::shared_ptr<vxlnetwork::socket> const & socket_a, std::shared_ptr<vxlnetwork::node> const & node_a) :
	receive_buffer (std::make_shared<std::vector<uint8_t>> ()),
	socket (socket_a),
	node (node_a)
{
	debug_assert (socket_a != nullptr);
	receive_buffer->resize (1024);
}

vxlnetwork::bootstrap_server::~bootstrap_server ()
{
	if (node->config.logging.bulk_pull_logging ())
	{
		node->logger.try_log ("Exiting incoming TCP/bootstrap server");
	}
	if (socket->type () == vxlnetwork::socket::type_t::bootstrap)
	{
		--node->bootstrap.bootstrap_count;
	}
	else if (socket->type () == vxlnetwork::socket::type_t::realtime)
	{
		--node->bootstrap.realtime_count;
		// Clear temporary channel
		auto exisiting_response_channel (node->network.tcp_channels.find_channel (remote_endpoint));
		if (exisiting_response_channel != nullptr)
		{
			exisiting_response_channel->temporary = false;
			node->network.tcp_channels.erase (remote_endpoint);
		}
	}
	stop ();
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (node->bootstrap.mutex);
	node->bootstrap.connections.erase (this);
}

void vxlnetwork::bootstrap_server::stop ()
{
	if (!stopped.exchange (true))
	{
		socket->close ();
	}
}

void vxlnetwork::bootstrap_server::receive ()
{
	// Increase timeout to receive TCP header (idle server socket)
	socket->set_default_timeout_value (node->network_params.network.idle_timeout);
	auto this_l (shared_from_this ());
	socket->async_read (receive_buffer, 8, [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
		// Set remote_endpoint
		if (this_l->remote_endpoint.port () == 0)
		{
			this_l->remote_endpoint = this_l->socket->remote_endpoint ();
		}
		// Decrease timeout to default
		this_l->socket->set_default_timeout_value (this_l->node->config.tcp_io_timeout);
		// Receive header
		this_l->receive_header_action (ec, size_a);
	});
}

void vxlnetwork::bootstrap_server::receive_header_action (boost::system::error_code const & ec, std::size_t size_a)
{
	if (!ec)
	{
		debug_assert (size_a == 8);
		vxlnetwork::bufferstream type_stream (receive_buffer->data (), size_a);
		auto error (false);
		vxlnetwork::message_header header (error, type_stream);
		if (!error)
		{
			auto this_l (shared_from_this ());
			switch (header.type)
			{
				case vxlnetwork::message_type::bulk_pull:
				{
					node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::bulk_pull, vxlnetwork::stat::dir::in);
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_bulk_pull_action (ec, size_a, header);
					});
					break;
				}
				case vxlnetwork::message_type::bulk_pull_account:
				{
					node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::bulk_pull_account, vxlnetwork::stat::dir::in);
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_bulk_pull_account_action (ec, size_a, header);
					});
					break;
				}
				case vxlnetwork::message_type::frontier_req:
				{
					node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::frontier_req, vxlnetwork::stat::dir::in);
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_frontier_req_action (ec, size_a, header);
					});
					break;
				}
				case vxlnetwork::message_type::bulk_push:
				{
					node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::bulk_push, vxlnetwork::stat::dir::in);
					if (is_bootstrap_connection ())
					{
						add_request (std::make_unique<vxlnetwork::bulk_push> (header));
					}
					break;
				}
				case vxlnetwork::message_type::keepalive:
				{
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_keepalive_action (ec, size_a, header);
					});
					break;
				}
				case vxlnetwork::message_type::publish:
				{
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_publish_action (ec, size_a, header);
					});
					break;
				}
				case vxlnetwork::message_type::confirm_ack:
				{
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_confirm_ack_action (ec, size_a, header);
					});
					break;
				}
				case vxlnetwork::message_type::confirm_req:
				{
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_confirm_req_action (ec, size_a, header);
					});
					break;
				}
				case vxlnetwork::message_type::node_id_handshake:
				{
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_node_id_handshake_action (ec, size_a, header);
					});
					break;
				}
				case vxlnetwork::message_type::telemetry_req:
				{
					if (is_realtime_connection ())
					{
						// Only handle telemetry requests if they are outside of the cutoff time
						auto cache_exceeded = std::chrono::steady_clock::now () >= last_telemetry_req + vxlnetwork::telemetry_cache_cutoffs::network_to_time (node->network_params.network);
						if (cache_exceeded)
						{
							last_telemetry_req = std::chrono::steady_clock::now ();
							add_request (std::make_unique<vxlnetwork::telemetry_req> (header));
						}
						else
						{
							node->stats.inc (vxlnetwork::stat::type::telemetry, vxlnetwork::stat::detail::request_within_protection_cache_zone);
						}
					}
					receive ();
					break;
				}
				case vxlnetwork::message_type::telemetry_ack:
				{
					socket->async_read (receive_buffer, header.payload_length_bytes (), [this_l, header] (boost::system::error_code const & ec, std::size_t size_a) {
						this_l->receive_telemetry_ack_action (ec, size_a, header);
					});
					break;
				}
				default:
				{
					if (node->config.logging.network_logging ())
					{
						node->logger.try_log (boost::str (boost::format ("Received invalid type from bootstrap connection %1%") % static_cast<uint8_t> (header.type)));
					}
					break;
				}
			}
		}
	}
	else
	{
		if (node->config.logging.bulk_pull_logging ())
		{
			node->logger.try_log (boost::str (boost::format ("Error while receiving type: %1%") % ec.message ()));
		}
	}
}

void vxlnetwork::bootstrap_server::receive_bulk_pull_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		auto error (false);
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto request (std::make_unique<vxlnetwork::bulk_pull> (error, stream, header_a));
		if (!error)
		{
			if (node->config.logging.bulk_pull_logging ())
			{
				node->logger.try_log (boost::str (boost::format ("Received bulk pull for %1% down to %2%, maximum of %3% from %4%") % request->start.to_string () % request->end.to_string () % (request->count ? request->count : std::numeric_limits<double>::infinity ()) % remote_endpoint));
			}
			if (is_bootstrap_connection () && !node->flags.disable_bootstrap_bulk_pull_server)
			{
				add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
			}
			receive ();
		}
	}
}

void vxlnetwork::bootstrap_server::receive_bulk_pull_account_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		auto error (false);
		debug_assert (size_a == header_a.payload_length_bytes ());
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto request (std::make_unique<vxlnetwork::bulk_pull_account> (error, stream, header_a));
		if (!error)
		{
			if (node->config.logging.bulk_pull_logging ())
			{
				node->logger.try_log (boost::str (boost::format ("Received bulk pull account for %1% with a minimum amount of %2%") % request->account.to_account () % vxlnetwork::amount (request->minimum_amount).format_balance (vxlnetwork::Mxrb_ratio, 10, true)));
			}
			if (is_bootstrap_connection () && !node->flags.disable_bootstrap_bulk_pull_server)
			{
				add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
			}
			receive ();
		}
	}
}

void vxlnetwork::bootstrap_server::receive_frontier_req_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		auto error (false);
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto request (std::make_unique<vxlnetwork::frontier_req> (error, stream, header_a));
		if (!error)
		{
			if (node->config.logging.bulk_pull_logging ())
			{
				node->logger.try_log (boost::str (boost::format ("Received frontier request for %1% with age %2%") % request->start.to_string () % request->age));
			}
			if (is_bootstrap_connection ())
			{
				add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
			}
			receive ();
		}
	}
	else
	{
		if (node->config.logging.network_logging ())
		{
			node->logger.try_log (boost::str (boost::format ("Error sending receiving frontier request: %1%") % ec.message ()));
		}
	}
}

void vxlnetwork::bootstrap_server::receive_keepalive_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		auto error (false);
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto request (std::make_unique<vxlnetwork::keepalive> (error, stream, header_a));
		if (!error)
		{
			if (is_realtime_connection ())
			{
				add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
			}
			receive ();
		}
	}
	else
	{
		if (node->config.logging.network_keepalive_logging ())
		{
			node->logger.try_log (boost::str (boost::format ("Error receiving keepalive: %1%") % ec.message ()));
		}
	}
}

void vxlnetwork::bootstrap_server::receive_telemetry_ack_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		auto error (false);
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto request (std::make_unique<vxlnetwork::telemetry_ack> (error, stream, header_a));
		if (!error)
		{
			if (is_realtime_connection ())
			{
				add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
			}
			receive ();
		}
	}
	else
	{
		if (node->config.logging.network_telemetry_logging ())
		{
			node->logger.try_log (boost::str (boost::format ("Error receiving telemetry ack: %1%") % ec.message ()));
		}
	}
}

void vxlnetwork::bootstrap_server::receive_publish_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		vxlnetwork::uint128_t digest;
		if (!node->network.publish_filter.apply (receive_buffer->data (), size_a, &digest))
		{
			auto error (false);
			vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
			auto request (std::make_unique<vxlnetwork::publish> (error, stream, header_a, digest));
			if (!error)
			{
				if (is_realtime_connection ())
				{
					if (!node->network_params.work.validate_entry (*request->block))
					{
						add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
					}
					else
					{
						node->stats.inc_detail_only (vxlnetwork::stat::type::error, vxlnetwork::stat::detail::insufficient_work);
					}
				}
				receive ();
			}
		}
		else
		{
			node->stats.inc (vxlnetwork::stat::type::filter, vxlnetwork::stat::detail::duplicate_publish);
			receive ();
		}
	}
	else
	{
		if (node->config.logging.network_message_logging ())
		{
			node->logger.try_log (boost::str (boost::format ("Error receiving publish: %1%") % ec.message ()));
		}
	}
}

void vxlnetwork::bootstrap_server::receive_confirm_req_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		auto error (false);
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto request (std::make_unique<vxlnetwork::confirm_req> (error, stream, header_a));
		if (!error)
		{
			if (is_realtime_connection ())
			{
				add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
			}
			receive ();
		}
	}
	else if (node->config.logging.network_message_logging ())
	{
		node->logger.try_log (boost::str (boost::format ("Error receiving confirm_req: %1%") % ec.message ()));
	}
}

void vxlnetwork::bootstrap_server::receive_confirm_ack_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		auto error (false);
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto request (std::make_unique<vxlnetwork::confirm_ack> (error, stream, header_a));
		if (!error)
		{
			if (is_realtime_connection ())
			{
				bool process_vote (true);
				if (header_a.block_type () != vxlnetwork::block_type::not_a_block)
				{
					for (auto & vote_block : request->vote->blocks)
					{
						if (!vote_block.which ())
						{
							auto const & block (boost::get<std::shared_ptr<vxlnetwork::block>> (vote_block));
							if (node->network_params.work.validate_entry (*block))
							{
								process_vote = false;
								node->stats.inc_detail_only (vxlnetwork::stat::type::error, vxlnetwork::stat::detail::insufficient_work);
							}
						}
					}
				}
				if (process_vote)
				{
					add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
				}
			}
			receive ();
		}
	}
	else if (node->config.logging.network_message_logging ())
	{
		node->logger.try_log (boost::str (boost::format ("Error receiving confirm_ack: %1%") % ec.message ()));
	}
}

void vxlnetwork::bootstrap_server::receive_node_id_handshake_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a)
{
	if (!ec)
	{
		auto error (false);
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto request (std::make_unique<vxlnetwork::node_id_handshake> (error, stream, header_a));
		if (!error)
		{
			if (socket->type () == vxlnetwork::socket::type_t::undefined && !node->flags.disable_tcp_realtime)
			{
				add_request (std::unique_ptr<vxlnetwork::message> (request.release ()));
			}
			receive ();
		}
	}
	else if (node->config.logging.network_node_id_handshake_logging ())
	{
		node->logger.try_log (boost::str (boost::format ("Error receiving node_id_handshake: %1%") % ec.message ()));
	}
}

void vxlnetwork::bootstrap_server::add_request (std::unique_ptr<vxlnetwork::message> message_a)
{
	debug_assert (message_a != nullptr);
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock (mutex);
	auto start (requests.empty ());
	requests.push (std::move (message_a));
	if (start)
	{
		run_next (lock);
	}
}

void vxlnetwork::bootstrap_server::finish_request ()
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock (mutex);
	if (!requests.empty ())
	{
		requests.pop ();
	}
	else
	{
		node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::request_underflow);
	}

	while (!requests.empty ())
	{
		if (!requests.front ())
		{
			requests.pop ();
		}
		else
		{
			run_next (lock);
		}
	}

	std::weak_ptr<vxlnetwork::bootstrap_server> this_w (shared_from_this ());
	node->workers.add_timed_task (std::chrono::steady_clock::now () + (node->config.tcp_io_timeout * 2) + std::chrono::seconds (1), [this_w] () {
		if (auto this_l = this_w.lock ())
		{
			this_l->timeout ();
		}
	});
}

void vxlnetwork::bootstrap_server::finish_request_async ()
{
	std::weak_ptr<vxlnetwork::bootstrap_server> this_w (shared_from_this ());
	node->background ([this_w] () {
		if (auto this_l = this_w.lock ())
		{
			this_l->finish_request ();
		}
	});
}

void vxlnetwork::bootstrap_server::timeout ()
{
	if (socket->has_timed_out ())
	{
		if (node->config.logging.bulk_pull_logging ())
		{
			node->logger.try_log ("Closing incoming tcp / bootstrap server by timeout");
		}
		{
			vxlnetwork::lock_guard<vxlnetwork::mutex> lock (node->bootstrap.mutex);
			node->bootstrap.connections.erase (this);
		}
		socket->close ();
	}
}

namespace
{
class request_response_visitor : public vxlnetwork::message_visitor
{
public:
	explicit request_response_visitor (std::shared_ptr<vxlnetwork::bootstrap_server> connection_a) :
		connection (std::move (connection_a))
	{
	}
	void keepalive (vxlnetwork::keepalive const & message_a) override
	{
		connection->node->network.tcp_message_manager.put_message (vxlnetwork::tcp_message_item{ std::make_shared<vxlnetwork::keepalive> (message_a), connection->remote_endpoint, connection->remote_node_id, connection->socket });
	}
	void publish (vxlnetwork::publish const & message_a) override
	{
		connection->node->network.tcp_message_manager.put_message (vxlnetwork::tcp_message_item{ std::make_shared<vxlnetwork::publish> (message_a), connection->remote_endpoint, connection->remote_node_id, connection->socket });
	}
	void confirm_req (vxlnetwork::confirm_req const & message_a) override
	{
		connection->node->network.tcp_message_manager.put_message (vxlnetwork::tcp_message_item{ std::make_shared<vxlnetwork::confirm_req> (message_a), connection->remote_endpoint, connection->remote_node_id, connection->socket });
	}
	void confirm_ack (vxlnetwork::confirm_ack const & message_a) override
	{
		connection->node->network.tcp_message_manager.put_message (vxlnetwork::tcp_message_item{ std::make_shared<vxlnetwork::confirm_ack> (message_a), connection->remote_endpoint, connection->remote_node_id, connection->socket });
	}
	void bulk_pull (vxlnetwork::bulk_pull const &) override
	{
		auto response (std::make_shared<vxlnetwork::bulk_pull_server> (connection, std::unique_ptr<vxlnetwork::bulk_pull> (static_cast<vxlnetwork::bulk_pull *> (connection->requests.front ().release ()))));
		response->send_next ();
	}
	void bulk_pull_account (vxlnetwork::bulk_pull_account const &) override
	{
		auto response (std::make_shared<vxlnetwork::bulk_pull_account_server> (connection, std::unique_ptr<vxlnetwork::bulk_pull_account> (static_cast<vxlnetwork::bulk_pull_account *> (connection->requests.front ().release ()))));
		response->send_frontier ();
	}
	void bulk_push (vxlnetwork::bulk_push const &) override
	{
		auto response (std::make_shared<vxlnetwork::bulk_push_server> (connection));
		response->throttled_receive ();
	}
	void frontier_req (vxlnetwork::frontier_req const &) override
	{
		auto response (std::make_shared<vxlnetwork::frontier_req_server> (connection, std::unique_ptr<vxlnetwork::frontier_req> (static_cast<vxlnetwork::frontier_req *> (connection->requests.front ().release ()))));
		response->send_next ();
	}
	void telemetry_req (vxlnetwork::telemetry_req const & message_a) override
	{
		connection->node->network.tcp_message_manager.put_message (vxlnetwork::tcp_message_item{ std::make_shared<vxlnetwork::telemetry_req> (message_a), connection->remote_endpoint, connection->remote_node_id, connection->socket });
	}
	void telemetry_ack (vxlnetwork::telemetry_ack const & message_a) override
	{
		connection->node->network.tcp_message_manager.put_message (vxlnetwork::tcp_message_item{ std::make_shared<vxlnetwork::telemetry_ack> (message_a), connection->remote_endpoint, connection->remote_node_id, connection->socket });
	}
	void node_id_handshake (vxlnetwork::node_id_handshake const & message_a) override
	{
		if (connection->node->config.logging.network_node_id_handshake_logging ())
		{
			connection->node->logger.try_log (boost::str (boost::format ("Received node_id_handshake message from %1%") % connection->remote_endpoint));
		}
		if (message_a.query)
		{
			boost::optional<std::pair<vxlnetwork::account, vxlnetwork::signature>> response (std::make_pair (connection->node->node_id.pub, vxlnetwork::sign_message (connection->node->node_id.prv, connection->node->node_id.pub, *message_a.query)));
			debug_assert (!vxlnetwork::validate_message (response->first, *message_a.query, response->second));
			auto cookie (connection->node->network.syn_cookies.assign (vxlnetwork::transport::map_tcp_to_endpoint (connection->remote_endpoint)));
			vxlnetwork::node_id_handshake response_message (connection->node->network_params.network, cookie, response);
			auto shared_const_buffer = response_message.to_shared_const_buffer ();
			connection->socket->async_write (shared_const_buffer, [connection = std::weak_ptr<vxlnetwork::bootstrap_server> (connection)] (boost::system::error_code const & ec, std::size_t size_a) {
				if (auto connection_l = connection.lock ())
				{
					if (ec)
					{
						if (connection_l->node->config.logging.network_node_id_handshake_logging ())
						{
							connection_l->node->logger.try_log (boost::str (boost::format ("Error sending node_id_handshake to %1%: %2%") % connection_l->remote_endpoint % ec.message ()));
						}
						// Stop invalid handshake
						connection_l->stop ();
					}
					else
					{
						connection_l->node->stats.inc (vxlnetwork::stat::type::message, vxlnetwork::stat::detail::node_id_handshake, vxlnetwork::stat::dir::out);
						connection_l->finish_request ();
					}
				}
			});
		}
		else if (message_a.response)
		{
			vxlnetwork::account const & node_id (message_a.response->first);
			if (!connection->node->network.syn_cookies.validate (vxlnetwork::transport::map_tcp_to_endpoint (connection->remote_endpoint), node_id, message_a.response->second) && node_id != connection->node->node_id.pub)
			{
				connection->remote_node_id = node_id;
				connection->socket->type_set (vxlnetwork::socket::type_t::realtime);
				++connection->node->bootstrap.realtime_count;
				connection->finish_request_async ();
			}
			else
			{
				// Stop invalid handshake
				connection->stop ();
			}
		}
		else
		{
			connection->finish_request_async ();
		}
		vxlnetwork::account node_id (connection->remote_node_id);
		vxlnetwork::socket::type_t type = connection->socket->type ();
		debug_assert (node_id.is_zero () || type == vxlnetwork::socket::type_t::realtime);
		connection->node->network.tcp_message_manager.put_message (vxlnetwork::tcp_message_item{ std::make_shared<vxlnetwork::node_id_handshake> (message_a), connection->remote_endpoint, connection->remote_node_id, connection->socket });
	}
	std::shared_ptr<vxlnetwork::bootstrap_server> connection;
};
}

void vxlnetwork::bootstrap_server::run_next (vxlnetwork::unique_lock<vxlnetwork::mutex> & lock_a)
{
	debug_assert (!requests.empty ());
	request_response_visitor visitor (shared_from_this ());
	auto type (requests.front ()->header.type);
	if (type == vxlnetwork::message_type::bulk_pull || type == vxlnetwork::message_type::bulk_pull_account || type == vxlnetwork::message_type::bulk_push || type == vxlnetwork::message_type::frontier_req || type == vxlnetwork::message_type::node_id_handshake)
	{
		// Bootstrap & node ID (realtime start)
		// Request removed from queue in request_response_visitor. For bootstrap with requests.front ().release (), for node ID with finish_request ()
		requests.front ()->visit (visitor);
	}
	else
	{
		// Realtime
		auto request (std::move (requests.front ()));
		requests.pop ();
		lock_a.unlock ();
		request->visit (visitor);
		lock_a.lock ();
	}
}

bool vxlnetwork::bootstrap_server::is_bootstrap_connection ()
{
	if (socket->type () == vxlnetwork::socket::type_t::undefined && !node->flags.disable_bootstrap_listener && node->bootstrap.bootstrap_count < node->config.bootstrap_connections_max)
	{
		++node->bootstrap.bootstrap_count;
		socket->type_set (vxlnetwork::socket::type_t::bootstrap);
	}
	return socket->type () == vxlnetwork::socket::type_t::bootstrap;
}

bool vxlnetwork::bootstrap_server::is_realtime_connection ()
{
	return socket->is_realtime_connection ();
}
