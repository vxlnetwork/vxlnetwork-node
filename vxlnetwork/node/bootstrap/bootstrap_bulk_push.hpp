#pragma once

#include <vxlnetwork/node/common.hpp>

#include <future>

namespace vxlnetwork
{
class bootstrap_attempt;
class bootstrap_client;

/**
 * Client side of a bulk_push request. Sends a sequence of blocks the other side did not report in their frontier_req response.
 */
class bulk_push_client final : public std::enable_shared_from_this<vxlnetwork::bulk_push_client>
{
public:
	explicit bulk_push_client (std::shared_ptr<vxlnetwork::bootstrap_client> const &, std::shared_ptr<vxlnetwork::bootstrap_attempt> const &);
	~bulk_push_client ();
	void start ();
	void push ();
	void push_block (vxlnetwork::block const &);
	void send_finished ();
	std::shared_ptr<vxlnetwork::bootstrap_client> connection;
	std::shared_ptr<vxlnetwork::bootstrap_attempt> attempt;
	std::promise<bool> promise;
	std::pair<vxlnetwork::block_hash, vxlnetwork::block_hash> current_target;
};
class bootstrap_server;

/**
 * Server side of a bulk_push request. Receives blocks and puts them in the block processor to be processed.
 */
class bulk_push_server final : public std::enable_shared_from_this<vxlnetwork::bulk_push_server>
{
public:
	explicit bulk_push_server (std::shared_ptr<vxlnetwork::bootstrap_server> const &);
	void throttled_receive ();
	void receive ();
	void received_type ();
	void received_block (boost::system::error_code const &, std::size_t, vxlnetwork::block_type);
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	std::shared_ptr<vxlnetwork::bootstrap_server> connection;
};
}
