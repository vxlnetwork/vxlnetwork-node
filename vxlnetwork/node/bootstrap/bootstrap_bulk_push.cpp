#include <vxlnetwork/node/bootstrap/bootstrap_attempt.hpp>
#include <vxlnetwork/node/bootstrap/bootstrap_bulk_push.hpp>
#include <vxlnetwork/node/node.hpp>
#include <vxlnetwork/node/transport/tcp.hpp>

#include <boost/format.hpp>

vxlnetwork::bulk_push_client::bulk_push_client (std::shared_ptr<vxlnetwork::bootstrap_client> const & connection_a, std::shared_ptr<vxlnetwork::bootstrap_attempt> const & attempt_a) :
	connection (connection_a),
	attempt (attempt_a)
{
}

vxlnetwork::bulk_push_client::~bulk_push_client ()
{
}

void vxlnetwork::bulk_push_client::start ()
{
	vxlnetwork::bulk_push message{ connection->node->network_params.network };
	auto this_l (shared_from_this ());
	connection->channel->send (
	message, [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
		if (!ec)
		{
			this_l->push ();
		}
		else
		{
			if (this_l->connection->node->config.logging.bulk_pull_logging ())
			{
				this_l->connection->node->logger.try_log (boost::str (boost::format ("Unable to send bulk_push request: %1%") % ec.message ()));
			}
		}
	},
	vxlnetwork::buffer_drop_policy::no_limiter_drop);
}

void vxlnetwork::bulk_push_client::push ()
{
	std::shared_ptr<vxlnetwork::block> block;
	bool finished (false);
	while (block == nullptr && !finished)
	{
		if (current_target.first.is_zero () || current_target.first == current_target.second)
		{
			finished = attempt->request_bulk_push_target (current_target);
		}
		if (!finished)
		{
			block = connection->node->block (current_target.first);
			if (block == nullptr)
			{
				current_target.first = vxlnetwork::block_hash (0);
			}
			else
			{
				if (connection->node->config.logging.bulk_pull_logging ())
				{
					connection->node->logger.try_log ("Bulk pushing range ", current_target.first.to_string (), " down to ", current_target.second.to_string ());
				}
			}
		}
	}
	if (finished)
	{
		send_finished ();
	}
	else
	{
		current_target.first = block->previous ();
		push_block (*block);
	}
}

void vxlnetwork::bulk_push_client::send_finished ()
{
	vxlnetwork::shared_const_buffer buffer (static_cast<uint8_t> (vxlnetwork::block_type::not_a_block));
	auto this_l (shared_from_this ());
	connection->channel->send_buffer (buffer, [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
		try
		{
			this_l->promise.set_value (false);
		}
		catch (std::future_error &)
		{
		}
	});
}

void vxlnetwork::bulk_push_client::push_block (vxlnetwork::block const & block_a)
{
	std::vector<uint8_t> buffer;
	{
		vxlnetwork::vectorstream stream (buffer);
		vxlnetwork::serialize_block (stream, block_a);
	}
	auto this_l (shared_from_this ());
	connection->channel->send_buffer (vxlnetwork::shared_const_buffer (std::move (buffer)), [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
		if (!ec)
		{
			this_l->push ();
		}
		else
		{
			if (this_l->connection->node->config.logging.bulk_pull_logging ())
			{
				this_l->connection->node->logger.try_log (boost::str (boost::format ("Error sending block during bulk push: %1%") % ec.message ()));
			}
		}
	});
}

vxlnetwork::bulk_push_server::bulk_push_server (std::shared_ptr<vxlnetwork::bootstrap_server> const & connection_a) :
	receive_buffer (std::make_shared<std::vector<uint8_t>> ()),
	connection (connection_a)
{
	receive_buffer->resize (256);
}

void vxlnetwork::bulk_push_server::throttled_receive ()
{
	if (!connection->node->block_processor.half_full ())
	{
		receive ();
	}
	else
	{
		auto this_l (shared_from_this ());
		connection->node->workers.add_timed_task (std::chrono::steady_clock::now () + std::chrono::seconds (1), [this_l] () {
			if (!this_l->connection->stopped)
			{
				this_l->throttled_receive ();
			}
		});
	}
}

