#pragma once

#include <vxlnetwork/crypto/blake2/blake2.h>
#include <vxlnetwork/lib/blockbuilders.hpp>
#include <vxlnetwork/lib/blocks.hpp>
#include <vxlnetwork/lib/config.hpp>
#include <vxlnetwork/lib/epoch.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/lib/rep_weights.hpp>
#include <vxlnetwork/lib/utility.hpp>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/optional/optional.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/variant/variant.hpp>

#include <unordered_map>

namespace boost
{
template <>
struct hash<::vxlnetwork::uint256_union>
{
	size_t operator() (::vxlnetwork::uint256_union const & value_a) const
	{
		return std::hash<::vxlnetwork::uint256_union> () (value_a);
	}
};

template <>
struct hash<::vxlnetwork::block_hash>
{
	size_t operator() (::vxlnetwork::block_hash const & value_a) const
	{
		return std::hash<::vxlnetwork::block_hash> () (value_a);
	}
};

template <>
struct hash<::vxlnetwork::public_key>
{
	size_t operator() (::vxlnetwork::public_key const & value_a) const
	{
		return std::hash<::vxlnetwork::public_key> () (value_a);
	}
};
template <>
struct hash<::vxlnetwork::uint512_union>
{
	size_t operator() (::vxlnetwork::uint512_union const & value_a) const
	{
		return std::hash<::vxlnetwork::uint512_union> () (value_a);
	}
};
template <>
struct hash<::vxlnetwork::qualified_root>
{
	size_t operator() (::vxlnetwork::qualified_root const & value_a) const
	{
		return std::hash<::vxlnetwork::qualified_root> () (value_a);
	}
};
template <>
struct hash<::vxlnetwork::root>
{
	size_t operator() (::vxlnetwork::root const & value_a) const
	{
		return std::hash<::vxlnetwork::root> () (value_a);
	}
};
}
namespace vxlnetwork
{
/**
 * A key pair. The private key is generated from the random pool, or passed in
 * as a hex string. The public key is derived using ed25519.
 */
class keypair
{
public:
	keypair ();
	keypair (std::string const &);
	keypair (vxlnetwork::raw_key &&);
	vxlnetwork::public_key pub;
	vxlnetwork::raw_key prv;
};

/**
 * Latest information about an account
 */
class account_info final
{
public:
	account_info () = default;
	account_info (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::amount const &, uint64_t, uint64_t, epoch);
	bool deserialize (vxlnetwork::stream &);
	bool operator== (vxlnetwork::account_info const &) const;
	bool operator!= (vxlnetwork::account_info const &) const;
	size_t db_size () const;
	vxlnetwork::epoch epoch () const;
	vxlnetwork::block_hash head{ 0 };
	vxlnetwork::account representative{};
	vxlnetwork::block_hash open_block{ 0 };
	vxlnetwork::amount balance{ 0 };
	/** Seconds since posix epoch */
	uint64_t modified{ 0 };
	uint64_t block_count{ 0 };
	vxlnetwork::epoch epoch_m{ vxlnetwork::epoch::epoch_0 };
};

/**
 * Information on an uncollected send
 */
class pending_info final
{
public:
	pending_info () = default;
	pending_info (vxlnetwork::account const &, vxlnetwork::amount const &, vxlnetwork::epoch);
	size_t db_size () const;
	bool deserialize (vxlnetwork::stream &);
	bool operator== (vxlnetwork::pending_info const &) const;
	vxlnetwork::account source{};
	vxlnetwork::amount amount{ 0 };
	vxlnetwork::epoch epoch{ vxlnetwork::epoch::epoch_0 };
};
class pending_key final
{
public:
	pending_key () = default;
	pending_key (vxlnetwork::account const &, vxlnetwork::block_hash const &);
	bool deserialize (vxlnetwork::stream &);
	bool operator== (vxlnetwork::pending_key const &) const;
	vxlnetwork::account const & key () const;
	vxlnetwork::account account{};
	vxlnetwork::block_hash hash{ 0 };
};

class endpoint_key final
{
public:
	endpoint_key () = default;

	/*
     * @param address_a This should be in network byte order
     * @param port_a This should be in host byte order
     */
	endpoint_key (std::array<uint8_t, 16> const & address_a, uint16_t port_a);

	/*
     * @return The ipv6 address in network byte order
     */
	std::array<uint8_t, 16> const & address_bytes () const;

