#pragma once

#include <vxlnetwork/node/bootstrap/bootstrap_bulk_pull.hpp>
#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/node/socket.hpp>

#include <atomic>

namespace vxlnetwork
{
class node;
namespace transport
{
	class channel_tcp;
}

class bootstrap_attempt;
class bootstrap_connections;
class frontier_req_client;
class pull_info;

/**
 * Owns the client side of the bootstrap connection.
 */
class bootstrap_client final : public std::enable_shared_from_this<bootstrap_client>
{
public:
	bootstrap_client (std::shared_ptr<vxlnetwork::node> const & node_a, vxlnetwork::bootstrap_connections & connections_a, std::shared_ptr<vxlnetwork::transport::channel_tcp> const & channel_a, std::shared_ptr<vxlnetwork::socket> const & socket_a);
	~bootstrap_client ();
	void stop (bool force);
	double sample_block_rate ();
	double elapsed_seconds () const;
	void set_start_time (std::chrono::steady_clock::time_point start_time_a);
	std::shared_ptr<vxlnetwork::node> node;
	vxlnetwork::bootstrap_connections & connections;
	std::shared_ptr<vxlnetwork::transport::channel_tcp> channel;
	std::shared_ptr<vxlnetwork::socket> socket;
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	std::atomic<uint64_t> block_count{ 0 };
	std::atomic<double> block_rate{ 0 };
	std::atomic<bool> pending_stop{ false };
	std::atomic<bool> hard_stop{ false };

private:
	mutable vxlnetwork::mutex start_time_mutex;
	std::chrono::steady_clock::time_point start_time_m;
};

/**
 * Container for bootstrap_client objects. Owned by bootstrap_initiator which pools open connections and makes them available
 * for use by different bootstrap sessions.
 */
class bootstrap_connections final : public std::enable_shared_from_this<bootstrap_connections>
{
public:
	explicit bootstrap_connections (vxlnetwork::node & node_a);
	std::shared_ptr<vxlnetwork::bootstrap_client> connection (std::shared_ptr<vxlnetwork::bootstrap_attempt> const & attempt_a = nullptr, bool use_front_connection = false);
	void pool_connection (std::shared_ptr<vxlnetwork::bootstrap_client> const & client_a, bool new_client = false, bool push_front = false);
	void add_connection (vxlnetwork::endpoint const & endpoint_a);
	std::shared_ptr<vxlnetwork::bootstrap_client> find_connection (vxlnetwork::tcp_endpoint const & endpoint_a);
	void connect_client (vxlnetwork::tcp_endpoint const & endpoint_a, bool push_front = false);
	unsigned target_connections (std::size_t pulls_remaining, std::size_t attempts_count) const;
	void populate_connections (bool repeat = true);
	void start_populate_connections ();
	void add_pull (vxlnetwork::pull_info const & pull_a);
	void request_pull (vxlnetwork::unique_lock<vxlnetwork::mutex> & lock_a);
	void requeue_pull (vxlnetwork::pull_info const & pull_a, bool network_error = false);
	void clear_pulls (uint64_t);
	void run ();
	void stop ();
	std::deque<std::weak_ptr<vxlnetwork::bootstrap_client>> clients;
	std::atomic<unsigned> connections_count{ 0 };
	vxlnetwork::node & node;
	std::deque<std::shared_ptr<vxlnetwork::bootstrap_client>> idle;
	std::deque<vxlnetwork::pull_info> pulls;
	std::atomic<bool> populate_connections_started{ false };
	std::atomic<bool> new_connections_empty{ false };
	std::atomic<bool> stopped{ false };
	vxlnetwork::mutex mutex;
	vxlnetwork::condition_variable condition;
};
}
