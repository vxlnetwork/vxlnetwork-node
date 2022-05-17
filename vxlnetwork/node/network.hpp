#pragma once

#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/node/peer_exclusion.hpp>
#include <vxlnetwork/node/transport/tcp.hpp>
#include <vxlnetwork/node/transport/udp.hpp>
#include <vxlnetwork/secure/network_filter.hpp>

#include <boost/thread/thread.hpp>

#include <memory>
#include <queue>
#include <unordered_set>
namespace vxlnetwork
{
class channel;
class node;
class stats;
class transaction;
class message_buffer final
{
public:
	uint8_t * buffer{ nullptr };
	std::size_t size{ 0 };
	vxlnetwork::endpoint endpoint;
};
/**
  * A circular buffer for servicing vxlnetwork realtime messages.
  * This container follows a producer/consumer model where the operating system is producing data in to
  * buffers which are serviced by internal threads.
  * If buffers are not serviced fast enough they're internally dropped.
  * This container has a maximum space to hold N buffers of M size and will allocate them in round-robin order.
  * All public methods are thread-safe
*/
class message_buffer_manager final
{
public:
	// Stats - Statistics
	// Size - Size of each individual buffer
	// Count - Number of buffers to allocate
	message_buffer_manager (vxlnetwork::stat & stats, std::size_t, std::size_t);
	// Return a buffer where message data can be put
	// Method will attempt to return the first free buffer
	// If there are no free buffers, an unserviced buffer will be dequeued and returned
	// Function will block if there are no free or unserviced buffers
	// Return nullptr if the container has stopped
	vxlnetwork::message_buffer * allocate ();
	// Queue a buffer that has been filled with message data and notify servicing threads
	void enqueue (vxlnetwork::message_buffer *);
	// Return a buffer that has been filled with message data
	// Function will block until a buffer has been added
	// Return nullptr if the container has stopped
	vxlnetwork::message_buffer * dequeue ();
	// Return a buffer to the freelist after is has been serviced
	void release (vxlnetwork::message_buffer *);
	// Stop container and notify waiting threads
	void stop ();

private:
	vxlnetwork::stat & stats;
	vxlnetwork::mutex mutex;
	vxlnetwork::condition_variable condition;
	boost::circular_buffer<vxlnetwork::message_buffer *> free;
	boost::circular_buffer<vxlnetwork::message_buffer *> full;
	std::vector<uint8_t> slab;
	std::vector<vxlnetwork::message_buffer> entries;
	bool stopped;
};
class tcp_message_manager final
{
public:
	tcp_message_manager (unsigned incoming_connections_max_a);
	void put_message (vxlnetwork::tcp_message_item const & item_a);
	vxlnetwork::tcp_message_item get_message ();
	// Stop container and notify waiting threads
	void stop ();

private:
	vxlnetwork::mutex mutex;
	vxlnetwork::condition_variable producer_condition;
	vxlnetwork::condition_variable consumer_condition;
	std::deque<vxlnetwork::tcp_message_item> entries;
	unsigned max_entries;
	static unsigned const max_entries_per_connection = 16;
	bool stopped{ false };