	/*
     * @return The port in host byte order
     */
	uint16_t port () const;

private:
	// Both stored internally in network byte order
	std::array<uint8_t, 16> address;
	uint16_t network_port{ 0 };
};

enum class no_value
{
	dummy
};

class unchecked_key final
{
public:
	unchecked_key () = default;
	explicit unchecked_key (vxlnetwork::hash_or_account const & dependency);
	unchecked_key (vxlnetwork::hash_or_account const &, vxlnetwork::block_hash const &);
	unchecked_key (vxlnetwork::uint512_union const &);
	bool deserialize (vxlnetwork::stream &);
	bool operator== (vxlnetwork::unchecked_key const &) const;
	vxlnetwork::block_hash const & key () const;
	vxlnetwork::block_hash previous{ 0 };
	vxlnetwork::block_hash hash{ 0 };
};

/**
 * Tag for block signature verification result
 */
enum class signature_verification : uint8_t
{
	unknown = 0,
	invalid = 1,
	valid = 2,
	valid_epoch = 3 // Valid for epoch blocks
};

/**
 * Information on an unchecked block
 */
class unchecked_info final
{
public:
	unchecked_info () = default;
	unchecked_info (std::shared_ptr<vxlnetwork::block> const &, vxlnetwork::account const &, vxlnetwork::signature_verification = vxlnetwork::signature_verification::unknown);
	unchecked_info (std::shared_ptr<vxlnetwork::block> const &);
	void serialize (vxlnetwork::stream &) const;
	bool deserialize (vxlnetwork::stream &);
	uint64_t modified () const;
	std::shared_ptr<vxlnetwork::block> block;
	vxlnetwork::account account{};

private:
	/** Seconds since posix epoch */
	uint64_t modified_m{ 0 };

public:
	vxlnetwork::signature_verification verified{ vxlnetwork::signature_verification::unknown };
};

class block_info final
{
public:
	block_info () = default;
	block_info (vxlnetwork::account const &, vxlnetwork::amount const &);
	vxlnetwork::account account{};
	vxlnetwork::amount balance{ 0 };
};

class confirmation_height_info final
{
public:
	confirmation_height_info () = default;
	confirmation_height_info (uint64_t, vxlnetwork::block_hash const &);

	void serialize (vxlnetwork::stream &) const;
	bool deserialize (vxlnetwork::stream &);

	/** height of the cemented frontier */
	uint64_t height{};

