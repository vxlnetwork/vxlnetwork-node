#pragma once

#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/node/socket.hpp>

#include <atomic>
#include <queue>

namespace vxlnetwork
{
class bootstrap_server;

/**
 * Server side portion of bootstrap sessions. Listens for new socket connections and spawns bootstrap_server objects when connected.
 */
class bootstrap_listener final
{
public:
	bootstrap_listener (uint16_t, vxlnetwork::node &);
	void start ();
	void stop ();
	void accept_action (boost::system::error_code const &, std::shared_ptr<vxlnetwork::socket> const &);
	std::size_t connection_count ();

	vxlnetwork::mutex mutex;
	std::unordered_map<vxlnetwork::bootstrap_server *, std::weak_ptr<vxlnetwork::bootstrap_server>> connections;
	vxlnetwork::tcp_endpoint endpoint ();
	vxlnetwork::node & node;
	std::shared_ptr<vxlnetwork::server_socket> listening_socket;
	bool on{ false };
	std::atomic<std::size_t> bootstrap_count{ 0 };
	std::atomic<std::size_t> realtime_count{ 0 };
	uint16_t port;
};

std::unique_ptr<container_info_component> collect_container_info (bootstrap_listener & bootstrap_listener, std::string const & name);

class message;

/**
 * Owns the server side of a bootstrap connection. Responds to bootstrap messages sent over the socket.
 */
class bootstrap_server final : public std::enable_shared_from_this<vxlnetwork::bootstrap_server>
{
public:
	bootstrap_server (std::shared_ptr<vxlnetwork::socket> const &, std::shared_ptr<vxlnetwork::node> const &);
	~bootstrap_server ();
	void stop ();
	void receive ();
	void receive_header_action (boost::system::error_code const &, std::size_t);
	void receive_bulk_pull_action (boost::system::error_code const &, std::size_t, vxlnetwork::message_header const &);
	void receive_bulk_pull_account_action (boost::system::error_code const &, std::size_t, vxlnetwork::message_header const &);
	void receive_frontier_req_action (boost::system::error_code const &, std::size_t, vxlnetwork::message_header const &);
	void receive_keepalive_action (boost::system::error_code const &, std::size_t, vxlnetwork::message_header const &);
	void receive_publish_action (boost::system::error_code const &, std::size_t, vxlnetwork::message_header const &);
	void receive_confirm_req_action (boost::system::error_code const &, std::size_t, vxlnetwork::message_header const &);
	void receive_confirm_ack_action (boost::system::error_code const &, std::size_t, vxlnetwork::message_header const &);
	void receive_node_id_handshake_action (boost::system::error_code const &, std::size_t, vxlnetwork::message_header const &);
	void receive_telemetry_ack_action (boost::system::error_code const & ec, std::size_t size_a, vxlnetwork::message_header const & header_a);
	void add_request (std::unique_ptr<vxlnetwork::message>);
	void finish_request ();
	void finish_request_async ();
	void timeout ();
	void run_next (vxlnetwork::unique_lock<vxlnetwork::mutex> & lock_a);
	bool is_bootstrap_connection ();
	bool is_realtime_connection ();
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	std::shared_ptr<vxlnetwork::socket> const socket;
	std::shared_ptr<vxlnetwork::node> node;
	vxlnetwork::mutex mutex;
	std::queue<std::unique_ptr<vxlnetwork::message>> requests;
	std::atomic<bool> stopped{ false };
	// Remote enpoint used to remove response channel even after socket closing
	vxlnetwork::tcp_endpoint remote_endpoint{ boost::asio::ip::address_v6::any (), 0 };
	vxlnetwork::account remote_node_id{};
	std::chrono::steady_clock::time_point last_telemetry_req{ std::chrono::steady_clock::time_point () };
};
}