	friend class network_tcp_message_manager_Test;
};
/**
  * Node ID cookies for node ID handshakes
*/
class syn_cookies final
{
public:
	syn_cookies (std::size_t);
	void purge (std::chrono::steady_clock::time_point const &);
	// Returns boost::none if the IP is rate capped on syn cookie requests,
	// or if the endpoint already has a syn cookie query
	boost::optional<vxlnetwork::uint256_union> assign (vxlnetwork::endpoint const &);
	// Returns false if valid, true if invalid (true on error convention)
	// Also removes the syn cookie from the store if valid
	bool validate (vxlnetwork::endpoint const &, vxlnetwork::account const &, vxlnetwork::signature const &);
	std::unique_ptr<container_info_component> collect_container_info (std::string const &);
	std::size_t cookies_size ();

private:
	class syn_cookie_info final
	{
	public:
		vxlnetwork::uint256_union cookie;
		std::chrono::steady_clock::time_point created_at;
	};
	mutable vxlnetwork::mutex syn_cookie_mutex;
	std::unordered_map<vxlnetwork::endpoint, syn_cookie_info> cookies;
	std::unordered_map<boost::asio::ip::address, unsigned> cookies_per_ip;
	std::size_t max_cookies_per_ip;
};
class network final
{
public:
	network (vxlnetwork::node &, uint16_t);
	~network ();
	vxlnetwork::networks id;
	void start ();
	void stop ();
	void flood_message (vxlnetwork::message &, vxlnetwork::buffer_drop_policy const = vxlnetwork::buffer_drop_policy::limiter, float const = 1.0f);
	void flood_keepalive (float const scale_a = 1.0f);
	void flood_keepalive_self (float const scale_a = 0.5f);
	void flood_vote (std::shared_ptr<vxlnetwork::vote> const &, float scale);
	void flood_vote_pr (std::shared_ptr<vxlnetwork::vote> const &);
	// Flood block to all PRs and a random selection of non-PRs
	void flood_block_initial (std::shared_ptr<vxlnetwork::block> const &);
	// Flood block to a random selection of peers
	void flood_block (std::shared_ptr<vxlnetwork::block> const &, vxlnetwork::buffer_drop_policy const = vxlnetwork::buffer_drop_policy::limiter);
	void flood_block_many (std::deque<std::shared_ptr<vxlnetwork::block>>, std::function<void ()> = nullptr, unsigned = broadcast_interval_ms);
	void merge_peers (std::array<vxlnetwork::endpoint, 8> const &);
	void merge_peer (vxlnetwork::endpoint const &);
	void send_keepalive (std::shared_ptr<vxlnetwork::transport::channel> const &);
	void send_keepalive_self (std::shared_ptr<vxlnetwork::transport::channel> const &);
	void send_node_id_handshake (std::shared_ptr<vxlnetwork::transport::channel> const &, boost::optional<vxlnetwork::uint256_union> const & query, boost::optional<vxlnetwork::uint256_union> const & respond_to);
	void send_confirm_req (std::shared_ptr<vxlnetwork::transport::channel> const & channel_a, std::pair<vxlnetwork::block_hash, vxlnetwork::block_hash> const & hash_root_a);
	void broadcast_confirm_req (std::shared_ptr<vxlnetwork::block> const &);
	void broadcast_confirm_req_base (std::shared_ptr<vxlnetwork::block> const &, std::shared_ptr<std::vector<std::shared_ptr<vxlnetwork::transport::channel>>> const &, unsigned, bool = false);
	void broadcast_confirm_req_batched_many (std::unordered_map<std::shared_ptr<vxlnetwork::transport::channel>, std::deque<std::pair<vxlnetwork::block_hash, vxlnetwork::root>>>, std::function<void ()> = nullptr, unsigned = broadcast_interval_ms, bool = false);
	void broadcast_confirm_req_many (std::deque<std::pair<std::shared_ptr<vxlnetwork::block>, std::shared_ptr<std::vector<std::shared_ptr<vxlnetwork::transport::channel>>>>>, std::function<void ()> = nullptr, unsigned = broadcast_interval_ms);
	std::shared_ptr<vxlnetwork::transport::channel> find_node_id (vxlnetwork::account const &);
	std::shared_ptr<vxlnetwork::transport::channel> find_channel (vxlnetwork::endpoint const &);
	bool not_a_peer (vxlnetwork::endpoint const &, bool);
	// Should we reach out to this endpoint with a keepalive message
	bool reachout (vxlnetwork::endpoint const &, bool = false);
	std::deque<std::shared_ptr<vxlnetwork::transport::channel>> list (std::size_t, uint8_t = 0, bool = true);
	std::deque<std::shared_ptr<vxlnetwork::transport::channel>> list_non_pr (std::size_t);
	// Desired fanout for a given scale
	std::size_t fanout (float scale = 1.0f) const;
	void random_fill (std::array<vxlnetwork::endpoint, 8> &) const;
	void fill_keepalive_self (std::array<vxlnetwork::endpoint, 8> &) const;
	// Note: The minimum protocol version is used after the random selection, so number of peers can be less than expected.
	std::unordered_set<std::shared_ptr<vxlnetwork::transport::channel>> random_set (std::size_t, uint8_t = 0, bool = false) const;
	// Get the next peer for attempting a tcp bootstrap connection
	vxlnetwork::tcp_endpoint bootstrap_peer (bool = false);
	vxlnetwork::endpoint endpoint ();
	void cleanup (std::chrono::steady_clock::time_point const &);
	void ongoing_cleanup ();
	// Node ID cookies cleanup
	vxlnetwork::syn_cookies syn_cookies;
	void ongoing_syn_cookie_cleanup ();
	void ongoing_keepalive ();
	std::size_t size () const;
	float size_sqrt () const;
	bool empty () const;
	void erase (vxlnetwork::transport::channel const &);
	void set_bandwidth_params (double, std::size_t);
	static std::string to_string (vxlnetwork::networks);

private:
	void process_message (vxlnetwork::message const &, std::shared_ptr<vxlnetwork::transport::channel> const &);

public:
	std::function<void (vxlnetwork::message const &, std::shared_ptr<vxlnetwork::transport::channel> const &)> inbound;
	vxlnetwork::message_buffer_manager buffer_container;
	boost::asio::ip::udp::resolver resolver;
	std::vector<boost::thread> packet_processing_threads;
	vxlnetwork::bandwidth_limiter limiter;
	vxlnetwork::peer_exclusion excluded_peers;
	vxlnetwork::tcp_message_manager tcp_message_manager;
	vxlnetwork::node & node;
	vxlnetwork::network_filter publish_filter;
	vxlnetwork::transport::udp_channels udp_channels;
	vxlnetwork::transport::tcp_channels tcp_channels;
	std::atomic<uint16_t> port{ 0 };
	std::function<void ()> disconnect_observer;
	// Called when a new channel is observed
	std::function<void (std::shared_ptr<vxlnetwork::transport::channel>)> channel_observer;
	std::atomic<bool> stopped{ false };
	static unsigned const broadcast_interval_ms = 10;
	static std::size_t const buffer_size = 512;
	static std::size_t const confirm_req_hashes_max = 7;
	static std::size_t const confirm_ack_hashes_max = 12;
};
std::unique_ptr<container_info_component> collect_container_info (network & network, std::string const & name);
}