void vxlnetwork::bulk_push_server::receive ()
{
	if (connection->node->bootstrap_initiator.in_progress ())
	{
		if (connection->node->config.logging.bulk_pull_logging ())
		{
			connection->node->logger.try_log ("Aborting bulk_push because a bootstrap attempt is in progress");
		}
	}
	else
	{
		auto this_l (shared_from_this ());
		connection->socket->async_read (receive_buffer, 1, [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
			if (!ec)
			{
				this_l->received_type ();
			}
			else
			{
				if (this_l->connection->node->config.logging.bulk_pull_logging ())
				{
					this_l->connection->node->logger.try_log (boost::str (boost::format ("Error receiving block type: %1%") % ec.message ()));
				}
			}
		});
	}
}

void vxlnetwork::bulk_push_server::received_type ()
{
	auto this_l (shared_from_this ());
	vxlnetwork::block_type type (static_cast<vxlnetwork::block_type> (receive_buffer->data ()[0]));
	switch (type)
	{
		case vxlnetwork::block_type::send:
		{
			connection->node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::send, vxlnetwork::stat::dir::in);
			connection->socket->async_read (receive_buffer, vxlnetwork::send_block::size, [this_l, type] (boost::system::error_code const & ec, std::size_t size_a) {
				this_l->received_block (ec, size_a, type);
			});
			break;
		}
		case vxlnetwork::block_type::receive:
		{
			connection->node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::receive, vxlnetwork::stat::dir::in);
			connection->socket->async_read (receive_buffer, vxlnetwork::receive_block::size, [this_l, type] (boost::system::error_code const & ec, std::size_t size_a) {
				this_l->received_block (ec, size_a, type);
			});
			break;
		}
		case vxlnetwork::block_type::open:
		{
			connection->node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::open, vxlnetwork::stat::dir::in);
			connection->socket->async_read (receive_buffer, vxlnetwork::open_block::size, [this_l, type] (boost::system::error_code const & ec, std::size_t size_a) {
				this_l->received_block (ec, size_a, type);
			});
			break;
		}
		case vxlnetwork::block_type::change:
		{
			connection->node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::change, vxlnetwork::stat::dir::in);
			connection->socket->async_read (receive_buffer, vxlnetwork::change_block::size, [this_l, type] (boost::system::error_code const & ec, std::size_t size_a) {
				this_l->received_block (ec, size_a, type);
			});
			break;
		}
		case vxlnetwork::block_type::state:
		{
			connection->node->stats.inc (vxlnetwork::stat::type::bootstrap, vxlnetwork::stat::detail::state_block, vxlnetwork::stat::dir::in);
			connection->socket->async_read (receive_buffer, vxlnetwork::state_block::size, [this_l, type] (boost::system::error_code const & ec, std::size_t size_a) {
				this_l->received_block (ec, size_a, type);
			});
			break;
		}
		case vxlnetwork::block_type::not_a_block:
		{
			connection->finish_request ();
			break;
		}
		default:
		{
			if (connection->node->config.logging.network_packet_logging ())
			{
				connection->node->logger.try_log ("Unknown type received as block type");
			}
			break;
		}
	}
}

void vxlnetwork::bulk_push_server::received_block (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::block_type type_a)
{
	if (!ec)
	{
		vxlnetwork::bufferstream stream (receive_buffer->data (), size_a);
		auto block (vxlnetwork::deserialize_block (stream, type_a));
		if (block != nullptr && !connection->node->network_params.work.validate_entry (*block))
		{
			connection->node->process_active (std::move (block));
			throttled_receive ();
		}
		else if (block == nullptr)
		{
			if (connection->node->config.logging.bulk_pull_logging ())
			{
				connection->node->logger.try_log ("Error deserializing block received from pull request");
			}
		}
		else // Work invalid
		{
			if (connection->node->config.logging.bulk_pull_logging ())
			{
				connection->node->logger.try_log (boost::str (boost::format ("Insufficient work for bulk push block: %1%") % block->hash ().to_string ()));
			}
			connection->node->stats.inc_detail_only (vxlnetwork::stat::type::error, vxlnetwork::stat::detail::insufficient_work);
		}
	}
}
