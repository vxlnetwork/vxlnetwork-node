#pragma once

#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/node/transport/transport.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <mutex>
#include <unordered_set>

namespace mi = boost::multi_index;

namespace vxlnetwork
{
class message_buffer;
namespace transport
{
	class udp_channels;
	class channel_udp final : public vxlnetwork::transport::channel
	{
		friend class vxlnetwork::transport::udp_channels;

	public:
		channel_udp (vxlnetwork::transport::udp_channels &, vxlnetwork::endpoint const &, uint8_t protocol_version);
		std::size_t hash_code () const override;
		bool operator== (vxlnetwork::transport::channel const &) const override;
		// TODO: investigate clang-tidy warning about default parameters on virtual/override functions
		//
		void send_buffer (vxlnetwork::shared_const_buffer const &, std::function<void (boost::system::error_code const &, std::size_t)> const & = nullptr, vxlnetwork::buffer_drop_policy = vxlnetwork::buffer_drop_policy::limiter) override;
		std::string to_string () const override;
		bool operator== (vxlnetwork::transport::channel_udp const & other_a) const
		{
			return &channels == &other_a.channels && endpoint == other_a.endpoint;
		}

		vxlnetwork::endpoint get_endpoint () const override
		{
			vxlnetwork::lock_guard<vxlnetwork::mutex> lk (channel_mutex);
			return endpoint;
		}

		vxlnetwork::tcp_endpoint get_tcp_endpoint () const override
		{
			vxlnetwork::lock_guard<vxlnetwork::mutex> lk (channel_mutex);
			return vxlnetwork::transport::map_endpoint_to_tcp (endpoint);
		}

		vxlnetwork::transport::transport_type get_type () const override
		{
			return vxlnetwork::transport::transport_type::udp;
		}

		std::chrono::steady_clock::time_point get_last_telemetry_req ()
		{
			vxlnetwork::lock_guard<vxlnetwork::mutex> lk (channel_mutex);
			return last_telemetry_req;
		}

		void set_last_telemetry_req (std::chrono::steady_clock::time_point const time_a)
		{
			vxlnetwork::lock_guard<vxlnetwork::mutex> lk (channel_mutex);
			last_telemetry_req = time_a;
		}

	private:
		vxlnetwork::endpoint endpoint;
		vxlnetwork::transport::udp_channels & channels;
		std::chrono::steady_clock::time_point last_telemetry_req{ std::chrono::steady_clock::time_point () };
	};
	class udp_channels final
	{
		friend class vxlnetwork::transport::channel_udp;

	public:
		udp_channels (vxlnetwork::node &, uint16_t, std::function<void (vxlnetwork::message const &, std::shared_ptr<vxlnetwork::transport::channel> const &)> sink);
		std::shared_ptr<vxlnetwork::transport::channel_udp> insert (vxlnetwork::endpoint const &, unsigned);
		void erase (vxlnetwork::endpoint const &);
		std::size_t size () const;
		std::shared_ptr<vxlnetwork::transport::channel_udp> channel (vxlnetwork::endpoint const &) const;
		void random_fill (std::array<vxlnetwork::endpoint, 8> &) const;
		std::unordered_set<std::shared_ptr<vxlnetwork::transport::channel>> random_set (std::size_t, uint8_t = 0) const;
		bool store_all (bool = true);
		std::shared_ptr<vxlnetwork::transport::channel_udp> find_node_id (vxlnetwork::account const &);
		void clean_node_id (vxlnetwork::account const &);
		void clean_node_id (vxlnetwork::endpoint const &, vxlnetwork::account const &);
		// Get the next peer for attempting a tcp bootstrap connection
		vxlnetwork::tcp_endpoint bootstrap_peer (uint8_t connection_protocol_version_min);
		void receive ();
		void start ();
		void stop ();
		void send (vxlnetwork::shared_const_buffer const & buffer_a, vxlnetwork::endpoint endpoint_a, std::function<void (boost::system::error_code const &, std::size_t)> const & callback_a);
		vxlnetwork::endpoint get_local_endpoint () const;
		void receive_action (vxlnetwork::message_buffer *);
		void process_packets ();
		std::shared_ptr<vxlnetwork::transport::channel> create (vxlnetwork::endpoint const &);
		bool max_ip_connections (vxlnetwork::endpoint const & endpoint_a);
		bool max_subnetwork_connections (vxlnetwork::endpoint const & endpoint_a);
		bool max_ip_or_subnetwork_connections (vxlnetwork::endpoint const & endpoint_a);
		// Should we reach out to this endpoint with a keepalive message
		bool reachout (vxlnetwork::endpoint const &);
		std::unique_ptr<container_info_component> collect_container_info (std::string const &);
		void purge (std::chrono::steady_clock::time_point const &);
		void ongoing_keepalive ();
		void list (std::deque<std::shared_ptr<vxlnetwork::transport::channel>> &, uint8_t = 0);
		void modify (std::shared_ptr<vxlnetwork::transport::channel_udp> const &, std::function<void (std::shared_ptr<vxlnetwork::transport::channel_udp> const &)>);
		vxlnetwork::node & node;
		std::function<void (vxlnetwork::message const &, std::shared_ptr<vxlnetwork::transport::channel> const &)> sink;

