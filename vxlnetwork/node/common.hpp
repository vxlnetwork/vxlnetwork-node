#pragma once

#include <vxlnetwork/boost/asio/ip/tcp.hpp>
#include <vxlnetwork/boost/asio/ip/udp.hpp>
#include <vxlnetwork/crypto_lib/random_pool.hpp>
#include <vxlnetwork/lib/asio.hpp>
#include <vxlnetwork/lib/jsonconfig.hpp>
#include <vxlnetwork/lib/memory.hpp>
#include <vxlnetwork/secure/common.hpp>
#include <vxlnetwork/secure/network_filter.hpp>

#include <bitset>

namespace vxlnetwork
{
using endpoint = boost::asio::ip::udp::endpoint;
bool parse_port (std::string const &, uint16_t &);
bool parse_address (std::string const &, boost::asio::ip::address &);
bool parse_address_port (std::string const &, boost::asio::ip::address &, uint16_t &);
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
bool parse_endpoint (std::string const &, vxlnetwork::endpoint &);
bool parse_tcp_endpoint (std::string const &, vxlnetwork::tcp_endpoint &);
uint64_t ip_address_hash_raw (boost::asio::ip::address const & ip_a, uint16_t port = 0);
}

namespace
{
uint64_t endpoint_hash_raw (vxlnetwork::endpoint const & endpoint_a)
{
	uint64_t result (vxlnetwork::ip_address_hash_raw (endpoint_a.address (), endpoint_a.port ()));
	return result;
}
uint64_t endpoint_hash_raw (vxlnetwork::tcp_endpoint const & endpoint_a)
{
	uint64_t result (vxlnetwork::ip_address_hash_raw (endpoint_a.address (), endpoint_a.port ()));
	return result;
}

template <std::size_t size>
struct endpoint_hash
{
};

template <>
struct endpoint_hash<8>
{
	std::size_t operator() (vxlnetwork::endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
	std::size_t operator() (vxlnetwork::tcp_endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
};

template <>
struct endpoint_hash<4>
{
	std::size_t operator() (vxlnetwork::endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
	std::size_t operator() (vxlnetwork::tcp_endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};

template <std::size_t size>
struct ip_address_hash
{
};

template <>
struct ip_address_hash<8>
{
	std::size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		return vxlnetwork::ip_address_hash_raw (ip_address_a);
	}
};

template <>
struct ip_address_hash<4>
{
	std::size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		uint64_t big (vxlnetwork::ip_address_hash_raw (ip_address_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
}

namespace std
{
template <>
struct hash<::vxlnetwork::endpoint>
{
	std::size_t operator() (::vxlnetwork::endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (std::size_t)> ehash;
		return ehash (endpoint_a);
	}
};

template <>
struct hash<::vxlnetwork::tcp_endpoint>
{
	std::size_t operator() (::vxlnetwork::tcp_endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (std::size_t)> ehash;
		return ehash (endpoint_a);
	}
};

#ifndef BOOST_ASIO_HAS_STD_HASH
template <>
struct hash<boost::asio::ip::address>
{
	std::size_t operator() (boost::asio::ip::address const & ip_a) const
	{
		ip_address_hash<sizeof (std::size_t)> ihash;
		return ihash (ip_a);
	}
};
#endif
}

namespace boost
{
template <>
struct hash<::vxlnetwork::endpoint>
{
	std::size_t operator() (::vxlnetwork::endpoint const & endpoint_a) const
	{
		std::hash<::vxlnetwork::endpoint> hash;
		return hash (endpoint_a);
	}
};

template <>
struct hash<::vxlnetwork::tcp_endpoint>
{
	std::size_t operator() (::vxlnetwork::tcp_endpoint const & endpoint_a) const
	{
		std::hash<::vxlnetwork::tcp_endpoint> hash;
		return hash (endpoint_a);
	}
};

template <>
struct hash<boost::asio::ip::address>
{
	std::size_t operator() (boost::asio::ip::address const & ip_a) const
	{
		std::hash<boost::asio::ip::address> hash;
		return hash (ip_a);
	}
};
}

namespace vxlnetwork
{
/**
 * Message types are serialized to the network and existing values must thus never change as
 * types are added, removed and reordered in the enum.
 */
enum class message_type : uint8_t
{
	invalid = 0x0,
	not_a_type = 0x1,
	keepalive = 0x2,
	publish = 0x3,
	confirm_req = 0x4,
	confirm_ack = 0x5,
	bulk_pull = 0x6,
	bulk_push = 0x7,
	frontier_req = 0x8,
	/* deleted 0x9 */
	node_id_handshake = 0x0a,
	bulk_pull_account = 0x0b,
	telemetry_req = 0x0c,
	telemetry_ack = 0x0d
};

std::string message_type_to_string (message_type);

enum class bulk_pull_account_flags : uint8_t
{
	pending_hash_and_amount = 0x0,
	pending_address_only = 0x1,
	pending_hash_amount_and_address = 0x2
};

class message_visitor;
class message_header final
{
public:
	message_header (vxlnetwork::network_constants const &, vxlnetwork::message_type);
	message_header (bool &, vxlnetwork::stream &);
	void serialize (vxlnetwork::stream &) const;
	bool deserialize (vxlnetwork::stream &);
	vxlnetwork::block_type block_type () const;
	void block_type_set (vxlnetwork::block_type);
	uint8_t count_get () const;
	void count_set (uint8_t);
	vxlnetwork::networks network;
	uint8_t version_max;
	uint8_t version_using;
	uint8_t version_min;
	std::string to_string ();

public:
	vxlnetwork::message_type type;
	std::bitset<16> extensions;
	static std::size_t constexpr size = sizeof (vxlnetwork::networks) + sizeof (version_max) + sizeof (version_using) + sizeof (version_min) + sizeof (type) + sizeof (/* extensions */ uint16_t);

	void flag_set (uint8_t);
	static uint8_t constexpr bulk_pull_count_present_flag = 0;
	bool bulk_pull_is_count_present () const;
	static uint8_t constexpr frontier_req_only_confirmed = 1;
	bool frontier_req_is_only_confirmed_present () const;
	static uint8_t constexpr node_id_handshake_query_flag = 0;
	static uint8_t constexpr node_id_handshake_response_flag = 1;
	bool node_id_handshake_is_query () const;
	bool node_id_handshake_is_response () const;

	/** Size of the payload in bytes. For some messages, the payload size is based on header flags. */
	std::size_t payload_length_bytes () const;

	static std::bitset<16> constexpr block_type_mask{ 0x0f00 };
	static std::bitset<16> constexpr count_mask{ 0xf000 };
	static std::bitset<16> constexpr telemetry_size_mask{ 0x3ff };
};

class message
{
public:
	explicit message (vxlnetwork::network_constants const &, vxlnetwork::message_type);
	explicit message (vxlnetwork::message_header const &);
	virtual ~message () = default;
	virtual void serialize (vxlnetwork::stream &) const = 0;
	virtual void visit (vxlnetwork::message_visitor &) const = 0;
	std::shared_ptr<std::vector<uint8_t>> to_bytes () const;
	vxlnetwork::shared_const_buffer to_shared_const_buffer () const;

	vxlnetwork::message_header header;
};

class work_pool;
class network_constants;
class message_parser final
{
public:
	enum class parse_status
	{
		success,
		insufficient_work,
		invalid_header,
		invalid_message_type,
		invalid_keepalive_message,
		invalid_publish_message,
		invalid_confirm_req_message,
		invalid_confirm_ack_message,
		invalid_node_id_handshake_message,
		invalid_telemetry_req_message,
		invalid_telemetry_ack_message,
		outdated_version,
		duplicate_publish_message
	};
	message_parser (vxlnetwork::network_filter &, vxlnetwork::block_uniquer &, vxlnetwork::vote_uniquer &, vxlnetwork::message_visitor &, vxlnetwork::work_pool &, vxlnetwork::network_constants const & protocol);
	void deserialize_buffer (uint8_t const *, std::size_t);
	void deserialize_keepalive (vxlnetwork::stream &, vxlnetwork::message_header const &);
	void deserialize_publish (vxlnetwork::stream &, vxlnetwork::message_header const &, vxlnetwork::uint128_t const & = 0);
	void deserialize_confirm_req (vxlnetwork::stream &, vxlnetwork::message_header const &);
	void deserialize_confirm_ack (vxlnetwork::stream &, vxlnetwork::message_header const &);
	void deserialize_node_id_handshake (vxlnetwork::stream &, vxlnetwork::message_header const &);
	void deserialize_telemetry_req (vxlnetwork::stream &, vxlnetwork::message_header const &);
	void deserialize_telemetry_ack (vxlnetwork::stream &, vxlnetwork::message_header const &);
	bool at_end (vxlnetwork::stream &);
	vxlnetwork::network_filter & publish_filter;
	vxlnetwork::block_uniquer & block_uniquer;
	vxlnetwork::vote_uniquer & vote_uniquer;
	vxlnetwork::message_visitor & visitor;
	vxlnetwork::work_pool & pool;
	parse_status status;
	vxlnetwork::network_constants const & network;
	std::string status_string ();
	static std::size_t const max_safe_udp_message_size;
};

class keepalive final : public message
{
public:
	explicit keepalive (vxlnetwork::network_constants const & constants);
	keepalive (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &);
	void visit (vxlnetwork::message_visitor &) const override;
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	bool operator== (vxlnetwork::keepalive const &) const;
	std::array<vxlnetwork::endpoint, 8> peers;
	static std::size_t constexpr size = 8 * (16 + 2);
};

class publish final : public message
{
public:
	publish (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &, vxlnetwork::uint128_t const & = 0, vxlnetwork::block_uniquer * = nullptr);
	publish (vxlnetwork::network_constants const & constants, std::shared_ptr<vxlnetwork::block> const &);
	void visit (vxlnetwork::message_visitor &) const override;
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &, vxlnetwork::block_uniquer * = nullptr);
	bool operator== (vxlnetwork::publish const &) const;
	std::shared_ptr<vxlnetwork::block> block;
	vxlnetwork::uint128_t digest{ 0 };
};

class confirm_req final : public message
{
public:
	confirm_req (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &, vxlnetwork::block_uniquer * = nullptr);
	confirm_req (vxlnetwork::network_constants const & constants, std::shared_ptr<vxlnetwork::block> const &);
	confirm_req (vxlnetwork::network_constants const & constants, std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::root>> const &);
	confirm_req (vxlnetwork::network_constants const & constants, vxlnetwork::block_hash const &, vxlnetwork::root const &);
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &, vxlnetwork::block_uniquer * = nullptr);
	void visit (vxlnetwork::message_visitor &) const override;
	bool operator== (vxlnetwork::confirm_req const &) const;
	std::shared_ptr<vxlnetwork::block> block;
	std::vector<std::pair<vxlnetwork::block_hash, vxlnetwork::root>> roots_hashes;
	std::string roots_string () const;
	static std::size_t size (vxlnetwork::block_type, std::size_t = 0);
};

class confirm_ack final : public message
{
public:
	confirm_ack (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &, vxlnetwork::vote_uniquer * = nullptr);
	confirm_ack (vxlnetwork::network_constants const & constants, std::shared_ptr<vxlnetwork::vote> const &);
	void serialize (vxlnetwork::stream &) const override;
	void visit (vxlnetwork::message_visitor &) const override;
	bool operator== (vxlnetwork::confirm_ack const &) const;
	std::shared_ptr<vxlnetwork::vote> vote;
	static std::size_t size (vxlnetwork::block_type, std::size_t = 0);
};

class frontier_req final : public message
{
public:
	explicit frontier_req (vxlnetwork::network_constants const & constants);
	frontier_req (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &);
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void visit (vxlnetwork::message_visitor &) const override;
	bool operator== (vxlnetwork::frontier_req const &) const;
	vxlnetwork::account start;
	uint32_t age;
	uint32_t count;
	static std::size_t constexpr size = sizeof (start) + sizeof (age) + sizeof (count);
};

enum class telemetry_maker : uint8_t
{
	nf_node = 0,
	nf_pruned_node = 1
};

class telemetry_data
{
public:
	vxlnetwork::signature signature{ 0 };
	vxlnetwork::account node_id{};
	uint64_t block_count{ 0 };
	uint64_t cemented_count{ 0 };
	uint64_t unchecked_count{ 0 };
	uint64_t account_count{ 0 };
	uint64_t bandwidth_cap{ 0 };
	uint64_t uptime{ 0 };
	uint32_t peer_count{ 0 };
	uint8_t protocol_version{ 0 };
	vxlnetwork::block_hash genesis_block{ 0 };
	uint8_t major_version{ 0 };
	uint8_t minor_version{ 0 };
	uint8_t patch_version{ 0 };
	uint8_t pre_release_version{ 0 };
	uint8_t maker{ static_cast<std::underlying_type_t<telemetry_maker>> (telemetry_maker::nf_node) }; // Where this telemetry information originated
	std::chrono::system_clock::time_point timestamp;
	uint64_t active_difficulty{ 0 };
	std::vector<uint8_t> unknown_data;

	void serialize (vxlnetwork::stream &) const;
	void deserialize (vxlnetwork::stream &, uint16_t);
	vxlnetwork::error serialize_json (vxlnetwork::jsonconfig &, bool) const;
	vxlnetwork::error deserialize_json (vxlnetwork::jsonconfig &, bool);
	void sign (vxlnetwork::keypair const &);
	bool validate_signature () const;
	bool operator== (vxlnetwork::telemetry_data const &) const;
	bool operator!= (vxlnetwork::telemetry_data const &) const;
	std::string to_string () const;

	// Size does not include unknown_data
	static auto constexpr size = sizeof (signature) + sizeof (node_id) + sizeof (block_count) + sizeof (cemented_count) + sizeof (unchecked_count) + sizeof (account_count) + sizeof (bandwidth_cap) + sizeof (peer_count) + sizeof (protocol_version) + sizeof (uptime) + sizeof (genesis_block) + sizeof (major_version) + sizeof (minor_version) + sizeof (patch_version) + sizeof (pre_release_version) + sizeof (maker) + sizeof (uint64_t) + sizeof (active_difficulty);
	static auto constexpr latest_size = size; // This needs to be updated for each new telemetry version
private:
	void serialize_without_signature (vxlnetwork::stream &) const;
};

class telemetry_req final : public message
{
public:
	explicit telemetry_req (vxlnetwork::network_constants const & constants);
	explicit telemetry_req (vxlnetwork::message_header const &);
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void visit (vxlnetwork::message_visitor &) const override;
};

class telemetry_ack final : public message
{
public:
	explicit telemetry_ack (vxlnetwork::network_constants const & constants);
	telemetry_ack (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &);
	telemetry_ack (vxlnetwork::network_constants const & constants, telemetry_data const &);
	void serialize (vxlnetwork::stream &) const override;
	void visit (vxlnetwork::message_visitor &) const override;
	bool deserialize (vxlnetwork::stream &);
	uint16_t size () const;
	bool is_empty_payload () const;
	static uint16_t size (vxlnetwork::message_header const &);
	vxlnetwork::telemetry_data data;
};

class bulk_pull final : public message
{
public:
	using count_t = uint32_t;
	explicit bulk_pull (vxlnetwork::network_constants const & constants);
	bulk_pull (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &);
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void visit (vxlnetwork::message_visitor &) const override;
	vxlnetwork::hash_or_account start{ 0 };
	vxlnetwork::block_hash end{ 0 };
	count_t count{ 0 };
	bool is_count_present () const;
	void set_count_present (bool);
	static std::size_t constexpr count_present_flag = vxlnetwork::message_header::bulk_pull_count_present_flag;
	static std::size_t constexpr extended_parameters_size = 8;
	static std::size_t constexpr size = sizeof (start) + sizeof (end);
};

class bulk_pull_account final : public message
{
public:
	explicit bulk_pull_account (vxlnetwork::network_constants const & constants);
	bulk_pull_account (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &);
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void visit (vxlnetwork::message_visitor &) const override;
	vxlnetwork::account account;
	vxlnetwork::amount minimum_amount;
	bulk_pull_account_flags flags;
	static std::size_t constexpr size = sizeof (account) + sizeof (minimum_amount) + sizeof (bulk_pull_account_flags);
};

class bulk_push final : public message
{
public:
	explicit bulk_push (vxlnetwork::network_constants const & constants);
	explicit bulk_push (vxlnetwork::message_header const &);
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void visit (vxlnetwork::message_visitor &) const override;
};

class node_id_handshake final : public message
{
public:
	node_id_handshake (bool &, vxlnetwork::stream &, vxlnetwork::message_header const &);
	node_id_handshake (vxlnetwork::network_constants const & constants, boost::optional<vxlnetwork::uint256_union>, boost::optional<std::pair<vxlnetwork::account, vxlnetwork::signature>>);
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void visit (vxlnetwork::message_visitor &) const override;
	bool operator== (vxlnetwork::node_id_handshake const &) const;
	boost::optional<vxlnetwork::uint256_union> query;
	boost::optional<std::pair<vxlnetwork::account, vxlnetwork::signature>> response;
	std::size_t size () const;
	static std::size_t size (vxlnetwork::message_header const &);
};

class message_visitor
{
public:
	virtual void keepalive (vxlnetwork::keepalive const &) = 0;
	virtual void publish (vxlnetwork::publish const &) = 0;
	virtual void confirm_req (vxlnetwork::confirm_req const &) = 0;
	virtual void confirm_ack (vxlnetwork::confirm_ack const &) = 0;
	virtual void bulk_pull (vxlnetwork::bulk_pull const &) = 0;
	virtual void bulk_pull_account (vxlnetwork::bulk_pull_account const &) = 0;
	virtual void bulk_push (vxlnetwork::bulk_push const &) = 0;
	virtual void frontier_req (vxlnetwork::frontier_req const &) = 0;
	virtual void node_id_handshake (vxlnetwork::node_id_handshake const &) = 0;
	virtual void telemetry_req (vxlnetwork::telemetry_req const &) = 0;
	virtual void telemetry_ack (vxlnetwork::telemetry_ack const &) = 0;
	virtual ~message_visitor ();
};

class telemetry_cache_cutoffs
{
public:
	static std::chrono::seconds constexpr dev{ 3 };
	static std::chrono::seconds constexpr beta{ 15 };
	static std::chrono::seconds constexpr live{ 60 };

	static std::chrono::seconds network_to_time (network_constants const & network_constants);
};

/** Helper guard which contains all the necessary purge (remove all memory even if used) functions */
class node_singleton_memory_pool_purge_guard
{
public:
	node_singleton_memory_pool_purge_guard ();

private:
	vxlnetwork::cleanup_guard cleanup_guard;
};
}