	/** hash of the highest cemented block, the cemented/confirmed frontier */
	vxlnetwork::block_hash frontier{};
};

namespace confirmation_height
{
	/** When the uncemented count (block count - cemented count) is less than this use the unbounded processor */
	uint64_t const unbounded_cutoff{ 16384 };
}

using vote_blocks_vec_iter = std::vector<boost::variant<std::shared_ptr<vxlnetwork::block>, vxlnetwork::block_hash>>::const_iterator;
class iterate_vote_blocks_as_hash final
{
public:
	iterate_vote_blocks_as_hash () = default;
	vxlnetwork::block_hash operator() (boost::variant<std::shared_ptr<vxlnetwork::block>, vxlnetwork::block_hash> const & item) const;
};
class vote final
{
public:
	vote () = default;
	vote (vxlnetwork::vote const &);
	vote (bool &, vxlnetwork::stream &, vxlnetwork::block_uniquer * = nullptr);
	vote (bool &, vxlnetwork::stream &, vxlnetwork::block_type, vxlnetwork::block_uniquer * = nullptr);
	vote (vxlnetwork::account const &, vxlnetwork::raw_key const &, uint64_t timestamp, uint8_t duration, std::shared_ptr<vxlnetwork::block> const &);
	vote (vxlnetwork::account const &, vxlnetwork::raw_key const &, uint64_t timestamp, uint8_t duration, std::vector<vxlnetwork::block_hash> const &);
	std::string hashes_string () const;
	vxlnetwork::block_hash hash () const;
	vxlnetwork::block_hash full_hash () const;
	bool operator== (vxlnetwork::vote const &) const;
	bool operator!= (vxlnetwork::vote const &) const;
	void serialize (vxlnetwork::stream &, vxlnetwork::block_type) const;
	void serialize (vxlnetwork::stream &) const;
	void serialize_json (boost::property_tree::ptree & tree) const;
	bool deserialize (vxlnetwork::stream &, vxlnetwork::block_uniquer * = nullptr);
	bool validate () const;
	boost::transform_iterator<vxlnetwork::iterate_vote_blocks_as_hash, vxlnetwork::vote_blocks_vec_iter> begin () const;
	boost::transform_iterator<vxlnetwork::iterate_vote_blocks_as_hash, vxlnetwork::vote_blocks_vec_iter> end () const;
	std::string to_json () const;
	uint64_t timestamp () const;
	uint8_t duration_bits () const;
	std::chrono::milliseconds duration () const;
	static uint64_t constexpr timestamp_mask = { 0xffff'ffff'ffff'fff0ULL };
	static uint64_t constexpr timestamp_max = { 0xffff'ffff'ffff'fff0ULL };
	static uint64_t constexpr timestamp_min = { 0x0000'0000'0000'0010ULL };
	static uint8_t constexpr duration_max = { 0x0fu };

private:
	// Vote timestamp
	uint64_t timestamp_m;

public:
	// The blocks, or block hashes, that this vote is for
	std::vector<boost::variant<std::shared_ptr<vxlnetwork::block>, vxlnetwork::block_hash>> blocks;
	// Account that's voting
	vxlnetwork::account account;
	// Signature of timestamp + block hashes
	vxlnetwork::signature signature;
	static std::string const hash_prefix;

private:
	uint64_t packed_timestamp (uint64_t timestamp, uint8_t duration) const;
};
/**
 * This class serves to find and return unique variants of a vote in order to minimize memory usage
 */
class vote_uniquer final
{
public:
	using value_type = std::pair<vxlnetwork::block_hash const, std::weak_ptr<vxlnetwork::vote>>;

	vote_uniquer (vxlnetwork::block_uniquer &);
	std::shared_ptr<vxlnetwork::vote> unique (std::shared_ptr<vxlnetwork::vote> const &);
	size_t size ();

private:
	vxlnetwork::block_uniquer & uniquer;
	vxlnetwork::mutex mutex{ mutex_identifier (mutexes::vote_uniquer) };
	std::unordered_map<std::remove_const_t<value_type::first_type>, value_type::second_type> votes;
	static unsigned constexpr cleanup_count = 2;
};

std::unique_ptr<container_info_component> collect_container_info (vote_uniquer & vote_uniquer, std::string const & name);

enum class vote_code
{
	invalid, // Vote is not signed correctly
	replay, // Vote does not have the highest timestamp, it's a replay
	vote, // Vote has the highest timestamp
	indeterminate // Unknown if replay or vote
};

enum class process_result
{
	progress, // Hasn't been seen before, signed correctly
	bad_signature, // Signature was bad, forged or transmission error
	old, // Already seen and was valid
	negative_spend, // Malicious attempt to spend a negative amount
	fork, // Malicious fork based on previous
	unreceivable, // Source block doesn't exist, has already been received, or requires an account upgrade (epoch blocks)
	gap_previous, // Block marked as previous is unknown
	gap_source, // Block marked as source is unknown
	gap_epoch_open_pending, // Block marked as pending blocks required for epoch open block are unknown
	opened_burn_account, // The impossible happened, someone found the private key associated with the public key '0'.
	balance_mismatch, // Balance and amount delta don't match
	representative_mismatch, // Representative is changed when it is not allowed
	block_position, // This block cannot follow the previous block
	insufficient_work // Insufficient work for this block, even though it passed the minimal validation
};
class process_return final
{
public:
	vxlnetwork::process_result code;
	vxlnetwork::signature_verification verified;
	vxlnetwork::amount previous_balance;
};
enum class tally_result
{
	vote,
	changed,
	confirm
};

class network_params;

/** Genesis keys and ledger constants for network variants */
class ledger_constants
{
public:
	ledger_constants (vxlnetwork::work_thresholds & work, vxlnetwork::networks network_a);
	vxlnetwork::work_thresholds & work;
	vxlnetwork::keypair zero_key;
	vxlnetwork::account vxlnetwork_beta_account;
	vxlnetwork::account vxlnetwork_live_account;
	vxlnetwork::account vxlnetwork_test_account;
	std::shared_ptr<vxlnetwork::block> vxlnetwork_dev_genesis;
	std::shared_ptr<vxlnetwork::block> vxlnetwork_beta_genesis;
	std::shared_ptr<vxlnetwork::block> vxlnetwork_live_genesis;
	std::shared_ptr<vxlnetwork::block> vxlnetwork_test_genesis;
	std::shared_ptr<vxlnetwork::block> genesis;
	vxlnetwork::uint128_t genesis_amount;
	vxlnetwork::account burn_account;
	vxlnetwork::account vxlnetwork_dev_final_votes_canary_account;
	vxlnetwork::account vxlnetwork_beta_final_votes_canary_account;
	vxlnetwork::account vxlnetwork_live_final_votes_canary_account;
	vxlnetwork::account vxlnetwork_test_final_votes_canary_account;
	vxlnetwork::account final_votes_canary_account;
	uint64_t vxlnetwork_dev_final_votes_canary_height;
	uint64_t vxlnetwork_beta_final_votes_canary_height;
	uint64_t vxlnetwork_live_final_votes_canary_height;
	uint64_t vxlnetwork_test_final_votes_canary_height;
	uint64_t final_votes_canary_height;
	vxlnetwork::epochs epochs;
};

namespace dev
{
	extern vxlnetwork::keypair genesis_key;
	extern vxlnetwork::network_params network_params;
	extern vxlnetwork::ledger_constants & constants;
	extern std::shared_ptr<vxlnetwork::block> & genesis;
}

/** Constants which depend on random values (always used as singleton) */
class hardened_constants
{
public:
	static hardened_constants & get ();

	vxlnetwork::account not_an_account;
	vxlnetwork::uint128_union random_128;

private:
	hardened_constants ();
};

/** Node related constants whose value depends on the active network */
class node_constants
{
public:
	node_constants (vxlnetwork::network_constants & network_constants);
	std::chrono::minutes backup_interval;
	std::chrono::seconds search_pending_interval;
	std::chrono::minutes unchecked_cleaning_interval;
	std::chrono::milliseconds process_confirmed_interval;

	/** The maximum amount of samples for a 2 week period on live or 1 day on beta */
	uint64_t max_weight_samples;
	uint64_t weight_period;
};

/** Voting related constants whose value depends on the active network */
class voting_constants
{
public:
	voting_constants (vxlnetwork::network_constants & network_constants);
	size_t const max_cache;
	std::chrono::seconds const delay;
};

/** Port-mapping related constants whose value depends on the active network */
class portmapping_constants
{
public:
	portmapping_constants (vxlnetwork::network_constants & network_constants);
	// Timeouts are primes so they infrequently happen at the same time
	std::chrono::seconds lease_duration;
	std::chrono::seconds health_check_period;
};

/** Bootstrap related constants whose value depends on the active network */
class bootstrap_constants
{
public:
	bootstrap_constants (vxlnetwork::network_constants & network_constants);
	uint32_t lazy_max_pull_blocks;
	uint32_t lazy_min_pull_blocks;
	unsigned frontier_retry_limit;
	unsigned lazy_retry_limit;
	unsigned lazy_destinations_retry_limit;
	std::chrono::milliseconds gap_cache_bootstrap_start_interval;
	uint32_t default_frontiers_age_seconds;
};

/** Constants whose value depends on the active network */
class network_params
{
public:
	/** Populate values based on \p network_a */
	network_params (vxlnetwork::networks network_a);

	unsigned kdf_work;
	vxlnetwork::work_thresholds work;
	vxlnetwork::network_constants network;
	vxlnetwork::ledger_constants ledger;
	vxlnetwork::voting_constants voting;
	vxlnetwork::node_constants node;
	vxlnetwork::portmapping_constants portmapping;
	vxlnetwork::bootstrap_constants bootstrap;
};

enum class confirmation_height_mode
{
	automatic,
	unbounded,
	bounded
};

/* Holds flags for various cacheable data. For most CLI operations caching is unnecessary
     * (e.g getting the cemented block count) so it can be disabled for performance reasons. */
class generate_cache
{
public:
	bool reps = true;
	bool cemented_count = true;
	bool unchecked_count = true;
	bool account_count = true;
	bool block_count = true;

	void enable_all ();
};

/* Holds an in-memory cache of various counts */
class ledger_cache
{
public:
	vxlnetwork::rep_weights rep_weights;
	std::atomic<uint64_t> cemented_count{ 0 };
	std::atomic<uint64_t> block_count{ 0 };
	std::atomic<uint64_t> pruned_count{ 0 };
	std::atomic<uint64_t> account_count{ 0 };
	std::atomic<bool> final_votes_confirmation_canary{ false };
};

/* Defines the possible states for an election to stop in */
enum class election_status_type : uint8_t
{
	ongoing = 0,
	active_confirmed_quorum = 1,
	active_confirmation_height = 2,
	inactive_confirmation_height = 3,
	stopped = 5
};

/* Holds a summary of an election */
class election_status final
{
public:
	std::shared_ptr<vxlnetwork::block> winner;
	vxlnetwork::amount tally;
	vxlnetwork::amount final_tally;
	std::chrono::milliseconds election_end;
	std::chrono::milliseconds election_duration;
	unsigned confirmation_request_count;
	unsigned block_count;
	unsigned voter_count;
	election_status_type type;
};

vxlnetwork::wallet_id random_wallet_id ();
}