	private:
		void close_socket ();
		class endpoint_tag
		{
		};
		class ip_address_tag
		{
		};
		class subnetwork_tag
		{
		};
		class random_access_tag
		{
		};
		class last_packet_received_tag
		{
		};
		class last_bootstrap_attempt_tag
		{
		};
		class last_attempt_tag
		{
		};
		class node_id_tag
		{
		};
		class channel_udp_wrapper final
		{
		public:
			std::shared_ptr<vxlnetwork::transport::channel_udp> channel;
			explicit channel_udp_wrapper (std::shared_ptr<vxlnetwork::transport::channel_udp> channel_a) :
				channel (std::move (channel_a))
			{
			}
			vxlnetwork::endpoint endpoint () const
			{
				return channel->get_endpoint ();
			}
			std::chrono::steady_clock::time_point last_packet_received () const
			{
				return channel->get_last_packet_received ();
			}
			std::chrono::steady_clock::time_point last_bootstrap_attempt () const
			{
				return channel->get_last_bootstrap_attempt ();
			}
			std::chrono::steady_clock::time_point last_telemetry_req () const
			{
				return channel->get_last_telemetry_req ();
			}
			boost::asio::ip::address ip_address () const
			{
				return vxlnetwork::transport::ipv4_address_or_ipv6_subnet (endpoint ().address ());
			}
			boost::asio::ip::address subnetwork () const
			{
				return vxlnetwork::transport::map_address_to_subnetwork (endpoint ().address ());
			}
			vxlnetwork::account node_id () const
			{
				return channel->get_node_id ();
			}
		};
		class endpoint_attempt final
		{
		public:
			vxlnetwork::endpoint endpoint;
			boost::asio::ip::address subnetwork;
			std::chrono::steady_clock::time_point last_attempt{ std::chrono::steady_clock::now () };

			explicit endpoint_attempt (vxlnetwork::endpoint const & endpoint_a) :
				endpoint (endpoint_a),
				subnetwork (vxlnetwork::transport::map_address_to_subnetwork (endpoint_a.address ()))
			{
			}
		};
		mutable vxlnetwork::mutex mutex;
		// clang-format off
		boost::multi_index_container<
		channel_udp_wrapper,
		mi::indexed_by<
			mi::random_access<mi::tag<random_access_tag>>,
			mi::ordered_non_unique<mi::tag<last_bootstrap_attempt_tag>,
				mi::const_mem_fun<channel_udp_wrapper, std::chrono::steady_clock::time_point, &channel_udp_wrapper::last_bootstrap_attempt>>,
			mi::hashed_unique<mi::tag<endpoint_tag>,
				mi::const_mem_fun<channel_udp_wrapper, vxlnetwork::endpoint, &channel_udp_wrapper::endpoint>>,
			mi::hashed_non_unique<mi::tag<node_id_tag>,
				mi::const_mem_fun<channel_udp_wrapper, vxlnetwork::account, &channel_udp_wrapper::node_id>>,
			mi::ordered_non_unique<mi::tag<last_packet_received_tag>,
				mi::const_mem_fun<channel_udp_wrapper, std::chrono::steady_clock::time_point, &channel_udp_wrapper::last_packet_received>>,
			mi::hashed_non_unique<mi::tag<ip_address_tag>,
				mi::const_mem_fun<channel_udp_wrapper, boost::asio::ip::address, &channel_udp_wrapper::ip_address>>,
			mi::hashed_non_unique<mi::tag<subnetwork_tag>,
				mi::const_mem_fun<channel_udp_wrapper, boost::asio::ip::address, &channel_udp_wrapper::subnetwork>>>>
		channels;
		boost::multi_index_container<
		endpoint_attempt,
		mi::indexed_by<
			mi::hashed_unique<mi::tag<endpoint_tag>,
				mi::member<endpoint_attempt, vxlnetwork::endpoint, &endpoint_attempt::endpoint>>,
			mi::hashed_non_unique<mi::tag<subnetwork_tag>,
				mi::member<endpoint_attempt, boost::asio::ip::address, &endpoint_attempt::subnetwork>>,
			mi::ordered_non_unique<mi::tag<last_attempt_tag>,
				mi::member<endpoint_attempt, std::chrono::steady_clock::time_point, &endpoint_attempt::last_attempt>>>>
		attempts;
		// clang-format on
		boost::asio::strand<boost::asio::io_context::executor_type> strand;
		std::unique_ptr<boost::asio::ip::udp::socket> socket;
		vxlnetwork::endpoint local_endpoint;
		std::atomic<bool> stopped{ false };
	};
} // namespace transport
} // namespace vxlnetwork
